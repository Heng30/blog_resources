#!/bin/sh

lm: linker_mgr.c
	gcc -g -W -Wall -O0 -o $@ $^ -lpthread

clean:
	-rm -f lm *.o
