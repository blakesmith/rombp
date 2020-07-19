#include <sys/param.h>

#include "ui.h"

static const int MENU_FONT_SIZE = 16;

static int new_text_texture(rombp_ui* ui, const char* text, SDL_Color color, SDL_Texture** texture) {
    SDL_Surface* text_surface = TTF_RenderText_Solid(ui->sdl.menu_font,
                                                     text,
                                                     color);
    if (text_surface == NULL) {
        fprintf(stderr, "Could not convert text to a surface: %s\n", SDL_GetError());
        return -1;
    }
    *texture = SDL_CreateTextureFromSurface(ui->sdl.renderer, text_surface);
    if (*texture == NULL) {
        fprintf(stderr, "Could not convert text to texture: %s\n", SDL_GetError());
        return -1;
    }
    SDL_FreeSurface(text_surface);
    return 0;
}

static int dir_alphasort(const struct dirent** a, const struct dirent** b) {
    if ((*a)->d_type == DT_DIR && (*b)->d_type == DT_REG) {
        return -1;
    } else if ((*a)->d_type == DT_REG && (*b)->d_type == DT_DIR) {
        return 1;
    } else {
        return alphasort(a, b);
    }
}

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

static void ui_bottom_bar_free(rombp_ui* ui) {
    if (ui->bottom_bar_text_1 != NULL) {
        SDL_DestroyTexture(ui->bottom_bar_text_1);
    }
    if (ui->bottom_bar_text_2 != NULL) {
        SDL_DestroyTexture(ui->bottom_bar_text_2);
    }
}

static int ui_render_menu_fonts(rombp_ui* ui) {
    ui->namelist_text = calloc(sizeof(SDL_Texture*), ui->namelist_size);
    if (ui->namelist_text == NULL) {
        fprintf(stderr, "Failed to allocate menu text texture array\n");
        return -1;
    }

    static const SDL_Color file_color = { 0xFF, 0xFF, 0xFF };
    static const SDL_Color directory_color = { 0xAE, 0xD6, 0xF1 };

    for (int i = 0; i < ui->namelist_size; i++) {
        unsigned char type = ui->namelist[i]->d_type;
        SDL_Texture *text_texture;

        int rc = new_text_texture(ui, ui->namelist[i]->d_name, type == DT_DIR ? directory_color : file_color, &text_texture);
        if (rc != 0) {
            fprintf(stderr, "Failed to create bottom bar text: %d\n", rc);
            return rc;
        }
        ui->namelist_text[i] = text_texture;
    }

    return 0;
}

static int ui_scan_directory(rombp_ui* ui) {
    ui_directory_free(ui);
    
    int namelist_size = scandir(ui->current_directory, &ui->namelist, NULL, dir_alphasort);
    if (namelist_size == -1) {
        fprintf(stderr, "Failed to scan directory: %s\n", ui->current_directory);
        return -1;
    }
    ui->namelist_size = namelist_size;
    ui->selected_item = 0;
    ui->selected_offset = 0;

    return ui_render_menu_fonts(ui);
}

static char* concat_path(char* parent, char* child) {
    size_t next_size = strlen(parent) + strlen(child) + 2;
    size_t parent_size = strlen(parent);

    char* next_path = malloc(next_size);
    if (next_path == NULL) {
        fprintf(stderr, "Failed to alloc next childectory path: %s\n", child);
        return NULL;
    }
    strncpy(next_path, parent, parent_size);
    strncpy(next_path + parent_size, "/", 2);
    strncpy(next_path + parent_size + 1, child, strlen(child) + 1);
    return next_path;
}

