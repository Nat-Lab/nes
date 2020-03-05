#ifndef NES_PPH_H
#define NES_PPH_H
#include <stdint.h>
#include <unistd.h>

uint8_t ppuread (uint16_t addr);
void ppuwrt (uint16_t dst, uint8_t val);
void ppucpy (uint16_t dst, const uint8_t *src, size_t sz);

void ppu_io_write(uint16_t address, uint8_t data);
uint8_t ppu_io_read(uint16_t address);
uint8_t ppu_get_reg(uint16_t addr);
void ppu_set_reg(uint16_t addr, uint8_t val);
void ppu_set_mirroring(uint8_t mir);
void ppu_sprram_write(uint8_t val);
void ppu_init();
void ppu_run();

#endif // NES_PPH_H