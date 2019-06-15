#!/bin/sh

pm : pipe_mgr.c
	gcc -g -W -Wall -O0 -o $@ $^ -lpthread

clean:
	rm -f *.o pm
