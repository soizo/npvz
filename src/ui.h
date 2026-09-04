#ifndef UI_H
#define UI_H

#include "game.h"

typedef enum {
    UI_PAIR_SUN = 1,
    UI_PAIR_READY,
    UI_PAIR_DANGER,
    UI_PAIR_INFO,
    UI_PAIR_GRID,
    UI_PAIR_CURSOR,
    UI_PAIR_HIT,
    UI_PAIR_DEATH
} UiColorPair;

typedef struct {
    int plants[PLANT_COUNT];
    int zombies[ZOMBIE_TYPE_COUNT];
    int mower;
    int armed_mine;
    int angry_zombie;
    int explosion;
} EmojiWidths;

void ui_set_emoji_widths(const EmojiWidths *widths, int table_enabled);
void ui_draw_hud(const Game *g, int start_y);
void ui_draw_game_footer(const Game *g, int start_y);
void ui_draw_feedback(const Game *g, int y);
void ui_draw_help(const Game *g, int center_y);
void ui_draw_pause(int center_y);
void ui_draw_endscreen(const Game *g, int center_y);

#endif
