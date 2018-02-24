all: arch
arch.o: arch.c
	gcc -c arch.c -fsanitize=address

arch: arch.o
	gcc -o arch arch.o -fsanitize=address

