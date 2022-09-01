#include "tcp.h"
#include "tcp_sock.h"
#include "ip.h"
#include "ether.h"

#include "log.h"
#include "list.h"

#include <stdlib.h>
#include <string.h>

// initialize tcp header according to the arguments
static void tcp_init_hdr(struct tcphdr *tcp, u16 sport, u16 dport, u32 seq, u32 ack,
						 u8 flags, u16 rwnd)
{
	memset((char *)tcp, 0, TCP_BASE_HDR_SIZE);
	tcp->sport = htons(sport);
	tcp->dport = htons(dport);
	tcp->seq = htonl(seq);
	tcp->ack = htonl(ack);
	tcp->off = TCP_HDR_OFFSET;
	tcp->flags = flags;
	tcp->rwnd = htons(rwnd);
}

//  append packet that just been sent to the send buffer
void tcp_send_buf_append(struct tcp_sock *tsk, char *packet, int len)
{
	tcp_send_buf_entry_t *entry = malloc(sizeof(tcp_send_buf_entry_t));
	entry->packet = malloc(len);
	entry->len = len;
	entry->retrans_times = 0;
	memcpy(entry->packet, packet, len);
	pthread_mutex_lock(&tsk->send_buf_lock);
	list_add_tail(&entry->list, &tsk->send_buf);
	pthread_mutex_unlock(&tsk->send_buf_lock);
}

// scan the send buffer and delete the entry that has been acked
// return whether send buffer is empty
int tcp_send_buf_clear(struct tcp_sock *tsk, u32 ack)
{
	pthread_mutex_lock(&tsk->send_buf_lock);
	tcp_send_buf_entry_t *entry, *q;
	list_for_each_entry_safe(entry, q, &tsk->send_buf, list)
	{
		struct tcphdr *tcp = packet_to_tcp_hdr(entry->packet);
		struct iphdr *ip = packet_to_ip_hdr(entry->packet);
		u32 len = ntohs(ip->tot_len) - IP_HDR_SIZE(ip) - TCP_HDR_SIZE(tcp);
		u32 seq_end = ntohl(tcp->seq) + len + ((tcp->flags & (TCP_SYN | TCP_FIN)) ? 1 : 0);
		if (less_or_equal_32b(seq_end, ack))
		{
			list_delete_entry(&entry->list);
			free(entry->packet);
			free(entry);
		}
		else
			break;
	}
	int ret = list_empty(&tsk->send_buf);
	pthread_mutex_unlock(&tsk->send_buf_lock);
	return ret;
}

//retran first entry of send buffer
int tcp_send_buf_retrans(struct tcp_sock *tsk)
{
	pthread_mutex_lock(&tsk->send_buf_lock);
	if (list_empty(&tsk->send_buf))
		return 1;
	tcp_send_buf_entry_t * entry = list_entry(tsk->send_buf.next, tcp_send_buf_entry_t, list);
	//尽可能大避免断开后重复建立连接浪费时间
	if (entry->retrans_times > 6)
	{
		pthread_mutex_unlock(&tsk->send_buf_lock);
		return 0;
	}
	int len = entry->len;
	int times = ++entry->retrans_times;
	char *packet = malloc(len);
	memcpy(packet, entry->packet, len);
	struct tcphdr *tcp = packet_to_tcp_hdr(packet);
	struct iphdr *ip = packet_to_ip_hdr(packet);
	tcp->ack = htonl(tsk->rcv_nxt);
	tcp->checksum = tcp_checksum(ip, tcp);
	ip_send_packet(packet, len);
	pthread_mutex_unlock(&tsk->send_buf_lock);
	return times;
}

// free send buffer
void tcp_send_buf_free(struct tcp_sock *tsk)
{
	tcp_send_buf_entry_t *entry, *q;
	list_for_each_entry_safe(entry, q, &tsk->send_buf, list)
	{
		list_delete_entry(&entry->list);
		free(entry->packet);
		free(entry);
	}
}

