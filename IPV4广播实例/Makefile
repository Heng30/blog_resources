#!/bin/sh

all: broadcast_cln broadcast_svr

broadcast_cln: broadcast_cln.c
	gcc -g -o $@ $<
	
broadcast_svr: broadcast_svr.c
	gcc -g -o $@ $<
	
clean:
	rm -f broadcast_cln broadcast_svr *.o