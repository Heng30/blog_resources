#!/bin/sh

CC := go build
INSTALL := go install

all: selectTest.go
	$(CC) $^

install:
	$(INSTALL)

clean:
	rm -f selectTest
