#include "ui.h"

int ui_start(rombp_sdl* sdl) {
    sdl->screen_width = 320;
    sdl->screen_height = 240;
    sdl->scaling_factor = 1.0;
    
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL could not initialize TTF! TTF_Error: %s\n", TTF_GetError());
        return -1;
    }
    sdl->window = SDL_CreateWindow("rombp",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   sdl->screen_width * sdl->scaling_factor,
                                   sdl->screen_height * sdl->scaling_factor,
                                   SDL_WINDOW_SHOWN);
    if (sdl->window == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if (sdl->renderer == NULL) {
        fprintf(stderr, "Could not initialize renderer: SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    if (SDL_RenderSetScale(sdl->renderer, sdl->scaling_factor, sdl->scaling_factor) < 0) {
        fprintf(stderr, "Could not set the renderer scaling: SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    return 0;
}
