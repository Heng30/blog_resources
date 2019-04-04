#!/bin/sh

md5Sum_test: md5Sum_test.c
	gcc -g -o $@ $^ -lcrypto
	
clean:
	rm -f md5Sum_test *.o