#define _XOPEN_SOURCE_EXTENDED 1
#include "ui.h"
#include <ctype.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define CARDS_PER_ROW 5
#define DECK_TAB_WIDTH 13
#define RULE_WIDTH 75
#define UI_SUN L"☀\uFE0E"
#define PAUSE_ART_ROWS 11
#define PAUSE_ART_COLS 32
#define PAUSE_ART_WIDTH 21

#ifndef NPVZ_DATA_DIR
#define NPVZ_DATA_DIR "/usr/local/share/npvz"
#endif

static EmojiWidths emoji_widths;
static int ui_origin_x;
static wchar_t pause_art[PAUSE_ART_ROWS][PAUSE_ART_COLS];
static int pause_art_loaded;
static int pause_art_rows;
static int pause_art_width;

static int load_pause_art(void) {
    if (pause_art_loaded) return pause_art_rows;
    pause_art_loaded = 1;

    const char *paths[] = {
        "asciiart/newspaper-zombie.txt",
        NPVZ_DATA_DIR "/asciiart/newspaper-zombie.txt"
    };
    FILE *file = NULL;
    for (int i = 0; i < 2 && file == NULL; i++) file = fopen(paths[i], "r");
    if (file == NULL) return 0;

    char line[PAUSE_ART_COLS];
    while (pause_art_rows < PAUSE_ART_ROWS && fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *start = line;
        if (*start == ' ' || *start == '\t') start++;
        char *end = start + strlen(start);
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';
        if (*start == '\0' || strncmp(start, "auth:", 5) == 0) continue;

        size_t length = mbstowcs(pause_art[pause_art_rows], start,
                                 PAUSE_ART_COLS - 1);
        if (length == (size_t)-1) continue;
        pause_art[pause_art_rows][length] = L'\0';
        int display_width = wcswidth(pause_art[pause_art_rows], PAUSE_ART_COLS);
        if (display_width < 0) display_width = (int)length;
        if (display_width > pause_art_width) pause_art_width = display_width;
        pause_art_rows++;
    }
    fclose(file);
    return pause_art_rows;
}

void ui_set_emoji_widths(const EmojiWidths *widths, int table_enabled) {
    emoji_widths = *widths;
    (void)table_enabled;
}

void ui_set_origin_x(int x) {
    ui_origin_x = x;
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
    move(y, ui_origin_x);
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
    mvaddstr(y, ui_origin_x, "DECK ONLINE");
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
}

static void draw_deck_row(const Game *g, int deck_row, int y) {
    int first = deck_row * CARDS_PER_ROW;
    int end = first + CARDS_PER_ROW;
    int slots = g->deck_count + 1;
    if (end > slots) end = slots;

    move(y, ui_origin_x);
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
    /* Width compensation needs both rows repainted from column zero. */
    wredrawln(stdscr, start_y + 2, 2);
    draw_deck_rule(start_y + 4, 0);
}

void ui_draw_hud(const Game *g, int start_y) {
    int display_wave = g->mode == MODE_LEVEL && g->wave > 5 ? 5 : g->wave;

    attron(A_BOLD | COLOR_PAIR(UI_PAIR_SUN));
    mvprintw(start_y, ui_origin_x, "%ls %d", UI_SUN, g->sun);
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_SUN));

    move(start_y, ui_origin_x + 9);
    addstr(g->mode == MODE_ENDLESS ? "ENDLESS" : "LEVEL");
    add_field_separator();
    if (g->mode == MODE_ENDLESS)
        printw("WAVE %d", g->wave);
    else
        printw("WAVE %d/5", display_wave);

    attron(A_BOLD | COLOR_PAIR(UI_PAIR_DANGER));
    mvprintw(start_y, ui_origin_x + 34, "THREAT//");
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
    mvaddch(y, ui_origin_x + first_separator, ACS_TTEE);
    if (second_separator >= 0)
        mvaddch(y, ui_origin_x + second_separator, ACS_TTEE);
    attroff(COLOR_PAIR(UI_PAIR_INFO));

    if (attrs != 0) attron(attrs);
    mvaddstr(y + 1, ui_origin_x, primary);
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
            mvprintw(y, ui_origin_x, "NEED %d MORE SUN",
                     missing > 0 ? missing : 0);
            attroff(A_BOLD | COLOR_PAIR(UI_PAIR_DANGER));
        }
        return;
    }

    attron(A_BOLD | COLOR_PAIR(attrs));
    mvprintw(y, ui_origin_x, "%s", message);
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
    mvprintw(start_y + 3, ui_origin_x,
             "MOVE//HJKL TAB  SELECT//1-9 QWERT  DEPLOY//ENTER  PAUSE//P ESC");
    attroff(COLOR_PAIR(UI_PAIR_INFO));
}

