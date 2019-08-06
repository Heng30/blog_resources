#!/bin/sh

tm: timer_mgr.c 
	gcc -O0 -g -W -Wall -o $@ $^ -lpthread

clean:
	-rm -f *.o tm
