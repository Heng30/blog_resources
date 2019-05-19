#! /bin/sh

gm : global_mgr.c
	gcc -g -O0 -W -Wall -o $@ $^ -lpthread
	
clean:
	-rm -f gm *.o