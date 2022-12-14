#include "base.h"
#include <stdio.h>

extern ustack_t *instance;

void broadcast_packet(iface_info_t *iface, const char *packet, int len)
{
	// TODO: broadcast packet 
	fprintf(stdout, "TODO: broadcast packet.\n");
	
	iface_info_t *iface_n = NULL;
	list_for_each_entry(iface_n, &instance->iface_list, list) {
		if (iface_n->fd != iface->fd) 
			iface_send_packet(iface_n, packet, len);
	}
}
