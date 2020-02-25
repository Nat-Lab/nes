#include "rom.h"
#include "mem.h"
#include "ppu.h"
#include "log.h"
#include <memory.h>
#include <unistd.h>

ssize_t rom_parse(nes_meta_t *meta, const uint8_t *rom, size_t sz) {
    #define WANT_SZ(n) if (sz < n) { log_fatal("unexpected end of file.\n"); return -1; } else sz -= n;
    const uint8_t *ptr = rom;
    WANT_SZ(sizeof(nes_hdr_t));

    const nes_hdr_t *hdr = (nes_hdr_t *) ptr;
    ptr += sizeof(nes_hdr_t);
    if (memcmp(hdr->magic, NES_MAGIC, 4) != 0) {
        log_fatal("bad res header magic.\n");
        return -1;
    };
    
    
    meta->chr_sz = hdr->chr_rom_sz_8k * 8 * 1024;
    meta->prgm_sz = hdr->prgm_rom_sz_16k * 16 * 1024;
    meta->mapper = (hdr->flag6 >> 4) | ((hdr->flag7) & 0b11110000);
    meta->mirror       = (hdr->flag6 & 0b00000001) != 0;
    meta->bat_ram      = (hdr->flag6 & 0b00000010) != 0;
    int has_trainer    = (hdr->flag6 & 0b00000100) != 0;
    meta->fourscreen   = (hdr->flag6 & 0b00001000) != 0;
    meta->console_type = (hdr->flag7 & 0b00000011);
    meta->nes20        = (hdr->flag7 & 0b00000100) != 0;

    if (has_trainer) {
        WANT_SZ(512);
        meta->trainer = ptr;
        ptr += 512;
    } else meta->trainer = NULL;

    WANT_SZ(meta->prgm_sz);
    meta->prgm = ptr;
    ptr += meta->prgm_sz;

    // TODO: CHR-RAM
    WANT_SZ(meta->chr_sz);
    meta->chr = ptr;
    ptr += meta->chr_sz;

    if (sz != 0) {
        log_warn("got to the end but rom not completely.\n");
    }

    return ptr - rom;
}

/**
 * @brief Load the parsed rom to CPU MEM and PPU MEM
 * 
 * @param meta parsed rom
 * @return int status
 * @retval -1 failed
 * @retval 0 loaded
 */
int rom_load(const nes_meta_t *meta) {
    if (meta->mapper == 0) { // TODO: other mappers
        if (meta->prgm_sz == 0x4000) { // mirror prgm-rom if sz is 16k
            mmemcpy(0x8000, meta->prgm, meta->prgm_sz);
            mmemcpy(0xC000, meta->prgm, meta->prgm_sz); 
        } else if (meta->prgm_sz == 0x8000) {
            mmemcpy(0x8000, meta->prgm, meta->prgm_sz);
        } else {
            log_fatal("bad prgm_sz: %d.\n", meta->prgm_sz);
            return -1;
        }
    } else {
        log_fatal("mapper %d not yet implemented.\n", meta->mapper);
        return -1;
    }

    ppucpy(0, meta->chr, 0x2000);    
    return 0;
}