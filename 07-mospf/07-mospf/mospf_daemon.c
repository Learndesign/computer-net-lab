#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"

#include "list.h"
#include "log.h"
#include "rtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;
pthread_mutex_t db_lock;

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);
	pthread_mutex_init(&db_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);
void *checking_database_thread(void *param);

static void send_mospf_lsu(void);
static void update_rtable();
void printdb();

void mospf_run()
{
	pthread_t hello, lsu, nbr, db;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
	pthread_create(&db, NULL, checking_database_thread, NULL);
}

void *sending_mospf_hello_thread(void *param)
{
	// fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
	while (1)
	{
		sleep(MOSPF_DEFAULT_HELLOINT);

		int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE;
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list)
		{
			char *packet = (char *)malloc(len);

			struct ether_header *eh = (struct ether_header *)packet;
			struct iphdr *ihr = packet_to_ip_hdr(packet);
			struct mospf_hdr *mospf_header = (struct mospf_hdr *)((char *)ihr + IP_BASE_HDR_SIZE);
			struct mospf_hello *hello = (struct mospf_hello *)((char *)mospf_header + MOSPF_HDR_SIZE);

			eh->ether_type = htons(ETH_P_IP);
			memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
			//组播目的MAC地址为01:00:5E:00:00:05
			u8 dhost[ETH_ALEN] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x05};
			memcpy(eh->ether_dhost, dhost, ETH_ALEN);
			//目的IP地址为224.0.0.5
			ip_init_hdr(ihr, iface->ip, MOSPF_ALLSPFRouters, len - ETHER_HDR_SIZE, IPPROTO_MOSPF);

			mospf_init_hdr(mospf_header, MOSPF_TYPE_HELLO,
						   MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE,
						   instance->router_id, 0);

			mospf_init_hello(hello, iface->mask);
			mospf_header->checksum = mospf_checksum(mospf_header);
			iface_send_packet(iface, packet, len);
		}
	}
	return NULL;
}

/* 周期检查邻居列表中⽼化的结点。 */
void *checking_nbr_thread(void *param)
{
	// fprintf(stdout, "TODO: neighbor list timeout operation.\n");
	while (1)
	{
		sleep(1);
		pthread_mutex_lock(&mospf_lock);
		int flag = 0;
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list)
		{
			mospf_nbr_t *nbr = NULL, *q;
			list_for_each_entry_safe(nbr, q, &iface->nbr_list, list)
			{
				if (nbr->alive++ >= 3 * MOSPF_DEFAULT_HELLOINT)
				{
					list_delete_entry(&nbr->list);
					free(nbr);
					iface->num_nbr--;
					flag = 1;
				}
			}
		}
		pthread_mutex_unlock(&mospf_lock);
		if (flag)
			send_mospf_lsu();
	}
	return NULL;
}

/* 周期性检查链路状态数据库中存储的⽼化的结点。 */
void *checking_database_thread(void *param)
{
	// fprintf(stdout, "TODO: link state database timeout operation.\n");
	while (1)
	{
		sleep(1);
		pthread_mutex_lock(&db_lock);
		mospf_db_entry_t *entry = NULL, *q;
		list_for_each_entry_safe(entry, q, &mospf_db, list)
		{
			if (entry->alive++ >= MOSPF_DATABASE_TIMEOUT)
			{
				list_delete_entry(&entry->list);
				free(entry);
			}
		}
		// printdb();
		// print_rtable();
		pthread_mutex_unlock(&db_lock);
	}
	return NULL;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	struct iphdr *ihr = packet_to_ip_hdr(packet);
	struct mospf_hdr *mhr = (struct mospf_hdr *)((char *)ihr + IP_BASE_HDR_SIZE);
	struct mospf_hello *hello = (struct mospf_hello *)((char *)mhr + MOSPF_HDR_SIZE);
	u32 rid = ntohl(mhr->rid);

	pthread_mutex_lock(&mospf_lock);
	mospf_nbr_t *nbr = NULL;
	list_for_each_entry(nbr, &iface->nbr_list, list)
	{
		if (nbr->nbr_id == rid)
		{
			nbr->alive = 0;
			pthread_mutex_unlock(&mospf_lock);
			return;
		}
	}
	nbr = malloc(sizeof(mospf_nbr_t));
	nbr->nbr_id = rid;
	nbr->nbr_ip = ntohl(ihr->saddr);
	nbr->alive = 0;
	nbr->nbr_mask = ntohl(hello->mask);
	list_add_tail(&nbr->list, &iface->nbr_list);
	iface->num_nbr++;
	pthread_mutex_unlock(&mospf_lock);
	send_mospf_lsu();
}

