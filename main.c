#include "disassembler.h"
#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>

// handles read3 for shift register read
uint8_t machineIN(CPUState* state, uint8_t port) {
    uint8_t res;
    switch (port) {
        case 3: {
            uint16_t shift_val = state->ports.read3;
            res = shift_val >> (8- state->ports.write2) & 0xff; 
        } break;
    }
    return res;
}

// sets shift register accordingly
void machineOUT(CPUState* state, uint8_t port) {
    switch (port) {
        case 2: state->ports.write2 = state->a & 0x7; break;
        case 4: {
            // grab bit 15..8 of shift register
            uint8_t shift1 = state->ports.read3 >> 8;
            state->ports.read3 = state->a << 8 | shift1;
        } break;
    }
}

CPUState* initializeCPU() {
    CPUState* cpu = (CPUState*) calloc(1,sizeof(CPUState));
    cpu->mem = malloc(0x10000); // 64KB memory
    return cpu;
}

void loadFile(CPUState* state, char* file, uint32_t pos) {
    FILE *fp = fopen(file, "rb");
    if (fp == NULL) {
        printf("Error: Invalid file %s", file);
        exit(1);
    }

    fseek(fp,0L,SEEK_END);
    int fsize = ftell(fp);
    fseek(fp,0L,SEEK_SET);

    fread(&state->mem[pos],1,fsize, fp);

    fclose(fp);
}

int main(int argc, char** argv) {
    int done = 0;
    CPUState* CPU = initializeCPU();
    
    /*
    loadFile(CPU, "./rom/invaders.h", 0x0000);
    loadFile(CPU, "./rom/invaders.g", 0x0800);
    loadFile(CPU, "./rom/invaders.f", 0x1000);
    loadFile(CPU, "./rom/invaders.e", 0x1800); 
    */

    // for testing 
    loadFile(CPU, "./rom/cpudiag.bin", 0x0100);
    CPU->pc = 0x100;        // testing starts at 0x100
    CPU->mem[368] = 0x7;    // fixing bug in asm
    CPU->mem[0x59c] = 0xc3; // JMP to skip over DAA/ac test
    CPU->mem[0x59d] = 0xc2;
    CPU->mem[0x59e] = 0x05;

    while (!done) {
        unsigned char* opcode = &CPU->mem[CPU->pc];
        if (*opcode == 0xdb) { // IN D8
            uint8_t port = opcode[1];
            CPU->a = machineIN(CPU,port);
            CPU->pc+=2; 
        } else if (*opcode == 0xd3) { // OUT D8
            uint8_t port = opcode[1];
            machineOUT(CPU,port);
            CPU->pc+=2;
        } else {
            EmulateCPU(CPU);
        }

    }
        


    return 0;
}