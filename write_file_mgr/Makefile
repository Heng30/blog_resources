#! /bin/sh

wm : write_file_mgr.c
	gcc -g -W -Wall -O0 -o $@ $^ -lpthread

clean:
	-rm -f wm *.o
