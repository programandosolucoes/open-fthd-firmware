CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -fPIC -I.
AR ?= ar

all: libfthd.a libfthd.so fthd-tool

libfthd.o: libfthd.c libfthd.h
	$(CC) $(CFLAGS) -c libfthd.c -o libfthd.o

libfthd.a: libfthd.o
	$(AR) rcs $@ $^

libfthd.so: libfthd.o
	$(CC) -shared -o $@ $^

fthd-tool: fthd_tool.c libfthd.a
	$(CC) $(CFLAGS) fthd_tool.c libfthd.a -o $@

clean:
	rm -f *.o *.a *.so fthd-tool

.PHONY: all clean
