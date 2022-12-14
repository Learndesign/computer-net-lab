#include "tcp_sock.h"

#include "log.h"

#include <stdlib.h>
#include <unistd.h>

const char *rfname = "./client-input.dat";
const char *wfname = "./server-output.dat";

// tcp server application, listens to port (specified by arg) and serves only one
// connection request
void *tcp_server(void *arg)
{
	u16 port = *(u16 *)arg;
	struct tcp_sock *tsk = alloc_tcp_sock();

	struct sock_addr addr;
	addr.ip = htonl(0);
	addr.port = port;
	if (tcp_sock_bind(tsk, &addr) < 0)
	{
		log(ERROR, "tcp_sock bind to port %hu failed", ntohs(port));
		exit(1);
	}

	if (tcp_sock_listen(tsk, 3) < 0)
	{
		log(ERROR, "tcp_sock listen failed");
		exit(1);
	}

	log(DEBUG, "listen to port %hu.", ntohs(port));

	struct tcp_sock *csk = tcp_sock_accept(tsk);

	log(DEBUG, "accept a connection.");

	char rbuf[1500];
	int rlen = 0;
	int tot = 0;
	FILE *fp = fopen(wfname, "wb");
	if (!fp)
		log(ERROR, "open err");
	while ((rlen = tcp_sock_read(csk, rbuf, sizeof(rbuf))) > 0)
	{
		fwrite(rbuf, 1, rlen, fp);
		tot += rlen;
	}
	fclose(fp);
	printf("tot:%d\n", tot);
	log(DEBUG, "close this connection.");

	tcp_sock_close(csk);

	return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data
void *tcp_client(void *arg)
{
	struct sock_addr *skaddr = arg;

	struct tcp_sock *tsk = alloc_tcp_sock();

	if (tcp_sock_connect(tsk, skaddr) < 0)
	{
		log(ERROR, "tcp_sock connect to server (" IP_FMT ":%hu)failed.",
			NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}

	FILE *fp = fopen(rfname, "rb");
	char wbuf[1400];
	int rlen;
	while ((rlen = fread(wbuf, 1, sizeof(wbuf), fp)) > 0)
	{
		tcp_sock_write(tsk, wbuf, rlen);
		usleep(1400);
	}
	fclose(fp);
	tcp_sock_close(tsk);
	return NULL;
}
