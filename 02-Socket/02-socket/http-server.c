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

#include "openssl/ssl.h"
#include "openssl/err.h"
#define THREAD_NUM 1024
#define BUFFER_SIZE 2048

void *handle_https_request(void* argc)
{
	// printf("> catch one https request\n");
	// fflush(stdout);

	SSL* ssl = (SSL*)argc;

    const char* response="HTTP/1.0 200 OK\r\nContent-Length: 27\r\n\r\nCNLab 2: Socket programming";
    //recv buffer
	char recv_buffer[BUFFER_SIZE] = {0};
	int recv_length;
	if (SSL_accept(ssl) == -1){
		perror("SSL_accept failed");
		exit(1);
	}
	//char buf[1024] = {0};
	recv_length= SSL_read(ssl, recv_buffer, BUFFER_SIZE);
	if (recv_length < 0) {
		perror("SSL_read failed");
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
		// printf("> begin: %d %d\n", range_begin, range_end);
		// fflush(stdout);

		// // print the target
		// printf("%s\n", URL_get);
		// fflush(stdout);
	}
	

	char send_buffer[BUFFER_SIZE] = {0};
	FILE *fp = fopen(URL_get, "r");
	

	if(fp == NULL){
		// printf("> [Error] no this file :%s", URL_get);
		// fflush(stdout);
		strcpy(send_buffer,"HTTP/1.1 404 File Not Found\r\nContent-Length: 27\r\n\r\nCNLab 2: Socket programming");
		SSL_write(ssl, send_buffer, strlen(send_buffer));
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
			
		// printf("file length: %d\n", file_length);
		// fflush(stdout);
		if(range_flag)
			strcat(send_buffer, "HTTP/1.1 206 Partial Content\r\nContent-Length: ");
		else
			strcat(send_buffer, "HTTP/1.1 200 OK\r\nContent-Length: ");
		sprintf(&(send_buffer[strlen(send_buffer)]),"%d" ,file_length);
		strcat(send_buffer, "\r\nContent-Type: text/html\r\nConnection: keep-alive\r\n\r\n");
		// printf("%s\n", send_buffer);
		// fflush(stdout);

		// send http first
		SSL_write(ssl, send_buffer, strlen(send_buffer));

		// send the data
		int flag = file_length % BUFFER_SIZE != 0;
		int time = file_length / BUFFER_SIZE + flag - 1;
		int ret = flag ? file_length % BUFFER_SIZE : BUFFER_SIZE;

		for (int i = 0; i < time; i++){
			fread(send_buffer, 1, BUFFER_SIZE, fp);
			SSL_write(ssl, send_buffer, BUFFER_SIZE);
		}

		if(flag)
			fread(send_buffer, 1, file_length % BUFFER_SIZE, fp);
		else
			fread(send_buffer, 1, BUFFER_SIZE, fp);

		SSL_write(ssl, send_buffer, ret);
	}


	//SSL_write(ssl, response, strlen(response));
    int sock = SSL_get_fd(ssl);
    SSL_free(ssl);
    close(sock);
}

void *handle_http_request(void *argc)
{
	// printf("> catch one http request\n");
	// fflush(stdout);
	int socketfd = *(int*)argc;
	// printf("> http client sock %d\n", socketfd);
	// fflush(stdout);
	
	//recv the http request
	char recv_buffer[BUFFER_SIZE] = {0};
	int recv_length;
	recv_length = recv(socketfd, recv_buffer, BUFFER_SIZE, 0);
	recv_buffer[recv_length] = '\0';
	printf("%s\n", recv_buffer);
	fflush(stdout);

	char URL_get[BUFFER_SIZE/2] = {0};
	int index = 0;
	int flag = 0;
	for (int i = 0; i < recv_length; i++){
		if(flag == 1){
			if(recv_buffer[i] == ' ')
				break;
			URL_get[index++] = recv_buffer[i];	
		}
		if(recv_buffer[i] == ' '){
			flag = 1;
		}
	}
	URL_get[index] = '\0';

	char send_buffer[BUFFER_SIZE] = {0};
	strcat(send_buffer, "HTTP/1.1 301 Moved Permanently\r\nLocation: https://10.0.0.1");
	strcat(send_buffer, URL_get);
	strcat(send_buffer,"\r\nContent-Length: 27\r\n\r\nCNLab 2: Socket programming");
	// printf("%s\n", send_buffer);
	// fflush(stdout);
	send(socketfd, send_buffer, strlen(send_buffer), 0);
	close(socketfd);
}



void *http_server(){
	// init socket, listening to port 40
	// set socket
	// printf("> http server createed!\n");
	// fflush(stdout);
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Opening socket failed");
		exit(1);
	}
	int enable = 1;
	// open address reuse function
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
		exit(1);
	}

	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(80);

	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		perror("Bind failed");
		exit(1);
	}
	listen(sock, 10);

	// Create many threads to blink the http clients
	pthread_t deal_http[THREAD_NUM];
	int id = 0;
	while (1){
		struct sockaddr_in caddr;
		socklen_t len;
		int csock = accept(sock, (struct sockaddr*)&caddr, &len);
		if (csock < 0) {
			perror("Accept failed");
			exit(1);
		}
		// printf("> http client sock %d\n", csock);
		// fflush(stdout);
		pthread_create(&deal_http[(id++) % THREAD_NUM], NULL, 
					handle_http_request, (void*)(&csock));
		id %= THREAD_NUM; 
	}
	close(sock);
}


int main()
{
	// create onthread to bink the http
	pthread_t http_thread;
	pthread_create(&http_thread, NULL, http_server, NULL);

	// init SSL Library
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	// printf("> https server createed!\n");
	// fflush(stdout);
	// enable TLS method
	const SSL_METHOD *method = TLS_server_method();
	SSL_CTX *ctx = SSL_CTX_new(method);

	// load certificate and private key
	if (SSL_CTX_use_certificate_file(ctx, "./keys/cnlab.cert", SSL_FILETYPE_PEM) <= 0) {
		perror("load cert failed");
		exit(1);
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, "./keys/cnlab.prikey", SSL_FILETYPE_PEM) <= 0) {
		perror("load prikey failed");
		exit(1);
	}

	// init socket, listening to port 443
	// set socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("Opening socket failed");
		exit(1);
	}
	int enable = 1;
	// open address reuse function
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
		exit(1);
	}

	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(443);

	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		perror("Bind failed");
		exit(1);
	}
	listen(sock, 10);

	// Crate many thread to deal the client 
	pthread_t deal_https[THREAD_NUM];
	int id = 0;
	while (1) {
		struct sockaddr_in caddr;
		socklen_t len;
		int csock = accept(sock, (struct sockaddr*)&caddr, &len);
		if (csock < 0) {
			perror("Accept failed");
			exit(1);
		}
		SSL *ssl = SSL_new(ctx); 
		SSL_set_fd(ssl, csock);
		pthread_create(&deal_https[(id++) % THREAD_NUM], NULL, 
				handle_https_request, (void *)ssl);
		id %= THREAD_NUM;
	}

	close(sock);
	SSL_CTX_free(ctx);

	// wait the http thread over, although it will never happen normally
	pthread_join(http_thread, NULL);
	return 0;
}
