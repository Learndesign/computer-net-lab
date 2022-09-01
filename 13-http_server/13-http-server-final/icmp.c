#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>

// send icmp packet
/* 
 * 按照格式填充 ICMP 包。其中，若发送的是 reply，则 Rest of ICMP Header 
 * 拷贝 Ping 包中的相应字段，否则 Rest of ICMP Header 前 4 字节设置为 0，
 * 接着拷贝收到数据包的 IP 头部和随后的 8 字节。
 */
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	// fprintf(stderr, "TODO: malloc and send icmp packet.\n");
	struct iphdr *ihr = packet_to_ip_hdr(in_pkt); // in_pkt的IP首部
	int pkt_len = 0;
	
	if (type == ICMP_ECHOREPLY)
		pkt_len = len;
	else
		pkt_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + IP_HDR_SIZE(ihr) + ICMP_HDR_SIZE + 8;
	
	char *packet = (char*)malloc(pkt_len * sizeof(char));
	struct ether_header *eh = (struct ether_header*)packet; 
	eh->ether_type = htons(ETH_P_IP);
	
	struct iphdr *packet_IPhead = packet_to_ip_hdr(packet); 
	rt_entry_t *rt = longest_prefix_match(ntohl(ihr->saddr));
	ip_init_hdr(packet_IPhead, rt->iface->ip, ntohl(ihr->saddr), pkt_len - ETHER_HDR_SIZE, 1);
	struct icmphdr *Icmp_head = (struct icmphdr*)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	Icmp_head->type = type;
	Icmp_head->code = code;
	
	int Rest_begin = ETHER_HDR_SIZE + IP_HDR_SIZE(packet_IPhead) + 4;
	
	if (type == ICMP_ECHOREPLY)
		memcpy(packet + Rest_begin, in_pkt + Rest_begin, pkt_len - Rest_begin);
	else {
		memset(packet + Rest_begin, 0, 4);
		memcpy(packet + Rest_begin + 4, in_pkt + ETHER_HDR_SIZE, IP_HDR_SIZE(ihr) + 8);
	}
	
	Icmp_head->checksum = icmp_checksum(Icmp_head, pkt_len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE);
	ip_send_packet(packet, pkt_len);
}
