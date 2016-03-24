CC=gcc
CFLAGS=-std=c99 -Wall -pedantic -g3

all: encode decode

encode: lzw.o lzwHelper.o /c/cs323/Hwk4/code.o
	$(CC) $(CFLAGS) -o $@ $^

decode: encode
	rm -f $@
	ln $< $@

lzw.o: lzw.c lzwHelper.h

lzwHelper.o: lzwHelper.c lzwHelper.h

.PHONY: clean

clean:
	rm -f *.o
