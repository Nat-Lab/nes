#include "mem.h"
#include <memory.h>

static uint8_t mem[0xffff];

/**
 * @brief Read from vCPU memory
 * 
 * @param addr vaddress
 * @return uint8_t byte on the address
 */
inline uint8_t memread (uint16_t addr) {
    return mem[addr];
}

/**
 * @brief Write to vCPU memory
 * 
 * @param dst dst vaddress
 * @param val value
 */
inline void memwrt (uint16_t dst, uint8_t val) {
    mem[dst] = val;
}

/**
 * @brief Copy into vCPU memory
 * 
 * @param dst dst vaddress
 * @param src source address
 * @param sz num of bytes to copy
 */
inline void mmemcpy (uint16_t dst, const uint8_t *src, size_t sz) {
    memcpy(mem + dst, src, sz);
}