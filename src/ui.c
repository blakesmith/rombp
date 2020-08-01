#include <sys/param.h>
#include <sys/stat.h>

#include "log.h"
#include "ui.h"

#ifdef TARGET_RG350
#define MENU_FONT_SIZE 18
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define SCALING_FACTOR 1.0
#define WINDOW_SETTING SDL_WINDOW_FULLSCREEN_DESKTOP
#define MENU_ITEM_COUNT 11
#define STARTING_DIR "/media"
#else
#define MENU_FONT_SIZE 36
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 960
#define SCALING_FACTOR 1.0
#define WINDOW_SETTING SDL_WINDOW_SHOWN
#define MENU_ITEM_COUNT 24
#define STARTING_DIR "/"
#endif

static const int menu_padding_left_right = 15;
static const int menu_padding_top_bottom = 25;

static const char* STATUS_BAR_TEXT_ROM = "Select ROM file | A=select, B=quit";
static const char* STATUS_BAR_TEXT_IPS = "Select IPS file | A=select, B=back";

static const char* BOTTOM_BAR_TEXT = "rombp v0.0.1";

static inline int get_texture_width(SDL_Texture* texture) {
    int width;
    SDL_QueryTexture(texture, NULL, NULL, &width, NULL);
    return width;
}

static inline int get_texture_height(SDL_Texture* texture) {
    int height;
    SDL_QueryTexture(texture, NULL, NULL, NULL, &height);
    return height;
}

