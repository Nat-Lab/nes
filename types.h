#ifndef NES_TYPES_H
#define NES_TYPES_H
#include <stdint.h>
#define NES_MAGIC "NES\x1a"

/**
 * @brief raw nes file header
 * 
 */
typedef struct nes_hdr nes_hdr_t;
struct __attribute__((__packed__)) nes_hdr {
    // magic string "NES\x1A"
    uint8_t magic[4];

    // size of PRG ROM in 16k unit
    uint8_t prgm_rom_sz_16k;

    // size of CHR ROM in 8k uint (if 0, use CHR RAM)
    uint8_t chr_rom_sz_8k;

    // mapper, mirroring, bat, trainer, VS/Playchoice, NES2.0
    uint8_t flag6;
    uint8_t flag7;

    // padding
    uint8_t padding[8];
};

/**
 * @brief Type of the consoles
 * 
 */
enum _console_type {
    NES, VS, PC10
};

/**
 * @brief interpreted nse header
 * 
 */
typedef struct nes_meta nes_meta_t;
struct nes_meta {
    // pointer to program ROM
    const uint8_t *prgm;

    // pointer to chr ROM
    const uint8_t *chr;

    // trainer, if exist, NULL if not exist
    const uint8_t *trainer;

    // size of PRG ROM
    uint32_t prgm_sz;

    // size of chr ROM
    uint32_t chr_sz;

    // 0: horizontal/mapper, 1: vertical
    uint8_t mirror;

    // has bat-powered ram
    uint8_t bat_ram;

    // not mirror, four-screen VRAM.
    uint8_t fourscreen;

    // mapper
    uint8_t mapper;

    // type of the console
    uint8_t console_type;

    // nes2.0
    uint8_t nes20;
};

#endif // NES_TYPES_H