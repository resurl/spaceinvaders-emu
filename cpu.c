#include <stdint.h>

typedef struct FlagRegister {
    uint8_t c:1;        // carry flag 
    uint8_t pad1:1;     // always 1, unused
    uint8_t p:1;        // parity flag 
    uint8_t pad2:1;     // always 0, unused
    uint8_t ac:1;       // auxillary carry flag
    uint8_t pad3:1;     // always 0, unused
    uint8_t z:1;        // zero flag
    uint8_t s:1;        // sign flag
} FlagRegister;

typedef struct CPUState {
    uint8_t b;          // registers
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint8_t a;          // accumulator
    uint16_t pc;        // special registers
    uint16_t sp;
    uint8_t *mem;       // arr of bytes
    struct FlagRegister flags;
    uint8_t int_enable; // ??
} CPUState;

void UnimplementedInstruction(CPUState* state) {
    state->pc = state->pc - 1;
    printf("Error: Unimplemented instrcution.\n");
    exit(1);
}

void EmulateCPU(CPUState* state) {
    unsigned char *opcode = &state->mem[state->pc];
    switch (*opcode) {
        
    }

    state->pc += 1;
}