#include "ppu.h"
#include "log.h"
#include <memory.h>
#define PPU_WARNUP 29658

#define CTRL_NMI  ppuctrl & 0b10000000 // do NMI in vblank?
#define CTRL_MS   ppuctrl & 0b01000000 // master/slave
#define CTRL_SPSZ ppuctrl & 0b00100000 // sprite size 0: 8*8, 1: 8*16
#define CTRL_BGTB ppuctrl & 0b00010000 // background pattern table addr: 0: 0x0000, 1: 0x1000
#define CTRL_STB  ppuctrl & 0b00001000 // 8*8 sprites pattern table addr: 0: 0x0000, 1: 0x1000
#define CTRL_RAI  ppuctrl & 0b00000100 // incr vram addr on ppudata r/w
#define CTRL_BNTA ppuctrl & 0b00000011 // base nametab addr (0: 0x2000; 1: 0x2400; 2: 0x2800; 3: 0x2C00)

#define SCTRL_NMI(x)  ppuctrl |= (x << 7) & 0b10000000
#define SCTRL_MS(x)   ppuctrl |= (x << 6) & 0b01000000
#define SCTRL_SPSZ(x) ppuctrl |= (x << 5) & 0b00100000
#define SCTRL_BGTB(x) ppuctrl |= (x << 4) & 0b00010000
#define SCTRL_STB(x)  ppuctrl |= (x << 3) & 0b00001000
#define SCTRL_RAI(x)  ppuctrl |= (x << 2) & 0b00000100
#define SCTRL_BNTA(x) ppuctrl |= (x     ) & 0b00000011

#define MASK_EB   ppumask & 0b10000000 // emphasize blue
#define MASK_EG   ppumask & 0b01000000 // emphasize green
#define MASK_ER   ppumask & 0b00100000 // emphasize red
#define MASK_SSP  ppumask & 0b00010000 // show sprites
#define MASK_SBG  ppumask & 0b00001000 // show background
#define MASK_SSP8 ppumask & 0b00000100 // show sprites in leftmost 8 pixels of screen
#define MASK_SBG8 ppumask & 0b00000010 // show background in leftmost 8 pixels of screen
#define MASK_GS   ppumask & 0b00000001 // greyscale

#define SMASK_EB(x)   ppumask |= (x << 7) & 0b10000000
#define SMASK_EG(x)   ppumask |= (x << 6) & 0b01000000
#define SMASK_ER(x)   ppumask |= (x << 5) & 0b00100000
#define SMASK_SSP(x)  ppumask |= (x << 4) & 0b00010000
#define SMASK_SBG(x)  ppumask |= (x << 3) & 0b00001000
#define SMASK_SSP8(x) ppumask |= (x << 2) & 0b00000100
#define SMASK_SBG8(x) ppumask |= (x << 1) & 0b00000010
#define SMASK_GS(x)   ppumask |= (x     ) & 0b00000001

#define STAT_VB  ppustatus & 0b10000000 // in vblank?
#define STAT_SH  ppustatus & 0b01000000 // sprite 0 Hit
#define STAT_SO  ppustatus & 0b00100000 // sprite overflow
#define STAT_LSB ppustatus & 0b00011111 // last lsb written to reg

#define SSTAT_VB(x)  ppustatus |= (x << 7) & 0b10000000
#define SSTAT_SH(x)  ppustatus |= (x << 6) & 0b01000000
#define SSTAT_SO(x)  ppustatus |= (x << 5) & 0b00100000
#define SSTAT_LSB(x) ppustatus |= (x     ) & 0b00011111

// mem
static uint8_t smem[0x100];
static uint8_t mem[0x4000];

// registers & status
uint8_t ppuctrl, ppumask, ppustatus, oamaddr, oamdata, ppuscroll[2], scroll_writepos, ppudata, oamdma;
uint16_t ppuaddr, mirror_xor;

uint8_t ready;

/**
 * @brief convert addr
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
    ppuaddr &= 0x3fff; // max 0x3fff
    switch(addr & 0b00000111) {
        case 0: {
            log_warn("read from write-only register 0x2000.\n");
            return (uint8_t) -1;
        }
        case 1: {
            log_warn("read from write-only register 0x2001.\n");
            return (uint8_t) -1;
        }
        case 2: {
            uint8_t val = ppustatus;
            SSTAT_VB(0);
            scroll_writepos = 0;
            return val;
        }
        case 3: {
            log_warn("read from write-only register 0x2003.\n");
            return (uint8_t) -1;
        }
        case 4: {
            return smem[oamaddr];
        }
        case 5: {
            log_warn("read from write-only register 0x2005.\n");
            return (uint8_t) -1;
        }
        case 6: {
            log_warn("read from write-only register 0x2006.\n");
            return (uint8_t) -1;
        }
        case 7: {
            uint8_t val = ppuread(ppuaddr);
            ppuaddr += (CTRL_RAI) ? 32 : 1;
            return val;
        }
    }

    log_error("PPU REG %u not implemented.\n", addr & 0b00000111);
    return (uint8_t) -1;
}

/**
 * @brief Set PPU register
 * 
 * @param addr address
 * @param val value
 */
void ppu_set_reg(uint16_t addr, uint8_t val) {
    ppuaddr &= 0x3fff; // max 0x3fff
    switch(addr & 0b00000111) {
        case 0: {
            if (ready) ppuctrl = val;
            else log_warn("ignoring write to ppu register during warm up.\n");
            return;
        }
        case 1: {
            if (ready) ppumask = val;
            else log_warn("ignoring write to ppu register during warm up.\n");
            return;
        }
        case 2: {
            log_warn("attempt to set read-only register 0x2002.\n");
            return;
        }
        case 3: {
            oamaddr = val;
            return;
        }
        case 4: {
            smem[oamaddr++] = val;
            return;
        }
        case 5: {
            if (!ready) {
                log_warn("ignoring write to ppu register during warm up.\n");
                return;
            }
            ppuscroll[scroll_writepos & 1] = val;
            ++scroll_writepos;
            return;
        }
        case 6: {
            if (!ready) {
                log_warn("ignoring write to ppu register during warm up.\n");
                return;
            }
            if (scroll_writepos & 1) {
                uint16_t v = ppuaddr & (uint16_t) ((uint16_t) 0xff00 | (uint16_t) val);
                SCTRL_BNTA(v >> 10);
                ppuscroll[0] = (ppuscroll[0] & (uint8_t) 7) | ((uint8_t) v & (uint8_t)0x1f) << 3;
                ppuscroll[1] = ((v & (uint16_t) 0x03e0) >> 2) | ((v & (uint16_t) 0x7000) >> 12);
            } else ppuaddr = (ppuaddr & (uint16_t) 0x00FF) | ((uint16_t) val << 8);
            ++scroll_writepos;
            return;
        }
        case 7: {
            if (ppuaddr > 0x1FFF || ppuaddr < 0x4000) {
                ppuwrt(ppuaddr ^ mirror_xor, val);
                ppuwrt(ppuaddr, val);
                return;
            }
            ppuwrt(ppuaddr, val);
            return;
        }
    }

    log_error("PPU REG %u not implemented.\n", addr & 0b00000111);
}