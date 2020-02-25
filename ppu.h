#ifndef NES_PPH_H
#define NES_PPH_H
#include <stdint.h>
#include <unistd.h>

uint8_t ppuread (uint16_t addr);
void ppuwrt (uint16_t dst, uint8_t val);
void ppucpy (uint16_t dst, const uint8_t *src, size_t sz);

#endif // NES_PPH_H