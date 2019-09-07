#! /bin/sh

tm : transfer_file_mgr.c
	gcc -g -W -Wall -O0 -o $@ $^ -lpthread

clean:
	-rm -f tm *.o