static int new_text_texture(rombp_ui* ui, const char* text, SDL_Color color, SDL_Texture** texture) {
    SDL_Surface* text_surface = TTF_RenderUTF8_Solid(ui->sdl.menu_font,
                                                     text,
                                                     color);
    if (text_surface == NULL) {
        rombp_log_err("Could not convert text to a surface: %s\n", SDL_GetError());
        return -1;
    }
    *texture = SDL_CreateTextureFromSurface(ui->sdl.renderer, text_surface);
    if (*texture == NULL) {
        rombp_log_err("Could not convert text to texture: %s\n", SDL_GetError());
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

static int ui_render_menu_fonts(rombp_ui* ui) {
    ui->namelist_text = calloc(sizeof(SDL_Texture*), ui->namelist_size);
    if (ui->namelist_text == NULL) {
        rombp_log_err("Failed to allocate menu text texture array\n");
        return -1;
    }

    static const SDL_Color file_color = { 0xFF, 0xFF, 0xFF };
    static const SDL_Color directory_color = { 0xAE, 0xD6, 0xF1 };

    for (int i = 0; i < ui->namelist_size; i++) {
        unsigned char type = ui->namelist[i]->d_type;
        SDL_Texture *text_texture;

        int rc = new_text_texture(ui, ui->namelist[i]->d_name, type == DT_DIR ? directory_color : file_color, &text_texture);
        if (rc != 0) {
            rombp_log_err("Failed to create bottom bar text: %d\n", rc);
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
        rombp_log_err("Failed to scan directory: %s\n", ui->current_directory);
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
        rombp_log_err("Failed to alloc next childectory path: %s\n", child);
        return NULL;
    }
    strncpy(next_path, parent, parent_size);
    strncpy(next_path + parent_size, "/", 2);
    strncpy(next_path + parent_size + 1, child, strlen(child) + 1);
    return next_path;
}

static int validate_directory(char* dir) {
    struct stat dir_stat;

    int rc = stat(dir, &dir_stat);
    if (rc != 0) {
        rombp_log_err("Failed to stat directory: %s\n", dir);
        return -1;
    }
    if (!S_ISDIR(dir_stat.st_mode)) {
        rombp_log_err("Not a valid directory: %s\n", dir);
        return -1;
    }

    return 0;
}

static int ui_change_directory(rombp_ui* ui, char* dir) {
    size_t size = strlen(dir) + 1;

    char* next_directory = NULL;
    if (ui->current_directory == NULL) {
        next_directory = malloc(size);
        if (next_directory == NULL) {
            rombp_log_err("Failed to alloc directory path: %s\n", dir);
            return -1;
        }
        strncpy(next_directory, dir, size);
    } else {
        next_directory = concat_path(ui->current_directory, dir);
        if (next_directory == NULL) {
            rombp_log_err("Couldn't get next directory\n");
            return -1;
        }
        free(ui->current_directory);
    }

    if (validate_directory(next_directory)) {
        rombp_log_err("Trying to change to change to invalid directory: %s\n", next_directory);
        free(next_directory);
        return -1;
    }

    ui->current_directory = next_directory;

    return 0;
}

static int ui_setup_status_bar(rombp_ui* ui, rombp_ui_status_bar *status_bar) {
    int rc;

    rc = new_text_texture(ui, status_bar->text, status_bar->text_color, &status_bar->text_texture);
    if (rc != 0) {
        rombp_log_err("Failed to create bottom bar text: %d\n", rc);
        return rc;
    }
    status_bar->position.h = get_texture_height(status_bar->text_texture);

    return 0;
}

static void ui_status_bar_free(rombp_ui_status_bar* status_bar) {
    if (status_bar->text_texture != NULL) {
        SDL_DestroyTexture(status_bar->text_texture);
        status_bar->text_texture = NULL;
    }
}

int ui_status_bar_reset_text(rombp_ui* ui, rombp_ui_status_bar* status_bar, const char* text) {
    ui_status_bar_free(status_bar);
    status_bar->text = text;
    status_bar->text_len = strlen(text);
    return ui_setup_status_bar(ui, status_bar);
}

int ui_start(rombp_ui* ui) {
    rombp_log_info("Starting UI\n");

    ui->namelist = NULL;
    ui->namelist_text = NULL;
    ui->current_directory = NULL;

    ui->current_screen = SELECT_ROM;
    ui->selected_item = 0;
    ui->selected_offset = 0;
    ui->sdl.screen_width = SCREEN_WIDTH;
    ui->sdl.screen_height = SCREEN_HEIGHT;
    ui->sdl.scaling_factor = SCALING_FACTOR;
    
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        rombp_log_err("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }
    if (TTF_Init() < 0) {
        rombp_log_err("SDL could not initialize TTF! TTF_Error: %s\n", TTF_GetError());
        return -1;
    }
    ui->sdl.window = SDL_CreateWindow("rombp",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   ui->sdl.screen_width * ui->sdl.scaling_factor,
                                   ui->sdl.screen_height * ui->sdl.scaling_factor,
                                   WINDOW_SETTING);
    if (ui->sdl.window == NULL) {
        rombp_log_err("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    ui->sdl.renderer = SDL_CreateRenderer(ui->sdl.window, -1, SDL_RENDERER_ACCELERATED);
    if (ui->sdl.renderer == NULL) {
        rombp_log_err("Could not initialize renderer: SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    if (SDL_RenderSetScale(ui->sdl.renderer, ui->sdl.scaling_factor, ui->sdl.scaling_factor) < 0) {
        rombp_log_err("Could not set the renderer scaling: SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    ui->sdl.menu_font = TTF_OpenFont("assets/fonts/ProggySmall.ttf", MENU_FONT_SIZE);
    if (ui->sdl.menu_font == NULL) {
        rombp_log_err("Failed to load menu font: %s\n", TTF_GetError());
        return -1;
    }

    int rc = ui_change_directory(ui, STARTING_DIR);
    if (rc < 0) {
        rombp_log_info("Could not switch to starting dir, falling back to /\n");
        // Fallback to trying the root dir
        rc = ui_change_directory(ui, "/");
        if (rc < 0) {
            rombp_log_err("Failed to change directory: %d\n", rc);
            return -1;
        }
    }
    rc = ui_scan_directory(ui);
    if (rc < 0) {
        rombp_log_err("Failed to scan directory, error code: %d\n", rc);
        return -1;
    }

    ui->nav_bar.text = STATUS_BAR_TEXT_ROM;
    ui->nav_bar.text_len = strlen(STATUS_BAR_TEXT_ROM);
    ui->nav_bar.text_texture = NULL;
    ui->nav_bar.text_color = (SDL_Color){ 0xFF, 0xFF, 0xFF };
    ui->nav_bar.background_color = (SDL_Color){ 0x21, 0x2F, 0x3C };
    ui->nav_bar.position = (SDL_Rect){
        .x = 0,
        .y = 0,
        .h = MENU_FONT_SIZE,
        .w = ui->sdl.screen_width
    };

    rc = ui_setup_status_bar(ui, &ui->nav_bar);
    if (rc < 0) {
        rombp_log_err("Failed to setup bottom bar: %d\n", rc);
        return -1;
    }

    ui->bottom_bar.text = BOTTOM_BAR_TEXT;
    ui->bottom_bar.text_len = strlen(BOTTOM_BAR_TEXT);
    ui->bottom_bar.text_texture = NULL;
    ui->bottom_bar.text_color = (SDL_Color){ 0xFF, 0xFF, 0xFF };
    ui->bottom_bar.background_color = (SDL_Color){ 0x21, 0x2F, 0x3C };
    ui->bottom_bar.position = (SDL_Rect){
        .x = 0,
        .y = ui->sdl.screen_height - MENU_FONT_SIZE,
        .h = MENU_FONT_SIZE,
        .w = ui->sdl.screen_width
    };

    rc = ui_setup_status_bar(ui, &ui->bottom_bar);
    if (rc < 0) {
        rombp_log_err("Failed to setup bottom bar: %d\n", rc);
        return -1;
    }

    return 0;
}

void ui_stop(rombp_ui* ui) {
    ui_directory_free(ui);
    ui_status_bar_free(&ui->nav_bar);
    ui_status_bar_free(&ui->bottom_bar);
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
        int rc = ui_status_bar_reset_text(ui, &ui->nav_bar, STATUS_BAR_TEXT_ROM);
        if (rc != 0) {
            rombp_log_err("Failed to reset status bar text");
        }
        return EV_NONE;
    }

    return EV_NONE;
}

static int replace_extension(char* input, const char* new_ext, char** output) {
    size_t output_length = strlen(input) + strlen(new_ext) + 1;
    char* out = malloc(output_length);
    if (out == NULL) {
        rombp_log_err("Failed to alloc extension replaced output\n");
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
        rombp_log_info("Got directory selection\n");
        rc = ui_change_directory(ui, selected_item->d_name);
        if (rc != 0) {
            rombp_log_err("Failed to change directory: %s, rc: %d\n",selected_item->d_name, rc);
            return -1;
        }
        return ui_scan_directory(ui);
    } else if (selected_item->d_type == DT_REG) {
        rombp_log_info("Got file selection\n");
        if (command->input_file == NULL) {
            ui_status_bar_free(&ui->bottom_bar);
            command->input_file = concat_path(ui->current_directory, selected_item->d_name);
            ui->current_screen = SELECT_IPS;
            rc = ui_status_bar_reset_text(ui, &ui->nav_bar, STATUS_BAR_TEXT_IPS);
            if (rc != 0) {
                rombp_log_err("Failed to reset status bar text to IPS: %d\n", rc);
            }
        } else if (command->ips_file == NULL) {
            command->ips_file = concat_path(ui->current_directory, selected_item->d_name);
            char* copied_output = strdup(command->ips_file);
            if (copied_output == NULL) {
                rombp_log_err("Failed to copy output_path string\n");
                return -1;
            }
            char* replaced_extension;
            rc = replace_extension(copied_output, ".smc", &replaced_extension);
            if (rc != 0) {
                rombp_log_err("Failed to replace extension for file: %s\n", copied_output);
                free(copied_output);
                return rc;
            }

            command->output_file = replaced_extension;
            free(copied_output);
        }
    }

    return 0;
}

static void ui_handle_down(rombp_ui* ui, int amount) {
    int nitems = MIN(MENU_ITEM_COUNT, ui->namelist_size);
    // Don't allow any paging offset if the number of directory items fits on the screen at once. Otherwise,
    // make the last allowed offset the difference of the two.
    int max_offset = ui->namelist_size == nitems ? 0 : ui->namelist_size - nitems;

    if (ui->selected_item == (nitems - 1)) {
        if (ui->selected_offset <= max_offset) {
            ui->selected_offset = MIN(max_offset, ui->selected_offset + amount);
        }
    } else {
        ui->selected_item = MIN((nitems - 1), ui->selected_item + amount);
    }
}

static void ui_handle_up(rombp_ui* ui, int amount) {
    if (ui->selected_item == 0) {
        if (ui->selected_offset != 0) {
            ui->selected_offset = ui->selected_offset - MIN(ui->selected_offset, amount);
        }
    } else {
        ui->selected_item = ui->selected_item - MIN(ui->selected_item, amount);
    }
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
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                    case SDLK_q:
                        return EV_QUIT;
                    case SDLK_b:
                    case SDLK_LALT: // B button on RG350
                        return ui_handle_back(ui, command);
                    case SDLK_RETURN:
                    case SDLK_y:
                    case SDLK_LCTRL: // A button on RG350
                        rc = ui_handle_select(ui, command);
                        if (rc != 0) {
                            rombp_log_err("Failed to handle select event: %d\n", rc);
                            return EV_NONE;
                        }
                        if (command->input_file != NULL && command->ips_file != NULL) {
                            ui->current_screen = SELECT_ROM;
                            rc = ui_status_bar_reset_text(ui, &ui->nav_bar, STATUS_BAR_TEXT_ROM);
                            if (rc != 0) {
                                rombp_log_err("Failed to reset status bar text");
                            }
                            return EV_PATCH_COMMAND;
                        }
                        break;
                    case SDLK_RIGHT:
                        ui_handle_down(ui, 10);
                        break;
                    case SDLK_DOWN:
                        ui_handle_down(ui, 1);
                        break;
                    case SDLK_LEFT:
                        ui_handle_up(ui, 10);
                        break;
                    case SDLK_UP:
                        ui_handle_up(ui, 1);
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
                        rombp_log_info("Window size is: %dx%d\n", w, h);
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

static int draw_status_bar(rombp_ui* ui, rombp_ui_status_bar* status_bar) {
    int rc;

    SDL_SetRenderDrawColor(ui->sdl.renderer,
                           status_bar->background_color.r,
                           status_bar->background_color.g,
                           status_bar->background_color.b,
                           0xFF);
    SDL_RenderFillRect(ui->sdl.renderer, &status_bar->position);

    if (status_bar->text_texture == NULL) {
        return 0;
    }

    // Make a copy of position
    SDL_Rect text_rect = status_bar->position;
    text_rect.w = get_texture_width(status_bar->text_texture);

    rc = SDL_RenderCopy(ui->sdl.renderer, status_bar->text_texture, NULL, &text_rect);
    if (rc < 0) {
        rombp_log_err("Failed to render text surface: %s\n", SDL_GetError());
        return rc;
    }

    return 0;
}

static int draw_menu(rombp_ui* ui) {
    int rc;
    SDL_Rect menu_item_rect;

    int nitems = MIN(MENU_ITEM_COUNT, ui->namelist_size);
    for (int i = 0; i < nitems; i++) {
        size_t paging_offset = ui->selected_offset + i;

        menu_item_rect.x = menu_padding_left_right;
        menu_item_rect.y = (i * MENU_FONT_SIZE) + menu_padding_top_bottom;
        menu_item_rect.h = get_texture_height(ui->namelist_text[paging_offset]);

        if (i == ui->selected_item) {
            menu_item_rect.w = ui->sdl.screen_width - menu_padding_left_right;
            SDL_SetRenderDrawColor(ui->sdl.renderer, 0x5B, 0x2C, 0x6F, 0xFF);
            SDL_RenderFillRect(ui->sdl.renderer, &menu_item_rect);
        }

        menu_item_rect.w = get_texture_width(ui->namelist_text[paging_offset]);

        rc = SDL_RenderCopy(ui->sdl.renderer, ui->namelist_text[paging_offset], NULL, &menu_item_rect);
        if (rc < 0) {
            rombp_log_err("Failed to render text surface: %s\n", SDL_GetError());
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
        rombp_log_err("Failed to draw menu: %d\n", rc);
        return rc;
    }

    rc = draw_status_bar(ui, &ui->nav_bar);
    if (rc != 0) {
        rombp_log_err("Failed to draw nav bar: %d\n", rc);
        return rc;
    }

    rc = draw_status_bar(ui, &ui->bottom_bar);
    if (rc != 0) {
        rombp_log_err("Failed to draw header bar: %d\n", rc);
        return rc;
    }

    SDL_RenderPresent(ui->sdl.renderer);

    return 0;
}