void *sending_mospf_lsu_thread(void *param)
{
	// fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
	while (1)
	{
		sleep(MOSPF_DEFAULT_LSUINT);
		send_mospf_lsu();
	}
	return NULL;
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
	struct iphdr *ihr = packet_to_ip_hdr(packet);
	struct mospf_hdr *mhr = (struct mospf_hdr *)((char *)ihr + IP_BASE_HDR_SIZE);
	struct mospf_lsu *lsu = (struct mospf_lsu *)((char *)mhr + MOSPF_HDR_SIZE);
	// if(lsu->ttl<=0)
	// 	return;

	u32 rid = ntohl(mhr->rid);
	u16 seq = ntohs(lsu->seq);
	u32 nadv = ntohl(lsu->nadv);
	pthread_mutex_lock(&db_lock);
	int found = 0;
	mospf_db_entry_t *entry = NULL, *q;
	list_for_each_entry_safe(entry, q, &mospf_db, list)
	{
		if (entry->rid == rid)
		{
			if (entry->seq >= seq)
			{
				pthread_mutex_unlock(&db_lock);
				return;
			}
			found = 1;
			free(entry->array);
			break;
		}
	}
	if (!found)
	{
		entry = (mospf_db_entry_t *)malloc(sizeof(mospf_db_entry_t));
		list_add_tail(&entry->list, &mospf_db);
	}
	entry->rid = rid;
	entry->seq = seq;
	entry->nadv = nadv;
	entry->alive = 0;
	entry->array = (struct mospf_lsa *)malloc(nadv * sizeof(struct mospf_lsa));
	struct mospf_lsa *lsa = (struct mospf_lsa *)(lsu + 1);
	for (int i = 0; i < nadv; i++)
	{
		entry->array[i].rid = ntohl(lsa[i].rid);
		entry->array[i].network = ntohl(lsa[i].network);
		entry->array[i].mask = ntohl(lsa[i].mask);
	}
	pthread_mutex_unlock(&db_lock);
	update_rtable();

	if (--lsu->ttl > 0)
	{
		iface_info_t *iface_pos = NULL;
		list_for_each_entry(iface_pos, &instance->iface_list, list)
		{
			mospf_nbr_t *nbr = NULL;
			list_for_each_entry(nbr, &iface_pos->nbr_list, list)
			{
				if (nbr->nbr_id == ntohl(mhr->rid))
					continue;
				char *forwarding_packet = (char *)malloc(len);
				memcpy(forwarding_packet, packet, len);

				struct iphdr *iph = packet_to_ip_hdr(forwarding_packet);
				iph->saddr = htonl(iface_pos->ip);
				iph->daddr = htonl(nbr->nbr_ip);

				struct mospf_hdr *mhr = (struct mospf_hdr *)((char *)iph + IP_HDR_SIZE(iph));
				mhr->checksum = mospf_checksum(mhr);
				iph->checksum = ip_checksum(iph);

				ip_send_packet(forwarding_packet, len);
			}
		}
	}
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (mospf->version != MOSPF_VERSION)
	{
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return;
	}
	if (mospf->checksum != mospf_checksum(mospf))
	{
		log(ERROR, "received mospf packet with incorrect checksum");
		return;
	}
	if (ntohl(mospf->aid) != instance->area_id)
	{
		log(ERROR, "received mospf packet with incorrect area id");
		return;
	}

	switch (mospf->type)
	{
	case MOSPF_TYPE_HELLO:
		handle_mospf_hello(iface, packet, len);
		break;
	case MOSPF_TYPE_LSU:
		handle_mospf_lsu(iface, packet, len);
		break;
	default:
		log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
		break;
	}
}

