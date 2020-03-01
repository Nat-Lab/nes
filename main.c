#include "rom.h"
#include "log.h"
#include "gfx.h"
#include "sdl.h"
#include "6502.h"
#include "ppu.h"
#include <fcntl.h>
#include <SDL2/SDL.h>

int main (int argc, char **argv) {
    const char *romfile = argv[1];
    uint8_t rom[0xffff];
    int romfd = open(romfile, O_RDONLY);

    if (romfd < 0) {
        log_fatal("can't open file: '%s'.\n", romfile);
        return -1;
    }

    ssize_t read_len = read(romfd, rom, 0xffff);
    if (read_len < 0) {
        log_fatal("failed to read rom.\n");
        return -1;
    }

    nes_meta_t meta;
    ssize_t parsed_sz = rom_parse(&meta, rom, (size_t) read_len);
    
    if (parsed_sz == read_len) {
        rom_load(&meta);
        log_debug("has_trainer: %s.\n", meta.trainer ? "yes" : "no");
        log_debug("prgm_sz: %d bytes.\n", meta.prgm_sz);
        log_debug("chr_sz: %d bytes.\n", meta.chr_sz);
        log_debug("mirror: %s.\n", meta.mirror ? "horizontal/mapper" : "vertical");
        log_debug("has_bat_ram: %s.\n", meta.bat_ram ? "yes" : "no");
        log_debug("fourscreen: %s.\n", meta.fourscreen ? "yes" : "no");
        log_debug("mapper: %d.\n", meta.mapper);
        log_debug("console_type: %d.\n", meta.console_type);
        log_debug("nes2.0: %s.\n", meta.nes20 ? "yes" : "no");
    } else {
        log_error("rom not fully pasred/unsupported rom.\n");
        return -1;
    }


    SDL_Event e;
    uint32_t ct, dt, t_ppu;
    uint64_t ll, ld;
    sdl_init();
    gfx_init();
    gfx_new_frame();

    init_6502();
    ppu_init();
    ppu_set_mirroring(meta.mirror & 1);
    while (1) {
        SDL_PollEvent(&e);
        if (e.type == SDL_QUIT) break;
        if (e.type == SDL_USEREVENT) {
            ct = SDL_GetTicks();
            ll = cycles_6502();
            ld = 0;
            ppu_run();
            t_ppu = SDL_GetTicks();
            for (; ld < 1364 / 12; ld = cycles_6502() - ll) {
                run_6502();
            }
            dt = SDL_GetTicks() - ct;
            if (dt > 16) {
                log_warn("can't keep up! frame time is %ums. (ppu: %ums, cpu: %ums)\n", dt, t_ppu - ct, dt - (t_ppu - ct));
            } 
        }
    }

    gfx_deinit();
    sdl_deinit();

    return 0;
}