static int ui_change_directory(rombp_ui* ui, char* dir) {
    size_t size = strlen(dir) + 1;

    if (ui->current_directory == NULL) {
        ui->current_directory = malloc(size);
        if (ui->current_directory == NULL) {
            fprintf(stderr, "Failed to alloc directory path: %s\n", dir);
            return -1;
        }
        strncpy(ui->current_directory, dir, size);
    } else {
        char* next_directory = concat_path(ui->current_directory, dir);
        if (next_directory == NULL) {
            fprintf(stderr, "Couldn't get next directory\n");
            return -1;
        }

        free(ui->current_directory);
        ui->current_directory = next_directory;
    }

    return 0;
}

static const char* BOTTOM_BAR_ITEM_1 = "Select ROM file | Y=select, B=quit";
static const char* BOTTOM_BAR_ITEM_2 = "Select IPS file | Y=select, B=back";

static int ui_setup_bottom_bar(rombp_ui* ui) {
    static const SDL_Color text_color = { 0xFF, 0xFF, 0xFF };
    int rc;

    rc = new_text_texture(ui, BOTTOM_BAR_ITEM_1, text_color, &ui->bottom_bar_text_1);
    if (rc != 0) {
        fprintf(stderr, "Failed to create bottom bar text: %d\n", rc);
        return rc;
    }
    rc = new_text_texture(ui, BOTTOM_BAR_ITEM_2, text_color, &ui->bottom_bar_text_2);
    if (rc != 0) {
        fprintf(stderr, "Failed to create bottom bar text: %d\n", rc);
        return rc;
    }

    return 0;
}

int ui_start(rombp_ui* ui) {
    printf("Starting UI\n");

    ui->namelist = NULL;
    ui->namelist_text = NULL;
    ui->current_directory = NULL;

    ui->current_screen = SELECT_ROM;
    ui->selected_item = 0;
    ui->selected_offset = 0;
    ui->sdl.screen_width = 640;
    ui->sdl.screen_height = 480;
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

    ui->sdl.menu_font = TTF_OpenFont("assets/fonts/PressStart2P.ttf", MENU_FONT_SIZE);
    if (ui->sdl.menu_font == NULL) {
        fprintf(stderr, "Failed to load menu font: %s\n", TTF_GetError());
        return -1;
    }

    int rc = ui_change_directory(ui, ".");
    if (rc < 0) {
        fprintf(stderr, "Failed to change directory: %d\n", rc);
        return -1;
    }
    rc = ui_scan_directory(ui);
    if (rc < 0) {
        fprintf(stderr, "Failed to scan directory, error code: %d\n", rc);
        return -1;
    }

    rc = ui_setup_bottom_bar(ui);
    if (rc < 0) {
        fprintf(stderr, "Failed to setup bottom bar: %d\n", rc);
        return -1;
    }

    return 0;
}