static void send_mospf_lsu()
{
	pthread_mutex_lock(&mospf_lock);
	iface_info_t *iface = NULL;
	int nbr_num = 0;
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		if (iface->num_nbr == 0)
		{
			nbr_num++;
		}
		else
		{
			nbr_num += iface->num_nbr;
		}
	}

	struct mospf_lsa array[nbr_num];
	int index = 0;

	list_for_each_entry(iface, &instance->iface_list, list)
	{
		if (iface->num_nbr == 0)
		{
			array[index].mask = htonl(iface->mask);
			array[index].network = htonl(iface->ip & iface->mask);
			array[index].rid = 0;
			index++;
		}
		else
		{
			mospf_nbr_t *nbr_pos = NULL;
			list_for_each_entry(nbr_pos, &iface->nbr_list, list)
			{
				array[index].mask = htonl(nbr_pos->nbr_mask);
				array[index].network = htonl(nbr_pos->nbr_ip & nbr_pos->nbr_mask);
				array[index].rid = htonl(nbr_pos->nbr_id);
				index++;
			}
		}
	}

	instance->sequence_num++;
	int len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE + nbr_num * MOSPF_LSA_SIZE;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list)
	{
		mospf_nbr_t *nbr_pos = NULL;
		list_for_each_entry(nbr_pos, &iface->nbr_list, list)
		{
			char *packet = (char *)malloc(len);
			bzero(packet, len);
			struct ether_header *eh = (struct ether_header *)packet;
			struct iphdr *ihr = packet_to_ip_hdr(packet);
			struct mospf_hdr *mospf_header = (struct mospf_hdr *)((char *)ihr + IP_BASE_HDR_SIZE);
			struct mospf_lsu *lsu = (struct mospf_lsu *)((char *)mospf_header + MOSPF_HDR_SIZE);

			eh->ether_type = htons(ETH_P_IP);
			memcpy(eh->ether_shost, iface->mac, ETH_ALEN);

			ip_init_hdr(ihr, iface->ip, nbr_pos->nbr_ip,
						len - ETHER_HDR_SIZE, IPPROTO_MOSPF);

			mospf_init_hdr(mospf_header, MOSPF_TYPE_LSU,
						   len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE,
						   instance->router_id, instance->area_id);

			mospf_init_lsu(lsu, nbr_num);
			memcpy(lsu + 1, array, sizeof(array));

			mospf_header->checksum = mospf_checksum(mospf_header);
			ip_send_packet(packet, len);
		}
	}
	pthread_mutex_unlock(&mospf_lock);
}

void printdb()
{
	mospf_db_entry_t *entry = NULL;
	fprintf(stdout, "RID\t\t\tNetwork\t\t\tMask\t\t\t Nbr\n");
	list_for_each_entry(entry, &mospf_db, list)
	{
		for (int i = 0; i < entry->nadv; i++)
		{
			fprintf(stdout, IP_FMT "\t" IP_FMT "\t" IP_FMT "\t" IP_FMT "\n",
					HOST_IP_FMT_STR(entry->rid),
					HOST_IP_FMT_STR(entry->array[i].network),
					HOST_IP_FMT_STR(entry->array[i].mask),
					HOST_IP_FMT_STR(entry->array[i].rid));
		}
	}
}

u32 router_list[ROUTER_NUM];
int graph[ROUTER_NUM][ROUTER_NUM];
int num;
int prev[ROUTER_NUM];
int dist[ROUTER_NUM];

