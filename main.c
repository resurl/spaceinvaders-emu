#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "cpu.h"
#include "disassembler.h"

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 224 
#define TICK (1000.0/60.0)
#define CYCLES_MS 2000
#define CYCLES_TICK (TICK * CYCLES_MS)

SDL_Window* window;
SDL_Surface* surface, *winsurface;

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

void inputHandler(CPUState* state) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_c: state->ports.read1 |= 1; break;        // start/credit
                case SDLK_u: state->ports.read1 |= 1 << 1; break;   // 2P start
                case SDLK_e: state->ports.read1 |= 1 << 2; break;   // 1P start
                case SDLK_w: state->ports.read1 |= 1 << 4; break;   // 1P shoot
                case SDLK_a: state->ports.read1 |= 1 << 5; break;   // 1P L
                case SDLK_d: state->ports.read1 |= 1 << 6; break;   // 1P R
                case SDLK_i: state->ports.read2 |= 1 << 4; break;   // P2 shoot
                case SDLK_j: state->ports.read2 |= 1 << 5; break;   // P2 L
                case SDLK_l: state->ports.read2 |= 1 << 6; break;   // P2 R
            }
        } else if (e.type == SDL_KEYUP) {
            switch (e.key.keysym.sym) {
                case SDLK_c: state->ports.read1 &= ~1; break;        // start/credit
                case SDLK_u: state->ports.read1 &= ~(1 << 1); break;   // 2P start
                case SDLK_e: state->ports.read1 &= ~(1 << 2); break;   // 1P start
                case SDLK_w: state->ports.read1 &= ~(1 << 4); break;   // 1P shoot
                case SDLK_a: state->ports.read1 &= ~(1 << 5); break;   // 1P L
                case SDLK_d: state->ports.read1 &= ~(1 << 6); break;   // 1P R
                case SDLK_i: state->ports.read2 &= ~(1 << 4); break;   // P2 shoot
                case SDLK_j: state->ports.read2 &= ~(1 << 5); break;   // P2 L
                case SDLK_l: state->ports.read2 &= ~(1 << 6); break;   // P2 R 
            }
        } else if (e.type == SDL_QUIT) {
            exit(0);
        }
    }
}

void initSDL() {
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("%s\n", SDL_GetError());
        exit(1);
    }

    window = SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN);

    if (window == NULL) {
        printf("SDL Error: %s\n", SDL_GetError());
        exit(1);
    }
    winsurface = SDL_GetWindowSurface(window);
    surface = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0, 0,0,0);
}

int main(int argc, char** argv) {
    int done = 0;
    CPUState* CPU = initializeCPU();
    initSDL();
    
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

    uint32_t now = SDL_GetTicks(); // in ms

    while (!done) {
        if (SDL_GetTicks() - now >= (1000.0/60.0)) {
            
            for (int i = 0; i < CYCLES_TICK/2;i++) {
                
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


            now = SDL_GetTicks();
        }


    }
        


    return 0;
}