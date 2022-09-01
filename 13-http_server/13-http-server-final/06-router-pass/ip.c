#include "ip.h"
#include "icmp.h"
#include "rtable.h"
#include "arp.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>

// handle ip packet
/*
 * 如果是ICMP echo-request报文，且目的IP地址等于接口IP地址，
 * 则发送ICMP echo-reply报文; 否则，请转发报文。
 */
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stderr, "TODO: handle ip packet.\n");
	struct iphdr *IP_head = packet_to_ip_hdr(packet);
	u32 dest_ip = ntohl(IP_head->daddr);

	if (dest_ip == iface->ip)
	{
		struct icmphdr *Icmp_head = (struct icmphdr *)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
		if (IP_head->protocol == IPPROTO_ICMP && Icmp_head->type == ICMP_ECHOREQUEST)
		{
			icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
		}
		free(packet);
	}
	else
	{
		rt_entry_t *rt = longest_prefix_match(dest_ip);

		if (rt)
		{ //路由表查找成功

			if (--IP_head->ttl <= 0)
			{
				icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
				free(packet);
			}
			else
			{
				IP_head->checksum = ip_checksum(IP_head);
				ip_send_packet(packet, len);
			}
		}
		else
		{
			icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_NET_UNREACH);
		}
	}
}