static void draw_box(int top, int left, int height, int width) {
    for (int y = -1; y <= height; y++)
        mvhline(top + y, left - 2, ' ', width + 4);

    mvaddch(top, left, ACS_ULCORNER);
    mvaddch(top, left + width - 1, ACS_URCORNER);
    mvaddch(top + height - 1, left, ACS_LLCORNER);
    mvaddch(top + height - 1, left + width - 1, ACS_LRCORNER);
    for (int x = 1; x < width - 1; x++) {
        mvaddch(top, left + x, ACS_HLINE);
        mvaddch(top + height - 1, left + x, ACS_HLINE);
    }
    for (int y = 1; y < height - 1; y++) {
        mvaddch(top + y, left, ACS_VLINE);
        mvaddch(top + y, left + width - 1, ACS_VLINE);
    }
}

void ui_draw_help(const Game *g, int center_y) {
    (void)g;
    int top = center_y - 6;
    int left = ui_origin_x + (UI_CANVAS_WIDTH - 50) / 2;

    draw_box(top, left, 12, 50);
    attron(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(top + 1, left + 18, "COMMAND HELP");
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(top + 2, left + 3, "MOVE        ARROWS / HJKL (WRAPS)");
    mvprintw(top + 3, left + 3, "NEXT ROW    TAB (WRAPS)");
    mvprintw(top + 4, left + 3, "SELECT      1-9");
    mvprintw(top + 5, left + 3, "ALT KEYS    Q W E R T = 6 7 8 9 0");
    mvprintw(top + 6, left + 3, "SHOVEL      0 / T");
    mvprintw(top + 7, left + 3, "PLACE / DIG ENTER / SPACE");
    mvprintw(top + 8, left + 3, "PAUSE       P / ESC");
    mvprintw(top + 9, left + 3, "BACK        P / ESC");
}

void ui_draw_pause(const Game *g, int center_y) {
    int art_rows = load_pause_art();
    int height = art_rows > 0 ? 13 : 9;
    int width = art_rows > 0 ? 62 : 40;
    int top = center_y - height / 2;
    int left = ui_origin_x + (UI_CANVAS_WIDTH - width) / 2;
    int title_x = art_rows > 0 ? left + 40 : left + 17;
    int item_x = art_rows > 0 ? left + 34 : left + 14;
    int footer_x = art_rows > 0 ? left + 27 : left + 4;
    static const char *items[] = { "RESUME", "HELP", "MENU" };

    draw_box(top, left, height, width);
    if (art_rows > 0) {
        int art_x = left + 2 + (PAUSE_ART_WIDTH - pause_art_width) / 2;
        for (int i = 0; i < art_rows; i++)
            mvaddwstr(top + 1 + i, art_x, pause_art[i]);
        for (int y = 1; y < height - 1; y++) mvaddch(top + y, left + 24, '|');
    }
    attron(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(top + 2, title_x, "PAUSED");
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_INFO));
    for (int i = 0; i < 3; i++) {
        if (g->menu_selection == i) attron(A_REVERSE | A_BOLD);
        mvprintw(top + 4 + i, item_x, "%c %-10s",
                 g->menu_selection == i ? '>' : ' ', items[i]);
        if (g->menu_selection == i) attroff(A_REVERSE | A_BOLD);
    }
    attron(COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(top + height - 2, footer_x,
             "SELECT//J K ENTER  RESUME//P ESC");
    attroff(COLOR_PAIR(UI_PAIR_INFO));
}

void ui_draw_endscreen(const Game *g, int center_y) {
    int display_wave = g->mode == MODE_LEVEL && g->wave > 5 ? 5 : g->wave;
    int top = center_y - 4;
    int left = ui_origin_x + (UI_CANVAS_WIDTH - 40) / 2;
    int pair = g->state == STATE_WON ? UI_PAIR_READY : UI_PAIR_DANGER;
    static const char *items[] = { "RESELECT", "MENU" };

    draw_box(top, left, 8, 40);
    attron(A_BOLD | COLOR_PAIR(pair));
    mvprintw(top + 1, left + (g->state == STATE_WON ? 14 : 12),
             "%s", g->state == STATE_WON ? "LEVEL CLEAR" : "LAWN OVERRUN");
    attroff(A_BOLD | COLOR_PAIR(pair));
    mvprintw(top + 2, left + 16, "WAVE %d", display_wave);
    for (int i = 0; i < 2; i++) {
        if (g->menu_selection == i) attron(A_REVERSE | A_BOLD);
        mvprintw(top + 4 + i, left + 12, "%c %-10s",
                 g->menu_selection == i ? '>' : ' ', items[i]);
        if (g->menu_selection == i) attroff(A_REVERSE | A_BOLD);
    }
    attron(COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(top + 6, left + 8, "MOVE//J K  CHOOSE//ENTER");
    attroff(COLOR_PAIR(UI_PAIR_INFO));
}
