# Copyright (c) 2005-2007 Bart Massey
# ALL RIGHTS RESERVED
# Please see the file COPYING in this directory for license information.

BIN=/home/bart/bin/i686

CC=gcc
CFLAGS=-g -O4 -Wall -ansi -pedantic
OBJS=simhash.o crc32.o heap.o hash.o

simhash: $(OBJS)
	$(CC) $(CFLAGS) -o simhash $(OBJS) -lm

clean:
	-rm -f $(OBJS) simhash

install: simhash
	cp simhash $(BIN)

heap.o: heap.h

crc32.o: crc.h
