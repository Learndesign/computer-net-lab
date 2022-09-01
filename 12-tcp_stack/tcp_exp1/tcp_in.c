#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"
#include "list.h"

#include <stdlib.h>
// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	pthread_mutex_lock(&tsk->wait_send->lock);
	tsk->snd_wnd = cb->rwnd;
	if (!greater_or_equal_32b(tsk->snd_nxt, (int)tsk->snd_una + tsk->snd_wnd))
		wake_up(tsk->wait_send);
	pthread_mutex_unlock(&tsk->wait_send->lock);
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

static inline void update_tsk(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(cb->seq, tsk->rcv_nxt) && greater_than_32b(cb->seq_end, tsk->rcv_nxt))
		tsk->rcv_nxt = cb->seq_end;
}

static void handle_recv_data(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	int len = (int)(cb->seq_end - tsk->rcv_nxt) - (cb->flags & (TCP_SYN | TCP_FIN) ? 1 : 0);
	if (!cb->pl_len || len <= 0)
	{
		update_tsk(tsk, cb);
		return;
	}
	pthread_mutex_lock(&tsk->wait_recv->lock);
	if (ring_buffer_free(tsk->rcv_buf) < cb->pl_len)
		log(DEBUG, "recv buffer full.");
	else
	{
		if (less_or_equal_32b(cb->seq, tsk->rcv_nxt))
		{
			write_ring_buffer(tsk->rcv_buf, cb->payload + cb->pl_len - len, len);
			update_tsk(tsk, cb);
			tcp_check_ofo_buf(tsk);
			wake_up(tsk->wait_recv);
			tsk->rcv_wnd = max(ring_buffer_free(tsk->rcv_buf), 1);
		}
		else
			tcp_ofo_buf_append(tsk, cb);
	}
	pthread_mutex_unlock(&tsk->wait_recv->lock);
}

static void handle_ack(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (cb->flags & TCP_ACK)
		if (less_than_32b(tsk->snd_una, cb->ack))
		{
			pthread_mutex_lock(&retrans_timer_lock);
			tsk->snd_una = cb->ack;
			tcp_reset_retrans_timer(tsk);
			if (tcp_send_buf_clear(tsk, tsk->snd_una))
			{
				tcp_unset_retrans_timer(tsk);
			}
			pthread_mutex_unlock(&retrans_timer_lock);
		}
	tcp_update_window(tsk, cb);
	if (tsk->state != TCP_LISTEN && less_than_32b(cb->seq, cb->seq_end))
		tcp_send_control_packet(tsk, TCP_ACK);
}

// check whether the sequence number of the incoming packet is in the receiving window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (tsk->state == TCP_LISTEN)
		return 1;
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	if (tsk->state == TCP_SYN_SENT)
	{
		if (cb->ack == tsk->snd_nxt)
			return 1;
		log(ERROR, "sent syn faild.");
		return 0;
	}
	if (!less_than_32b(cb->seq, rcv_end))
	{
		log(ERROR, "sent out window.");
		return 0;
	}
	if (!less_or_equal_32b(cb->ack, tsk->snd_nxt))
	{
		log(ERROR, "ack that never been sent.");
		return 0;
	}
	if (!less_or_equal_32b(tsk->rcv_nxt, cb->seq_end) ||
		(tsk->rcv_nxt == cb->seq_end && less_than_32b(cb->seq, cb->seq_end)))
	{
		// log(ERROR, "error,drop.");
		tcp_send_control_packet(tsk, TCP_ACK);
		return 0;
	}
	if (greater_than_32b(cb->seq, tsk->rcv_nxt) && cb->flags & (TCP_FIN | TCP_SYN))
	{
		log(ERROR, "SYN|FIN out of order.");
		return 0;
	}
	return 1;
}

// Process the incoming packet according to TCP state machine.
// 根据TCP状态机对接收的报文进行处理
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	// fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	if (!tsk)
		return;
	if (!is_tcp_seq_valid(tsk, cb))
		return;
	if (cb->flags & TCP_RST)
	{
		tsk->state = TCP_CLOSED;
		tcp_unhash(tsk);
		tcp_bind_unhash(tsk);
		return;
	}

	switch (tsk->state)
	{
	case TCP_LISTEN:
	{
		if (cb->flags & TCP_SYN)
		{
			struct tcp_sock *csk = alloc_tcp_sock();
			csk->sk_sip = cb->daddr;
			csk->sk_sport = cb->dport;
			csk->sk_dip = cb->saddr;
			csk->sk_dport = cb->sport;
			csk->rcv_nxt = cb->seq_end;
			csk->parent = tsk;
			csk->state = TCP_SYN_RECV;
			pthread_mutex_lock(&tsk->wait_accept->lock);
			tcp_sock_listen_enqueue(csk);
			pthread_mutex_unlock(&tsk->wait_accept->lock);
			tcp_hash(csk);
			tcp_send_control_packet(csk, TCP_SYN | TCP_ACK);
		}
		return;
	}
	case TCP_SYN_SENT:
	{
		if ((cb->flags & (TCP_ACK | TCP_SYN)) == (TCP_ACK | TCP_SYN))
		{
			tsk->state = TCP_ESTABLISHED;
			tsk->rcv_nxt = cb->seq_end;
			tsk->snd_una = cb->ack - 1;
			handle_ack(tsk, cb);
			// tcp_send_control_packet(tsk,TCP_ACK);
			wake_up(tsk->wait_connect);
		}
		return;
	}
	default:
	{
		break;
	}
	}

	if (tsk->state == TCP_ESTABLISHED || tsk->state == TCP_SYN_RECV || tsk->state == TCP_FIN_WAIT_1 || tsk->state == TCP_FIN_WAIT_2)
		handle_recv_data(tsk, cb);
	else
		update_tsk(tsk, cb);
	handle_ack(tsk, cb);

	switch (tsk->state)
	{
	case TCP_SYN_RECV:
	{
		if (cb->flags & TCP_ACK && cb->ack == tsk->snd_nxt)
		{
			struct tcp_sock *psk = tsk->parent;
			pthread_mutex_lock(&psk->wait_accept->lock);
			if (!tcp_sock_accept_queue_full(psk))
			{
				tsk->state = TCP_ESTABLISHED;
				tcp_sock_accept_enqueue(tsk);
				wake_up(psk->wait_accept);
			}
			pthread_mutex_unlock(&psk->wait_accept->lock);
		}
		break;
	}
	case TCP_ESTABLISHED:
	{
		if (cb->flags & TCP_FIN)
		{
			tsk->state = TCP_CLOSE_WAIT;
			while (greater_than_32b(tsk->snd_nxt, tsk->snd_una))
				sleep_on(tsk->wait_send);
			// tcp_send_control_packet(tsk,TCP_ACK);
			wake_up(tsk->wait_recv);
		}
		break;
	}
	case TCP_LAST_ACK:
	{
		if (cb->flags & (TCP_ACK && cb->ack == tsk->snd_nxt))
		{
			tsk->state = TCP_CLOSED;
			tcp_unhash(tsk);
		}
		break;
	}
	case TCP_FIN_WAIT_1:
	{
		tsk->state = TCP_FIN_WAIT_2;

		break;
	}
	case TCP_FIN_WAIT_2:
	{
		tsk->state = TCP_TIME_WAIT;
		// tcp_send_control_packet(tsk,TCP_ACK);
		tcp_set_timewait_timer(tsk);
		break;
	}
	default:
	{
		break;
	}
	}
}
