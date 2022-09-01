#ifndef __TCP_TIMER_H__
#define __TCP_TIMER_H__

#include "list.h"

#include <stddef.h>

struct tcp_timer
{
	int type;	 // time-wait: 0		retrans: >1
	int timeout; // in micro second
	struct list_head list;
	int enable;
};
extern pthread_mutex_t retrans_timer_lock;
struct tcp_sock;
#define timewait_to_tcp_sock(t) \
	(struct tcp_sock *)((char *)(t)-offsetof(struct tcp_sock, timewait))

#define retranstimer_to_tcp_sock(t) \
	(struct tcp_sock *)((char *)(t)-offsetof(struct tcp_sock, retrans_timer))
#define TCP_TIMER_SCAN_INTERVAL 100000
#define TCP_RETRANS_SCAN_INTERVAL 11000
#define TCP_MSL 1000000
#define TCP_TIMEWAIT_TIMEOUT (2 * TCP_MSL)
#define TCP_RETRANS_INTERVAL 6500
#define MAX_RETRANS_NUM 8

// the thread that scans timer_list periodically
void *tcp_timer_thread(void *arg);

void *tcp_retrans_timer_thread(void *arg);

// add the timer of tcp sock to timer_list
void tcp_set_timewait_timer(struct tcp_sock *);

void tcp_set_retrans_timer(struct tcp_sock *tsk);
void tcp_reset_retrans_timer(struct tcp_sock *tsk);
void tcp_unset_retrans_timer(struct tcp_sock *tsk);

#endif
