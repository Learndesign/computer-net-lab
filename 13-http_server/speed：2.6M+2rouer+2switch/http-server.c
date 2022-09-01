#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>

#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "tcp_sock.h"
#include "log.h"

#define THREAD_NUM 20
// 修改buffersize减少启动tcp_sock_write次数，但加速效果一般
#define BUFFER_SIZE 10000

void *handle_http_request(void* argc)
{
	// printf("> catch one https request\n");
	// fflush(stdout);

	 struct tcp_sock * sock = (struct tcp_sock *)argc;

    const char* response="HTTP/1.0 200 OK\r\nContent-Length: 27\r\n\r\nCNLab 2: Socket programming";
    //recv buffer
	char recv_buffer[BUFFER_SIZE] = {0};
	int recv_length;

	recv_length= tcp_sock_read(sock, recv_buffer, BUFFER_SIZE);
	if (recv_length < 0) {
		perror("sock_read failed");
		exit(1);
	}
	recv_buffer[recv_length] = '\0';
	printf("%s\n", recv_buffer);
	fflush(stdout);

	// find out the https URL
	char URL_get[BUFFER_SIZE/2] = {0};
	int index = 0;
	int flag = 0;
	for (int i = 0; i < recv_length; i++){
		if(flag == 1){
			if(recv_buffer[i] == ' ')
				break;
			URL_get[index++] = recv_buffer[i];	
		}
		if(recv_buffer[i] == '/'){
			flag = 1;
		}
	}
	URL_get[index] = '\0';
	// whther range
	char *range = strstr(recv_buffer, "Range: bytes=");
	int range_flag = range == NULL ? 0 : 1;
	int range_begin = 0;
	int range_end = 0;
	// need read length
	int file_length;
	if(range_flag){
		range += strlen("Range: bytes=");
		
		char *diff = strchr(range, '-');
		*diff = '\0';
		range_begin = atoi(range); range_end = atoi(++diff);
	}
	

	char send_buffer[BUFFER_SIZE] = {0};
	FILE *fp = fopen(URL_get, "r");
	

	if(fp == NULL){
		// printf("> [Error] no this file :%s", URL_get);
		// fflush(stdout);
		strcpy(send_buffer,"HTTP/1.1 404 File Not Found\r\nContent-Length: 27\r\n\r\nCNLab 2: Socket programming");
		tcp_sock_write(sock, send_buffer, strlen(send_buffer));
		return NULL;
	} else {
		fseek(fp, 0, SEEK_END);
        file_length = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if(range_flag){
			file_length = range_end ? range_end - range_begin + 1 
								: file_length - range_begin; 
			fseek(fp, range_begin, SEEK_SET);
		}
			
		printf("file length: %d\n", file_length);
		fflush(stdout);
		if(range_flag)
			strcat(send_buffer, "HTTP/1.1 206 Partial Content\r\nContent-Length: ");
		else
			strcat(send_buffer, "HTTP/1.1 200 OK\r\nContent-Length: ");
		
		// strcat(send_buffer, "HTTP/1.1 200 OK\r\nContent-Length: ");
		
		sprintf(&(send_buffer[strlen(send_buffer)]),"%d" ,file_length);
		strcat(send_buffer, "\r\nContent-Type: text/html\r\nConnection: keep-alive\r\n\r\n");
		// printf("%s\n", send_buffer);
		// fflush(stdout);

		// send http first
		tcp_sock_write(sock, send_buffer, strlen(send_buffer));

		// send the data
		int flag = file_length % BUFFER_SIZE != 0;
		int time = file_length / BUFFER_SIZE + flag - 1;
		int ret = flag ? file_length % BUFFER_SIZE : BUFFER_SIZE;

		int send_len = 0;
		for (int i = 0; i < time; i++)
		{
			fread(send_buffer, 1, BUFFER_SIZE, fp);
			tcp_sock_write(sock, send_buffer, BUFFER_SIZE);
			send_len += BUFFER_SIZE;
			log(DEBUG, "has send %d/%d", send_len, file_length);
		}

		if(flag)
			fread(send_buffer, 1, file_length % BUFFER_SIZE, fp);
		else
			fread(send_buffer, 1, BUFFER_SIZE, fp);

		tcp_sock_write(sock, send_buffer, ret);
		send_len += ret;
		log(DEBUG, "has send %d/%d", send_len, file_length);
	}


    tcp_sock_close(sock);
}

void *http_server(){
	struct tcp_sock * sock = alloc_tcp_sock();
	if (sock == NULL) {
		perror("Opening socket failed");
		exit(1);
	}
	struct sock_addr addr;
	addr.ip = htonl(0);
	addr.port = htons(80);

	if ((tcp_sock_bind(sock, &addr) < 0)) {
		perror("Bind failed");
		exit(1);
	}
	tcp_sock_listen(sock, 3);
   	log(DEBUG, "listen to port %hu.", ntohs(addr.port));
	// Create many threads to blink the http clients
	pthread_t deal_http[THREAD_NUM];
	int id = 0;
	while (1){
		struct tcp_sock *csock = tcp_sock_accept(sock);
		if (csock == NULL) {
			perror("Accept failed");
			exit(1);
		}
		// printf("> http client sock %d\n", csock);
		// fflush(stdout);
		pthread_create(&deal_http[(id++) % THREAD_NUM], NULL, 
					handle_http_request, (void*)csock);
		id %= THREAD_NUM; 
	}
}
