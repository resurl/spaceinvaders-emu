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
    //printf("%d", par);
    return (0 == (par&1));
}

uint16_t returnAddr(unsigned char* opcode) {
    return (opcode[2] << 8 | opcode[1]);
}

void ret(CPUState* state, int res) {
    if (res) {
        //printf("%02x%02x",state->mem[state->sp+1],state->mem[state->sp]);
        state->pc = state->mem[state->sp+1] << 8 | state->mem[state->sp];
        state->sp += 2;
    } 
}

void call(CPUState* state, int res, unsigned char* op) {
    if (res) {
        //printf("%02x%02x", (state->pc >> 8),(state->pc & 0xff));
        state->mem[state->sp-1] = (state->pc >> 8);
        state->mem[state->sp-2] = (state->pc & 0xff)+2;
        state->sp -= 2;
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

void mvi(CPUState* state, uint8_t* reg, unsigned char* opcode) {
    *reg = opcode[1];
    state->pc++;

}

uint16_t hl(CPUState* state) {
    uint16_t hl = state->h << 8 | state->l;
    return hl;
}

// adds register value to accumulator, if reg = NULL then it's from memory (HL)
// should this return the answer instead and CPU handles store to mem?? thinking about coupling b/c of state
// TODO: testing
void add(CPUState* state, uint8_t regval) {
    uint16_t answer = (uint16_t) state->a + (uint16_t) regval;
    state->flags.z = ((answer & 0xff) == 0);
    state->flags.s = ((answer & 0x80) != 0);
    state->flags.c = (answer > 0xff);
    state->flags.p = parity(answer, 8);
    state->a = answer & 0xff;
    //printf("reg a val: %d", state->a);
}

void adc(CPUState* state, uint8_t regval) {
    uint16_t sum = (uint16_t) state->a + (uint16_t) regval + state->flags.c;
    state->flags.z = ((sum & 0xff) == 0);
    state->flags.c = (sum > 0xff);
    state->flags.p = parity(sum, 8);
    state->flags.s = ((sum & 0x80) != 0);
    state->a = sum & 0xff;
}

void sub(CPUState* state, uint8_t regval) {
    uint16_t diff = (uint16_t) state->a - (uint16_t) regval;
    state->flags.z = (diff & 0xff) == 0;
    state->flags.c = (diff > 0xff);
    state->flags.s = ((diff & 0x80) == 0x80);
    state->flags.p = parity(diff, 8);
    state->a = diff & 0xff;
}

void sbb(CPUState* state, uint8_t regval) {
    uint16_t diff = (uint16_t) state->a - (uint16_t) regval - state->flags.c;
    state->flags.z = (diff & 0xff) == 0;
    state->flags.c = (diff > 0xff);
    state->flags.s = ((diff & 0x80) == 0x80);
    state->flags.p = parity(diff, 8);
    state->a = diff & 0xff;
}

void dad(CPUState* state, uint16_t val) {
    uint16_t hlval = state->h << 8 | state->l;
    uint32_t sum = hlval + val;
    state->flags.c = sum > 0xffff;
    state->h = (sum & 0xffff) >> 8;
    state->l = sum & 0xff;
}

void ana(CPUState* state, uint8_t regval) {
    state->a = state->a & regval;
    state->flags.c = 0;
    state->flags.z = state->a == 0;
    state->flags.s = (state->a & 0x80) == 0x80;
    state->flags.p = parity(state->a,8);
}

void ora(CPUState* state, uint8_t regval) {
    state->a = state->a | regval;
    state->flags.c = 0;
    state->flags.z = state->a == 0;
    state->flags.s = (state->a & 0x80) == 0x80;
    state->flags.p = parity(state->a,8);
}

void xra(CPUState* state, uint8_t regval) {
    state->a = state->a ^ regval;
    state->flags.z = (state->a == 0); 
    state->flags.c = 0;
    state->flags.s = ((state->a & 0x80) == 0x80);
    state->flags.p = parity(state->a, 8);
}

void cmp(CPUState* state, uint8_t regval) {
    uint16_t diff = state->a - regval;
    state->flags.s = regval > state->a;
    state->flags.z = regval == state->a;
    state->flags.c = ((diff & 0xff00) > 0xff);
    state->flags.p = parity(diff,8);
}
// assuming reg is valid pointer to register value
// for space invaders, only have to implement for b and c regs, also don't have to handle ac (yet?)
void dcr(uint8_t* reg, CPUState* state) {
    uint16_t answer = (uint16_t) *reg - 1;
    state->flags.z = ((answer & 0xff) == 0);
    state->flags.p = parity(answer, 8);
    state->flags.s = ((answer & 0x80) != 0);
    *reg = answer;
}

void stax(CPUState* state, uint8_t* reg1, uint8_t* reg2) {
    uint16_t adr = *reg1 << 8 | *reg2;
    state->mem[adr] = state->a;
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

void inr(CPUState* state, uint8_t* reg) {
     *reg = *reg + 1; 
     state->flags.z = (*reg == 0);
     state->flags.c = (*reg & 0x80 == 0x80);
     state->flags.p = parity(*reg, 8);
}

void dcx(uint8_t* reg1, uint8_t* reg2) {
    uint16_t pair = *reg1 << 8 | *reg2;
    pair -= 1;
    *reg1 = pair >> 8;
    *reg2 = pair & 0xff;
}

void mov(uint8_t* reg1, uint8_t regval) {
    *reg1 = regval;
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
        case 0x02: stax(state, &state->b, &state->c); break;                                    // STAX B; (BC) <- A
        case 0x03: {
            state->c++;
            if (state->c == 0)  
                state->b++;
        } break;                                    // INX B; BC <- BC + 1
        case 0x04: {
            state->b++;
            updateAllFlags(state->b, state);        // !!! check this 
        } break;                                    // INR B; B <- B + 1
        case 0x05: dcr(&state->b, state); break;  // dec B by 1
        case 0x06: mvi(state,&state->b,opcode); break; // MVI B, D8 B <- mem[pc+1]
        case 0x07: {
            uint8_t msb = (state->a & 0x80) == 0x80;
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
        case 0x0b: dcx(&state->b, &state->c);  break; // dec BC
        case 0x0c: inr(state, &state->c);  break; 
        case 0x0d: dcr(&state->c, state);  break;
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
        case 0x12: stax(state, &state->d, &state->e); break; // STAX D
        case 0x13: {
            state->e++;
            if (state->e==0)
                state->d++;
        } // INX D
            break;
        case 0x14: inr(state,&state->d); break; // INR D; D = D+1
        case 0x15: dcr(&state->d, state); break; // DCR D
        case 0x16: mvi(state, &state->d, opcode); break; // MVI D,D8
        case 0x17: {
            uint8_t msb = (state->a & 0x80) == 0x80;
            state->a = state->a << 1 | state->flags.c;
            state->flags.c = msb;
        }  break; // RAL 
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
        case 0x1b: dcx(&state->d, &state->e); break; // DCX D 
        case 0x1c: inr(state, &state->e); break; // INR E
        case 0x1d: dcr(&state->e, state); break; // DCR E
        case 0x1e: mvi(state, &state->e, opcode); break; // MVI E,D8
        case 0x1f: {
            uint8_t lsb = state->a & 1;
            state->a = state->a >> 1 | state->flags.c << 7;
            state->flags.c = lsb;
        }  break; // RAR
        case 0x21: {
            state->h = opcode[2];
            state->l = opcode[1];
            state->pc += 2; 
        } break; // LXI H, D16; HL = d16
        case 0x22: {
            uint16_t adr = returnAddr(opcode);
            state->mem[adr] = state->l;
            state->mem[adr+1] = state->h;
            state->pc+=2;
        }  break; //  adr; (adr) <- L, (adr+1) <- H
        case 0x23: {
            state->l++;
            if (state->l == 0)
                state->h++;
            
        } break;
        case 0x24: inr(state, &state->h); break; // INR H;
        case 0x25: dcr(&state->h, state); break; // DCR H
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
        case 0x2a: {
            uint16_t adr = returnAddr(opcode);
            state->l = state->mem[adr];
            state->h = state->mem[adr+1];
            state->pc+=2;
        }  break; // LHLD adr; L <- (adr), H <- (adr+1)
        case 0x2b: dcx(&state->h, &state->l); break; // DCX H
        case 0x2c: inr(state, &state->l); break; // INR L
        case 0x2d: dcr(&state->l, state); break; // DCR L
        case 0x2e: mvi(state, &state->l, opcode); break; // MVI L,D8
        case 0x2f: state->a = ~state->a; break;
        case 0x31: {
            state->sp = opcode[2] << 8 | opcode[1];
            state->pc += 2;     
        }  break; // LXI SP, D16; SP = d16
        case 0x32: {
            uint16_t addr = opcode[2] << 8 | opcode[1];
            state->mem[addr] = state->a;
            state->pc += 2;
        }  break; // STA adr; (adr) = A
        case 0x33: state->sp += 1;  break;
        case 0x34: inr(state, &state->mem[hl(state)]);  break;
        case 0x35: dcr(&state->mem[hl(state)], state);  break;
        case 0x36: {
            uint16_t addr = state->h << 8 | state->l;
            state->mem[addr] = opcode[1];
            state->pc++;
        }  break;
        case 0x37: state->flags.c = 1; break; // STC
        case 0x39: dad(state, state->sp); break; // DAD SP
        case 0x3a: {
            uint16_t addr = opcode[2] << 8 | opcode[1];
            state->a = state->mem[addr];
            state->pc += 2; 
        }  break;
        case 0x3b: {
            state->sp = state->sp - 1;
        } break; // DCX SP
        case 0x3c: inr(state, &state->a); break; // INR A; 
        case 0x3d: dcr(&state->a, state);  break; // DCR A;
        case 0x3e: mvi(state, &state->a, opcode); break; // MVI A, D8
        case 0x3f: state->flags.c = !state->flags.c; break; // CMC; CY=!Cy
        case 0x40: mov(&state->b, state->b);  break; // MOV B,B
        case 0x41: mov(&state->b, state->c);  break; // MOV B,C
        case 0x42: mov(&state->b, state->d);  break; // MOV B,D
        case 0x43: mov(&state->b, state->e);  break; // MOV B,E
        case 0x44: mov(&state->b, state->h);  break; // MOV B,H
        case 0x45: mov(&state->b, state->l);  break; // MOV B,L
        case 0x46: mov(&state->b, state->mem[hl(state)]); break; // MOV B,(HL)
        case 0x47: mov(&state->b, state->a); break; // MOV B,A; B <- A
        case 0x48: mov(&state->c, state->b); break; // MOV C,B
        case 0x49: mov(&state->c, state->c);  break; // MOV C,C
        case 0x4a: mov(&state->c, state->d);  break; // MOV C,D
        case 0x4b: mov(&state->c, state->e);  break; // MOV C,E
        case 0x4c: mov(&state->c, state->h);  break; // MOV C,H
        case 0x4d: mov(&state->c, state->l);  break; // MOV C,L
        case 0x4e: mov(&state->c, state->mem[hl(state)]);  break; // MOV C,(HL)
        case 0x4f: mov(&state->c, state->a);  break; // MOV C,A
        case 0x50: mov(&state->d, state->b);  break; // MOV D,B
        case 0x51: mov(&state->d, state->c);  break; // MOV D,C
        case 0x52: mov(&state->d, state->d);  break; // MOV D,D
        case 0x53: mov(&state->d, state->e);  break; // MOV D,E
        case 0x54: mov(&state->d, state->h);  break; // MOV D,H
        case 0x55: mov(&state->d, state->l);  break; // MOV D,L
        case 0x56: {
            uint16_t addr = state->h << 8 | state->l;
            state->d = state->mem[addr];
        } break; // MOV D,(HL)
        case 0x57: mov(&state->d, state->a);  break; // MOV D,A
        case 0x58: mov(&state->e, state->b);  break; // MOV E,B
        case 0x59: mov(&state->e, state->c);  break; // MOV E,C 
        case 0x5a: mov(&state->e, state->d);  break; // MOV E,D
        case 0x5b: mov(&state->e, state->e);  break; // MOV E,E
        case 0x5c: mov(&state->e, state->h);  break; // MOV E,H
        case 0x5d: mov(&state->e, state->l);  break; // MOB E,L
        case 0x5e: {
            uint16_t addr = state->h << 8 | state->l;
            state->e = state->mem[addr];
        } break; // MOV E,(HL)
        case 0x5f: mov(&state->e, state->a);  break; // MOV E,A
        case 0x60: mov(&state->h, state->b);  break; // MOV H,B
        case 0x61: mov(&state->h, state->c);  break; // MOV H,C
        case 0x62: mov(&state->h, state->d);  break; // MOV H,D
        case 0x63: mov(&state->h, state->e);  break; // MOV H,E
        case 0x64: mov(&state->h, state->h);  break; // MOV H,H; H <- H
        case 0x65: mov(&state->h, state->l);  break; // MOV H,L
        case 0x66: {
            uint16_t addr = state->h << 8 | state->l;
            state->h = state->mem[addr];
        } break; // MOV H,(HL)
        case 0x67: mov(&state->h, state->a);  break; // MOV H,A
        case 0x68: {
            state->l = state->b;
        }  break; // MOV L,B; L <- B
        case 0x69: mov(&state->l, state->c);  break; // MOV L,C
        case 0x6a: {
            state->l = state->d;
        }  break; // MOV L,D; L <- D
        case 0x6b: mov(&state->l, state->e);  break; // MOV L,E
        case 0x6c: mov(&state->l, state->h);  break; // MOV L,H
        case 0x6d: mov(&state->l, state->l);  break; // MOV L,L
        case 0x6e: {
            uint16_t addr = state->h << 8 | state->l;
            state->l = state->mem[addr];
        }  break; // MOV L, M; L <- (HL)
        case 0x6f: mov(&state->l,state->a); break; // MOV L,A
        case 0x70: mov(&state->mem[hl(state)], state->b); break; // MOV (HL),B
        case 0x71: mov(&state->mem[hl(state)], state->c);  break; // MOV (HL),C
        case 0x72: mov(&state->mem[hl(state)], state->d);  break; // MOV (HL),D
        case 0x73: mov(&state->mem[hl(state)], state->e);  break; // MOV (HL),E
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
        case 0x79: mov(&state->a, state->c);  break;
        case 0x7a: state->a = state->d; break;
        case 0x7b: state->a = state->e; break;
        case 0x7c: state->a = state->h; break;
        case 0x7d: mov(&state->a, state->l);  break;
        case 0x7e: {
            uint16_t addr = state->h << 8 | state->l;
            state->a = state->mem[addr];
        } break;
        case 0x7f: mov(&state->a, state->a);  break;
        case 0x80: add(state, state->b); break;
        case 0x81: add(state, state->c); break;
        case 0x82: add(state, state->d); break;
        case 0x83: add(state, state->e); break;
        case 0x84: add(state, state->h); break;
        case 0x85: add(state, state->l); break;
        case 0x86: add(state, state->mem[hl(state)]); break;
        case 0x87: add(state, state->a); break;
        case 0x88: adc(state, state->b); break; // ADC B
        case 0x89: adc(state, state->c);  break; // ADC C
        case 0x8a: adc(state, state->d);  break; // ADC D
        case 0x8b: adc(state, state->e); break; // ADC E
        case 0x8c: adc(state, state->h); break; // ADC H
        case 0x8d: adc(state, state->l);  break; // ADC L
        case 0x8e: adc(state, state->mem[hl(state)]);  break; // ADC (HL)
        case 0x8f: adc(state, state->a);  break; // ADC A
        case 0x90: sub(state, state->b); break; // SUB B
        case 0x91: sub(state, state->c);  break; // SUB C
        case 0x92: sub(state, state->d);  break; // SUB D
        case 0x93: sub(state, state->e);  break; // SUB E;
        case 0x94: sub(state, state->h); break; // SUB H; A <- A - H
        case 0x95: sub(state, state->l);  break; // SUB L
        case 0x96: {
            uint8_t hl = state->mem[state->h << 8 | state->l];
            uint16_t diff = state->a - hl;
            state->flags.z = diff == 0;
            state->flags.s = ((diff & 0x80) == 0x80);
            state->flags.c = ((diff & 0xff00) != 0);
            state->flags.p = parity(diff, 8);
            state->a = diff & 0x80;
        }  break;
        case 0x97: sub(state, state->a); break; // SUB A; A <- A - A
        case 0x98: sbb(state, state->b); break; // SBB B
        case 0x99: sbb(state, state->c);  break; // SBB C
        case 0x9a: sbb(state, state->d);  break; // SBB D
        case 0x9b: sbb(state, state->e);  break; // SBB E
        case 0x9c: sbb(state, state->h);  break; // SBB H
        case 0x9d: sbb(state, state->l);  break; // SBB L
        case 0x9e: sbb(state, state->mem[hl(state)]); break; // SBB (HL)
        case 0x9f: sbb(state, state->a);  break; // SBB A
        case 0xa0: ana(state, state->b); break; // ANA B
        case 0xa1: ana(state, state->c); break; // ANA C
        case 0xa2: ana(state, state->d); break;
        case 0xa3: ana(state, state->e); break;
        case 0xa4: ana(state, state->h); break;
        case 0xa5: ana(state, state->l); break;
        case 0xa6: ana(state, state->mem[hl(state)]);  break;
        case 0xa7: ana(state, state->a); break;
        case 0xa8: xra(state, state->b); break;
        case 0xa9: xra(state, state->c);  break;
        case 0xaa: xra(state, state->d);  break;
        case 0xab: xra(state, state->e);  break;
        case 0xac: xra(state, state->h);  break;
        case 0xad: xra(state, state->l);  break; // XRA L; A <- A ^ L
        case 0xae: xra(state, state->mem[hl(state)]);  break;
        case 0xaf: xra(state, state->a);  break;
        case 0xb0: ora(state, state->b); break;
        case 0xb1: ora(state, state->c);  break;
        case 0xb2: ora(state, state->d);  break;
        case 0xb3: ora(state, state->e);  break;
        case 0xb4: ora(state, state->h);  break;
        case 0xb5: ora(state, state->l);  break; // ORA L; A <- A | L
        case 0xb6: {
            uint16_t addr = state->h << 8 | state->l;
            state->a = state->a | state->mem[addr];
            state->flags.z = (state->a == 0);
            state->flags.p = parity(state->a, 8);
            state->flags.s = ((state->a & 0x80) != 0);
            state->flags.c = 0;
        }  break; // ORA M; A <- A | (HL)
        case 0xb7: ora(state, state->a);  break;
        case 0xb8: cmp(state, state->b); break; // CMP B; A - B
        case 0xb9: cmp(state, state->c); break;
        case 0xba: cmp(state, state->d); break;
        case 0xbb: cmp(state, state->e);  break; // CMP E; A - E
        case 0xbc: cmp(state, state->h);  break;
        case 0xbd: cmp(state, state->l);  break; // CMP L; A - L
        case 0xbe: cmp(state, state->mem[hl(state)]);  break;
        case 0xbf: cmp(state, state->a);  break;
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
            call(state, 1, opcode);
            /* state->mem[state->sp-1] = ((state->pc+2) & 0xff00) >> 8;
            state->mem[state->sp-2] = (state->pc+2) & 0xff;
            state->sp -=2;
            state->pc = opcode[2] << 8 | opcode[1]; */
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
        case 0xe9: state->pc = state->h << 8 | state->l; break;
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
        case 0xf9: {
            state->sp = state->h << 8 | state->l;
        } break;
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