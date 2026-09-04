#define _XOPEN_SOURCE_EXTENDED 1
#include "ui.h"
#include <ctype.h>
#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define CARDS_PER_ROW 5
#define DECK_TAB_WIDTH 13
#define RULE_WIDTH 75
#define UI_SUN L"☀\uFE0E"

static EmojiWidths emoji_widths;

void ui_set_emoji_widths(const EmojiWidths *widths, int table_enabled) {
    emoji_widths = *widths;
    (void)table_enabled;
}

static void add_field_separator(void) {
    addch(' ');
    addch(ACS_VLINE);
    addch(' ');
}

static void add_context_separator(int field_attrs) {
    addch(' ');
    if (field_attrs != 0) attroff(field_attrs);
    attron(COLOR_PAIR(UI_PAIR_INFO));
    addch(ACS_VLINE);
    attroff(COLOR_PAIR(UI_PAIR_INFO));
    if (field_attrs != 0) attron(field_attrs);
    addch(' ');
}

static void draw_rule(int y) {
    attron(COLOR_PAIR(UI_PAIR_INFO));
    move(y, 0);
    hline(ACS_HLINE, RULE_WIDTH);
    attroff(COLOR_PAIR(UI_PAIR_INFO));
}

static int row_has_zombie(const Board *b, int row) {
    for (int i = 0; i < b->zombie_count; i++) {
        if (b->zombies[i].alive && b->zombies[i].row == row) return 1;
    }
    return 0;
}

static void uppercase_name(const char *name, char *out, int size) {
    int i = 0;
    while (name[i] != '\0' && i < size - 1) {
        out[i] = (char)toupper((unsigned char)name[i]);
        i++;
    }
    out[i] = '\0';
}

static void draw_cell_padding(int count) {
    while (count-- > 0) addch(' ' | A_BOLD);
}

static void draw_deck_card(const Game *g, int deck_index) {
    PlantType type = g->deck[deck_index];
    const PlantDef *def = &PLANT_DEFS[type];
    int selected = g->selected_plant == type;
    int cooling = g->card_cooldowns[type] > 0;
    int affordable = g->sun >= def->cost;
    int emoji_width = emoji_widths.plants[type];

    if (selected) attron(A_REVERSE | A_BOLD);
    printw("(%d %ls", deck_index + 1, def->emoji);
    draw_cell_padding(1);
    if (selected) {
        printw("%-5s", cooling ? "COOL" : affordable ? "READY" : "NEED");
    } else if (cooling || !affordable) {
        attron(A_DIM);
        printw("%-5s", cooling ? "COOL" : "NEED");
        attroff(A_DIM);
    } else {
        attron(A_BOLD | COLOR_PAIR(UI_PAIR_READY));
        printw("READY");
        attroff(A_BOLD | COLOR_PAIR(UI_PAIR_READY));
    }
    draw_cell_padding(DECK_TAB_WIDTH - (9 + emoji_width));
    if (selected) attroff(A_REVERSE | A_BOLD);
}

static void draw_shovel_cell(const Game *g) {
    if (g->shovel_mode) attron(A_REVERSE | A_BOLD);
    printw("(0 %ls SHOVEL", L"⛏\uFE0E");
    draw_cell_padding(DECK_TAB_WIDTH - 11);
    if (g->shovel_mode) attroff(A_REVERSE | A_BOLD);
}

static void draw_deck_rule(int y, int top) {
    draw_rule(y);
    if (!top) return;
    attron(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
    mvaddstr(y, 0, "DECK ONLINE");
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
}

static void draw_deck_row(const Game *g, int deck_row, int y) {
    int first = deck_row * CARDS_PER_ROW;
    int end = first + CARDS_PER_ROW;
    int slots = g->deck_count + 1;
    if (end > slots) end = slots;

    move(y, 0);
    for (int slot = first; slot < end; slot++) {
        if (slot < g->deck_count)
            draw_deck_card(g, slot);
        else
            draw_shovel_cell(g);
    }
}

static void draw_deck(const Game *g, int start_y) {
    draw_deck_rule(start_y + 1, 1);
    draw_deck_row(g, 0, start_y + 2);
    if (g->deck_count + 1 > CARDS_PER_ROW)
        draw_deck_row(g, 1, start_y + 3);
    draw_deck_rule(start_y + 4, 0);
}

void ui_draw_hud(const Game *g, int start_y) {
    int display_wave = g->mode == MODE_LEVEL && g->wave > 5 ? 5 : g->wave;

    attron(A_BOLD | COLOR_PAIR(UI_PAIR_SUN));
    mvprintw(start_y, 0, "%ls %d", UI_SUN, g->sun);
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_SUN));

    move(start_y, 9);
    addstr(g->mode == MODE_ENDLESS ? "ENDLESS" : "LEVEL");
    add_field_separator();
    if (g->mode == MODE_ENDLESS)
        printw("WAVE %d", g->wave);
    else
        printw("WAVE %d/5", display_wave);

    attron(A_BOLD | COLOR_PAIR(UI_PAIR_DANGER));
    mvprintw(start_y, 34, "THREAT//");
    for (int row = 0; row < BOARD_ROWS; row++)
        addwstr(row_has_zombie(&g->board, row) ? L"█" : L"░");
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_DANGER));

    draw_deck(g, start_y);
}

