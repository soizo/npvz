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
                /* draw plant emoji */
                mvprintw(y, x, "%ls", PLANT_DEFS[p->type].emoji);
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

static void draw_zombies(const Board *b) {
    int top = grid_top();
    for (int i = 0; i < b->zombie_count; i++) {
        const Zombie *z = &b->zombies[i];
        if (!z->alive) continue;
        int y = top + z->row * CELL_HEIGHT;
        int x = GRID_LEFT + (int)(z->x * CELL_WIDTH);
        if (x >= GRID_LEFT && x < GRID_LEFT + BOARD_COLS * CELL_WIDTH + CELL_WIDTH) {
            attron(COLOR_PAIR(3));
            mvprintw(y, x, "%ls", ZOMBIE_DEFS[z->type].emoji);
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

void render_frame(const Game *g) {
    erase();

    ui_draw_hud(g, 0);
    draw_grid(&g->board, g->cursor_row, g->cursor_col);
    draw_vfx_bg(&g->board);  /* bg + 💥 under everything */
    draw_zombies(&g->board);  /* zombies cover 💥 if overlapping */
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
