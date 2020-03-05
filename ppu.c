#include "ppu.h"
#include "6502.h"
#include "log.h"
#include "gfx.h"
#include <memory.h>
#define PPU_WARNUP 29658

#define CTRL_NMI  ppuctrl & 0b10000000 // do NMI in vblank?
#define CTRL_MS   ppuctrl & 0b01000000 // master/slave
#define CTRL_SPSZ ppuctrl & 0b00100000 // sprite size 0: 8*8, 1: 8*16
#define CTRL_BGTB ppuctrl & 0b00010000 // background pattern table addr: 0: 0x0000, 1: 0x1000
#define CTRL_STB  ppuctrl & 0b00001000 // 8*8 sprites pattern table addr: 0: 0x0000, 1: 0x1000
#define CTRL_RAI  ppuctrl & 0b00000100 // incr vram addr on ppudata r/w
#define CTRL_BNTA ppuctrl & 0b00000011 // base nametab addr (0: 0x2000; 1: 0x2400; 2: 0x2800; 3: 0x2C00)

#define SCTRL_NMI(x)  (x) ? ppuctrl |= 0b10000000 : ppuctrl &= 0b01111111
#define SCTRL_MS(x)   (x) ? ppuctrl |= 0b01000000 : ppuctrl &= 0b10111111
#define SCTRL_SPSZ(x) (x) ? ppuctrl |= 0b00100000 : ppuctrl &= 0b11011111
#define SCTRL_BGTB(x) (x) ? ppuctrl |= 0b00010000 : ppuctrl &= 0b11101111
#define SCTRL_STB(x)  (x) ? ppuctrl |= 0b00001000 : ppuctrl &= 0b11110111
#define SCTRL_RAI(x)  (x) ? ppuctrl |= 0b00000100 : ppuctrl &= 0b11111011
#define SCTRL_BNTA(x) ppuctrl = (ppuctrl & 0b11111100) | ((x) & 0b00000011)

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

#define SSTAT_VB(x)  if (x) ppustatus |= 0b10000000; else ppustatus &= 0b01111111
#define SSTAT_SH(x)  if (x) ppustatus |= 0b01000000; else ppustatus &= 0b10111111
#define SSTAT_SO(x)  if (x) ppustatus |= 0b00100000; else ppustatus &= 0b11011111
#define SSTAT_LSB(x) ppustatus = (ppustatus & 0b11100000) | ((x) & 0b00011111)

static const uint16_t bnta[4] = { 0x2000, 0x2400, 0x2800, 0x2C00 };

// mem
static uint8_t smem[0x100];
static uint8_t mem[0x4000];

// registers & status
uint8_t ppuctrl, ppumask, ppustatus, oamaddr, oamdata, xscroll, yscroll, wpos, ppudata, oamdma;
uint16_t ppuaddr, mirror_xor, mirror, scanline;

uint8_t xscroll_wrt_count;
uint8_t ppuaddr_rh;
uint8_t ppur7r;
uint8_t tmpaddr;

// hittest
uint8_t bg[264][248];
uint8_t hit;

// l-h uint8_ts pair
uint8_t ppu_lhtab[256][256][8];
uint8_t ppu_lhtabf[256][256][8];

typedef struct pal pal_t;
struct pal {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

static const pal_t palette[64] = {
    { 0x80, 0x80, 0x80 },{ 0x00, 0x00, 0xBB },{ 0x37, 0x00, 0xBF },{ 0x84, 0x00, 0xA6 },
    { 0xBB, 0x00, 0x6A },{ 0xB7, 0x00, 0x1E },{ 0xB3, 0x00, 0x00 },{ 0x91, 0x26, 0x00 },
    { 0x7B, 0x2B, 0x00 },{ 0x00, 0x3E, 0x00 },{ 0x00, 0x48, 0x0D },{ 0x00, 0x3C, 0x22 },
    { 0x00, 0x2F, 0x66 },{ 0x00, 0x00, 0x00 },{ 0x05, 0x05, 0x05 },{ 0x05, 0x05, 0x05 },
    { 0xC8, 0xC8, 0xC8 },{ 0x00, 0x59, 0xFF },{ 0x44, 0x3C, 0xFF },{ 0xB7, 0x33, 0xCC },
    { 0xFF, 0x33, 0xAA },{ 0xFF, 0x37, 0x5E },{ 0xFF, 0x37, 0x1A },{ 0xD5, 0x4B, 0x00 },
    { 0xC4, 0x62, 0x00 },{ 0x3C, 0x7B, 0x00 },{ 0x1E, 0x84, 0x15 },{ 0x00, 0x95, 0x66 },
    { 0x00, 0x84, 0xC4 },{ 0x11, 0x11, 0x11 },{ 0x09, 0x09, 0x09 },{ 0x09, 0x09, 0x09 },
    { 0xFF, 0xFF, 0xFF },{ 0x00, 0x95, 0xFF },{ 0x6F, 0x84, 0xFF },{ 0xD5, 0x6F, 0xFF },
    { 0xFF, 0x77, 0xCC },{ 0xFF, 0x6F, 0x99 },{ 0xFF, 0x7B, 0x59 },{ 0xFF, 0x91, 0x5F },
    { 0xFF, 0xA2, 0x33 },{ 0xA6, 0xBF, 0x00 },{ 0x51, 0xD9, 0x6A },{ 0x4D, 0xD5, 0xAE },
    { 0x00, 0xD9, 0xFF },{ 0x66, 0x66, 0x66 },{ 0x0D, 0x0D, 0x0D },{ 0x0D, 0x0D, 0x0D },
    { 0xFF, 0xFF, 0xFF },{ 0x84, 0xBF, 0xFF },{ 0xBB, 0xBB, 0xFF },{ 0xD0, 0xBB, 0xFF },
    { 0xFF, 0xBF, 0xEA },{ 0xFF, 0xBF, 0xCC },{ 0xFF, 0xC4, 0xB7 },{ 0xFF, 0xCC, 0xAE },
    { 0xFF, 0xD9, 0xA2 },{ 0xCC, 0xE1, 0x99 },{ 0xAE, 0xEE, 0xB7 },{ 0xAA, 0xF7, 0xEE },
    { 0xB3, 0xEE, 0xFF },{ 0xDD, 0xDD, 0xDD },{ 0x11, 0x11, 0x11 },{ 0x11, 0x11, 0x11 }
};

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
    if (addr < 0x4000) {
        addr = 0x3F00 | (addr & 0x1F);
        if (addr == 0x3F10 || addr == 0x3F14 || addr == 0x3F18 || addr == 0x3F1C) return addr - 0x10;
        else return addr;
    }
    log_fatal("bad ppu address: $%.4x.\n", addr);
    return (uint16_t) -1;
}

