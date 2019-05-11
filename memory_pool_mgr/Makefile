#! /bin/sh

CC := gcc
FLAG := -g -O0 -Wall
LIB := -lpthread
SRC := memory_pool_mgr.c
PROG := mm

$(PROG) : $(SRC)
	$(CC) $(FLAG) -o $@ $< $(LIB)

clean:
	-rm -f $(PROG) *.o


