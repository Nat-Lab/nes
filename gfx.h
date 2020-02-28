#ifndef NES_GFX_H
#define NES_GFX_H
#include <stdint.h>
void gfx_new_frame();
void gfx_set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
void gfx_deinit();
void gfx_render();
int gfx_init();
#endif // NES_GFX_H