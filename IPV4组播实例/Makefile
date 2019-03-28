#!/bin/sh

all: multicast_cln multicast_svr

multicast_svr: multicast_svr.c
	gcc -g -o $@ $<
	
multicast_cln: multicast_cln.c
	gcc -g -o $@ $<
	
clean:
	rm -f multicast_cln multicast_svr *.o