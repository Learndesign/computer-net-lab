#ifndef __MOSPF_DAEMON_H__
#define __MOSPF_DAEMON_H__
#define ROUTER_NUM 4
#define FALSE -1

#include "base.h"
#include "types.h"
#include "list.h"

void mospf_init();
void mospf_run();
void handle_mospf_packet(iface_info_t *iface, char *packet, int len);


void init_graph();
int get_router_list_index(u32 rid);
void Dijkstra(int prev[], int dist[]);
int min_dist(int *dist, int *visited, int num);
void update_router(int prev[], int dist[]);

#endif
