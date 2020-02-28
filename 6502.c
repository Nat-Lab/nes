#include "log.h"
#include "6502.h"
#include "mem.h"
#include "ppu.h"

/* registers */
uint8_t acc; // accumulator
uint8_t x; // index x
uint8_t y; // index y
uint16_t pc; // prog counter
uint8_t sp; // stack ptr
uint8_t s; // status

uint8_t op; // next op
uint16_t a; // next op address
uint16_t v; // next op value
uint64_t cycles; // total cycles

/* heleprs for get status flag */
#define S_CARRY (s & (uint8_t) 0b00000001)
#define S_ZERO  (s & (uint8_t) 0b00000010)
#define S_ID    (s & (uint8_t) 0b00000100) // Interrupt Disable
#define S_DEC   (s & (uint8_t) 0b00001000) // Decimal, not used in NES
#define S_B     (s & (uint8_t) 0b00010000) // The "B" flag
#define S_R     (s & (uint8_t) 0b00100000) // 
#define S_OVFL  (s & (uint8_t) 0b01000000) // overflow
#define S_NEG   (s & (uint8_t) 0b10000000) // negative

/* helpers for settting status flag */
#define SE_CARRY()  s |= (uint8_t) 0b00000001
#define SE_ZERO()   s |= (uint8_t) 0b00000010
#define SE_ID()     s |= (uint8_t) 0b00000100
#define SE_DEC()    s |= (uint8_t) 0b00001000
#define SE_B()      s |= (uint8_t) 0b00010000
#define SE_R()      s |= (uint8_t) 0b00100000
#define SE_OVFL()   s |= (uint8_t) 0b01000000
#define SE_NEG()    s |= (uint8_t) 0b10000000

/* helpers for clearing status flag */
#define CL_CARRY()  s &= (uint8_t) 0b11111110
#define CL_ZERO()   s &= (uint8_t) 0b11111101
#define CL_ID()     s &= (uint8_t) 0b11111011
#define CL_DEC()    s &= (uint8_t) 0b11110111
#define CL_B()      s &= (uint8_t) 0b11101111
#define CL_R()      s &= (uint8_t) 0b11011111
#define CL_OVFL()   s &= (uint8_t) 0b10111111
#define CL_NEG()    s &= (uint8_t) 0b01111111

/* helpers for conditional set/clear flag */
#define SIF_CARRY(x) { if (x) { SE_CARRY(); } else CL_CARRY(); }
#define SIF_ZERO(x)  { if (x) { SE_ZERO();  } else CL_ZERO(); }
#define SIF_ID(x)    { if (x) { SE_ID();    } else CL_ID(); }
#define SIF_DEC(x)   { if (x) { SE_DEC();   } else CL_DEC(); }
#define SIF_B(x)     { if (x) { SE_B();     } else CL_B(); }
#define SIF_R(x)     { if (x) { SE_R();     } else CL_R(); }
#define SIF_OVFL(x)  { if (x) { SE_OVFL();  } else CL_OVFL(); }
#define SIF_NEG(x)   { if (x) { SE_NEG();   } else CL_NEG(); }

/* set sign bit base on val */
#define CHK_NZ(x) \
{\
    uint8_t tmp = x;\
    if (tmp & 0b10000000) { SE_NEG(); } else CL_NEG();\
    if (tmp == 0) { SE_ZERO(); } else CL_ZERO();\
}

/* stack helper */
#define PSH(x) cpuwrt(0x0100 + (sp--), x)
#define POP() cpuread(0x0100 + (++sp))

/* IRQ vectors */
#define I_NMI 0xfffa
#define I_RST 0xfffc
#define I_BRK 0xfffe

#define DEBUG_6502
#ifdef DEBUG_6502
#define STRING(s) #s
#define PRINT_OP(op, amn, opn) log_debug("opcode: %.2x (am: %s, op: %s), am_result: %u, value: %u.\n", op, amn, opn, a, v);
#else
#define PRINT_OP(o, a, n)
#endif 


/* make case statement for OP */
#define OP(opcode, amname, opname, cycle) \
case opcode: {\
    AM_##amname(); OP_##opname(); cycles += cycle; PRINT_OP(opcode, #amname, #opname);\
    break;\
}

