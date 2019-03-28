#include <stdio.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h> 
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

// const char *multicast_addr= "224.0.1.1"; /* 多播组地址 */

int main(int argc, const char *argv[]) 
{ 
	if (argc < 3) {
		printf("Usage: %s multicast_addr multicast_port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
    int fd = 0; 
    struct sockaddr_in svr, in; 
    const char *buf = "I am multicast server"; 
	const char *multicast_addr = argv[1]; /* 多播组地址 */
	int port = strtol(argv[2], NULL, 10); 
	
    memset(&svr, 0, sizeof(svr)); 
	memset(&in, 0, sizeof(in));
	
    
    // svr.sin_addr.s_addr = inet_addr(multicast_addr); 
    svr.sin_family = AF_INET; 
	if (-1 == inet_pton(AF_INET, argv[1], &svr.sin_addr)) {
		printf("inet_pton error, errno: %d - %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	} 
    svr.sin_port = htons(port); 
	
	in.sin_family = AF_INET;
	in.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf( "socket error, errno: %d - %s\n", errno, strerror(errno)); 
		exit(EXIT_FAILURE);
    } 
	
	if(bind(fd, (struct sockaddr*)&in, sizeof(in)) < 0) { 
       printf("bind error: errno: %d - %s\n", errno, strerror(errno)); 
	   close(fd);
       exit(EXIT_FAILURE);
    } 

    while (true) { 
        if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr*)&svr, sizeof(svr)) < 0) { 
            printf( "sendto error, errno: %d - %s\n", errno, strerror(errno)); 
			// exit(EXIT_FAILURE);
        } else { 
            printf("send to multicast group: %s, msg: %s\n", multicast_addr, buf); 
        } 
		
		sleep(1);
    } 
	
	close(fd);
    return 0; 
} 
