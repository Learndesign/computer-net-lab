#include "ip.h"
#include "icmp.h"
#include "arpcache.h"
#include "rtable.h"
#include "arp.h"

// #include "log.h"

#include <stdio.h>
#include <stdlib.h>

// initialize ip header 
void ip_init_hdr(struct iphdr *ip, u32 saddr, u32 daddr, u16 len, u8 proto)
{
	ip->version = 4;
	ip->ihl = 5;
	ip->tos = 0;
	ip->tot_len = htons(len);
	ip->id = rand();
	ip->frag_off = htons(IP_DF);
	ip->ttl = DEFAULT_TTL;
	ip->protocol = proto;
	ip->saddr = htonl(saddr);
	ip->daddr = htonl(daddr);
	ip->checksum = ip_checksum(ip);
}

// lookup in the routing table, to find the entry with the same and longest prefix.
// the input address is in host byte order
rt_entry_t *longest_prefix_match(u32 dst)
{
	// fprintf(stderr, "TODO: longest prefix match for the packet.\n");
	rt_entry_t *rt_entry = NULL, *rt_longest = NULL;
	list_for_each_entry(rt_entry, &rtable, list){
		if((rt_entry->dest & rt_entry->mask) == (dst & rt_entry->mask)) {
			if(!rt_longest || rt_longest->mask < rt_entry->mask) 
				rt_longest = rt_entry;	
		}
	}
	return rt_longest;
}

// send IP packet
//
// Different from forwarding packet, ip_send_packet sends packet generated by
// router itself. This function is used to send ICMP packets.
void ip_send_packet(char *packet, int len)
{
	// fprintf(stderr, "TODO: send ip packet.\n");
	struct iphdr *pkt_IPhead = packet_to_ip_hdr(packet);
	u32 dst_ip = ntohl(pkt_IPhead->daddr);
	rt_entry_t *rt = longest_prefix_match(dst_ip);
	
	if (rt->gw) // 该路由器任何端口IP都与目的IP不在同一网段
		iface_send_packet_by_arp(rt->iface, rt->gw, packet, len);
	else 
		iface_send_packet_by_arp(rt->iface, dst_ip, packet, len);
}
