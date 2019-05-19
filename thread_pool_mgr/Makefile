#!/bin/sh

tpm : thread_pool_mgr.c
	gcc -g -O0 -W -Wall -o $@ $^ -lpthread

clean:
	-rm -f tpm *.o
