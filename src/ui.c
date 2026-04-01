#include "ui.h"
#include <ncurses.h>
#include <string.h>

void ui_draw_hud(const Game *g, int start_y) {
    /* sun counter */
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(start_y, 0, " SUN: %d ", g->sun);
    attroff(COLOR_PAIR(1) | A_BOLD);

    mvprintw(start_y, 16, "Wave: %d  Remaining: %d  Tick: %d",
             g->wave, g->zombies_remaining + g->board.zombie_count, g->tick);

    ui_draw_cards(g, start_y + 1);
}

void ui_draw_cards(const Game *g, int start_y) {
    int x = 0;
    for (int i = 1; i < PLANT_COUNT; i++) {
        const PlantDef *def = &PLANT_DEFS[i];
        int selected = (g->selected_plant == (PlantType)i);
        int affordable = (g->sun >= def->cost);
        int cooled = (g->card_cooldowns[i] <= 0);
        int usable = affordable && cooled;

        if (selected) attron(A_REVERSE);
        if (!usable) attron(A_DIM);

        mvprintw(start_y, x, " %d:%ls %3d ", i, def->emoji, def->cost);

        if (!usable) attroff(A_DIM);
        if (selected) attroff(A_REVERSE);

        x += 12;
    }

    /* shovel indicator */
    if (g->shovel_mode) {
        attron(A_REVERSE);
        mvprintw(start_y, x, " 0:🪏     ");
        attroff(A_REVERSE);
    } else {
        mvprintw(start_y, x, " 0:🪏     ");
    }

    /* help line */
    mvprintw(start_y + 1, 0, " [1-5]Select [0]Shovel [Enter]Place/Dig [P]Pause [Q]Quit ");
}

void ui_draw_endscreen(const Game *g) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int cy = rows / 2;
    int cx = cols / 2;

    if (g->state == STATE_WON) {
        attron(A_BOLD | COLOR_PAIR(2));
        mvprintw(cy - 1, cx - 8, "+================+");
        mvprintw(cy,     cx - 8, "|  LEVEL CLEAR!  |");
        mvprintw(cy + 1, cx - 8, "+================+");
        attroff(A_BOLD | COLOR_PAIR(2));
    } else {
        attron(A_BOLD | COLOR_PAIR(3));
        mvprintw(cy - 1, cx - 8, "+================+");
        mvprintw(cy,     cx - 8, "|   GAME OVER    |");
        mvprintw(cy + 1, cx - 8, "+================+");
        attroff(A_BOLD | COLOR_PAIR(3));
    }
    mvprintw(cy + 3, cx - 10, "Press [Q] to quit, [R] to restart");
}
