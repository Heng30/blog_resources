#! /bin/sh

lm : log_mgr.c
	gcc -g -W -Wall -O0 -o $@ $^ -lpthread

clean:
	-rm -f lm *.o
