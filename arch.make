CC=gcc
CFLAGS=-fsanitize=address

arch: arch.c

clean: 
	-rm arch
