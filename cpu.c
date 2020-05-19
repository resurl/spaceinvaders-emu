#include <stdlib.h>
#include <stdio.h>
#include "disassembler.c"

int main (int argc, char** argv) {
    int pc = 0;
    FILE *fp = fopen(argv[1], "rb"); // rb is reading in binary

    if (fp == NULL) {
        printf("cannot find file\n");
        exit(1);
    }

    fseek(fp, 0L, SEEK_END);
    int fsize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    unsigned char *buffer = malloc(fsize);

    fread(buffer, 1, fsize, fp);
    fclose(fp);
    
    while(pc < fsize)
        pc += Disassemble8080(buffer, pc);
        
    return 0;
}