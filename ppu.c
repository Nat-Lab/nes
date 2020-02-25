#include "ppu.h"
#include "log.h"
#include <memory.h>

// mem
static uint8_t sprite_mem[0x100];
static uint8_t mem[0x4000];

// registers
static uint8_t ppuctrl;
static uint8_t ppumask;
static uint8_t ppustatus;
static uint8_t oamaddr;
static uint8_t ppuscroll;
static uint8_t ppuaddr;
static uint8_t ppudata;
static uint8_t oamdma;

/**
 * @brief 
 * 
 * @param addr 
 * @return uint16_t 
 */
static inline uint16_t to_ppu_addr(uint16_t addr) {
    if (addr < 0x3000) { // patten table 0~1, nametable 0~3
        return addr;
    } else if (addr < 0x3f00) { // mirror nametable 0~3
        return addr - 0x1000;
    } else if (addr < 0x3f20) { // palette ram index
        return addr;
    } else if (addr < 0x4000) {
        return addr - 0x0020;
    }
    log_fatal("bad ppu address: $%.4x.\n", addr);
    return (uint16_t) -1;
}

/**
 * @brief read from PPU memory
 * 
 * @param addr vaddress
 * @return uint8_t byte on the address
 */
inline uint8_t ppuread (uint16_t addr) {
    return mem[to_ppu_addr(addr)];
}

/**
 * @brief Write to PPU memory
 * 
 * @param dst vaddress
 * @param val byte on the address
 */
inline void ppuwrt (uint16_t dst, uint8_t val) {
    mem[to_ppu_addr(dst)] = val;
}

/**
 * @brief Copy to PPU memory
 * 
 * @param dst dst vaddress
 * @param src src address
 * @param sz num of bytes to copy
 */
inline void ppucpy (uint16_t dst, const uint8_t *src, size_t sz) {
    memcpy(mem + dst, src, sz);
}