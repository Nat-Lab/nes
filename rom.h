#ifndef NES_ROM_H
#define NES_ROM_H
#include <unistd.h>
#include <stdint.h>
#include "types.h"

ssize_t rom_parse(nes_meta_t *meta, const uint8_t *rom, size_t sz);
int rom_load(const nes_meta_t *meta);

#endif // NES_ROM_H