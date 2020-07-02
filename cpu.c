#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "disassembler.h"

#define FOR_CPUDIAG

typedef struct FlagRegister {
    uint8_t c:1;        // carry flag 
    uint8_t p:1;        // parity flag 
    uint8_t ac:1;       // auxillary carry flag
    uint8_t z:1;        // zero flag
    uint8_t s:1;        // sign flag
    uint8_t pad:3;
} FlagRegister;

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
    struct FlagRegister flags;
    uint8_t int_enable; // ??
} CPUState;

uint8_t parity(int x, int size) {
    int par = 0;
    x = (x & (1<<size)-1); // truncates num to desired length
    for(int i =0; i < size; i++) {
        if(x & 1) par++;
        x = x >> 1;
    }
    printf("%d", par);
    return (0 == (par&1));
}

uint16_t returnAddr(unsigned char* opcode) {
    return (opcode[2] << 8 | opcode[1]);
}

void ret(CPUState* state, int res) {
    if (res) {
        state->pc = (state->mem[state->sp] << 8) | state->mem[state->sp+1];
        state->sp += 2;
    } 
}

void call(CPUState* state, int res, unsigned char* op) {
    if (res) {
        state->mem[state->sp-1] = (state->pc >> 8);
        state->mem[state->sp-2] = (state->pc & 0xff);
        state->sp += 2;
        state->pc = returnAddr(op);
    } else {
        state->pc += 2;
    }
}

void printRegs(CPUState* state, uint8_t psw) {
    printf(" af: %02x%02x", state->a, psw);
    printf(" bc: %02x%02x", state->b, state->c);
    printf(" de: %02x%02x", state->d, state->e);
    printf(" hl: %02x%02x", state->h, state->l);
    printf(" pc: %04x", state->pc);
    printf(" sp: %02x", state->sp);
}

void printFlags(CPUState* state) {
    printf("    sz###p#c: %d%d", state->flags.s, state->flags.z);
    printf("###%d", state->flags.p);
    printf("#%d", state->flags.c);
}

// adds register value to accumulator, if reg = NULL then it's from memory (HL)
// should this return the answer instead and CPU handles store to mem?? thinking about coupling b/c of state
// TODO: testing
void addOp(uint8_t* reg, CPUState* state) {
    uint16_t answer = (uint16_t) state->a;
    //printf("initial a val: %d", answer);

    if (reg == NULL) {
        uint16_t* m = (uint16_t*) (state->h << 8 | state->l);
        //printf("m addr: %d, m val: %d", m, *m);
        answer += *m;
        //printf("answer: %d", answer);
        //free(m); // i think this is ok LOL
    } else {
        //printf("m addr: %d, m val: %d", reg, *reg);
        answer += (uint16_t) *reg;
        //printf("answer: %d", answer);
    }

    state->flags.z = ((answer & 0xff) == 0);
    state->flags.s = ((answer & 0x80) != 0);
    state->flags.c = (answer > 0xff);
    state->flags.p = parity(answer, 8);
    state->a = answer & 0xff;

    //printf("reg a val: %d", state->a);
}

void adcOp(uint8_t* reg, CPUState* state) {
    uint16_t sum = (uint16_t) state->a + (uint16_t) *reg + state->flags.c;
    state->flags.z = ((sum & 0xff) == 0);
    state->flags.c = (sum > 0xff);
    state->flags.p = parity(sum, 8);
    state->flags.s = ((sum & 0x80) != 0);
    state->a = sum & 0xff;
}

void subOp(uint8_t* reg, CPUState* state) {
    uint16_t diff = (uint16_t) state->a - (uint16_t) *reg;
    state->flags.z = (diff & 0xff) == 0;
    state->flags.c = (diff > 0xff);
    state->flags.s = (diff & 0x80 != 0);
    state->flags.p = parity(diff, 8);
    state->a = diff & 0xff;
}

// assuming reg is valid pointer to register value
// for space invaders, only have to implement for b and c regs, also don't have to handle ac (yet?)
void dcrOp(uint8_t* reg, CPUState* state) {
    uint16_t answer = (uint16_t) *reg - 1;
    state->flags.z = ((answer & 0xff) == 0);
    state->flags.p = parity(answer, 8);
    state->flags.s = ((answer & 0x80) != 0);
    *reg = answer;
}

