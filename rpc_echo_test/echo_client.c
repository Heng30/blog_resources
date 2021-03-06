#include <stdio.h>  
#include <unistd.h>
#include "echo.h"      /* echo.h generated by rpcgen */  

#define CHILE_MAX 10
 
int main(int argc, char **argv)  
{  
	if (argc != 3) {  
		fprintf(stderr, "usage: %s host message\n", argv[0]);  
		exit(EXIT_FAILURE);  
	} 
	
	CLIENT *clnt = NULL;  
	char *result = NULL, *server = NULL, *message = NULL; 
	char buf[100] = {0};
	int i = 0;
	
	server = argv[1];  
	message = buf;  
	
	for (i = 1; i < 10; i++) {
		if (!fork()) {
			snprintf(buf, sizeof(buf), "%s-%d", argv[2], i);
			break;
		}
		
		if (i == CHILE_MAX - 1) {
			snprintf(buf, sizeof(buf), "%s-%d", argv[2], 0);
		}
	}
	
	clnt = clnt_create(server, ECHOPROG, ECHOVERS, "TCP");   
	if (!clnt) {   
		clnt_pcreateerror(server);  
		exit(EXIT_FAILURE);  
	}  
	
	if (!(result = *echo_1(&message, clnt))) {  
		clnt_perror(clnt, server);  
		exit(EXIT_FAILURE);   
	}  
	
	printf("client send: %s\n", message);
	printf("client recieve: %s\n\n", result);    
	clnt_destroy(clnt);  
	exit(EXIT_SUCCESS);
}
