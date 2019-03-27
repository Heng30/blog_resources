#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

int main(int argc, const char *argv[])  
{  
	if (argc < 2) {
		printf("Usage: %s broadcast_port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
    struct sockaddr_in addrto, from;   
	int sock = 0, ret = 0; 
	const int opt = 1;
	int len = sizeof(struct sockaddr_in);  
    char smsg[100]; 
	int port = strtol(argv[1], NULL, 10);
	
    memset(&from, 0, sizeof(from));
    memset(&addrto, 0, sizeof(addrto));  
	setvbuf(stdout, NULL, _IONBF, 0);   /* 设置标准输出无缓冲区 */
    fflush(stdout);
	
    addrto.sin_family = AF_INET;  
    addrto.sin_addr.s_addr = htonl(INADDR_ANY);  
    addrto.sin_port = htons(port);  

    from.sin_family = AF_INET;  
    from.sin_addr.s_addr = htonl(INADDR_ANY);  
    from.sin_port = htons(port);  
      
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {     
        printf("socket error, errno: %d - %s\n", errno, strerror(errno));   
        exit(EXIT_FAILURE); 
    }     
   
    if (-1 == setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt))) {
        printf("setsocket error, errno: %d - %s\n", errno, strerror(errno));  
        exit(EXIT_FAILURE); 
    }  
  
    if (bind(sock,(struct sockaddr *)&(addrto), sizeof(struct sockaddr_in)) == -1) {     
        printf("bind error, errno: %d - %s\n", errno, strerror(errno));  
        exit(EXIT_FAILURE);   
    }  
  
    while (true) {   
        ret = recvfrom(sock, smsg, sizeof(smsg) - 1, 0, (struct sockaddr*)&from, (socklen_t*)&len);  
        if (ret <= 0) printf("read error, errno: %d - %s\n", errno, strerror(errno));          
        else {
			smsg[ret] = '\0';
			printf("recv msg: %s\n", smsg); 
		}     
        sleep(1);  
    }  
  
    return 0;  
}  


