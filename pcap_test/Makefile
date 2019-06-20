#!/bin/sh

pt: pcap_test.c
	gcc -g -o $@ $^ -lpcap
	
clean:
	rm -f pt *.o