/** begin AM_* **/
#define AM_NII() log_warn("cpu got unimplemented addressing mode.\n");
#define AM_IMP()
#define AM_IMM() v = cpuread(pc++);
#define AM_ABS() \
{\
    uint16_t p = pc; pc += 2;\
    a = ((uint16_t) cpuread(p)) | (uint16_t) ((uint16_t) cpuread(p+1) << 8);\
    v = cpuread(a);\
}
#define AM_ABX() \
{\
    uint16_t p = pc; pc += 2;\
    a = (((uint16_t) cpuread(p)) | (uint16_t) ((uint16_t) cpuread(p+1) << 8)) + x; \
    v = cpuread(a); \
    if (a >> 8 != p >>8) cycles++;\
}
#define AM_ABY() \
{\
    uint16_t p = pc; pc += 2;\
    a = (((uint16_t) cpuread(p)) | (uint16_t) ((uint16_t) cpuread(p+1) << 8)) + y; \
    v = cpuread(a); \
    if (a >> 8 != p >>8) cycles++;\
}
#define AM_ZPG() a = cpuread(pc++); v = cpuread(a);
#define AM_ZPX() a = (cpuread(pc++) + x) & 0x00ff; v = cpuread(a);
#define AM_ZPY() a = (cpuread(pc++) + y) & 0x00ff; v = cpuread(a);
#define AM_IND() \
{\
    uint16_t p = pc; pc += 2;\
    uint16_t b1 = ((uint16_t) cpuread(p)) | (uint16_t) ((uint16_t) cpuread(p+1));\
    uint16_t b2 = (b1 & (uint16_t) 0xff00) | ((b1 + 1) & (uint16_t) 0x00ff);\
    a = ((uint16_t) cpuread(b1)) | (uint16_t) ((uint16_t) cpuread(b2) << 8);\
}
#define AM_INX() \
{\
    uint8_t b = cpuread(pc++) + x;\
    a = ((uint16_t) cpuread(b)) | (uint16_t) ((uint16_t) cpuread(b+1) << 8);\
    v = cpuread(a);\
}
#define AM_INY() \
{\
    uint8_t b = cpuread(pc++);\
    a = (((cpuread((b + 1) & 0xFF) << 8) | cpuread(b)) + y) & 0xFFFF; v = cpuread(a);\
    if ((a >> 8) != (pc >> 8)) { cycles++; }\
}
#define AM_REL() a = cpuread(pc++); if (a & 0x80) a -= 0x100; a += pc; if ((a >> 8) != (pc >> 8)) cycles++;
/** end AM_* **/

