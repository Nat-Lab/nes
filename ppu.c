#include "ppu.h"
#include "log.h"
#include <memory.h>
#define REG(x) reg[x - 0x2000]

// mem
static uint8_t sprite_mem[0x100];
static uint8_t mem[0x4000];

// registers
static uint8_t reg[8];
enum ppu_regname {
    PPUCTRL, PPUMASK, PPUSTATUS, OAMADDR, OAMDATA, PPUSCROLL, PPUADDR, PPUDATA,
    OAMDMA
};


uint16_t ppuaddr;

/**
 * @brief 
 * 
 * @param addr 
 * @return uint16_t 
 */
static inline uint16_t to_ppu_addr(uint16_t addr) {
    if (addr < 0x3000) return addr; // patten table 0~1, nametable 0~3
    if (addr < 0x3f00) return addr - 0x1000; // mirror nametable 0~3
    if (addr < 0x3f20) return addr; // palette ram index
    if (addr < 0x4000) return addr - 0x0020;
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

/**
 * @brief Get PPU register
 * 
 * @param addr address of the register
 * @return uint8_t value of the register
 */
uint8_t ppu_get_reg(uint16_t addr) {
    log_error("PPU REG not implemented.\n");
    return (uint8_t) -1;
}

/**
 * @brief Set PPU register
 * 
 * @param addr address
 * @param val value
 */
void ppu_set_reg(uint16_t addr, uint8_t val) {
    log_error("PPU REG not implemented.\n");
}