#!/bin/sh

bm : buffer_mgr.c 
	gcc -g -O0 -W -Wall -o $@ $^ -lpthread

clean:
	-rm -f bm *.o 1_data.txt 2_data.txt 3_data.txt
