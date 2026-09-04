#define _XOPEN_SOURCE_EXTENDED 1
#include "render.h"
#include "ui.h"
#include <locale.h>
#include <ncurses.h>
#include <string.h>
#include <wchar.h>

/* layout constants */
#define HUD_HEIGHT    3
#define CELL_WIDTH    4   /* emoji(2) + spacing(2) */
#define CELL_HEIGHT   2
#define GRID_LEFT     6   /* leave room for mower column */

static int grid_top(void) {
    return HUD_HEIGHT + 1;
}

void render_init(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    start_color();
    use_default_colors();

    /* color pairs */
    init_pair(1, COLOR_YELLOW, -1);   /* sun/sunflower */
    init_pair(2, COLOR_GREEN, -1);    /* plants */
    init_pair(3, COLOR_RED, -1);      /* zombies */
    init_pair(4, COLOR_CYAN, -1);     /* snow pea / projectiles */
    init_pair(5, COLOR_WHITE, -1);    /* grid */
    init_pair(6, COLOR_BLACK, COLOR_YELLOW); /* cursor highlight */

    /* flash effects */
    if (COLORS >= 256) {
        init_pair(7, -1, 240);    /* hit flash: gray bg */
        init_pair(8, -1, 255);    /* death flash: white bg */
    } else {
        init_pair(7, -1, COLOR_WHITE);    /* hit: white bg fallback */
        init_pair(8, COLOR_BLACK, COLOR_WHITE); /* death: bright white fallback */
    }
}

void render_cleanup(void) {
    endwin();
}

static void clear_grid_area(void) {
    int top = grid_top();
    int total_w = GRID_LEFT + BOARD_COLS * CELL_WIDTH + CELL_WIDTH;
    for (int r = 0; r < BOARD_ROWS; r++) {
        int y = top + r * CELL_HEIGHT;
        move(y, 0);
        for (int i = 0; i < total_w; i++) addch(' ');
        move(y + 1, 0);
        for (int i = 0; i < total_w; i++) addch(' ');
    }
}

static void draw_grid(const Board *b, int cursor_row, int cursor_col) {
    int top = grid_top();

    /* force-clear the grid area with single-width spaces to
       eliminate any wide-character ghosts from the previous frame */
    clear_grid_area();

    for (int r = 0; r < BOARD_ROWS; r++) {
        int y = top + r * CELL_HEIGHT;

        /* draw mower */
        if (b->mowers[r].active && !b->mowers[r].triggered) {
            mvprintw(y, 0, "🚜");
        } else {
            mvprintw(y, 0, "  ");
        }

        for (int c = 0; c < BOARD_COLS; c++) {
            int x = GRID_LEFT + c * CELL_WIDTH;
            const Plant *p = &b->cells[r][c];

            /* highlight cursor */
            if (r == cursor_row && c == cursor_col) {
                attron(COLOR_PAIR(6));
            }

            if (p->type != PLANT_NONE && p->hp > 0) {
                /* potato mine: show brown circle when unarmed */
                if (p->type == PLANT_POTATOMINE && p->explode_timer > 0) {
                    mvprintw(y, x, "%ls", L"🕹️");
                } else {
                    mvprintw(y, x, "%ls", PLANT_DEFS[p->type].emoji);
                }
            } else {
                /* empty cell: □ */
                mvprintw(y, x, "□");
            }

            if (r == cursor_row && c == cursor_col) {
                attroff(COLOR_PAIR(6));
            }
        }
    }
}

static int zombie_total_hp(const Zombie *z) {
    return z->hp + z->armor_hp;
}

static void draw_zombies(const Board *b) {
    int top = grid_top();

    /* build sorted index: ascending by total HP so highest HP draws last (on top) */
    int idx[MAX_ZOMBIES];
    int count = 0;
    for (int i = 0; i < b->zombie_count; i++) {
        if (b->zombies[i].alive) idx[count++] = i;
    }
    for (int i = 1; i < count; i++) {
        int key = idx[i];
        int j = i - 1;
        while (j >= 0 && zombie_total_hp(&b->zombies[idx[j]]) > zombie_total_hp(&b->zombies[key])) {
            idx[j + 1] = idx[j];
            j--;
        }
        idx[j + 1] = key;
    }

    for (int i = 0; i < count; i++) {
        const Zombie *z = &b->zombies[idx[i]];
        int y = top + z->row * CELL_HEIGHT;
        int x = GRID_LEFT + (int)(z->x * CELL_WIDTH);
        if (x >= GRID_LEFT && x < GRID_LEFT + BOARD_COLS * CELL_WIDTH + CELL_WIDTH) {
            attron(COLOR_PAIR(3));
            /* newspaper zombie shows 😡 when enraged (paper destroyed) */
            if (z->type == ZOMBIE_NEWSPAPER && z->armor_hp <= 0) {
                mvprintw(y, x, "%ls", L"😡");
            } else {
                mvprintw(y, x, "%ls", ZOMBIE_DEFS[z->type].emoji);
            }
            attroff(COLOR_PAIR(3));
        }
    }
}

static void draw_projectiles(const Board *b) {
    int top = grid_top();
    attron(COLOR_PAIR(4));
    for (int i = 0; i < b->projectile_count; i++) {
        const Projectile *pr = &b->projectiles[i];
        if (!pr->alive) continue;
        int y = top + pr->row * CELL_HEIGHT;
        int x = GRID_LEFT + (int)(pr->x * CELL_WIDTH);

        /* skip drawing if projectile overlaps a plant cell */
        int cell_col = (int)pr->x;
        if (cell_col >= 0 && cell_col < BOARD_COLS) {
            int cell_x = GRID_LEFT + cell_col * CELL_WIDTH;
            /* emoji occupies 2 columns at cell_x; skip if overlapping */
            if (x >= cell_x && x < cell_x + 2
                && b->cells[pr->row][cell_col].type != PLANT_NONE
                && b->cells[pr->row][cell_col].hp > 0) {
                continue;
            }
        }

        if (x >= GRID_LEFT) {
            mvprintw(y, x, "\xC2\xB7");  /* middle dot · U+00B7 */
        }
    }
    attroff(COLOR_PAIR(4));
}

