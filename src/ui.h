#ifndef ROMBP_UI_H_
#define ROMBP_UI_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct rombp_sdl {
    int screen_width;
    int screen_height;
    float scaling_factor;
    SDL_Window* window;
    SDL_Renderer* renderer;
} rombp_sdl;

typedef struct rombp_ui {
    rombp_sdl sdl;
} rombp_ui;

typedef enum rombp_ui_event {
    EV_NONE,
    EV_PATCH_COMMAND,
    EV_QUIT,
} rombp_ui_event;

typedef struct rombp_patch_command {
    const char* input_file;
    const char* output_file;
    const char* ips_file;
} rombp_patch_command;

int ui_start(rombp_ui* ui);
void ui_stop(rombp_ui* ui);
void ui_draw(rombp_ui* ui);
rombp_ui_event ui_handle_event(rombp_ui* ui, rombp_patch_command* command);

#endif
