#include "arpcache.h"
#include "arp.h"
#include "ether.h"
#include "icmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static arpcache_t arpcache;

// initialize IP->mac mapping, request list, lock and sweeping thread
void arpcache_init()
{
	bzero(&arpcache, sizeof(arpcache_t));

	init_list_head(&(arpcache.req_list));

	pthread_mutex_init(&arpcache.lock, NULL);

	pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

// release all the resources when exiting
void arpcache_destroy()
{
	pthread_mutex_lock(&arpcache.lock);

	struct arp_req *req_entry = NULL, *req_q;
	list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
		struct cached_pkt *pkt_entry = NULL, *pkt_q;
		list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) {
			list_delete_entry(&(pkt_entry->list));
			free(pkt_entry->packet);
			free(pkt_entry);
		}

		list_delete_entry(&(req_entry->list));
		free(req_entry);
	}

	pthread_kill(arpcache.thread, SIGTERM);

	pthread_mutex_unlock(&arpcache.lock);
}

// lookup the IP->mac mapping
//
// traverse the table to find whether there is an entry with the same IP
// and mac address with the given arguments
int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN])
{
	// fprintf(stderr, "TODO: lookup ip address in arp cache.\n");
	pthread_mutex_lock(&arpcache.lock);
	for(int i=0;i<MAX_ARP_SIZE;i++){
		if(arpcache.entries[i].valid && arpcache.entries[i].ip4 == ip4){
			memcpy(mac,arpcache.entries[i].mac,ETH_ALEN);
			pthread_mutex_unlock(&arpcache.lock);
			return 1;
		}
	}
	pthread_mutex_unlock(&arpcache.lock);
	return 0;
}

// append the packet to arpcache
/* 
 * 在存储待处理数据包的列表中查找，如果已经有一个具有相同IP地址和iface的条目
 * (这意味着对应的arp请求已经发出)，只需将这个数据包附加到该条目的尾部(该条目可能包含多个数据包);
 * 否则，malloc指定IP地址和iface的新表项，附加报文，发送arp请求。 
 */
void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet, int len)
{
	// fprintf(stderr, "TODO: append the ip address if lookup failed, and send arp request if necessary.\n");
	struct arp_req *entry = NULL;
	struct cached_pkt *cached_packet = (struct cached_pkt*)malloc(sizeof(struct cached_pkt));
	cached_packet->packet = packet;
	cached_packet->len = len;
	
	int found=0;
	
	pthread_mutex_lock(&arpcache.lock);
	
	list_for_each_entry(entry, &(arpcache.req_list), list){
		if (entry->ip4 == ip4 && entry->iface == iface){
			found = 1;
			break;
		}
	}
	
	if (found)
		list_add_tail(&(cached_packet->list), &(entry->cached_packets));
	else{
		struct arp_req *req = (struct arp_req*)malloc(sizeof(struct arp_req));
		req->iface = iface;
		req->ip4 = ip4;
		req->sent = time(NULL);
		req->retries = 1;
		init_list_head(&(req->cached_packets));
		list_add_tail(&(cached_packet->list), &(req->cached_packets));
		list_add_tail(&(req->list), &(arpcache.req_list));
		arp_send_request(iface, ip4);
	}
	
	pthread_mutex_unlock(&arpcache.lock);
}

/* 
 * 在arpcache中插入IP->mac映射，如果缓存已满（32），随机选一个替换出去
 * 如果有等待这个映射的数据包，填充每个数据包的以太头，然后发送出去
 */
void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN])
{
	// fprintf(stderr, "TODO: insert ip->mac entry, and send all the pending packets.\n");
	pthread_mutex_lock(&arpcache.lock);
	
	int index = rand() % 32;// 随机⽣成的0～31的整数值,没找到无效项则对这一项覆盖
	
	for (int i=0; i<MAX_ARP_SIZE; i++){ 
	// valid==0说明表项⽆效，ip只对应一个mac
		if (!arpcache.entries[i].valid || arpcache.entries[i].ip4 == ip4){ 
			index = i;
			break;
		}
	}

	arpcache.entries[index].ip4 = ip4;
	memcpy(arpcache.entries[index].mac, mac, ETH_ALEN);
	arpcache.entries[index].added = time(NULL);
	arpcache.entries[index].valid = 1;

	struct arp_req *entry = NULL;
	struct arp_req *entry_next = NULL;

	list_for_each_entry_safe(entry, entry_next, &(arpcache.req_list), list) {
		if (entry->ip4 == ip4){
			struct cached_pkt *pkt = NULL;
			struct cached_pkt *pkt_next;
			
			list_for_each_entry_safe(pkt, pkt_next, &(entry->cached_packets), list){
				memcpy(pkt->packet, mac, ETH_ALEN);
				iface_send_packet(entry->iface, pkt->packet, pkt->len);
				free(pkt);
			}

			list_delete_entry(&(entry->list));
			free(entry);
		}
	}
	pthread_mutex_unlock(&arpcache.lock);
}

// sweep arpcache periodically
/* 
 * 对于IP->mac表项，如果该表项在表中存在时间超过15秒，则将其从表中删除。 
 * 对于待处理的报文，如果已经发送了5次arp请求，但是没有收到arp的回应，
 * 则对于每个等待的报文，发送icmp packet (DEST_HOST_UNREACHABLE)，然后丢弃这些报文。
 * 如果在1秒前发送了arp请求，但没有收到应答，则重新发送arp请求。   
 */
void *arpcache_sweep(void *arg) 
{
	while (1) {
		sleep(1);
		// fprintf(stderr, "TODO: sweep arpcache periodically: remove old entries, resend arp requests .\n");
		pthread_mutex_lock(&arpcache.lock);

		time_t now = time(NULL);
		struct arp_req *entry = NULL;
		struct arp_req *entry_next;

		for (int i=0; i<MAX_ARP_SIZE; i++) {
			if (arpcache.entries[i].valid && now - arpcache.entries[i].added > ARP_ENTRY_TIMEOUT)
				arpcache.entries[i].valid = 0;
		}
		
		list_for_each_entry_safe(entry, entry_next, &(arpcache.req_list), list) {
			if (entry->retries >= ARP_REQUEST_MAX_RETRIES) {
				struct cached_pkt *pkt = NULL, *pkt_next;
				list_for_each_entry_safe(pkt, pkt_next, &(entry->cached_packets), list){
					pthread_mutex_unlock(&arpcache.lock);
					icmp_send_packet(pkt->packet, pkt->len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
					pthread_mutex_lock(&arpcache.lock);
					free(pkt);
				}
				list_delete_entry(&(entry->list));
				free(entry);
				continue;
			}
			if (now - entry->sent > 1){
				arp_send_request(entry->iface, entry->ip4);
				entry->sent = now;
				entry->retries++;
			}
		}
		
		pthread_mutex_unlock(&arpcache.lock);
	}
	return NULL;
}