void update_rtable()
{
	init_graph();
	Dijkstra(prev, dist);
	update_router(prev, dist);
}

void init_graph()
{
	memset(graph, INT8_MAX, sizeof(graph));
	mospf_db_entry_t *db_entry = NULL;
	router_list[0] = instance->router_id;
	num = 1;
	list_for_each_entry(db_entry, &mospf_db, list)
	{
		router_list[num] = db_entry->rid;
		num++;
	}

	db_entry = NULL;
	list_for_each_entry(db_entry, &mospf_db, list)
	{
		int u = get_router_list_index(db_entry->rid);
		for (int i = 0; i < db_entry->nadv; i++)
		{
			if (!db_entry->array[i].rid)
			{
				continue;
			}
			int v = get_router_list_index(db_entry->array[i].rid);
			graph[u][v] = graph[v][u] = 1;
		}
	}
}

int get_router_list_index(u32 rid)
{
	for (int i = 0; i < num; i++)
		if (router_list[i] == rid)
		{
			return i;
		}
	return -1;
}

void Dijkstra(int prev[], int dist[])
{
	int visited[ROUTER_NUM];
	for (int i = 0; i < ROUTER_NUM; i++)
	{
		dist[i] = INT8_MAX;
		prev[i] = FALSE;
		visited[i] = 0;
	}

	dist[0] = 0;

	for (int i = 0; i < num; i++)
	{
		//在没有访问的节点中选择离源节点最近的那个
		int u = min_dist(dist, visited, num);
		visited[u] = 1;
		//更新通过u后其他节点到源节点的距离
		for (int v = 0; v < num; v++)
		{
			if (visited[v] == 0 && dist[u] + graph[u][v] < dist[v])
			{
				dist[v] = dist[u] + graph[u][v];
				prev[v] = u;
			}
		}
	}
}

int min_dist(int *dist, int *visited, int num)
{
	int index = -1;
	for (int u = 0; u < num; u++)
	{
		if (visited[u])
		{
			continue;
		}
		if (index == -1 || dist[u] < dist[index])
		{
			index = u;
		}
	}
	return index;
}

void update_router(int prev[], int dist[])
{
	int visited[ROUTER_NUM] = {0};
	visited[0] = 1;

	rt_entry_t *rt_entry, *q;
	//删除非邻居节点
	list_for_each_entry_safe(rt_entry, q, &rtable, list)
	{
		if (rt_entry->gw)
			remove_rt_entry(rt_entry);
	}

	for (int i = 0; i < num; i++)
	{
		int t = min_dist(dist, visited, num);
		visited[t] = 1;

		mospf_db_entry_t *db_entry;
		list_for_each_entry(db_entry, &mospf_db, list)
		{
			if (db_entry->rid == router_list[t])
			{
				while (prev[t] != 0)
				{
					t = prev[t];
				}
				iface_info_t *iface;
				u32 gw;
				int Found = 0;
				list_for_each_entry(iface, &instance->iface_list, list)
				{
					mospf_nbr_t *nbr_pos;
					list_for_each_entry(nbr_pos, &iface->nbr_list, list)
					{
						if (nbr_pos->nbr_id == router_list[t])
						{
							Found = 1;
							gw = nbr_pos->nbr_ip;
							break;
						}
					}
					if (Found)
					{
						for (int i = 0; i < db_entry->nadv; i++)
						{
							u32 network = db_entry->array[i].network;
							u32 mask = db_entry->array[i].mask;
							int Flag = 0;
							rt_entry_t *rt_entry = NULL;
							list_for_each_entry(rt_entry, &rtable, list)
							{
								if (rt_entry->dest == network && rt_entry->mask == mask)
								{
									Flag = 1;
								}
							}
							if (!Flag)
							{
								rt_entry_t *new_entry = new_rt_entry(network, mask, gw, iface);
								add_rt_entry(new_entry);
							}
						}
						break;
					}
				}
			}
		}
	}
}