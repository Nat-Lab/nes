#include "gfx.h"
#include "sdl.h"
#include "log.h"
#include <SDL2/SDL.h>
#define NES_W 256
#define NES_H 240

static int gfx_initialized = 0;
static SDL_Texture *texture = NULL;
static SDL_Renderer *rndr = NULL;
static SDL_Window *window = NULL;
static uint32_t pixbuf[NES_W * NES_H];
/**
 * @brief start a new frame
 * 
 */
void gfx_new_frame() {
    memset(pixbuf, 0, sizeof(pixbuf));
}

/**
 * @brief set a pixel of the current frame
 * 
 * @param x num of pixels from top of the window
 * @param y num of pixels from left of thw window
 * @param r red channel
 * @param g green channel
 * @param b blue channel
 */
inline void gfx_set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= NES_W || y >= NES_H) {
        //log_warn("pixel (%d, %d) out of bound.\n", x, y);
        return;
    }
    pixbuf[y * NES_W + x] = (0xff000000 | (r << 16) | (g << 8)| b);
}

/**
 * @brief init SDL gfx
 * 
 * @return int status
 * @retval -1 failed
 * @retval 0 OK
 */
int gfx_init() {
    if (!sdl_ready()) {
        log_error("SDL is not ready. call sdl_init();\n");
        return -1;
    }

    if (gfx_initialized == 1) {
        log_warn("GFX already initialized.\n");
        return 0;
    } else if (gfx_initialized == -1) {
        log_fatal("GFX: end of life cycle.\n");
        return -1;
    }

    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0")) {
        log_warn("failed to set render scale mode.\n");
    }

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        log_fatal("SDL video subsystem init failed.\n");
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return -1;
    }

    window = SDL_CreateWindow("nato-nes", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, NES_W, NES_H, SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        log_fatal("SDL_CreateWindow: %s\n", SDL_GetError());
        return -1;
	}
    
    rndr = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (rndr == NULL) {
        log_fatal("SDL_CreateRenderer: %s\n", SDL_GetError());
        return -1;
    }

    texture = SDL_CreateTexture(rndr, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NES_W, NES_H);
    if (texture == NULL) {
        log_fatal("SDL_CreateTexture: %s\n", SDL_GetError());
        return -1;
    }

    gfx_initialized = 1;
    
    return 0;
}

/**
 * @brief de-init gfx
 * 
 */
void gfx_deinit() {
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(rndr);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    gfx_initialized = -1;
}

/**
 * @brief render current frame
 * 
 */
inline void gfx_render() {
    if (gfx_initialized != 1) {
        log_error("render requested in bad state.\n");
        return;
    }
    void *pixels;
    int pitch;
    if (SDL_LockTexture(texture, NULL, &pixels, &pitch) < 0) {
        log_error("failed to lock texture: %s.\n", SDL_GetError());
        return;
    }
    memcpy(pixels, pixbuf, sizeof(pixbuf));
    SDL_UnlockTexture(texture);
    SDL_RenderClear(rndr);
    SDL_RenderCopy(rndr, texture, NULL, NULL);
    SDL_RenderPresent(rndr);
}