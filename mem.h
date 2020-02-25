#ifndef NES_MEN_H
#define NES_MEN_H
#include <stdint.h>
#include <unistd.h>

uint8_t memread (uint16_t addr);
void memwrt (uint16_t dst, uint8_t val);
void mmemcpy (uint16_t dst, const uint8_t *src, size_t sz);

#endif // NES_MEN_H