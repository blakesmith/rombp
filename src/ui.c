#include "ui.h"

int ui_start(rombp_ui* ui) {
    ui->sdl->screen_width = 320;
    ui->sdl->screen_height = 240;
    ui->sdl->scaling_factor = 1.0;
    
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL could not initialize TTF! TTF_Error: %s\n", TTF_GetError());
        return -1;
    }
    ui->sdl->window = SDL_CreateWindow("rombp",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   ui->sdl->screen_width * ui->sdl->scaling_factor,
                                   ui->sdl->screen_height * ui->sdl->scaling_factor,
                                   SDL_WINDOW_SHOWN);
    if (ui->sdl->window == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    ui->sdl->renderer = SDL_CreateRenderer(ui->sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if (ui->sdl->renderer == NULL) {
        fprintf(stderr, "Could not initialize renderer: SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    if (SDL_RenderSetScale(ui->sdl->renderer, ui->sdl->scaling_factor, ui->sdl->scaling_factor) < 0) {
        fprintf(stderr, "Could not set the renderer scaling: SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    return 0;
}

static void ui_resize_window(rombp_ui* ui, int width, int height) {
    ui->sdl->screen_width = width;
    ui->sdl->screen_height = height;
}

rombp_ui_event ui_handle_event(rombp_ui* ui, rombp_patch_command* command) {
    SDL_Event event;

    while (SDL_PollEvent(&event) != 0) {
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                    case SDLK_q:
                        return EV_QUIT;
                    default:
                        break;
                }
                break;
            case SDL_WINDOWEVENT: {
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_SIZE_CHANGED: {
                        int w = event.window.data1;
                        int h = event.window.data2;
                        ui_resize_window(ui, w, h);
                        printf("Window size is: %dx%d\n", w, h);
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
            case SDL_QUIT:
                return EV_QUIT;
            default:
                break;
        }
    }

    return EV_NONE;
}

void ui_draw(rombp_ui* ui) {
}
