// The PSNokia menu: shown before the game starts and, during the game, when
// L3 and R3 are pressed together. It draws on the whole 1080p window and
// reads the controller itself, so the game is paused while it is open.
#ifndef PSN_MENU_H
#define PSN_MENU_H

#include "SDL.h"

// What the menu shows and changes, provided by the display and input code
typedef struct psn_menu_host {
    const char* title;          // the game's name
    const char* subtitle;       // e.g. "Nokia · 240x320"
    // The paused game's last frame (in-game menu), or NULL at start
    SDL_Surface* (*game_frame)(void);
    // Display shape (fit, 4:3, ...) and filter
    int (*view_count)(void);
    const char* (*view_name)(int view);
    int (*get_view)(void);
    void (*set_view)(int view);
    int (*get_smooth)(void);
    void (*set_smooth)(int smooth);
    // Controller buttons and the phone keys they send
    int (*button_count)(void);
    const char* (*button_name)(int button);
    int (*get_button_key)(int button);
    void (*set_button_key)(int button, int key);
    void (*reset_buttons)(void);
    void (*save_buttons)(void);
} psn_menu_host;

enum { PSN_MENU_RESUME, PSN_MENU_RESTART, PSN_MENU_CLOSE };

// Runs the menu until the player leaves it; returns what to do next.
// in_game: 0 before the game starts ("Start the game"), 1 during it
// ("Return to the game", and Restart is available).
int psn_menu_run(SDL_Window* window, int in_game, const psn_menu_host* host);

#endif