void updateAllFlags(uint16_t val, CPUState* state) {
    state->flags.z = ((val & 0xff) == 0);
    state->flags.p = parity(val, 8);
    state->flags.s = ((val & 0x80) >= 0x80);
}

void UnimplementedInstruction(CPUState* state) {
    state->pc--;
    Disassemble8080(state->mem,state->pc);
    printf("Error: Unimplemented instruction %04x\n", state->mem[state->pc]);
    exit(1);
}

void EmulateCPU(CPUState* state) {
    unsigned char *opcode = &state->mem[state->pc]; // something like 0xff
    Disassemble8080(state->mem,state->pc);
    state->pc++;
    switch (*opcode) {
        case 0x00: break;                           // NOP
        case 0x01: {
            state->b = opcode[2];
            state->c = opcode[1];
            state->pc += 2;
        }                                           // LXI, B D8; BC = D8
            break;
        case 0x02: {
            uint16_t addr = state->b << 8 | state->c;
            state->mem[addr] = state->a;
        } break;                                    // STAX B; (BC) <- A
        case 0x03: {
            state->c++;
            if (state->c == 0)  
                state->b++;
        } break;                                    // INX B; BC <- BC + 1
        case 0x04: {
            state->b++;
            updateAllFlags(state->b, state);        // !!! check this 
        } break;                                    // INR B; B <- B + 1
        case 0x05: dcrOp(&state->b, state); break;  // dec B by 1
        case 0x06: {
            state->b = opcode[1];
            state->pc++;
        }                                           // MVI B, D8 B <- mem[pc+1]
            break;
        case 0x07: {
            uint8_t msb = state->a & 0x80;
            state->a = (state->a << 1) & 0xfe | msb;
            state->flags.c = msb;
        }  break;                                   // A << 1, bit 0 & carry = last bit 7 
        case 0x09: {
            uint32_t bc = (state->b) << 8 | state->c;
            uint32_t hl = (state->h) << 8 | state->l;
            uint32_t sum = bc + hl;
            state->h = (sum & 0xff00) >> 8;
            state->l = sum & 0xff;
            state->flags.c = (sum & 0xffff0000) > 0;
        } // HL <- BC + HL
            break;
        case 0x0a: {
            uint16_t addr = state->b << 8 | state->c;
            state->a = state->mem[addr];
        }  break; // LDAX B; A <- (BC)
        case 0x0b: UnimplementedInstruction(state);  break; // dec BC
        case 0x0c: UnimplementedInstruction(state);  break; 
        case 0x0d: dcrOp(&state->c, state);  break;
        case 0x0e: {
            state->c = opcode[1];
            state->pc++;
        }
            break;
        case 0x0f: {
            int8_t lowestBit = state->a & 0x1;
            state->flags.c = lowestBit;
            state->a = (lowestBit << 7) | (state->a >> 1);
        } // A >> 1, the truncated bit becomes bit 7 and the carry 
            break;
        case 0x11: {
            state->d = opcode[2];
            state->e = opcode[1];
            state->pc += 2;
        } // mov data[1],data[2] to ED
            break;   
        case 0x12: UnimplementedInstruction(state);  break;
        case 0x13: {
            state->e++;
            if (state->e==0)
                state->d++;
        } // INX D
            break;
        case 0x14: UnimplementedInstruction(state);  break;
        case 0x15: UnimplementedInstruction(state);  break;
        case 0x16: UnimplementedInstruction(state);  break;
        case 0x17: UnimplementedInstruction(state);  break;
        case 0x19: {
            uint32_t hl = (state->h) << 8 | state->l;
            uint32_t de = (state->d) << 8 | state->e;
            uint32_t sum = hl + de;
            state->h = (sum & 0xff00) >> 8;
            state->l =  sum & 0xff;
            state->flags.c = ((sum & 0xffff0000) > 0);
        } // DAD D
            break;
        case 0x1a: {
            uint16_t addr = (state->d << 8) | state->e;
            state->a = state->mem[addr];
        } // LDAX D
            break;
        case 0x1b: UnimplementedInstruction(state);  break; 
        case 0x1c: UnimplementedInstruction(state);  break;
        case 0x1d: UnimplementedInstruction(state);  break;
        case 0x1e: UnimplementedInstruction(state);  break;
        case 0x1f: UnimplementedInstruction(state);  break;
        case 0x21: {
            state->h = opcode[2];
            state->l = opcode[1];
            state->pc += 2; 
        } break; // LXI H, D16; HL = d16
        case 0x22: UnimplementedInstruction(state);  break;
        case 0x23: {
            state->l++;
            if (state->l == 0)
                state->h++;
            
        } break;
        case 0x24: UnimplementedInstruction(state);  break;
        case 0x25: UnimplementedInstruction(state);  break;
        case 0x26: {
            state->h = opcode[1];
            state->pc++;
        } break;
        case 0x27: UnimplementedInstruction(state);  break;
        case 0x29: {
            uint32_t hl = state->h << 8 | state->l;
            uint32_t sum = hl + hl;
            state->h = (sum & 0xff00) >> 8;
            state->l = sum & 0xff;
            state->flags.c = ((sum & 0xffff0000) > 1);
        } break;
        case 0x2a: UnimplementedInstruction(state);  break;
        case 0x2b: UnimplementedInstruction(state);  break;
        case 0x2c: UnimplementedInstruction(state);  break;
        case 0x2d: UnimplementedInstruction(state);  break;
        case 0x2e: UnimplementedInstruction(state);  break;
        case 0x2f: UnimplementedInstruction(state);  break;
        case 0x31: {
            state->sp = opcode[2] << 8 | opcode[1];
            state->pc += 2;     
        }  break; // LXI SP, D16; SP = d16
        case 0x32: {
            uint16_t addr = opcode[2] << 8 | opcode[1];
            state->mem[addr] = state->a;
            state->pc += 2;
        }  break; // STA adr; (adr) = A
        case 0x33: UnimplementedInstruction(state);  break;
        case 0x34: UnimplementedInstruction(state);  break;
        case 0x35: UnimplementedInstruction(state);  break;
        case 0x36: {
            uint16_t addr = state->h << 8 | state->l;
            state->mem[addr] = opcode[1];
            state->pc++;
        }  break;
        case 0x37: UnimplementedInstruction(state);  break;
        case 0x39: UnimplementedInstruction(state);  break;
        case 0x3a: {
            uint16_t addr = opcode[2] << 8 | opcode[1];
            state->a = state->mem[addr];
            state->pc += 2; 
        }  break;
        case 0x3b: UnimplementedInstruction(state);  break;
        case 0x3c: UnimplementedInstruction(state);  break;
        case 0x3d: UnimplementedInstruction(state);  break;
        case 0x3e: {
            state->a = opcode[1];
            state->pc++;
        }  break; // MVI A, D8
        case 0x3f: UnimplementedInstruction(state);  break;
        case 0x40: UnimplementedInstruction(state);  break;
        case 0x41: UnimplementedInstruction(state);  break;
        case 0x42: UnimplementedInstruction(state);  break;
        case 0x43: UnimplementedInstruction(state);  break;
        case 0x44: UnimplementedInstruction(state);  break;
        case 0x45: UnimplementedInstruction(state);  break;
        case 0x46: UnimplementedInstruction(state);  break;
        case 0x47: UnimplementedInstruction(state);  break;
        case 0x48: UnimplementedInstruction(state);  break;
        case 0x49: UnimplementedInstruction(state);  break;
        case 0x4a: UnimplementedInstruction(state);  break;
        case 0x4b: UnimplementedInstruction(state);  break;
        case 0x4c: UnimplementedInstruction(state);  break;
        case 0x4d: UnimplementedInstruction(state);  break;
        case 0x4e: UnimplementedInstruction(state);  break;
        case 0x4f: UnimplementedInstruction(state);  break;
        case 0x50: UnimplementedInstruction(state);  break;
        case 0x51: UnimplementedInstruction(state);  break;
        case 0x52: UnimplementedInstruction(state);  break;
        case 0x53: UnimplementedInstruction(state);  break;
        case 0x54: UnimplementedInstruction(state);  break;
        case 0x55: UnimplementedInstruction(state);  break;
        case 0x56: {
            uint16_t addr = state->h << 8 | state->l;
            state->d = state->mem[addr];
        } break; // mov d, m
        case 0x57: UnimplementedInstruction(state);  break;
        case 0x58: UnimplementedInstruction(state);  break;
        case 0x59: UnimplementedInstruction(state);  break;
        case 0x5a: UnimplementedInstruction(state);  break;
        case 0x5b: UnimplementedInstruction(state);  break;
        case 0x5c: UnimplementedInstruction(state);  break;
        case 0x5d: UnimplementedInstruction(state);  break;
        case 0x5e: {
            uint16_t addr = state->h << 8 | state->l;
            state->e = state->mem[addr];
        } break;
        case 0x5f: UnimplementedInstruction(state);  break;
        case 0x60: UnimplementedInstruction(state);  break;
        case 0x61: UnimplementedInstruction(state);  break;
        case 0x62: UnimplementedInstruction(state);  break;
        case 0x63: UnimplementedInstruction(state);  break;
        case 0x64: {
            state->h = state->h;
        }  break; // MOV H,H; H <- H
        case 0x65: UnimplementedInstruction(state);  break;
        case 0x66: {
            uint16_t addr = state->h << 8 | state->l;
            state->h = state->mem[addr];
        } break;
        case 0x67: UnimplementedInstruction(state);  break;
        case 0x68: {
            state->l = state->b;
        }  break; // MOV L,B; L <- B
        case 0x69: UnimplementedInstruction(state);  break;
        case 0x6a: {
            state->l = state->d;
        }  break; // MOV L,D; L <- D
        case 0x6b: UnimplementedInstruction(state);  break;
        case 0x6c: UnimplementedInstruction(state);  break;
        case 0x6d: UnimplementedInstruction(state);  break;
        case 0x6e: {
            uint16_t addr = state->h << 8 | state->l;
            state->l = state->mem[addr];
        }  break; // MOV L, M; L <- (HL)
        case 0x6f: state->l = state->a; break;
        case 0x70: UnimplementedInstruction(state);  break;
        case 0x71: UnimplementedInstruction(state);  break;
        case 0x72: UnimplementedInstruction(state);  break;
        case 0x73: UnimplementedInstruction(state);  break;
        case 0x74: {
            uint16_t addr = state->h << 8 | state->l;
            state->mem[addr] = state->h;
        }  break; // MOV M,H; (HL) <- H
        case 0x75: {
            uint16_t addr = state->h << 8 | state->l;
            state->mem[addr] = state->l;
        }  break; // MOV M,L; (HL) <- L
        case 0x76: exit(0); break;
        case 0x77: {
            uint16_t addr = state->h << 8 | state->l;
            state->mem[addr] = state->a;    
        } break;
        case 0x78: state->a = state->b;  break;
        case 0x79: UnimplementedInstruction(state);  break;
        case 0x7a: state->a = state->d; break;
        case 0x7b: state->a = state->e; break;
        case 0x7c: state->a = state->h; break;
        case 0x7d: UnimplementedInstruction(state);  break;
        case 0x7e: {
            uint16_t addr = state->h << 8 | state->l;
            state->a = state->mem[addr];
        } break;
        case 0x7f: UnimplementedInstruction(state);  break;
        case 0x80: addOp(&state->b, state); break;
        case 0x81: addOp(&state->c, state); break;
        case 0x82: addOp(&state->d, state); break;
        case 0x83: addOp(&state->e, state); break;
        case 0x84: addOp(&state->h, state); break;
        case 0x85: addOp(&state->l, state); break;
        case 0x86: addOp(NULL, state); break;
        case 0x87: addOp(&state->a, state); break;
        case 0x88: UnimplementedInstruction(state);  break;
        case 0x89: adcOp(&state->c, state);  break;
        case 0x8a: UnimplementedInstruction(state);  break;
        case 0x8b: UnimplementedInstruction(state);  break;
        case 0x8c: UnimplementedInstruction(state);  break;
        case 0x8d: UnimplementedInstruction(state);  break;
        case 0x8e: UnimplementedInstruction(state);  break;
        case 0x8f: UnimplementedInstruction(state);  break;
        case 0x90: UnimplementedInstruction(state);  break;
        case 0x91: UnimplementedInstruction(state);  break;
        case 0x92: UnimplementedInstruction(state);  break;
        case 0x93: UnimplementedInstruction(state);  break;
        case 0x94: {
            uint16_t diff = (uint16_t) state->a - (uint16_t) state->h;
            state->flags.z = (diff & 0xff) == 0;
            state->flags.c = (diff > 0xff);
            state->flags.s = (diff & 0x80 != 0);
            state->flags.p = parity(diff, 8);
            state->a = diff & 0xff;
        }  break; // SUB H; A <- A - H
        case 0x95: UnimplementedInstruction(state);  break;
        case 0x96: UnimplementedInstruction(state);  break;
        case 0x97: subOp(&state->a,state); break; // SUB A; A <- A - A
        case 0x98: UnimplementedInstruction(state);  break;
        case 0x99: UnimplementedInstruction(state);  break;
        case 0x9a: UnimplementedInstruction(state);  break;
        case 0x9b: UnimplementedInstruction(state);  break;
        case 0x9c: UnimplementedInstruction(state);  break;
        case 0x9d: UnimplementedInstruction(state);  break;
        case 0x9e: UnimplementedInstruction(state);  break;
        case 0x9f: UnimplementedInstruction(state);  break;
        case 0xa0: UnimplementedInstruction(state);  break;
        case 0xa1: UnimplementedInstruction(state);  break;
        case 0xa2: UnimplementedInstruction(state);  break;
        case 0xa3: UnimplementedInstruction(state);  break;
        case 0xa4: UnimplementedInstruction(state);  break;
        case 0xa5: UnimplementedInstruction(state);  break;
        case 0xa6: UnimplementedInstruction(state);  break;
        case 0xa7: {
            state->a = state->a & state->a;
            state->flags.z = (state->a == 0); 
            state->flags.c = 0;
            state->flags.ac = 0;
            state->flags.s = ((state->a & 0x80) == 0x80);
            state->flags.p = parity(state->a, 8); // implm. this
        } break;
        case 0xa8: {
            state->a = state->a ^ state->b;
            state->flags.z = (state->a == 0); 
            state->flags.c = 0;
            state->flags.ac = 0;
            state->flags.s = ((state->a & 0x80) == 0x80);
            state->flags.p = parity(state->a, 8);
        }  break; // XRA B; 
        case 0xa9: UnimplementedInstruction(state);  break;
        case 0xaa: UnimplementedInstruction(state);  break;
        case 0xab: UnimplementedInstruction(state);  break;
        case 0xac: UnimplementedInstruction(state);  break;
        case 0xad: {
            state->a = state->a ^ state->l;
            state->flags.z = (state->a == 0); 
            state->flags.c = 0;
            state->flags.ac = 0;
            state->flags.s = ((state->a & 0x80) == 0x80);
            state->flags.p = parity(state->a, 8);
        }  break; // XRA L; A <- A ^ L
        case 0xae: UnimplementedInstruction(state);  break;
        case 0xaf: {
            state->a = state->a ^ state->a;
            state->flags.z = (state->a == 0); 
            state->flags.c = 0;
            state->flags.ac = 0;
            state->flags.s = ((state->a & 0x80) == 0x80);
            state->flags.p = parity(state->a, 8);
        } break;
        case 0xb0: UnimplementedInstruction(state);  break;
        case 0xb1: {
            state->a = state->a | state->c;
            state->flags.z = (state->a == 0);
            state->flags.p = parity(state->a, 8);
            state->flags.s = ((state->a & 0x80) != 0);
            state->flags.c = 0;
        }  break;
        case 0xb2: UnimplementedInstruction(state);  break;
        case 0xb3: UnimplementedInstruction(state);  break;
        case 0xb4: UnimplementedInstruction(state);  break;
        case 0xb5: {
            state->a = state->a | state->l;
            state->flags.z = (state->a == 0);
            state->flags.p = parity(state->a, 8);
            state->flags.s = ((state->a & 0x80) != 0);
            state->flags.c = 0;
            //UnimplementedInstruction(state);
        }  break; // ORA L; A <- A | L
        case 0xb6: {
            uint16_t addr = state->h << 8 | state->l;
            state->a = state->a | state->mem[addr];
            state->flags.z = (state->a == 0);
            state->flags.p = parity(state->a, 8);
            state->flags.s = ((state->a & 0x80) != 0);
            state->flags.c = 0;
        }  break; // ORA M; A <- A | (HL)
        case 0xb7: UnimplementedInstruction(state);  break;
        case 0xb8: {
            uint16_t diff = state->a - state->b;
            state->flags.c = (state->a < state->b);
            state->flags.z = (state->b == state->a);
            state->flags.s = ((diff & 0xff00) != 0);
            state->flags.p = parity(diff, 8);
            //UnimplementedInstruction(state);
        }  break; // CMP B; A - B
        case 0xb9: UnimplementedInstruction(state);  break;
        case 0xba: UnimplementedInstruction(state);  break;
        case 0xbb: {
            uint16_t diff = state->a - state->e;
            state->flags.c = (state->a < state->e);
            state->flags.z = (state->b == state->a);
            state->flags.s = ((diff & 0xff00) != 0);
            state->flags.p = parity(diff, 8);
        }  break; // CMP E; A - E
        case 0xbc: UnimplementedInstruction(state);  break;
        case 0xbd: {
            uint16_t diff = state->a - state->l;
            state->flags.c = (state->a < state->l);
            state->flags.z = (state->b == state->a);
            state->flags.s = ((diff & 0xff00) != 0);
            state->flags.p = parity(diff, 8);
        }  break; // CMP L; A - L
        case 0xbe: UnimplementedInstruction(state);  break;
        case 0xbf: UnimplementedInstruction(state);  break;
        case 0xc0: ret(state, !state->flags.z); break; // RNZ; if zero bit unset, return
        case 0xc1: {
            state->c = state->mem[state->sp];
            state->b = state->mem[state->sp + 1];
            state->sp += 2;
        } break;
        case 0xc2: {
            if (state->flags.z == 0) // if not zero 
                state->pc = opcode[2] << 8 | opcode[1];
            else
                state->pc += 2;
        }  break;
        case 0xc3: {
            state->pc = opcode[2] << 8 | opcode[1];
        }  break;
        case 0xc4: call(state, !state->flags.z,opcode); break; // CNZ adr; if not zero, call addr
        case 0xc5: {
            state->mem[state->sp-2] = state->c;
            state->mem[state->sp-1] = state->b;
            state->sp -= 2;
        } break;
        case 0xc6: {
            uint16_t sum = (uint16_t) state->a + (uint16_t) opcode[1];
            state->flags.z = ((sum & 0xff) == 0); 
            state->flags.c = (sum > 0xff);
            state->flags.s = ((sum & 0x80) == 0x80);
            state->flags.p = parity(sum, 8); // implm. this
            state->a = (uint8_t) sum;
            state->pc++;
        }  break; // ADI d8; A <- A + d8
        case 0xc7: UnimplementedInstruction(state);  break;
        case 0xc8: ret(state, state->flags.z);  break; // RZ if zero flag is set, RET
        case 0xc9: {
            state->pc = state->mem[state->sp+1] << 8 | state->mem[state->sp];
            state->sp += 2;
        } break;
        case 0xca: {
            if (state->flags.z)
                state->pc = returnAddr(opcode);
            else
                state->pc += 2;
        }  break;   // JZ addr; if zero flag set, pc <- adr
        case 0xcc: call(state, state->flags.z, opcode); break; // CZ adr; if zero flag set (1), call adr
        case 0xcd: 
        #ifdef FOR_CPUDIAG    
            if (5 ==  ((opcode[2] << 8) | opcode[1]))    
            {    
                if (state->c == 9)    
                {    
                    uint16_t offset = (state->d<<8) | (state->e);    
                    char *str = &state->mem[offset+3];  //skip the prefix bytes    
                    while (*str != '$')    
                        printf("%c", *str++);    
                    printf("\n");    
                }    
                else if (state->c == 2)    
                {    
                    //saw this in the inspected code, never saw it called    
                    printf ("print char routine called\n");    
                }    
            }    
            else if (0 ==  ((opcode[2] << 8) | opcode[1]))    
            {    
                exit(0);    
            }    
            else    
        #endif    
        {
            state->mem[state->sp-1] = ((state->pc+2) & 0xff00) >> 8;
            state->mem[state->sp-2] = (state->pc+2) & 0xff;
            state->sp -=2;
            state->pc = opcode[2] << 8 | opcode[1];
        }  break;   // CALL addr
        case 0xce: { 
            uint16_t sum = state->a + opcode[1] + state->flags.c;
            state->flags.z = ((sum & 0xff) == 0);
            state->flags.c = (sum > 0xff);
            state->flags.p = parity(sum, 8);
            state->flags.s = ((sum & 0x80) != 0);
            state->a = sum & 0xff;
            state->pc++;
        }  break; // ACI d8; A <- A + d8 + carry
        case 0xcf: UnimplementedInstruction(state);  break;
        case 0xd0: {
            ret(state, !state->flags.c); 
        }  break; // RNC; if carry bit unset, return
        case 0xd1: {
            state->e = state->mem[state->sp];
            state->d = state->mem[state->sp+1];
            state->sp += 2;
        } break;
        case 0xd2: {
            if (state->flags.c == 0)
                state->pc = returnAddr(opcode);
            else
                state->pc += 2;
        }  break; // JNC adr; if carry flags is not set, pc <- adr
        case 0xd3: {
            // write contents of accumulator to device # D8
            state->pc++;
        }  break; // OUT D8
        case 0xd4: call(state, !state->flags.c, opcode); break; // if no carry (carry=0), call addr
        case 0xd5: {
            state->mem[state->sp-2] = state->e;
            state->mem[state->sp-1] = state->d;
            state->sp -= 2;
        } break;
        case 0xd6: {
            uint16_t diff = state->a - opcode[1];
            state->flags.c = (opcode[1] > state->a);
            state->flags.p = parity(diff, 8);
            state->flags.z = (diff == 0);
            state->flags.s = ((diff & 0x80) == 0x80); 
            state->a = diff & 0xff;
            state->pc++; 
        }  break; // SUI d8; A = A - d8
        case 0xd7: UnimplementedInstruction(state);  break;
        case 0xd8: {
            ret(state, state->flags.c);
        }  break; // RC; if carry bit set, return
        case 0xda: {
            if(state->flags.c)
                state->pc = returnAddr(opcode);
            else 
                state->pc += 2;
        }  break; // JC adr; if carry is 1, pc <- adr
        case 0xdb: UnimplementedInstruction(state);  break;
        case 0xdc: call(state, state->flags.c, opcode); break; // if carry, call adr 
        case 0xde: {
            uint16_t diff = state->a - opcode[1] - state->flags.c;
            state->flags.c = (opcode[1] > state->a);
            state->flags.p = parity(diff, 8);
            state->flags.z = (diff == 0);
            state->flags.s = ((diff & 0x80) == 0x80); 
            state->pc++;
            state->a = diff & 0xff;
        }  break; // SBI D8; A = A - D8 - carry flag
        case 0xdf: UnimplementedInstruction(state);  break;
        case 0xe0: {
            ret(state, !state->flags.p);
        }  break; // RPO; if odd parity, return 
        case 0xe1: {
            state->l = state->mem[state->sp];
            state->h = state->mem[state->sp+1];
            state->sp += 2;
        } break;
        case 0xe2: {
            if(state->flags.p == 0) 
                state->pc = returnAddr(opcode);
            else
                state->pc += 2;
        }  break; // JPO addr; if odd parity, pc <- adr 
        case 0xe3: {
            uint8_t temp = state->mem[state->sp+1];
            state->mem[state->sp+1] = state->h;
            state->h = temp;
            temp = state->mem[state->sp];
            state->mem[state->sp] = state->l;
            state->l = temp;
        }  break; // XTHL; H <-> (SP+1) L <-> (SP)
        case 0xe4: {
            call(state, !state->flags.p, opcode);
        }  break; // CPO adr; if parity odd (0), call adr
        case 0xe5: {
            state->mem[state->sp-2] = state->l;
            state->mem[state->sp-1] = state->h;
            state->sp -=2;
        } break;
        case 0xe6: {
            state->a = state->a & opcode[1];
            state->flags.z = (state->a == 0);
            state->flags.s = ((state->a & 0x80) == 0x80);
            state->flags.c = 0;
            state->flags.ac = 0;
            state->flags.p = parity(state->a, 8);
            state->pc++;
        }  break; // ANI d8; 
        case 0xe7: UnimplementedInstruction(state);  break;
        case 0xe8: {
            ret(state, state->flags.p);
        }  break; // RPE; if even parity (1), return
        case 0xe9: UnimplementedInstruction(state);  break;
        case 0xea: {
            if (state->flags.p)
                state->pc = returnAddr(opcode);
            else
                state->pc += 2;
        }  break;   // JPE adr; if parity even, pc <- adr
        case 0xeb: {
            uint8_t temp = state->d;
            state->d = state->h;
            state->h = temp;
            temp = state->e;
            state->e = state->l;
            state->l = temp;
        } break;
        case 0xec: {
            call(state, state->flags.p, opcode);
        }  break; // CPE adr; if even parity (1) call adr
        case 0xee: {
            state->a = state->a ^ opcode[1];
            state->flags.z = (state->a == 0);
            state->flags.c = 0;
            state->flags.p = parity(state->a, 8);
            state->flags.s = (state->a & 0x80) == 0x80;
            state->pc++; 
        }  break; // XRI D8; A = A^D8
        case 0xef: UnimplementedInstruction(state);  break;
        case 0xf0: {
            ret(state, !state->flags.s);
        }  break; // RP; if pos (s=0), return
        case 0xf1: {
            int8_t spVal = state->mem[state->sp];
            state->flags.c = spVal & 1;
            state->flags.p = (spVal >> 1) & 1;
            state->flags.ac = (spVal >> 2) & 1;
            state->flags.z = (spVal >> 3) & 1;
            state->flags.s = (spVal >> 4) & 1;
            state->a = state->mem[state->sp+1];
            state->sp +=2;
        } break;
        case 0xf2: {
            if (state->flags.s == 0)
                state->pc = returnAddr(opcode);
            else
                state->pc += 2;
        }  break; // JP adr; if positive (sign bit is 0), pc <- adr
        case 0xf3: UnimplementedInstruction(state);  break;
        case 0xf4: call(state, !state->flags.s, opcode); break; // CP adr; if positive, call addr
        case 0xf5: {
            state->mem[state->sp-1] = state->a;
            uint8_t psw = (state->flags.c |
                          state->flags.p << 1|
                          state->flags.ac << 2 |
                          state->flags.z << 3|
                          state->flags.s << 4);
            state->mem[state->sp-2] = psw;
            state->sp -= 2;
        } break;
        case 0xf6: {
            state->a = state->a | opcode[1];
            state->flags.z = state->a;
            state->flags.p = parity(state->a, 8);
            state->flags.s = ((state->a & 0x80) == 0x80);
            state->flags.c = 0;
            state->pc++;
        }  break; // ORI d8; A = A | d8
        case 0xf7: UnimplementedInstruction(state); break;
        case 0xf8: {
            ret(state, state->flags.s);
        } break; // RM; if minus (s=1), return
        case 0xf9: UnimplementedInstruction(state); break;
        case 0xfa: {
            if (state->flags.s) // if sign flag is set
                state->pc = opcode[2] << 8 | opcode[1];
            else
                state->pc += 2;
        }  break; // JM adr; if flag s == 1, PC <- adr;
        case 0xfb: state->int_enable = 1; break;
        case 0xfc: {
            call(state, state->flags.s, opcode);
        }  break; // CM adr; if M (sign bit = 1) call addr
        case 0xfe: {
            uint8_t diff = state->a - opcode[1];
            state->flags.c = (opcode[1] > state->a);
            state->flags.p = parity(diff, 8);
            state->flags.z = (diff == 0);
            state->flags.s = ((diff & 0x80) == 0x80); 
            state->pc++;
        }  break; // CPI D8
        case 0xff: UnimplementedInstruction(state); break;
        default: break;
    }

    uint8_t psw = (state->flags.c |
                    state->flags.p << 1|
                    state->flags.ac << 2 |
                    state->flags.z << 3|
                    state->flags.s << 4);
    
    // for debugging
    printRegs(state, psw);
    printFlags(state);
    printf("\n");
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

    while (done == 0)
        EmulateCPU(CPU);

    return 0;
}