void ui_stop(rombp_ui* ui) {
    ui_directory_free(ui);
    ui_bottom_bar_free(ui);
    if (ui->current_directory != NULL) {
        free(ui->current_directory);
    }

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

static rombp_ui_event ui_handle_back(rombp_ui* ui, rombp_patch_command* command) {
    if (command->input_file == NULL) {
        return EV_QUIT;
    } else if (command->input_file != NULL) {
        free(command->input_file);
        command->input_file = NULL;
        ui->current_screen = SELECT_ROM;
        return EV_NONE;
    }

    return EV_NONE;
}

static int do_replace_ext(char* input, const char* new_ext) {
    char *end = input + strlen(input);

    // First, find the location of the last dot, before the extension name.
    while (end > input && *end != '.' && *end != '\\' && *end != '/') {
        --end;
    }
    if ((end > input && *end == '.') &&
        (*(end - 1) != '\\' && *(end - 1) != '/')) {
        // Where we would normally add a NULL terminating
        // byte, replace the extension.
        strncpy(end, new_ext, strlen(new_ext) + 1);
        return 0;
    } else {
        return -1;
    }
}

static int replace_extension(char* input, const char* new_ext, char** output) {
    size_t output_length = strlen(input) + strlen(new_ext) + 1;
    char* out = malloc(output_length);
    if (out == NULL) {
        fprintf(stderr, "Failed to alloc extension replaced output\n");
        return -1;
    }

    strncpy(out, input, output_length);
    // First, jump to the end of the string
    char *end = out + strlen(out);

    // Then, find the location of the last dot, before the extension name.
    while (end > out && *end != '.' && *end != '\\' && *end != '/') {
        --end;
    }

    if ((end > out && *end == '.') &&
        (*(end - 1) != '\\' && *(end - 1) != '/')) {
        // Where we would normally add a NULL terminating
        // byte, replace the extension.
        strncpy(end, new_ext, strlen(new_ext) + 1);
        *output = out;
        return 0;
    } else {
        return -1;
    }
}

static int ui_handle_select(rombp_ui* ui, rombp_patch_command* command) {
    struct dirent* selected_item = ui->namelist[ui->selected_item + ui->selected_offset];
    int rc;

    if (selected_item->d_type == DT_DIR) {
        printf("Got directory selection\n");
        rc = ui_change_directory(ui, selected_item->d_name);
        if (rc != 0) {
            fprintf(stderr, "Failed to change directory: %s, rc: %d\n",selected_item->d_name, rc);
            return -1;
        }
        return ui_scan_directory(ui);
    } else if (selected_item->d_type == DT_REG) {
        printf("Got file selection\n");
        if (command->input_file == NULL) {
            command->input_file = concat_path(ui->current_directory, selected_item->d_name);
            ui->current_screen = SELECT_IPS;
        } else if (command->ips_file == NULL) {
            command->ips_file = concat_path(ui->current_directory, selected_item->d_name);
            char* copied_output = strdup(command->ips_file);
            if (copied_output == NULL) {
                fprintf(stderr, "Failed to copy output_path string\n");
                return -1;
            }
            char* replaced_extension;
            rc = replace_extension(copied_output, ".smc", &replaced_extension);
            if (rc != 0) {
                fprintf(stderr, "Failed to replace extension for file: %s\n", copied_output);
                free(copied_output);
                return rc;
            }

            command->output_file = replaced_extension;
            free(copied_output);
        }
    }

    return 0;
}

void ui_free_command(rombp_patch_command* command) {
    if (command->input_file != NULL) {
        free(command->input_file);
        command->input_file = NULL;
    }
    if (command->ips_file != NULL) {
        free(command->ips_file);
        command->ips_file = NULL;
    }
    if (command->output_file != NULL) {
        free(command->output_file);
        command->output_file = NULL;
    }
}

rombp_ui_event ui_handle_event(rombp_ui* ui, rombp_patch_command* command) {
    SDL_Event event;
    int rc;

    while (SDL_PollEvent(&event) != 0) {
        int nitems = MIN(MENU_ITEM_COUNT, ui->namelist_size);
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                    case SDLK_q:
                        return EV_QUIT;
                    case SDLK_b:
                        return ui_handle_back(ui, command);
                    case SDLK_RETURN:
                    case SDLK_y:
                        rc = ui_handle_select(ui, command);
                        if (rc != 0) {
                            fprintf(stderr, "Failed to handle select event: %d\n", rc);
                            return EV_NONE;
                        }
                        if (command->input_file != NULL && command->ips_file != NULL) {
                            ui->current_screen = SELECT_ROM;
                            return EV_PATCH_COMMAND;
                        }
                        break;
                    case SDLK_DOWN:
                        if (ui->selected_item == (nitems - 1)) {
                            // Don't allow any paging offset if the number of directory items fits on the screen at once. Otherwise,
                            // make the last allowed offset the difference of the two.
                            int max_offset = ui->namelist_size == nitems ? 0 : ui->namelist_size - nitems;

                            ui->selected_item = nitems - 1;
                            if (ui->selected_offset != max_offset) {
                                ui->selected_offset++;
                            }
                        } else {
                            ui->selected_item = ui->selected_item + 1;
                        }
                        break;
                    case SDLK_UP:
                        if (ui->selected_item == 0) {
                            ui->selected_item = 0;
                            if (ui->selected_offset != 0) {
                                ui->selected_offset--;
                            }
                        } else {
                            ui->selected_item = ui->selected_item - 1;
                        }
                        break;
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

static int draw_bottom_bar(rombp_ui* ui) {
    int rc;
    SDL_Rect bottom_bar_rect;

    bottom_bar_rect.x = 0;
    bottom_bar_rect.y = ui->sdl.screen_height - MENU_FONT_SIZE;
    bottom_bar_rect.h = MENU_FONT_SIZE;
    bottom_bar_rect.w = ui->sdl.screen_width;

    SDL_SetRenderDrawColor(ui->sdl.renderer, 0x21, 0x2F, 0x3C, 0xFF);
    SDL_RenderFillRect(ui->sdl.renderer, &bottom_bar_rect);

    switch (ui->current_screen) {
        case SELECT_ROM:
            bottom_bar_rect.w = MENU_FONT_SIZE * strlen(BOTTOM_BAR_ITEM_1);
            rc = SDL_RenderCopy(ui->sdl.renderer, ui->bottom_bar_text_1, NULL, &bottom_bar_rect);
            if (rc < 0) {
                fprintf(stderr, "Failed to render text surface: %s\n", SDL_GetError());
                return rc;
            }
            break;
        case SELECT_IPS:
            bottom_bar_rect.w = MENU_FONT_SIZE * strlen(BOTTOM_BAR_ITEM_2);
            rc = SDL_RenderCopy(ui->sdl.renderer, ui->bottom_bar_text_2, NULL, &bottom_bar_rect);
            if (rc < 0) {
                fprintf(stderr, "Failed to render text surface: %s\n", SDL_GetError());
                return rc;
            }
            break;
    }

    return 0;
}

static int draw_menu(rombp_ui* ui) {
    static const int menu_padding_left_right = 15;
    static const int menu_padding_top_bottom = 5;

    int rc;
    SDL_Rect menu_item_rect;

    int nitems = MIN(MENU_ITEM_COUNT, ui->namelist_size);
    for (int i = 0; i < nitems; i++) {
        menu_item_rect.x = menu_padding_left_right;
        menu_item_rect.y = (i * MENU_FONT_SIZE) + menu_padding_top_bottom;
        menu_item_rect.h = MENU_FONT_SIZE;


        if (i == ui->selected_item) {
            menu_item_rect.w = ui->sdl.screen_width - menu_padding_left_right;
            SDL_SetRenderDrawColor(ui->sdl.renderer, 0x5B, 0x2C, 0x6F, 0xFF);
            SDL_RenderFillRect(ui->sdl.renderer, &menu_item_rect);
        }

        size_t paging_offset = ui->selected_offset + i;
        menu_item_rect.w = MENU_FONT_SIZE * strlen(ui->namelist[paging_offset]->d_name);

        rc = SDL_RenderCopy(ui->sdl.renderer, ui->namelist_text[paging_offset], NULL, &menu_item_rect);
        if (rc < 0) {
            fprintf(stderr, "Failed to render text surface: %s\n", SDL_GetError());
            return rc;
        }

    }

    return 0;
}

int ui_draw(rombp_ui* ui) {
    int rc;

    SDL_SetRenderDrawColor(ui->sdl.renderer, 0x00, 0x10, 0x00, 0xFF);
    SDL_RenderClear(ui->sdl.renderer);

    rc = draw_menu(ui);
    if (rc != 0) {
        fprintf(stderr, "Failed to draw menu: %d\n", rc);
        return rc;
    }

    rc = draw_bottom_bar(ui);
    if (rc != 0) {
        fprintf(stderr, "Failed to draw bottom bar: %d\n", rc);
        return rc;
    }

    SDL_RenderPresent(ui->sdl.renderer);

    return 0;
}
