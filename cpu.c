#include "mem.h"
#include "ppu.h"

/**
 * @brief read from CPU address
 * 
 * @param addr address
 * @return uint8_t value
 */
inline uint8_t cpuread(uint16_t addr) {
    if (addr < 0x2000) return memread(addr); // internal ram & mirror
    if (addr < 0x4000) return ppu_get_reg(addr);
    if (addr < 0x4020) {
        log_warn("FIXME: I/O and audio not implemented.\n");
        return (uint8_t) -1;
    }
    if (addr < 0xFFFF) return memread(addr);
}

/**
 * @brief write to CPU address
 * 
 * @param addr address
 * @param val value
 */
inline void cpuwrt(uint16_t addr, uint8_t val) {
    if (addr < 0x2000) { // internal ram & mirror
        uint16_t ba = addr & 0x07ff; // base addr
        memwrt(ba, val); 
        memwrt(ba + 0x0800, val); // mirrored  
    }
    if (addr < 0x4000) ppu_set_reg(addr, val);
    if (addr < 0x4020) {
        log_warn("FIXME: I/O and audio not implemented.\n");
        return (uint8_t) -1;
    }
    if (addr < 0xFFFF) {
        log_warn("writing to PRG.\n");
        return memwrt(addr, val);
    }
}