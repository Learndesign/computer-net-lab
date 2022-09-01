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
	tsk->snd_wnd = cb->rwnd;
	if (!greater_or_equal_32b(tsk->snd_nxt, (int)tsk->snd_una + tsk->snd_wnd))
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

static inline void update_tsk(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(cb->seq, tsk->rcv_nxt) && greater_than_32b(cb->seq_end, tsk->rcv_nxt))
		// tsk->rcv_nxt = cb->seq + 1;
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
			// pthread_mutex_lock(&tsk->wait_recv->lock);

			write_ring_buffer(tsk->rcv_buf, cb->payload + cb->pl_len - len, len);
			// pthread_mutex_unlock(&tsk->wait_recv->lock);

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

/* 当 ACK 确认了部分数据，重启定时器，只需要将时间重置即可。当 ACK 确认了所有
数据/SYN/FIN，扫描发送队列，移除 seq_end 小于当前 ACK 的项。若发送队列清空，则
关闭定时器。此外可能会收到旧的包，可能是因为不必要的重传，也可能是 ACK 丢失。
如果包含数据，则回复 ACK。 */
static void handle_ack(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (cb->flags & TCP_ACK)
		if (less_than_32b(tsk->snd_una, cb->ack))
		{
			pthread_mutex_lock(&retrans_timer_lock);
			tsk->snd_una = cb->ack;
			tcp_reset_retrans_timer(tsk);
			if (tcp_send_buf_clear(tsk, tsk->snd_una)) //返回1意味着send_buf已经被清空
			{
				tcp_unset_retrans_timer(tsk);
			}
			pthread_mutex_unlock(&retrans_timer_lock);
		}
	tcp_update_window(tsk, cb);
	if (tsk->state != TCP_LISTEN && tsk->state != TCP_FIN_WAIT_1 && less_than_32b(cb->seq, cb->seq_end))
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
		log(ERROR, "SEQ OUT OF WINDOW.");
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
		tcp_set_state(tsk, TCP_CLOSED);

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
			// csk->rcv_nxt = cb->seq_end;
			csk->rcv_nxt = cb->seq + 1;
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
			tcp_set_state(tsk, TCP_ESTABLISHED);

			// tsk->rcv_nxt = cb->seq_end;
			tsk->rcv_nxt = cb->seq + 1;
			tsk->snd_una = cb->ack;
			handle_ack(tsk, cb);
			// tcp_send_control_packet(tsk, TCP_ACK);
			wake_up(tsk->wait_connect);
		}
		return;
	}
	default:
	{
		break;
	}
	}

	// if (tsk->state == TCP_ESTABLISHED )
	if (tsk->state == TCP_ESTABLISHED || tsk->state == TCP_SYN_RECV || tsk->state == TCP_FIN_WAIT_1 || tsk->state == TCP_FIN_WAIT_2)
	{
		handle_recv_data(tsk, cb);
	}
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
				tcp_set_state(tsk, TCP_ESTABLISHED);
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
			tcp_set_state(tsk, TCP_CLOSE_WAIT);
			// fprintf(stdout, "error closed\n");
			while (greater_than_32b(tsk->snd_nxt, tsk->snd_una))
				sleep_on(tsk->wait_send);
			// tcp_send_control_packet(tsk, TCP_ACK);
			wake_up(tsk->wait_recv);
		}
		// else
		// {
		// 	if (cb->pl_len == 0)
		// 	{
		// 		tsk->rcv_nxt = cb->seq + 1;
		// 		tsk->snd_una = cb->ack;
		// 	}
		// 	else
		// 	{
		// 		handle_recv_data(tsk, cb);
		// 		tsk->rcv_nxt += 1;
		// 	}
		// }
		break;
	}
	case TCP_LAST_ACK:
	{
		if (cb->flags & (TCP_ACK && cb->ack == tsk->snd_nxt))
		{
			tcp_set_state(tsk, TCP_CLOSED);
			tcp_unhash(tsk);
		}
		break;
	}
	case TCP_FIN_WAIT_1:
	{
		tcp_set_state(tsk, TCP_FIN_WAIT_2);
		break;
	}
	case TCP_FIN_WAIT_2:
	{
		tcp_set_state(tsk, TCP_TIME_WAIT);
		// tcp_send_control_packet(tsk, TCP_ACK);
		tcp_set_timewait_timer(tsk);
		break;
	}
	default:
	{
		break;
	}
	}
}
