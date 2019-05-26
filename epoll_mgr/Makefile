#!/bin/sh

em : epoll_mgr.c 
	gcc -g -O0 -W -Wall -o $@ $^ -lpthread

clean:
	-rm -rf em *.o
