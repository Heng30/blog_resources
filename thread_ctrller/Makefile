#!/bin/sh

tc: thread_ctrller.c
	gcc -O0 -g -W -Wall -o $@ $^ -lpthread

clean:
	-rm -f *.o tc
