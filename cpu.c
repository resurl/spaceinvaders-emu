#include <stdint.h>

struct FlagRegister {
    uint8_t c:1;        // carry flag 
    uint8_t pad1:1;     // always 1, unused
    uint8_t p:1;        // parity flag 
    uint8_t pad2:1;     // always 0, unused
    uint8_t ac:1;       // auxillary carry flag
    uint8_t pad3:1;     // always 0, unused
    uint8_t z:1;        // zero flag
    uint8_t s:1;        // sign flag
};

struct CPUState {

};