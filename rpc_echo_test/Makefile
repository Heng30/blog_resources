#!/bin/sh

RPCGEN := rpcgen
CC := gcc
PREFIX_PROG := echo

all : $(PREFIX_PROG).h $(PREFIX_PROG)_client $(PREFIX_PROG)_server

$(PREFIX_PROG).h : $(PREFIX_PROG).x
	$(RPCGEN) $<
	
$(PREFIX_PROG)_client : $(PREFIX_PROG)_client.c $(PREFIX_PROG)_clnt.c
	$(CC) -o $@ $^

$(PREFIX_PROG)_server : $(PREFIX_PROG)_proc.c $(PREFIX_PROG)_svc.c
	$(CC) -o $@ $^
	
clean: 
	rm -f $(PREFIX_PROG)_client $(PREFIX_PROG)_server $(PREFIX_PROG)_clnt.c $(PREFIX_PROG)_svc.c $(PREFIX_PROG).h *.o