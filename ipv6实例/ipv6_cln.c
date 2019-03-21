#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
	static const char *str = "hello, I am client";
	int client_fd = 0;
	struct sockaddr_in6 server_addr;
	int server_port = 0;
	char *server_ip = NULL;
	char *interface = NULL;
	char msg[BUF_SIZE];
	int rdlen = 0;
	
	if (argc < 3) {
		printf("Usage %s ip port [interface]\n", argv[0]);
		exit(-1);
	} else {
		server_ip = argv[1];
		server_port = strtol(argv[2], NULL, 10);
		if (argc == 4) {
			interface = argv[3];
		}
	}
	
	client_fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (client_fd < 0) {
		printf("socket error, errno: %d - %s\n", errno, strerror(errno));
		exit(-1);
	}
		
	server_addr.sin6_family = AF_INET6;
	server_addr.sin6_port = htons(server_port);
	if (inet_pton(AF_INET6, argv[1], &server_addr.sin6_addr) <= 0) {
		printf("inet_pton error, errno: %d - %s\n", errno, strerror(errno));
		exit(-1);
	}
	
	if (interface) server_addr.sin6_scope_id = if_nametoindex(interface); /* 仅通过链路地址通讯时需要设置 */
	
	if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		printf("connect error, errno: %d - %s\n", errno, strerror(errno));
		exit(-1);
	}
	printf("connect successfully...\n");
	
	if (write(client_fd, str, strlen(str)) != strlen(str)) {
		printf("write error, errno: %d - %s\n", errno, strerror(errno));
	}
	
	if ((rdlen = read(client_fd, msg, sizeof(msg) - 1)) > 0) {
		msg[rdlen] = '\0';
		printf("recv msg: %s\n", msg);
	} else printf("read error, errno: %d - %s\n", errno, strerror(errno));
	
	close (client_fd);
	return 0;
}
