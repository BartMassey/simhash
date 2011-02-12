# Copyright (c) 2005-2007 Bart Massey
# ALL RIGHTS RESERVED
# Please see the file COPYING in this directory for license information.

DESTDIR=/usr/local
BIN=$(DESTDIR)/bin
MAN=$(DESTDIR)/man

CC=gcc
CFLAGS=-g -O4 -Wall -ansi -pedantic
OBJS=simhash.o crc32.o heap.o hash.o

simhash: $(OBJS)
	$(CC) $(CFLAGS) -o simhash $(OBJS) -lm

clean:
	-rm -f $(OBJS) simhash

install: simhash simhash.man
	cp simhash $(BIN)
	cp simhash.man $(MAN)/man1/simhash.1

heap.o: heap.h

crc32.o: crc.h
