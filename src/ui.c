#include <sys/param.h>

#include "ui.h"

static void ui_directory_free(rombp_ui* ui) {
    if (ui->namelist != NULL) {
        for (int i = 0; i < ui->namelist_size; i++) {
            free(ui->namelist[i]);
        }
        free(ui->namelist);
        ui->namelist = NULL;
    }
    if (ui->namelist_text != NULL) {
        for (int i = 0; i < ui->namelist_size; i++) {
            SDL_DestroyTexture(ui->namelist_text[i]);
        }
        free(ui->namelist_text);
        ui->namelist_text = NULL;
    }
}

static int ui_render_menu_fonts(rombp_ui* ui) {
    ui->namelist_text = calloc(sizeof(SDL_Texture*), ui->namelist_size);
    if (ui->namelist_text == NULL) {
        fprintf(stderr, "Failed to allocate menu text texture array\n");
        return -1;
    }
    for (int i = 0; i < ui->namelist_size; i++) {
        SDL_Color inactive_color = { 0xFF, 0xFF, 0xFF };
        SDL_Surface* text_surface = TTF_RenderText_Solid(ui->sdl.menu_font, ui->namelist[i]->d_name, inactive_color);
        if (text_surface == NULL) {
            fprintf(stderr, "Could not convert menu text to a surface: %s\n", SDL_GetError());
            return -1;
        }
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(ui->sdl.renderer, text_surface);
        if (text_texture == NULL) {
            fprintf(stderr, "Could not convert menu text to texture: %s\n", SDL_GetError());
            return -1;
        }
        SDL_FreeSurface(text_surface);
        ui->namelist_text[i] = text_texture;
    }

    return 0;
}

static int ui_scan_directory(rombp_ui* ui, const char* dir) {
    ui_directory_free(ui);
    
    int namelist_size = scandir(dir, &ui->namelist, NULL, alphasort);
    if (namelist_size == -1) {
        fprintf(stderr, "Failed to scan directory: %s\n", dir);
        return -1;
    }
    ui->namelist_size = namelist_size;
    ui->selected_item = 0;

    return ui_render_menu_fonts(ui);
}

int ui_start(rombp_ui* ui) {
    printf("Starting UI\n");

    ui->namelist = NULL;
    ui->namelist_text = NULL;

    ui->selected_item = 0;
    ui->sdl.screen_width = 320;
    ui->sdl.screen_height = 240;
    ui->sdl.scaling_factor = 2.0;
    
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL could not initialize TTF! TTF_Error: %s\n", TTF_GetError());
        return -1;
    }
    ui->sdl.window = SDL_CreateWindow("rombp",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   ui->sdl.screen_width * ui->sdl.scaling_factor,
                                   ui->sdl.screen_height * ui->sdl.scaling_factor,
                                   SDL_WINDOW_SHOWN);
    if (ui->sdl.window == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    ui->sdl.renderer = SDL_CreateRenderer(ui->sdl.window, -1, SDL_RENDERER_ACCELERATED);
    if (ui->sdl.renderer == NULL) {
        fprintf(stderr, "Could not initialize renderer: SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    if (SDL_RenderSetScale(ui->sdl.renderer, ui->sdl.scaling_factor, ui->sdl.scaling_factor) < 0) {
        fprintf(stderr, "Could not set the renderer scaling: SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    ui->sdl.menu_font = TTF_OpenFont("assets/fonts/PressStart2P.ttf", 16);
    if (ui->sdl.menu_font == NULL) {
        fprintf(stderr, "Failed to load menu font: %s\n", TTF_GetError());
        return -1;
    }

    int rc = ui_scan_directory(ui, ".");
    if (rc < 0) {
        fprintf(stderr, "Failed to scan directory, error code: %d\n", rc);
        return -1;
    }

    return 0;
}

void ui_stop(rombp_ui* ui) {
    TTF_CloseFont(ui->sdl.menu_font);
    SDL_DestroyRenderer(ui->sdl.renderer);
    SDL_DestroyWindow(ui->sdl.window);
    TTF_Quit();

    SDL_Quit();
}

static void ui_resize_window(rombp_ui* ui, int width, int height) {
    ui->sdl.screen_width = width;
    ui->sdl.screen_height = height;
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
                    case SDLK_RETURN:
                        // TODO: Stop hardcoding, get from an actual FS dir list
                        command->input_file = "fixtures/Super Metroid (JU) [!].smc";
                        command->output_file = "Ascent.Super Metroid (JU) [!].smc";
                        command->ips_file = "fixtures/Ascent1.12.IPS";
                        return EV_PATCH_COMMAND;
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

static void draw_menu(rombp_ui* ui) {
    static const int menu_padding = 50;
    const SDL_Rect menu_item_rect = {
        menu_padding,
        menu_padding,
        ui->sdl.screen_width - (menu_padding * 2),
        ui->sdl.screen_height / MENU_ITEM_COUNT,
    };
    SDL_SetRenderDrawColor(ui->sdl.renderer, 0x00, 0xFF, 0x00, 0xFF);
    SDL_RenderFillRect(ui->sdl.renderer, &menu_item_rect);
}

void ui_draw(rombp_ui* ui) {
    SDL_SetRenderDrawColor(ui->sdl.renderer, 0x00, 0x10, 0x00, 0xFF);
    SDL_RenderClear(ui->sdl.renderer);

    draw_menu(ui);

    SDL_RenderPresent(ui->sdl.renderer);
}