/** begin OP_* **/
#define OP_NII() log_warn("cpu got unimplemented instruction.\n"); // not implemented
#define OP_LDA() CHK_NZ(acc = v); // load acc
#define OP_LDX() CHK_NZ(x = v);   // load x
#define OP_LDY() CHK_NZ(y = v);   // load y
#define OP_STA() cpuwrt(a, acc); // wrt acc
#define OP_STX() cpuwrt(a, x); // wrt x
#define OP_STY() cpuwrt(a, y); // wrt y
// add w/ carry
#define OP_ADC() \
{\
    uint16_t r = acc + v + (S_CARRY ? 1 : 0);\
    SIF_CARRY(r >> 8); uint8_t r8 = (uint8_t) r;\
    SIF_OVFL(!((acc ^ v) & 0b10000000) && ((acc ^ r8) & 0b10000000));\
    CHK_NZ(acc = r8);\
}
// sub w/ carry
#define OP_SBC() \
{\
    uint16_t r = acc - v - (S_CARRY ? 0 : 1);\
    SIF_CARRY(!(r >> 8)); uint8_t r8 = (uint8_t) r;\
    SIF_OVFL(((acc ^ v) & 0x80) && ((acc ^ r8) & 0x80));\
    CHK_NZ(acc = r8);\
}
#define OP_INC() CHK_NZ(v+1); cpuwrt(a, v+1); // inc value
#define OP_DEC() CHK_NZ(v-1); cpuwrt(a, v-1); // dec value
#define OP_AND() CHK_NZ(acc &= v); // and
#define OP_ORA() CHK_NZ(acc |= v); // or
#define OP_EOR() CHK_NZ(acc ^= v); // eor
#define OP_INX() CHK_NZ(++x); // incr x
#define OP_DEX() CHK_NZ(--x); // desc x
#define OP_INY() CHK_NZ(++y); // incr y
#define OP_DEY() CHK_NZ(--y); // decr y
#define OP_TAX() CHK_NZ(x = acc); // acc to x
#define OP_TXA() CHK_NZ(acc = x); // x to acc
#define OP_TAY() CHK_NZ(y = acc); // acc to y
#define OP_TYA() CHK_NZ(acc = y); // y to acc
#define OP_TSX() CHK_NZ(x = sp); // sp to x
#define OP_TXS() sp = x; // x to sp
#define OP_CLC() CL_CARRY(); // clear carry
#define OP_SEC() SE_CARRY(); // set carry
#define OP_CLD() CL_DEC(); // clear deci
#define OP_SED() SE_DEC(); // set deci
#define OP_CLV() CL_OVFL(); // clear ovfl
#define OP_CLI() CL_ID(); // clear inter-disable
#define OP_SEI() SE_ID(); // set inter-disable
// compare acc
#define OP_CMP() \
{\
    uint16_t r = (uint16_t) acc - v; SIF_CARRY(!(r & 0x8000));\
    CHK_NZ((uint8_t) r);\
}
// compare x
#define OP_CPX() \
{\
    uint16_t r = (uint16_t) x - v; SIF_CARRY(!(r & 0x8000));\
    CHK_NZ((uint8_t) r);\
}
// compare y
#define OP_CPY() \
{\
    uint16_t r = (uint16_t) y - v; SIF_CARRY(!(r & 0x8000));\
    CHK_NZ((uint8_t) r);\
}
// bit test
#define OP_BIT() SIF_OVFL(v & 0b01000000); SIF_NEG(v & 0b10000000); SIF_ZERO(!(acc & v));
// << 1
#define OP_ASL() SIF_CARRY(v & 0b10000000); CHK_NZ(v = v << 1); cpuwrt(a, v);
// acc << 1
#define OP_ASLA() SIF_CARRY(acc & 0b10000000); CHK_NZ(acc = acc << 1);
// >> 1
#define OP_LSR() SIF_CARRY(v & 1); CHK_NZ(v = v >> 1); cpuwrt(a, v);
// acc >> 1
#define OP_LSRA() SIF_CARRY(acc & 1); CHK_NZ(acc = acc >> 1);
// << 1
#define OP_ROL() v = v << 1; CHK_NZ(v |= S_CARRY); SIF_CARRY(v & 0b100000000); cpuwrt(a, v);
#define OP_ROLA() \
{\
    uint16_t a16 = acc; acc = acc << 1;  CHK_NZ(acc = a16 |= S_CARRY); SIF_CARRY(a16 & 0x100);\
}
#define OP_ROR() v |= S_CARRY << 8; SIF_CARRY(v & 1); CHK_NZ(v = v >> 1); cpuwrt(a, v);
#define OP_RORA() \
{\
    uint16_t a16 = acc; a16 |= S_CARRY << 8; SIF_CARRY(a16 & 1); CHK_NZ(acc = a16 = a16 >> 1);\
}
// stack ops
#define OP_PHA() PSH(acc);
#define OP_PLA() CHK_NZ(acc = POP());
// save status
#define OP_PHP() PSH(s | (uint8_t)(0b00110000));
#define OP_PLP() s = POP(); SE_R(); CL_B();
// jmp/branch
#define OP_JMP() pc = a;
#define OP_BEQ() if (S_ZERO) pc = a;
#define OP_BNE() if (!S_ZERO) pc = a;
#define OP_BCS() if (S_CARRY) pc = a;
#define OP_BCC() if (!S_CARRY) pc = a;
#define OP_BMI() if (S_NEG) pc = a;
#define OP_BPL() if (!S_NEG) pc = a;
#define OP_BVS() if (S_OVFL) pc = a;
#define OP_BVC() if (!S_OVFL) pc = a;
#define OP_JSR() \
{\
    uint16_t lp = pc; PSH(lp >> 8); PSH(lp); pc = a;\
}
#define OP_RTS() \
{\
    uint8_t l = POP(), h = POP(); pc = ((uint16_t) l | ((uint16_t) h << 8));\
}
#define OP_NOP() // nop
// break (intr)
#define OP_BRK() ++pc; PSH(pc >> 8); PSH(pc); SE_B(); SE_R(); PSH(s); SE_ID(); pc = ((uint16_t) cpuread(I_NMI) | (uint16_t) ((uint16_t) cpuread(I_NMI + 1) << 8));
// return from break (intr)
#define OP_RTI() \
{\
    s = POP(); SE_R(); CL_B(); uint8_t l = POP(); uint8_t h = POP(); pc = (uint16_t) l | (uint16_t) h << 8;\
}
// extend
#define OP_ASR() OP_AND(); OP_LSRA();
#define OP_ANC() OP_AND(); SIF_CARRY(S_NEG);
#define OP_ARR() OP_AND(); OP_RORA(); // FIXME?
#define OP_AXS() \
{\
    uint16_t x16 = (acc & x) - v; SIF_CARRY((x16 & 0x8000) == 0); CHK_NZ(x = x16);\
}
#define OP_LAX() CHK_NZ(acc = x = v);
#define OP_SAX() cpuwrt(a, acc & x);
#define OP_LAX() CHK_NZ(acc = x = v);
#define OP_SAX() cpuwrt(a, acc & x);
#define OP_DCP() OP_DEC(); OP_CMP();
#define OP_ISB() OP_INC(); OP_SBC();
#define OP_RLA() OP_ROL(); OP_AND();
#define OP_RRA() OP_ROR(); OP_ADC();
#define OP_SLO() OP_ASL(); OP_ORA();
#define OP_SRE() OP_LSR(); OP_EOR();
#define OP_SHY() OP_NII();
#define OP_SHX() OP_NII();
#define OP_TAS() OP_NII();
#define OP_AHX() OP_NII();
#define OP_XAA() OP_NII();
#define OP_LAS() OP_NII();
/** end OP_* **/