/* draw VFX backgrounds — called before grid so content layers on top,
   but the background color bleeds through */
static void draw_vfx_bg(const Board *b) {
    int top = grid_top();
    for (int i = 0; i < b->vfx_count; i++) {
        const Vfx *v = &b->vfx[i];
        int x = GRID_LEFT + (int)(v->x * CELL_WIDTH);
        int y = top + v->row * CELL_HEIGHT;

        if (v->type == VFX_HIT) {
            attron(COLOR_PAIR(7));
            mvprintw(y, x, "  ");
            attroff(COLOR_PAIR(7));
        } else if (v->type == VFX_DEATH_SHOT) {
            attron(COLOR_PAIR(8));
            mvprintw(y, x, "  ");
            attroff(COLOR_PAIR(8));
        } else if (v->type == VFX_DEATH_BOOM) {
            /* white bg + 💥 */
            attron(COLOR_PAIR(8));
            mvprintw(y, x, "💥");
            attroff(COLOR_PAIR(8));
        }
    }
}

static void draw_menu(const Game *g) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int cy = rows / 2 - 4;
    int cx = cols / 2;

    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(cy,   cx - 10, "                        ");
    mvprintw(cy,   cx - 10, " _ __  _ ____   _____");
    mvprintw(cy+1, cx - 10, "| '_ \\| '_ \\ \\ / /_  /");
    mvprintw(cy+2, cx - 10, "| | | | |_) \\ V / / / ");
    mvprintw(cy+3, cx - 10, "|_| |_| .__/ \\_/ /___|");
    mvprintw(cy+4, cx - 10, "      |_|              ");
    attroff(A_BOLD | COLOR_PAIR(2));

    cy += 7;
    const char *options[] = { "Level Mode  (5 waves)", "Endless Mode" };
    for (int i = 0; i < 2; i++) {
        if (i == g->menu_selection) {
            attron(A_REVERSE | A_BOLD);
            mvprintw(cy + i * 2, cx - 12, "  > %s  ", options[i]);
            attroff(A_REVERSE | A_BOLD);
        } else {
            mvprintw(cy + i * 2, cx - 12, "    %s  ", options[i]);
        }
    }

    mvprintw(cy + 5, cx - 14, "[Up/Down] Select  [Enter] Start  [Q] Quit");
}

static void draw_card_select(const Game *g) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;
    int cx = cols / 2;
    int y = 2;

    attron(A_BOLD | COLOR_PAIR(2));
    mvprintw(y, cx - 12, "=== Choose Your Plants ===");
    attroff(A_BOLD | COLOR_PAIR(2));

    y += 2;
    mvprintw(y, cx - 16, "Slots: %d  [Left/Right to adjust 6-9]", g->max_slots);
    y += 1;
    mvprintw(y, cx - 16, "Deck:  %d / %d", g->deck_count, g->max_slots);
    y += 2;

    /* show selected deck */
    {
        int dx = cx - 16;
        mvprintw(y, dx, "Deck: ");
        dx += 6;
        for (int i = 0; i < g->deck_count; i++) {
            const PlantDef *def = &PLANT_DEFS[g->deck[i]];
            mvprintw(y, dx, "%d:%ls ", i + 1, def->emoji);
            dx += 5;
        }
        for (int i = g->deck_count; i < g->max_slots; i++) {
            mvprintw(y, dx, "[ ] ");
            dx += 4;
        }
    }
    y += 2;

    /* list all available plants */
    for (int i = 1; i < PLANT_COUNT; i++) {
        const PlantDef *def = &PLANT_DEFS[i];
        int in_deck = 0;
        for (int j = 0; j < g->deck_count; j++) {
            if (g->deck[j] == (PlantType)i) { in_deck = 1; break; }
        }

        int is_cursor = (g->card_cursor == i - 1);
        if (is_cursor) attron(A_REVERSE);
        if (in_deck) attron(A_BOLD | COLOR_PAIR(1));

        mvprintw(y, cx - 16, " %ls %-12s  %3d sun  %s ",
                 def->emoji, def->name, def->cost,
                 in_deck ? "[*]" : "[ ]");

        if (in_deck) attroff(A_BOLD | COLOR_PAIR(1));
        if (is_cursor) attroff(A_REVERSE);
        y++;
    }

    y += 1;
    mvprintw(y, cx - 18, "[Up/Down]Navigate [Enter]Toggle [G]Start [Q]Back");
}

void render_frame(const Game *g) {
    erase();

    if (g->state == STATE_MENU) {
        draw_menu(g);
        refresh();
        return;
    }

    if (g->state == STATE_CARD_SELECT) {
        draw_card_select(g);
        refresh();
        return;
    }

    ui_draw_hud(g, 0);
    draw_grid(&g->board, g->cursor_row, g->cursor_col);
    draw_vfx_bg(&g->board);
    draw_zombies(&g->board);
    draw_projectiles(&g->board);

    if (g->state == STATE_WON || g->state == STATE_LOST) {
        ui_draw_endscreen(g);
    }

    if (g->state == STATE_PAUSED) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        (void)rows;
        mvprintw(grid_top() + BOARD_ROWS, (cols - 10) / 2, "[ PAUSED ]");
    }

    refresh();
}
