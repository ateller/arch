CC=gcc
CFLAGS=-fsanitize=address -g

dream-archiver: arch.c
	$(CC) -o dream-archiver arch.c $(CFLAGS)
clean: 
	-rm dream-archiver
