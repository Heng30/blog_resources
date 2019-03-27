#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

int main(int argc, const char *argv[])  
{  
	if (argc < 3) {
		printf("Usage: %s broadcast_ip broadcast_port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
    int sock = 0, ret = 0;
	const int opt = 1;  
	struct sockaddr_in addrto;  
	const char *msg = "I am broadcast server";  
	int port = strtol(argv[2], NULL, 10);
	
    memset(&addrto, 0, sizeof(addrto)); 
	setvbuf(stdout, NULL, _IONBF, 0);  /* 设置标准输出无缓冲区 */
    fflush(stdout);  
	
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {     
        printf("socket error, errno: %d - %s\n", errno, strerror(errno));   
        exit(EXIT_FAILURE);  
    }     
      
    /* 设置该套接字为广播类型 */ 
    if (-1 == setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt))) { 
        printf("setsocket error, errno: %d - %s\n", errno, strerror(errno));  
		exit(EXIT_FAILURE);
    }  
  
    addrto.sin_family = AF_INET;  
   // addrto.sin_addr.s_addr = inet_addr(argv[1]);  
	if (-1 == inet_pton(AF_INET, argv[1], &addrto.sin_addr)) {
		printf("inet_pton error, errno: %d - %s\n", errno, strerror(errno));
		close(sock);
		exit(EXIT_FAILURE);
	}
    addrto.sin_port = htons(port);  
  
    while (true) {
        ret = sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&addrto, sizeof(addrto));  
        if (ret < 0) {  
            printf("send error, errno: %d - %s\n", errno, strerror(errno));  
        } else printf("send msg: %s\n", msg);    
        
		sleep(1);
    }  
  
    return 0;  
}  


