/* echo_proc.c: implementation of the remote procedure "echo_server" */   
#include <stdio.h>   
#include <time.h> 
#include <string.h>
#include "echo.h" 

unsigned int client_count = 0;

/* Remote version of "echo" */   
char** echo_1_svc(char **msg, struct svc_req *req)  
{  
   static char *result = "Hello, i am rpc server"; 
   __sync_add_and_fetch(&client_count, 1);
   printf("client_count: %d\n", client_count);
   printf("server recieve: %s\n\n", *msg);  

   return &result;  
}
