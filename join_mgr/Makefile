#!/bin/sh

jm : join_mgr.c
	gcc -g -O0 -W -Wall -o $@ $^ -lpthread

clean:
	-rm -f jm *.o
