#include <stdio.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define BUF_SIZE 1024

// const char *multicast_addr = "224.0.1.1 "; 

int main(int argc, char *argv[]) 
{ 
	if (argc < 3) {
		printf("Usage: %s multicast_addr multicast_port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
    int fd = 0, recvlen = 0; 
    struct sockaddr_in svr, cln; 
    int cln_len = sizeof(cln); 
    struct ip_mreq mreq; 
    char buf[BUF_SIZE] = {0}; 
	const char *multicast_addr = argv[1]; /* 多播组地址 */
	int port = strtol(argv[2], NULL, 10); 

    memset(&svr, 0, sizeof(svr));
	memset(&cln, 0, sizeof(cln));
	memset(&mreq, 0, sizeof(mreq));
	 
	// svr.sin_addr.s_addr = htonl(INADDR_ANY); 
    svr.sin_family = AF_INET; 
	if (-1 == inet_pton(AF_INET, argv[1], &svr.sin_addr)) {
		printf("inet_pton error, errno: %d - %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	} 
    svr.sin_port = htons(port); 

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
        printf("socket error, errno: %d - %s\n", errno, strerror(errno)); 
        exit(EXIT_FAILURE);
    } 

    if (bind(fd, (struct sockaddr*)&svr, sizeof(svr)) < 0) { 
        printf("bind error, errno: %d - %s\n", errno, strerror(errno)); 
		close(fd);
        exit(EXIT_FAILURE);
    } 

	/* 加入多播组 */
    // mreq.imr_multiaddr.s_addr = inet_addr(multicast_addr);
	if (-1 == inet_pton(AF_INET, multicast_addr, &mreq.imr_multiaddr.s_addr)) {
		printf("inet_pton error, errno: %d - %s\n", errno, strerror(errno));
		close(fd);
		exit(EXIT_FAILURE);
	} 
    mreq.imr_interface.s_addr = htonl(INADDR_ANY); 

    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) { 
        printf("setsockopt error, errno: %d - %s\n", errno, strerror(errno)); 
        close(fd);
		exit(EXIT_FAILURE); 
    } 

    while (true) { 
        if ((recvlen = recvfrom(fd, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&cln, &cln_len)) <= 0) { 
            printf("recvfrom error, errno: %d - %s\n", errno, strerror(errno)); 
        } else { 
            buf[recvlen] = '\0'; 
            printf("receive msg from: %s, msg: %s\n", inet_ntoa(cln.sin_addr), buf); 
        } 
		sleep(1);
    } 
	
	close(fd);
    return 0; 
}


