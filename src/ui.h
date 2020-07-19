#ifndef ROMBP_UI_H_
#define ROMBP_UI_H_

#include <dirent.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef enum rombp_screen {
    SELECT_ROM = 0,
    SELECT_IPS = 1,
} rombp_screen;

typedef struct rombp_ui_status_bar {
    char* text;
    size_t text_len;
    SDL_Texture* text_texture;
    SDL_Color color;
    SDL_Rect position;
} rombup_ui_status_bar;

typedef struct rombp_sdl {
    int screen_width;
    int screen_height;
    float scaling_factor;
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* menu_font;

    SDL_Texture* status_bar_text_rom;
    SDL_Texture* status_bar_text_ips;
} rombp_sdl;

#define MENU_ITEM_COUNT 28

typedef struct rombp_ui {
    rombp_screen current_screen;
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
