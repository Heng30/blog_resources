#!/bin/sh

hm : hash_bucket_mgr.c
	gcc -g -O0 -W -Wall -o $@ $^ -lpthread
	
clean:
	-rm -f hm *.o