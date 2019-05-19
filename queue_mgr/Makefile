#!/bin/sh

qm : queue_mgr.c
	gcc -g -O0 -W -Wall -o $@ $^ -lpthread

clean:
	-rm -f qm *.o
