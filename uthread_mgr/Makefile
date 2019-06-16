#!/bin/sh

um : uthread_mgr.c
	gcc -g -O0 -W -Wall -Wimplicit-fallthrough=1 -o $@ $^ -lpthread

clean:
	-rm -f um *.o
