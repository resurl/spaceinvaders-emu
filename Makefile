CC=gcc
LIBS=`sdl2-config --cflags --libs` 
TARGET=main.c
OBJS=main.c cpu.c disassembler.c 

build: main.c
	$(CC) -o 8080 $(OBJS) $(LIBS)