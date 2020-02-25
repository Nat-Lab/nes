#include "sdl.h"
#include "log.h"
#include <SDL2/SDL.h>

static int sdl_initialized = 0;

/**
 * @brief check if SDL is ready
 * 
 * @return int status
 * @retval -1 failed
 * @retval 0 OK
 */
int sdl_ready() {
    return sdl_initialized == 1;
}

/**
 * @brief initialize SDL
 * 
 * @return int status
 * @return int status
 * @retval -1 failed
 * @retval 0 OK
 */
int sdl_init() {
    if (sdl_initialized == 1) {
        log_warn("SDL already initialized.\n");
        return 0;
    } else if (sdl_initialized == -1) {
        log_fatal("SDL: end of life cycle.\n");
        return -1;
    }

    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER)) {
        log_fatal("SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

    sdl_initialized = 1;

    return 0;    
}

/**
 * @brief Do SDL_Quit
 * 
 */
void sdl_deinit() {
    SDL_Quit();
    sdl_initialized = -1;
}

/**
 * @brief SDL event loop
 * 
 */
void sdl_event_loop() {
    SDL_Event e;
    int quit = 0;
    while (!quit){
        while (SDL_PollEvent(&e)){
            if (e.type == SDL_QUIT){
                quit = 1;
            }
            // TODO
        }
    }
}