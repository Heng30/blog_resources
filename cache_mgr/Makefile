#!/bin/sh

cm: cache_mgr.c 
	gcc -O0 -g -W -Wall -o $@ $^ -lpthread

clean:
	-rm -f *.o cm
