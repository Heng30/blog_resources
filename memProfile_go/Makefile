#!/bin/sh

CC := go build
INSTALL:= go install

all: memProfile.go
	$(CC) $^

install:
	$(INSTALL) 

clean:
	rm -f memProfile mem_profile.dat