// send a tcp packet
//
// Given that the payload of the tcp packet has been filled, initialize the tcp
// header and ip header (remember to set the checksum in both header), and emit
// the packet by calling ip_send_packet.
void tcp_send_packet(struct tcp_sock *tsk, char *packet, int len)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	struct tcphdr *tcp = (struct tcphdr *)((char *)ip + IP_BASE_HDR_SIZE);

	int ip_tot_len = len - ETHER_HDR_SIZE;
	int tcp_data_len = ip_tot_len - IP_BASE_HDR_SIZE - TCP_BASE_HDR_SIZE;
	u32 saddr = tsk->sk_sip;
	u32 daddr = tsk->sk_dip;
	u16 sport = tsk->sk_sport;
	u16 dport = tsk->sk_dport;
	u32 seq = tsk->snd_nxt;
	u32 ack = tsk->rcv_nxt;
	u16 rwnd = tsk->rcv_wnd;

	tcp_init_hdr(tcp, sport, dport, seq, ack, TCP_PSH | TCP_ACK, rwnd);
	ip_init_hdr(ip, saddr, daddr, ip_tot_len, IPPROTO_TCP);

	tcp->checksum = tcp_checksum(ip, tcp);
	ip->checksum = ip_checksum(ip);
	tsk->snd_nxt += tcp_data_len;

	tcp_send_buf_append(tsk, packet, len);
	tcp_set_retrans_timer(tsk);
	ip_send_packet(packet, len);
}

// send a tcp control packet
//
// The control packet is like TCP_ACK, TCP_SYN, TCP_FIN (excluding TCP_RST).
// All these packets do not have payload and the only difference among these is
// the flags.
void tcp_send_control_packet(struct tcp_sock *tsk, u8 flags)
{
	char *packet = malloc(TCP_HEADER_LEN);
	if (!packet)
	{
		log(ERROR, "malloc tcp control packet failed.");
		return;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	struct tcphdr *tcp = (struct tcphdr *)((char *)ip + IP_BASE_HDR_SIZE);

	u16 tot_len = IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE;

	ip_init_hdr(ip, tsk->sk_sip, tsk->sk_dip, tot_len, IPPROTO_TCP);
	tcp_init_hdr(tcp, tsk->sk_sport, tsk->sk_dport, tsk->snd_nxt,
				 tsk->rcv_nxt, flags, tsk->rcv_wnd);

	tcp->checksum = tcp_checksum(ip, tcp);

	if (flags & (TCP_SYN | TCP_FIN))
	{
		tsk->snd_nxt += 1;
		tcp_send_buf_append(tsk, packet, TCP_HEADER_LEN);
		tcp_set_retrans_timer(tsk);
	}

	ip_send_packet(packet, TCP_HEADER_LEN);
}

// send tcp reset packet
//
// Different from tcp_send_control_packet, the fields of reset packet is
// from tcp_cb instead of tcp_sock.
void tcp_send_reset(struct tcp_cb *cb)
{
	char *packet = malloc(TCP_HEADER_LEN);
	log(DEBUG, "SEND RESET.");
	if (!packet)
	{
		log(ERROR, "malloc tcp control packet failed.");
		return;
	}
	struct iphdr *ip = packet_to_ip_hdr(packet);
	struct tcphdr *tcp = (struct tcphdr *)((char *)ip + IP_BASE_HDR_SIZE);

	u16 tot_len = IP_BASE_HDR_SIZE + TCP_BASE_HDR_SIZE;
	ip_init_hdr(ip, cb->daddr, cb->saddr, tot_len, IPPROTO_TCP);
	tcp_init_hdr(tcp, cb->dport, cb->sport, 0, cb->seq_end, TCP_RST | TCP_ACK, 0);
	tcp->checksum = tcp_checksum(ip, tcp);

	ip_send_packet(packet, TCP_HEADER_LEN);
}