static void draw_context_table(const Game *g, int y) {
    char primary[48];
    int attrs = 0;
    int has_cost = 0;
    int cost = 0;

    if (g->shovel_mode) {
        snprintf(primary, sizeof(primary), "SHOVEL ACTIVE");
        attrs = A_BOLD | COLOR_PAIR(UI_PAIR_READY);
    } else if (g->selected_plant == PLANT_NONE) {
        snprintf(primary, sizeof(primary), "NO PLANT SELECTED");
    } else {
        PlantType type = g->selected_plant;
        const PlantDef *def = &PLANT_DEFS[type];
        char name[24];
        uppercase_name(def->name, name, sizeof(name));
        has_cost = 1;
        cost = def->cost;
        if (g->card_cooldowns[type] > 0) {
            snprintf(primary, sizeof(primary), "%s COOL", name);
            attrs = A_DIM;
        } else if (g->sun < def->cost) {
            snprintf(primary, sizeof(primary), "%s NEED %d SUN",
                     name, def->cost - g->sun);
            attrs = A_DIM;
        } else {
            snprintf(primary, sizeof(primary), "%s READY", name);
            attrs = A_BOLD | COLOR_PAIR(UI_PAIR_READY);
        }
    }

    int first_separator = (int)strlen(primary) + 1;
    int second_separator = -1;
    if (has_cost) {
        char digits[16];
        snprintf(digits, sizeof(digits), "%d", cost);
        int sun_width = wcswidth(UI_SUN, 8);
        if (sun_width < 1) sun_width = 1;
        int cost_width = sun_width + 1 + (int)strlen(digits);
        second_separator = first_separator + 2 + cost_width + 1;
    }

    draw_rule(y);
    attron(COLOR_PAIR(UI_PAIR_INFO));
    mvaddch(y, first_separator, ACS_TTEE);
    if (second_separator >= 0) mvaddch(y, second_separator, ACS_TTEE);
    attroff(COLOR_PAIR(UI_PAIR_INFO));

    if (attrs != 0) attron(attrs);
    mvaddstr(y + 1, 0, primary);
    add_context_separator(attrs);
    if (has_cost) {
        printw("%ls %d", UI_SUN, cost);
        add_context_separator(attrs);
    }
    printw("CELL %02d:%02d", g->cursor_row + 1, g->cursor_col + 1);
    if (attrs != 0) attroff(attrs);
}