/**
 * @brief read from PPU memory
 * 
 * @param addr vaddress
 * @return uint8_t uint8_t on the address
 */
inline uint8_t ppuread (uint16_t addr) {
    return mem[to_ppu_addr(addr)];
}

/**
 * @brief Write to PPU memory
 * 
 * @param dst vaddress
 * @param val uint8_t on the address
 */
inline void ppuwrt (uint16_t dst, uint8_t val) {
    mem[to_ppu_addr(dst)] = val;
}

/**
 * @brief Copy to PPU memory
 * 
 * @param dst dst vaddress
 * @param src src address
 * @param sz num of uint8_ts to copy
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
inline uint8_t ppu_get_reg(uint16_t address) {
    ppuaddr &= 0x3FFF;
    switch (address & 7) {
        case 0:
        case 1:
        case 3:
        case 5:
        case 6: {
            //log_warn("ignored read from write-only register 0x%.4x.\n", address + 0x2000);
            return (uint8_t) -1;
        }
        case 2: {
            uint8_t value = ppustatus;
            SSTAT_VB(0);
            SSTAT_SH(0);
            ppuaddr_rh = 0;
            tmpaddr = 0;
            ppur7r = 1;
            return value;
        }
        case 4: return smem[oamaddr];
        case 7: {
            uint8_t data;
            
            if (ppuaddr < 0x3F00) {
                data = ppuread(ppuaddr);
            }
            else {
                data = ppuread(ppuaddr);
            }
            
            if (ppur7r) ppur7r = 0;
            else ppuaddr += (CTRL_RAI) ? 32 : 1;
            return data;
        }
        default: return (uint8_t) -1;
    }
}

/**
 * @brief Set PPU register
 * 
 * @param addr address
 * @param val value
 */  
inline void ppu_set_reg(uint16_t addr, uint8_t val) {
    addr &= 7;
    ppuaddr &= 0x3FFF;
    switch(addr) {
        case 0: {
            ppuctrl = val;
            return;
        }
        case 1: {
            ppumask = val; 
            return;
        }
        case 2: {
            log_warn("write to read-only register 0x2002.\n");
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
            if (xscroll_wrt_count) {
                yscroll = val;
            }
            else xscroll = val;

            xscroll_wrt_count = !xscroll_wrt_count;
            return;
        }
        case 6: {
            if (ppuaddr_rh)
                ppuaddr = (tmpaddr << 8) + val;
            else
                tmpaddr = val;

            ppuaddr_rh ^= 1;
            ppur7r = 1;
            break;
        }
        case 7: {
            if (ppuaddr > 0x1fff || ppuaddr < 0x4000) {
                ppuwrt(ppuaddr ^ mirror_xor, val);
                ppuwrt(ppuaddr, val);
            }
            else ppuwrt(ppuaddr, val);
        }
    }
    // unreached
}

/**
 * @brief init ppu
 * 
 */
inline void ppu_init() {
    ppuctrl = ppumask = oamaddr = xscroll = yscroll = wpos = ppudata = 0;
    xscroll_wrt_count = ppuaddr_rh = ppur7r = tmpaddr = 0;
    ppuaddr = mirror = mirror_xor = 0;
    ppustatus = 0b10100000;
    scanline = 0;

    // from NJU-ProjectN/LiteNES
    for (int h = 0; h < 0x100; h++) {
        for (int l = 0; l < 0x100; l++) {
            for (int x = 0; x < 8; x++) {
                ppu_lhtab[l][h][x] = (((h >> (7 - x)) & 1) << 1) | ((l >> (7 - x)) & 1);
                ppu_lhtabf[l][h][x] = (((h >> x) & 1) << 1) | ((l >> x) & 1);
            }
        }
    } 
}

