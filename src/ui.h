#ifndef UI_H
#define UI_H

#include "game.h"

/* draw the HUD: sun count, card bar, wave info */
void ui_draw_hud(const Game *g, int start_y);

/* draw the plant selection card bar */
void ui_draw_cards(const Game *g, int start_y);

/* draw game over / victory screen */
void ui_draw_endscreen(const Game *g);

#endif
