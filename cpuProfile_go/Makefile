#!/bin/sh

CC := go build
INSTALL:= go install

all: cpuProfile.go
	$(CC) $^

install:
	$(INSTALL) 

clean:
	rm -f cpuProfile cpu_profile.dat
