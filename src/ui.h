#ifndef ROMBP_UI_H_
#define ROMBP_UI_H_

#include <dirent.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct rombp_sdl {
    int screen_width;
    int screen_height;
    float scaling_factor;
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* menu_font;
} rombp_sdl;

#define MENU_ITEM_COUNT 28

typedef struct rombp_ui {
    rombp_sdl sdl;
    // The item that's actually selected
    uint16_t selected_item;
    // Scrolling support: The starting offset to start rendering items in the
    // directory listing.
    uint16_t selected_offset;

    char* current_directory;
    struct dirent** namelist;
    SDL_Texture** namelist_text;
    int namelist_size;

    SDL_Texture* bottom_bar_text;
} rombp_ui;

typedef enum rombp_ui_event {
    EV_NONE,
    EV_PATCH_COMMAND,
    EV_QUIT,
} rombp_ui_event;

typedef struct rombp_patch_command {
    char* input_file;
    char* output_file;
    char* ips_file;
} rombp_patch_command;

int ui_start(rombp_ui* ui);
void ui_stop(rombp_ui* ui);
int ui_draw(rombp_ui* ui);
rombp_ui_event ui_handle_event(rombp_ui* ui, rombp_patch_command* command);
void ui_free_command(rombp_patch_command* command);

#endif
