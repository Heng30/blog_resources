#!/bin/sh

all : svr6 cln6

svr6 : ipv6_svr.c
	gcc -g -o $@ $^
	
cln6 : ipv6_cln.c
	gcc -g -o $@ $^
	
clean:
	rm -f svr6 cln6 *.o