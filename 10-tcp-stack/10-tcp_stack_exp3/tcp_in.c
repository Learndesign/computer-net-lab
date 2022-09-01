#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>
// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (old_snd_wnd == 0)
		wake_up(tsk->wait_send);
}

// update the snd_wnd safely: cb->ack should be between snd_una and snd_nxt
static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(tsk->snd_una, cb->ack) && less_or_equal_32b(cb->ack, tsk->snd_nxt))
		tcp_update_window(tsk, cb);
}

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window检查接收报文的序列号是否在接收窗口内
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end))
	{
		return 1;
	}
	else
	{
		log(ERROR, "received packet with invalid seq, drop it.");
		return 0;
	}
}

void handle_recv_data(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	while (ring_buffer_free(tsk->rcv_buf) < cb->pl_len)
	{
		sleep_on(tsk->wait_recv);
	}

	pthread_mutex_lock(&tsk->rcv_buf->lock);
	write_ring_buffer(tsk->rcv_buf, cb->payload, cb->pl_len);
	tsk->rcv_wnd -= cb->pl_len;
	pthread_mutex_unlock(&tsk->rcv_buf->lock);
	wake_up(tsk->wait_recv);
	tsk->rcv_nxt = cb->seq + cb->pl_len;
	tsk->snd_una = cb->ack;
	tcp_send_control_packet(tsk, TCP_ACK);
}

// Process the incoming packet according to TCP state machine.
// 根据TCP状态机对接收的报文进行处理
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	// fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	if (!tsk)
		return;
	if (cb->flags & TCP_RST)
	{
		tcp_sock_close(tsk);
		return;
	}

	switch (tsk->state)
	{
	case TCP_LISTEN:
	{
		if (cb->flags & TCP_SYN)
		{
			struct tcp_sock *csk = alloc_tcp_sock();
			memcpy((char *)csk, (char *)tsk, sizeof(struct tcp_sock));
			csk->parent = tsk;
			csk->sk_sip = cb->daddr;
			csk->sk_sport = cb->dport;
			csk->sk_dip = cb->saddr;
			csk->sk_dport = cb->sport;
			csk->iss = tcp_new_iss();
			csk->snd_nxt = csk->iss;
			csk->rcv_nxt = cb->seq + 1;
			tcp_sock_listen_enqueue(csk);
			tcp_set_state(csk, TCP_SYN_RECV);
			tcp_hash(csk); // hash to established_table
			tcp_send_control_packet(csk, TCP_SYN | TCP_ACK);
		}
		return;
	}
	case TCP_SYN_SENT:
	{
		if (cb->flags & (TCP_ACK | TCP_SYN))
		{
			tcp_set_state(tsk, TCP_ESTABLISHED);
			tsk->rcv_nxt = cb->seq + 1;
			tsk->snd_una = cb->ack;
			wake_up(tsk->wait_connect);
			tcp_send_control_packet(tsk, TCP_ACK);
		}
		return;
	}
	case TCP_SYN_RECV:
	{
		if (cb->flags & TCP_ACK)
		{
			if (tcp_sock_accept_queue_full(tsk))
				return;
			struct tcp_sock *csk = tcp_sock_listen_dequeue(tsk->parent);
			if (csk != tsk)
			{
				log(ERROR, "csk != tsk\n");
			}
			tcp_sock_accept_enqueue(tsk);
			tcp_set_state(tsk, TCP_ESTABLISHED);
			tsk->rcv_nxt = cb->seq;
			tsk->snd_una = cb->ack;
			wake_up(tsk->parent->wait_accept);
		}
		return;
	}
	default:
		break;
	}

	if (!is_tcp_seq_valid(tsk, cb))
		return;

	switch (tsk->state)
	{
	case TCP_ESTABLISHED:
	{
		if (cb->flags & TCP_FIN)
		{
			tcp_set_state(tsk, TCP_CLOSE_WAIT);
			tsk->rcv_nxt = cb->seq + 1;
			tsk->snd_una = cb->ack;
			tcp_send_control_packet(tsk, TCP_ACK);
			wait_exit(tsk->wait_recv);
			wait_exit(tsk->wait_send);
			// wake_up(tsk->wait_recv);
		}
		else if (cb->flags & TCP_ACK)
		{
			if (cb->pl_len == 0)
			{
				tsk->rcv_nxt = cb->seq;
				tsk->snd_una = cb->ack;
				tcp_update_window_safe(tsk, cb);
			}
			else
				handle_recv_data(tsk, cb);
		}
		break;
	}
	case TCP_LAST_ACK:
	{
		if (cb->flags & TCP_ACK)
		{
			tcp_set_state(tsk, TCP_CLOSED);
			tsk->rcv_nxt = cb->seq;
			tsk->snd_una = cb->ack;
			tcp_unhash(tsk);
			// tcp_bind_unhash(tsk);
		}
		break;
	}
	case TCP_FIN_WAIT_1:
	{
		if (cb->flags & TCP_ACK)
		{
			tcp_set_state(tsk, TCP_FIN_WAIT_2);
			tsk->rcv_nxt = cb->seq;
			tsk->snd_una = cb->ack;
		}
		break;
	}
	case TCP_FIN_WAIT_2:
	{
		if (cb->flags & TCP_FIN)
		{
			tcp_set_state(tsk, TCP_TIME_WAIT);
			tsk->rcv_nxt = cb->seq + 1;
			tsk->snd_una = cb->ack;
			tcp_send_control_packet(tsk, TCP_ACK);
			tcp_set_timewait_timer(tsk);
		}
		break;
	}
	default:
		break;
	}
}