void ui_draw_feedback(const Game *g, int y) {
    const char *message = NULL;
    const char *detail = NULL;
    int attrs = 0;

    switch (g->feedback) {
    case FEEDBACK_NONE: return;
    case FEEDBACK_NO_PLANT: message = "SELECT A PLANT FIRST"; attrs = UI_PAIR_DANGER; break;
    case FEEDBACK_COOLDOWN: message = "CARD IS COOLING DOWN"; attrs = UI_PAIR_DANGER; break;
    case FEEDBACK_OCCUPIED: message = "CELL OCCUPIED"; detail = "CHOOSE AN EMPTY CELL"; attrs = UI_PAIR_DANGER; break;
    case FEEDBACK_PLANTED: message = "PLANT DEPLOYED"; attrs = UI_PAIR_READY; break;
    case FEEDBACK_SHOVEL_ON: message = "SHOVEL ACTIVE"; attrs = UI_PAIR_READY; break;
    case FEEDBACK_SHOVEL_OFF: message = "SHOVEL STOWED"; attrs = UI_PAIR_INFO; break;
    case FEEDBACK_REMOVED: message = "PLANT REMOVED"; attrs = UI_PAIR_READY; break;
    case FEEDBACK_NOTHING_TO_REMOVE: message = "NO PLANT IN THIS CELL"; attrs = UI_PAIR_DANGER; break;
    case FEEDBACK_DECK_FULL: message = "DECK FULL"; detail = "REMOVE A PLANT FIRST"; attrs = UI_PAIR_DANGER; break;
    case FEEDBACK_EMPTY_DECK: message = "SELECT AT LEAST ONE PLANT"; attrs = UI_PAIR_DANGER; break;
    case FEEDBACK_NEED_SUN:
        if (g->selected_plant != PLANT_NONE) {
            int missing = PLANT_DEFS[g->selected_plant].cost - g->sun;
            attron(A_BOLD | COLOR_PAIR(UI_PAIR_DANGER));
            mvprintw(y, 0, "NEED %d MORE SUN", missing > 0 ? missing : 0);
            attroff(A_BOLD | COLOR_PAIR(UI_PAIR_DANGER));
        }
        return;
    }

    attron(A_BOLD | COLOR_PAIR(attrs));
    mvprintw(y, 0, "%s", message);
    if (detail != NULL) {
        add_field_separator();
        printw("%s", detail);
    }
    attroff(A_BOLD | COLOR_PAIR(attrs));
}

void ui_draw_game_footer(const Game *g, int start_y) {
    draw_context_table(g, start_y);
    ui_draw_feedback(g, start_y + 2);
    attron(COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(start_y + 3, 0,
             "MOVE//WASD  DEPLOY//ENTER  HELP//?  PAUSE//P  QUIT//Q");
    attroff(COLOR_PAIR(UI_PAIR_INFO));
}

static void draw_box(int top, int left, int height, int width) {
    for (int y = 0; y < height; y++) {
        move(top + y, left);
        for (int x = 0; x < width; x++) addch(' ');
    }

    mvaddch(top, left, '+');
    mvaddch(top, left + width - 1, '+');
    mvaddch(top + height - 1, left, '+');
    mvaddch(top + height - 1, left + width - 1, '+');
    for (int x = 1; x < width - 1; x++) {
        mvaddch(top, left + x, '-');
        mvaddch(top + height - 1, left + x, '-');
    }
    for (int y = 1; y < height - 1; y++) {
        mvaddch(top + y, left, '|');
        mvaddch(top + y, left + width - 1, '|');
    }
}

void ui_draw_help(const Game *g, int center_y) {
    (void)g;
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;
    int top = center_y - 5;
    int left = (cols - 44) / 2;

    draw_box(top, left, 11, 44);
    attron(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(top + 1, left + 15, "COMMAND HELP");
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(top + 2, left + 3, "MOVE        ARROWS / WASD / HJKL");
    mvprintw(top + 3, left + 3, "SELECT      1-9");
    mvprintw(top + 4, left + 3, "SHOVEL      0");
    mvprintw(top + 5, left + 3, "PLACE / DIG ENTER / SPACE");
    mvprintw(top + 6, left + 3, "PAUSE       P");
    mvprintw(top + 7, left + 3, "CLOSE HELP  ?");
    mvprintw(top + 8, left + 3, "QUIT        Q");
}

void ui_draw_pause(int center_y) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;
    int top = center_y - 2;
    int left = (cols - 34) / 2;

    draw_box(top, left, 5, 34);
    attron(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(top + 1, left + 14, "PAUSED");
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(top + 3, left + 4, "CONTINUE//P   QUIT//Q");
}

void ui_draw_endscreen(const Game *g, int center_y) {
    int rows, cols;
    int display_wave = g->mode == MODE_LEVEL && g->wave > 5 ? 5 : g->wave;
    getmaxyx(stdscr, rows, cols);
    (void)rows;
    int top = center_y - 3;
    int left = (cols - 46) / 2;
    int pair = g->state == STATE_WON ? UI_PAIR_READY : UI_PAIR_DANGER;

    draw_box(top, left, 7, 46);
    attron(A_BOLD | COLOR_PAIR(pair));
    mvprintw(top + 1, left + (g->state == STATE_WON ? 17 : 15),
             "%s", g->state == STATE_WON ? "LEVEL CLEAR" : "LAWN OVERRUN");
    attroff(A_BOLD | COLOR_PAIR(pair));
    mvprintw(top + 2, left + 19, "WAVE %d", display_wave);
    mvprintw(top + 4, left + 5, "RESELECT//R   MENU//M   QUIT//Q");
}