/**
 * @brief read from CPU address
 * 
 * @param addr address
 * @return uint8_t value
 */
static inline uint8_t cpuread(uint16_t addr) {
    switch (addr >> 13) {
        case 0: return memread(addr & 0x07FF);
        case 1: return ppu_get_reg(addr);
        case 2: return 255; // TODO
        case 3: return memread(addr & 0x1FFF);
        default: return memread(addr);
    }
    return memread(addr);
}

/**
 * @brief write to CPU address
 * 
 * @param addr address
 * @param val value
 */
static inline void cpuwrt(uint16_t addr, uint8_t val) {
    // DMA transfer
    switch (addr >> 13) {
        case 0: return memwrt(addr & 0x07FF, val);
        case 1: return ppu_set_reg(addr, val);
        case 2: return; // TODO
        case 3: return memwrt(addr & 0x1FFF, val);
        default: {
            log_warn("prg-rom write!\n");
            return memwrt(addr, val);
        }
    }
}

/**
 * @brief reset CPU
 * 
 */
inline void reset_6502() {
    pc = ((uint16_t) cpuread(I_RST) | (uint16_t) ((uint16_t) cpuread(I_RST + 1) << 8));
    sp -= 3;
    SE_ID();
}

/**
 * @brief print CPU status
 * 
 */
inline void status_6502() {
    log_debug("acc: %3u, x: %3u, y: %3u, pc: %6u, sp: %3u, s: %3u, cycles: %llu.\n", acc, x, y, pc, sp, s, cycles);
}

/**
 * @brief CPU NMI 
 * 
 */
inline void interrupt_6502() {
    // TODO
}

/**
 * @brief init CPU
 * 
 */
inline void init_6502() {
    s = 0b00100100;
    sp = 0;
    a = x = y = 0;
}

/**
 * @brief Get current CPU cycle count.
 * 
 * @return uint64_t cycle.
 */
