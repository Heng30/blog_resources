#!/bin/sh

sm: syslog_mgr.c
	gcc -O0 -W -Wall -o $@ $^

clean:
	-rm -f sm *.o
