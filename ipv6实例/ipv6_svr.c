#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define LISTER_QUEUE 16
#define BUF_SIZE 1024

int main(int argc, char *argv[])
{	
	if (argc < 2) {
		printf("Usage: %s port\n", argv[0]);
		exit(-1);
	}
	
	static  const char *msg = "hello, I am server";
	int listen_port = strtol(argv[1], NULL, 10);
	int listen_fd = 0;
	socklen_t len = 0;
	struct sockaddr_in6 server_addr, client_addr;
	char client_ip[INET6_ADDRSTRLEN] = {0};
	int port = 0;
	int client_fd = 0;
	int rdlen = 0;
	char buf[BUF_SIZE] = {0};

	if ((listen_fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
		printf("socket error, errno: %d - %s\n", errno, strerror(errno));
		exit(1);
	}
	
	server_addr.sin6_family = AF_INET6;
	server_addr.sin6_port = htons(listen_port);
	server_addr.sin6_addr = in6addr_any;
	len = sizeof(client_addr);
	
	if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		printf("socket error, errno: %d - %s\n", errno, strerror(errno));
		exit(-1);
	}
	
	if (listen(listen_fd, LISTER_QUEUE) < 0) {
		printf("socket error, errno: %d - %s\n", errno, strerror(errno));
		exit(-1);
	}
	printf("listen...\n");
	
	while (true) {
		client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &len);
		if (client_fd < 0) {
			printf("accept error, errno: %d - %s\n", errno, strerror(errno));
			exit(-1);
		}
		
		if (!inet_ntop(AF_INET6, &client_addr.sin6_addr, client_ip, sizeof(client_ip))) {
			printf("inet_ntop error, errno: %d - %s\n", errno, strerror(errno));
			exit(-1);
		}
		port = ntohs(client_addr.sin6_port);
		printf("client ip: %s, port: %d, fd: %d\n", client_ip, port, client_fd);
		
		if ((rdlen = read(client_fd, buf, sizeof(buf) - 1)) > 0) {
			buf[rdlen] = '\0';
			printf("recv msg: %s\n", buf);
		} else {
			printf("read error, errno: %d - %s\n", errno, strerror(errno));
		}
		
		if (write(client_fd, msg, strlen(msg)) != strlen(msg)) {
			printf("write error, errno: %d - %s\n", errno, strerror(errno));
		}
		close(client_fd);
	}

        return 0;
}