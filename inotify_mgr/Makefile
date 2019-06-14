#! /bin/sh

im : inotify_mgr.c rb_tree_mgr.c
	gcc -g -W -Wall -O0 -o $@ $^ -lpthread

clean:
	-rm -f im *.o
