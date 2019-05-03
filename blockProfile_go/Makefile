#!/bin/sh

CC := go build
INSTALL:= go install

all: blockProfile.go
	$(CC) $^

install:
	$(INSTALL) 

clean:
	rm -f blockProfile block_profile.dat