inline uint64_t cycles_6502() {
    return cycles;
}

/**
 * @brief Run one instruction
 * 
 */
inline void run_6502() {
    uint8_t op = cpuread(pc++);
#ifdef DEBUG_6502
    if (cycles >= 0) {
        printf("op: %.2x, a: %u, v: %u, acc: %u, x: %u, y: %u, pc: %u, sp: %u, s: %u, cyc: %llu.\n", op, a, v, acc, x, y, pc, sp, s, cycles);
    }
#endif
    switch(op) {
        OP(0x00, IMP, BRK, 7) OP(0x01, INX, ORA, 6) OP(0x03, INX, SLO, 8) OP(0x04, ZPG, NOP, 2) OP(0x05, ZPG, ORA, 3) 
        OP(0x06, ZPG, ASL, 5) OP(0x07, ZPG, SLO, 5) OP(0x08, IMP, PHP, 3) OP(0x09, IMM, ORA, 2) OP(0x0A, IMP, ASLA, 2) 
        OP(0x0B, IMM, ANC, 2) OP(0x0C, ABS, NOP, 4) OP(0x0D, ABS, ORA, 4) OP(0x0E, ABS, ASL, 6) OP(0x0F, ABS, SLO, 6) 
        OP(0x10, REL, BPL, 2) OP(0x11, INY, ORA, 5) OP(0x13, INY, SLO, 8) OP(0x14, ZPX, NOP, 4) OP(0x15, ZPX, ORA, 4) 
        OP(0x16, ZPX, ASL, 6) OP(0x17, ZPX, SLO, 6) OP(0x18, IMP, CLC, 2) OP(0x19, ABY, ORA, 4) OP(0x1A, IMP, NOP, 2) 
        OP(0x1B, ABY, SLO, 7) OP(0x1C, ABX, NOP, 4) OP(0x1D, ABX, ORA, 4) OP(0x1E, ABX, ASL, 7) OP(0x1F, ABX, SLO, 7) 
        OP(0x20, ABS, JSR, 6) OP(0x21, INX, AND, 6) OP(0x23, INX, RLA, 8) OP(0x24, ZPG, BIT, 3) OP(0x25, ZPG, AND, 3) 
        OP(0x26, ZPG, ROL, 5) OP(0x27, ZPG, RLA, 5) OP(0x28, IMP, PLP, 4) OP(0x29, IMM, AND, 2) OP(0x2A, IMP, ROLA, 2) 
        OP(0x2B, IMM, ANC, 2) OP(0x2C, ABS, BIT, 4) OP(0x2D, ABS, AND, 2) OP(0x2E, ABS, ROL, 6) OP(0x2F, ABS, RLA, 6) 
        OP(0x30, REL, BMI, 2) OP(0x31, INY, AND, 5) OP(0x33, INY, RLA, 8) OP(0x34, ZPX, NOP, 4) OP(0x35, ZPX, AND, 4) 
        OP(0x36, ZPX, ROL, 6) OP(0x37, ZPX, RLA, 6) OP(0x38, IMP, SEC, 2) OP(0x39, ABY, AND, 4) OP(0x3A, IMP, NOP, 2) 
        OP(0x3B, ABY, RLA, 7) OP(0x3C, ABX, NOP, 4) OP(0x3D, ABX, AND, 4) OP(0x3E, ABX, ROL, 7) OP(0x3F, ABX, RLA, 7) 
        OP(0x40, IMP, RTI, 6) OP(0x41, INX, EOR, 6) OP(0x43, INX, SRE, 8) OP(0x44, ZPG, NOP, 3) OP(0x45, ZPG, EOR, 3) 
        OP(0x46, ZPG, LSR, 5) OP(0x47, ZPG, SRE, 5) OP(0x48, IMP, PHA, 3) OP(0x49, IMM, EOR, 2) OP(0x4A, IMP, LSRA, 2) 
        OP(0x4B, IMM, ASR, 2) OP(0x4C, ABS, JMP, 3) OP(0x4D, ABS, EOR, 4) OP(0x4E, ABS, LSR, 6) OP(0x4F, ABS, SRE, 6) 
        OP(0x50, REL, BVC, 2) OP(0x51, INY, EOR, 5) OP(0x53, INY, SRE, 8) OP(0x54, ZPX, NOP, 4) OP(0x55, ZPX, EOR, 4) 
        OP(0x56, ZPX, LSR, 6) OP(0x57, ZPX, SRE, 6) OP(0x58, IMP, CLI, 2) OP(0x59, ABY, EOR, 4) OP(0x5A, IMP, NOP, 2) 
        OP(0x5B, ABY, SRE, 7) OP(0x5C, ABX, NOP, 4) OP(0x5D, ABX, EOR, 4) OP(0x5E, ABX, LSR, 7) OP(0x5F, ABX, SRE, 7) 
        OP(0x60, IMP, RTS, 6) OP(0x61, INX, ADC, 6) OP(0x63, INX, RRA, 8) OP(0x64, ZPG, NOP, 3) OP(0x65, ZPG, ADC, 3) 
        OP(0x66, ZPG, ROR, 5) OP(0x67, ZPG, RRA, 5) OP(0x68, IMP, PLA, 4) OP(0x69, IMM, ADC, 2) OP(0x6A, IMP, RORA, 2) 
        OP(0x6B, IMM, ARR, 2) OP(0x6C, IND, JMP, 5) OP(0x6D, ABS, ADC, 4) OP(0x6E, ABS, ROR, 6) OP(0x6F, ABS, RRA, 6) 
        OP(0x70, REL, BVS, 2) OP(0x71, INY, ADC, 5) OP(0x73, INY, RRA, 8) OP(0x74, ZPX, NOP, 4) OP(0x75, ZPX, ADC, 4) 
        OP(0x76, ZPX, ROR, 6) OP(0x77, ZPX, RRA, 6) OP(0x78, IMP, SEI, 2) OP(0x79, ABY, ADC, 4) OP(0x7A, IMP, NOP, 2) 
        OP(0x7B, ABY, RRA, 7) OP(0x7C, ABX, NOP, 4) OP(0x7D, ABX, ADC, 4) OP(0x7E, ABX, ROR, 7) OP(0x7F, ABX, RRA, 7) 
        OP(0x80, IMM, NOP, 2) OP(0x81, INX, STA, 6) OP(0x82, IMM, NOP, 2) OP(0x83, INX, SAX, 6) OP(0x84, ZPG, STY, 3) 
        OP(0x85, ZPG, STA, 3) OP(0x86, ZPG, STX, 3) OP(0x87, ZPG, SAX, 3) OP(0x88, IMP, DEY, 2) OP(0x89, IMM, NOP, 2) 
        OP(0x8A, IMP, TXA, 2) OP(0x8B, IMM, XAA, 2) OP(0x8C, ABS, STY, 4) OP(0x8D, ABS, STA, 4) OP(0x8E, ABS, STX, 4) 
        OP(0x8F, ABS, SAX, 4) OP(0x90, REL, BCC, 2) OP(0x91, INY, STA, 6) OP(0x93, INY, AHX, 6) OP(0x94, ZPX, STY, 4) 
        OP(0x95, ZPX, STA, 4) OP(0x96, ZPY, STX, 4) OP(0x97, ZPY, SAX, 4) OP(0x98, IMP, TYA, 2) OP(0x99, ABY, STA, 5) 
        OP(0x9A, IMP, TXS, 2) OP(0x9B, ABY, TAS, 5) OP(0x9C, ABX, SHY, 5) OP(0x9D, ABX, STA, 5) OP(0x9E, ABY, SHX, 5) 
        OP(0x9F, ABY, AHX, 5) OP(0xA0, IMM, LDY, 2) OP(0xA1, INX, LDA, 6) OP(0xA2, IMM, LDX, 2) OP(0xA3, INX, LAX, 6) 
        OP(0xA4, ZPG, LDY, 3) OP(0xA5, ZPG, LDA, 3) OP(0xA6, ZPG, LDX, 3) OP(0xA7, ZPG, LAX, 3) OP(0xA8, IMP, TAY, 2) 
        OP(0xA9, IMM, LDA, 2) OP(0xAA, IMP, TAX, 2) OP(0xAB, IMM, LAX, 6) OP(0xAC, ABS, LDY, 4) OP(0xAD, ABS, LDA, 4) 
        OP(0xAE, ABS, LDX, 4) OP(0xAF, ABS, LAX, 4) OP(0xB0, REL, BCS, 2) OP(0xB1, INY, LDA, 5) OP(0xB3, INY, LAX, 5) 
        OP(0xB4, ZPX, LDY, 4) OP(0xB5, ZPX, LDA, 4) OP(0xB6, ZPY, LDX, 4) OP(0xB7, ZPY, LAX, 4) OP(0xB8, IMP, CLV, 2) 
        OP(0xB9, ABY, LDA, 4) OP(0xBA, IMP, TSX, 2) OP(0xBB, ABY, LAS, 4) OP(0xBC, ABX, LDY, 4) OP(0xBD, ABX, LDA, 4) 
        OP(0xBE, ABY, LDX, 4) OP(0xBF, ABY, LAX, 4) OP(0xC0, IMM, CPY, 2) OP(0xC1, INX, CMP, 6) OP(0xC2, IMM, NOP, 6) 
        OP(0xC3, INX, DCP, 8) OP(0xC4, ZPG, CPY, 3) OP(0xC5, ZPG, CMP, 3) OP(0xC6, ZPG, DEC, 5) OP(0xC7, ZPG, DCP, 5) 
        OP(0xC8, IMP, INY, 2) OP(0xC9, IMM, CMP, 2) OP(0xCA, IMP, DEX, 2) OP(0xCB, IMM, AXS, 2) OP(0xCC, ABS, CPY, 4) 
        OP(0xCD, ABS, CMP, 4) OP(0xCE, ABS, DEC, 6) OP(0xCF, ABS, DCP, 6) OP(0xD0, REL, BNE, 2) OP(0xD1, INY, CMP, 5) 
        OP(0xD3, INY, DCP, 8) OP(0xD4, ZPX, NOP, 4) OP(0xD5, ZPX, CMP, 4) OP(0xD6, ZPX, DEC, 6) OP(0xD7, ZPX, DCP, 6) 
        OP(0xD8, IMP, CLD, 2) OP(0xD9, ABY, CMP, 4) OP(0xDA, IMP, NOP, 2) OP(0xDB, ABY, DCP, 7) OP(0xDC, ABX, NOP, 4) 
        OP(0xDD, ABX, CMP, 4) OP(0xDE, ABX, DEC, 7) OP(0xDF, ABX, DCP, 7) OP(0xE0, IMM, CPX, 2) OP(0xE1, INX, SBC, 6) 
        OP(0xE2, IMM, NOP, 2) OP(0xE3, INX, ISB, 8) OP(0xE4, ZPG, CPX, 3) OP(0xE5, ZPG, SBC, 3) OP(0xE6, ZPG, INC, 5) 
        OP(0xE7, ZPG, ISB, 5) OP(0xE8, IMP, INX, 2) OP(0xE9, IMM, SBC, 2) OP(0xEA, IMP, NOP, 2) OP(0xEB, IMM, SBC, 2) 
        OP(0xEC, ABS, CPX, 4) OP(0xED, ABS, SBC, 4) OP(0xEE, ABS, INC, 6) OP(0xEF, ABS, ISB, 6) OP(0xF0, REL, BEQ, 2) 
        OP(0xF1, INY, SBC, 5) OP(0xF3, INY, ISB, 8) OP(0xF4, ZPX, NOP, 4) OP(0xF5, ZPX, SBC, 4) OP(0xF6, ZPX, INC, 6) 
        OP(0xF7, ZPX, ISB, 6) OP(0xF8, IMP, SED, 2) OP(0xF9, ABY, SBC, 4) OP(0xFA, IMP, NOP, 2) OP(0xFB, ABY, ISB, 7) 
        OP(0xFC, ABX, NOP, 4) OP(0xFD, ABX, SBC, 4) OP(0xFE, ABX, INC, 7) OP(0xFF, ABX, ISB, 7)  
        default: {
            log_error("cpu got bad opcode %.2x\n", op);
        }
    }
}