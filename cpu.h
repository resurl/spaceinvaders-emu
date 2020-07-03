#include <stdint.h>
#ifndef __cpu_h__
#define __cpu_h__


typedef struct FlagRegister {
    uint8_t c:1;        // carry flag 
    uint8_t p:1;        // parity flag 
    uint8_t ac:1;       // auxillary carry flag
    uint8_t z:1;        // zero flag
    uint8_t s:1;        // sign flag
    uint8_t pad:3;
} FlagRegister;

// IO ports 
typedef struct Ports {
    uint8_t read1;      // inputs
    uint8_t read2;      // inputs
    uint8_t read3;      // bit shift register read

    // only including relevant ports 
    uint8_t write2;
    uint8_t write4;
} Ports;

typedef struct CPUState {
    uint8_t b;          // registers 0..6
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint8_t a;          // accumulator
    uint16_t pc;        // special registers
    uint16_t sp;
    uint8_t *mem;       // arr of bytes
    struct Ports ports;
    struct FlagRegister flags;
    uint8_t int_enable; // ??
} CPUState;

void    EmulateCPU(CPUState* state);

#endif