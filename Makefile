CC=gcc
CFLAGS=-g -O4 -Wall -ansi -pedantic
OBJS=simhash.o crc32.o heap.o

simhash: $(OBJS)
	$(CC) $(CFLAGS) -o simhash $(OBJS)

clean:
	-rm -f $(OBJS) simhash

heap.o: heap.h

crc32.o: crc.h

