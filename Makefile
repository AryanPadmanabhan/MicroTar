CFLAGS = -Wall -Werror -g
CC = gcc $(CFLAGS)
SHELL = /bin/bash
CWD = $(shell pwd | sed 's/.*\///g')
AN = proj1

microtar: microtar_main.c file_list.o microtar.o
	$(CC) -o $@ $^ -lm

file_list.o: file_list.c file_list.h
	$(CC) -c $<

microtar.o: microtar.c microtar.h
	$(CC) -c $<


clean:
	rm -f *.o microtar
