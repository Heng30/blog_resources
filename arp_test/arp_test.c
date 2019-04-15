#include <net/if_arp.h>
#include <stdio.h>
#include <string.h> 
#include <netinet/in.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/un.h> 
#include <errno.h> 
#include <stdlib.h>
#include <unistd.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[])
{
	if (argc < 4) {
		printf("Usage: %s s interface ip mac\n", argv[0]); /* set */
		printf("Usage: %s g interface ip\n", argv[0]); /* get */
		printf("Usage: %s d interface ip\n", argv[0]); /* delete */
		exit(EXIT_FAILURE);
	}
	
	int fd = 0;
	struct arpreq arpreq;
	struct sockaddr_in *sin = NULL;
	unsigned char *ptr = NULL;
	
	memset(&arpreq, 0, sizeof(arpreq));
	ptr = &arpreq.arp_ha.sa_data[0];
	
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("socket error, error: %d - %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	sin = (struct sockaddr_in*)&arpreq.arp_pa;
	memset(sin, 0, sizeof(struct sockaddr_in));
	sin->sin_family = AF_INET;	
	
	if ( 1 != inet_pton(AF_INET, argv[3], &sin->sin_addr)) {
		printf("inet_pton error, errno: %d - %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);	
	}
	strncpy(arpreq.arp_dev, argv[2], sizeof(arpreq.arp_dev));
	
	if (*argv[1] == 's') { /* set */
		arpreq.arp_flags = ATF_COM; /* completed entry(enaddr vaild) */
		sscanf(argv[4], "%x:%x:%x:%x:%x:%x", ptr, ptr+1, ptr+2, ptr+3, ptr+4, ptr+5);
		if (ioctl(fd, SIOCSARP, &arpreq) < 0) {
			printf("ioctl error, errno: %d - %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		printf("set arp successfully, ip: %s, mac: %s\n", argv[3], argv[4]);
	} else if (*argv[1] == 'd') { /* delete */
		if (ioctl(fd, SIOCDARP, &arpreq) < 0) {
			printf("ioctl error, errno: %d - %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		printf("del arp successfully, ip: %s\n", argv[3]);
	} else if (*argv[1] == 'g') { /* get */
		if (ioctl(fd, SIOCGARP, &arpreq) < 0) {
			printf("ioctl error, errno: %d - %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		printf("%x:%x:%x:%x:%x:%x\n", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
	}
	
	exit(EXIT_SUCCESS);
}