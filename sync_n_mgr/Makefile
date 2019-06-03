#! /bin/sh

sm : sync_n_mgr.c 
	gcc -g -W -Wall -O0 -o $@ $^ -lpthread

clean:
	-rm -f sm *.o
