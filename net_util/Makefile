#!/bin/sh

nu : net_util.c
	gcc -g -O0 -W -Wall -o $@ $^ -lpthread
	
clean:
	-rm -f nu *.o