/**
 * @brief render background
 * 
 * @param mirror mirror
 */
static inline void rndr_bg(uint8_t mirror) {
    for (uint8_t tile_x = MASK_SBG8 ? 0 : 1; tile_x < 32; tile_x++) {
        if (((tile_x << 3) - xscroll + (mirror ? 256 : 0)) > 256) continue;
        int tile_y = scanline >> 3;
        int tile_index = ppuread(bnta[CTRL_BNTA] + tile_x + (tile_y << 5) + (mirror ? 0x400 : 0));
        uint16_t tile_address = ((CTRL_BGTB) ? 0x1000 : 0) + 16 * tile_index;

        int y_in_tile = scanline & 0x7;
        uint8_t l = ppuread(tile_address + y_in_tile);
        uint8_t h = ppuread(tile_address + y_in_tile + 8);

        for (int x = 0; x < 8; x++) {
            uint8_t color = ppu_lhtab[l][h][x];

            if (color != 0) { 
                uint16_t attribute_address = (bnta[CTRL_BNTA] + (mirror ? 0x400 : 0) + 0x3C0 + (tile_x >> 2) + (scanline >> 5) * 8);
                uint8_t top = (scanline % 32) < 16;
                uint8_t left = (tile_x % 4 < 2);
                uint8_t palette_attribute = ppuread(attribute_address);

                if (!top) {
                    palette_attribute >>= 4;
                }
                if (!left) {
                    palette_attribute >>= 2;
                }
                palette_attribute &= 3;

                uint16_t palette_address = 0x3F00 + (palette_attribute << 2);
                int idx = ppuread(palette_address + color);

                bg[(tile_x << 3) + x][scanline] = color;
                
                gfx_set_pixel((tile_x << 3) + x - xscroll + (mirror ? 256 : 0), scanline + 1, palette[idx].r, palette[idx].g,  palette[idx].b);
            }
        }
    }
}

/**
 * @brief render sprites
 * 
 */
static inline void rndr_spr() {
    int scanline_sprite_count = 0;
    int n;
    for (n = 0; n < 0x100; n += 4) {
        uint8_t sprite_x = smem[n + 3];
        uint8_t sprite_y = smem[n];

        // Skip if sprite not on scanline
        if (sprite_y > scanline || sprite_y + (CTRL_SPSZ ? 16 : 8) < scanline)
           continue;

        scanline_sprite_count++;

        // PPU can't render > 8 sprites
        if (scanline_sprite_count > 8) {
            SSTAT_SO(1);
            // break;
        }

        uint8_t vflip = smem[n + 2] & 0x80;
        uint8_t hflip = smem[n + 2] & 0x40;

        uint16_t tile_address = (CTRL_STB ? 0x1000 : 0x0000) + 16 * smem[n + 1];
        int y_in_tile = scanline & 0x7;
        uint8_t l = ppuread(tile_address + (vflip ? (7 - y_in_tile) : y_in_tile));
        uint8_t h = ppuread(tile_address + (vflip ? (7 - y_in_tile) : y_in_tile) + 8);

        uint8_t palette_attribute = smem[n + 2] & 0x3;
        uint16_t palette_address = 0x3F10 + (palette_attribute << 2);
        int x;
        for (x = 0; x < 8; x++) {
            int color = hflip ? ppu_lhtabf[l][h][x] : ppu_lhtab[l][h][x];

            if (color != 0) {
                int screen_x = sprite_x + x;
                int idx = ppuread(palette_address + color);
                
                if (smem[n + 2] & 0x20) {
                    gfx_set_pixel(screen_x, sprite_y + y_in_tile + 1, palette[idx].r, palette[idx].g,  palette[idx].b); // FIXME: bbg
                }
                else {
                    gfx_set_pixel(screen_x, sprite_y + y_in_tile + 1, palette[idx].r, palette[idx].g,  palette[idx].b); // FIXME: bbg
                }

                if (MASK_SBG && !hit && n == 0 && bg[screen_x][sprite_y + y_in_tile] == color) {
                    SSTAT_SH(1);
                    hit = 1;
                }
            }
        }
    }
}

extern inline void ppu_run() {
    ++scanline;

    if (MASK_SBG) {
        rndr_bg(0);
        //rndr_bg(1);
    }

    if (MASK_SSP) {
        rndr_spr();
    }

    if (scanline == 241) {
        SSTAT_VB(1);
        SSTAT_SH(0);
        if (CTRL_NMI) interrupt_6502();
    } else if (scanline == 262) {
        scanline = -1;
        hit = 0;
        SSTAT_VB(0);
        gfx_render();
        gfx_new_frame();
    }
    
}

inline void ppu_sprram_write(uint8_t val) {
    smem[oamaddr++] = val;
}

void ppu_set_mirroring(uint8_t mir) {
    mirror = mir;
    mirror_xor = 0x400 << mir;
}