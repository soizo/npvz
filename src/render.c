#define _XOPEN_SOURCE_EXTENDED 1
#include "render.h"
#include "crowd.h"
#include "ui.h"
#include <errno.h>
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <wchar.h>

#define HUD_HEIGHT 5
#define CELL_WIDTH 4
#define CELL_HEIGHT 2
#define MOWER_LEFT 5
#define GRID_LEFT 9
#define GAME_MIN_ROWS 24
#define GAME_MIN_COLS 80
#define WIDTH_QUERY_TIMEOUT_US 50000
#define BOARD_WIDTH (GRID_LEFT + BOARD_COLS * CELL_WIDTH + CELL_WIDTH)

static const wchar_t *ARMED_MINE_EMOJI = L"🕹\uFE0F";
static const wchar_t *ANGRY_ZOMBIE_EMOJI = L"😡\uFE0F";
static const wchar_t *EXPLOSION_EMOJI = L"💥";
static EmojiWidths emoji_widths;

static int write_terminal(const char *bytes, size_t length) {
    while (length > 0) {
        ssize_t written = write(STDOUT_FILENO, bytes, length);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return 0;
        bytes += written;
        length -= (size_t)written;
    }
    return 1;
}

static int read_cursor_column(void) {
    char response[32];
    fd_set input;
    struct timeval timeout = { .tv_sec = 0, .tv_usec = WIDTH_QUERY_TIMEOUT_US };
    if (!write_terminal("\033[6n", 4)) return -1;

    FD_ZERO(&input);
    FD_SET(STDIN_FILENO, &input);
    if (select(STDIN_FILENO + 1, &input, NULL, NULL, &timeout) <= 0) return -1;
    ssize_t used = read(STDIN_FILENO, response, sizeof(response) - 1);
    if (used <= 0) return -1;
    response[used] = '\0';

    for (ssize_t i = 0; i < used; i++) {
        int row, col;
        if (response[i] == '\033'
            && sscanf(response + i, "\033[%d;%dR", &row, &col) == 2)
            return col;
    }
    return -1;
}

static int measure_terminal_width(const wchar_t *glyph) {
    char utf8[32];
    mbstate_t state = {0};
    const wchar_t *source = glyph;
    size_t length = wcsrtombs(utf8, &source, sizeof(utf8), &state);
    if (length == (size_t)-1 || source != NULL) return -1;
    if (!write_terminal("\033[H", 3)
        || !write_terminal(utf8, length)) return -1;

    int width = read_cursor_column() - 1;
    return width >= 1 && width <= 8 ? width : -1;
}

static int fallback_width(const wchar_t *glyph) {
    int width = wcswidth(glyph, 8);
    return width > 0 ? width : 2;
}

static void set_fallback_widths(EmojiWidths *widths) {
    for (int type = PLANT_SUNFLOWER; type < PLANT_COUNT; type++)
        widths->plants[type] = fallback_width(PLANT_DEFS[type].emoji);
    for (int type = ZOMBIE_NORMAL; type < ZOMBIE_TYPE_COUNT; type++)
        widths->zombies[type] = fallback_width(ZOMBIE_DEFS[type].emoji);
    widths->mower = fallback_width(MOWER_EMOJI);
    widths->armed_mine = fallback_width(ARMED_MINE_EMOJI);
    widths->angry_zombie = fallback_width(ANGRY_ZOMBIE_EMOJI);
    widths->explosion = fallback_width(EXPLOSION_EMOJI);
}

static void calibrate_emoji_widths(void) {
    EmojiWidths widths = {0};
    int table_enabled = 1;

    erase();
    refresh();
    for (int type = PLANT_SUNFLOWER; type < PLANT_COUNT && table_enabled; type++) {
        widths.plants[type] = measure_terminal_width(PLANT_DEFS[type].emoji);
        table_enabled = widths.plants[type] > 0;
    }
    for (int type = ZOMBIE_NORMAL; type < ZOMBIE_TYPE_COUNT && table_enabled; type++) {
        widths.zombies[type] = measure_terminal_width(ZOMBIE_DEFS[type].emoji);
        table_enabled = widths.zombies[type] > 0;
    }
    if (table_enabled) {
        widths.mower = measure_terminal_width(MOWER_EMOJI);
        widths.armed_mine = measure_terminal_width(ARMED_MINE_EMOJI);
        widths.angry_zombie = measure_terminal_width(ANGRY_ZOMBIE_EMOJI);
        widths.explosion = measure_terminal_width(EXPLOSION_EMOJI);
        table_enabled = widths.mower > 0 && widths.armed_mine > 0
                     && widths.angry_zombie > 0 && widths.explosion > 0;
    }
    if (!table_enabled) set_fallback_widths(&widths);

    emoji_widths = widths;
    ui_set_emoji_widths(&widths, table_enabled);
    clearok(stdscr, TRUE);
    erase();
    refresh();
}

static void add_field_separator(void) {
    addch(' ');
    addch(ACS_VLINE);
    addch(' ');
}

static int grid_top(void) {
    return HUD_HEIGHT;
}

static int grid_bottom(void) {
    return grid_top() + BOARD_ROWS * CELL_HEIGHT;
}

static void screen_min_size(GameState state, int *rows, int *cols) {
    if (state == STATE_MENU) {
        *rows = 18;
        *cols = 50;
    } else if (state == STATE_CARD_SELECT) {
        *rows = 22;
        *cols = 80;
    } else {
        *rows = GAME_MIN_ROWS;
        *cols = GAME_MIN_COLS;
    }
}

static void draw_size_gate(int rows, int cols, int min_rows, int min_cols) {
    int y = rows / 2;
    int x = cols > 34 ? (cols - 34) / 2 : 0;

    attron(A_BOLD | COLOR_PAIR(UI_PAIR_DANGER));
    mvprintw(y - 1, x, "NPVZ NEEDS %dx%d", min_cols, min_rows);
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_DANGER));
    mvprintw(y + 1, x, "CURRENT %dx%d", cols, rows);
    add_field_separator();
    printw("RESIZE TO CONTINUE");
}

void render_init(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(UI_PAIR_SUN, COLOR_YELLOW, -1);
        init_pair(UI_PAIR_READY, COLOR_GREEN, -1);
        init_pair(UI_PAIR_DANGER, COLOR_MAGENTA, -1);
        init_pair(UI_PAIR_INFO, COLOR_CYAN, -1);
        init_pair(UI_PAIR_GRID, COLOR_WHITE, -1);
        init_pair(UI_PAIR_CURSOR, COLOR_BLACK, COLOR_GREEN);
        init_pair(UI_PAIR_DAMAGE, COLOR_BLACK, COLOR_MAGENTA);
        init_pair(UI_PAIR_DEATH_FLASH, COLOR_BLACK, COLOR_WHITE);
        init_pair(UI_PAIR_CROWD, COLOR_BLACK,
                  COLORS >= 256 ? 240 : COLOR_WHITE);
    }

    calibrate_emoji_widths();
    nodelay(stdscr, TRUE);
}

void render_cleanup(void) {
    endwin();
}

typedef struct {
    int before[BOARD_ROWS][BOARD_WIDTH + 1];
} BoardLayout;

static int glyph_delta(const wchar_t *glyph, int terminal_width) {
    return fallback_width(glyph) - terminal_width;
}

static int plant_delta(PlantType type) {
    return glyph_delta(PLANT_DEFS[type].emoji, emoji_widths.plants[type]);
}

static const wchar_t *plant_glyph(const Plant *plant, int *terminal_width) {
    if (plant->type == PLANT_POTATOMINE && plant->explode_timer > 0) {
        *terminal_width = emoji_widths.armed_mine;
        return ARMED_MINE_EMOJI;
    }
    *terminal_width = emoji_widths.plants[plant->type];
    return PLANT_DEFS[plant->type].emoji;
}

static const wchar_t *zombie_glyph(const Zombie *zombie, int *terminal_width) {
    if (zombie->type == ZOMBIE_NEWSPAPER && zombie->armor_hp <= 0) {
        *terminal_width = emoji_widths.angry_zombie;
        return ANGRY_ZOMBIE_EMOJI;
    }
    *terminal_width = emoji_widths.zombies[zombie->type];
    return ZOMBIE_DEFS[zombie->type].emoji;
}

static int projectile_visible(const Board *b, const Projectile *projectile) {
    int x = GRID_LEFT + (int)(projectile->x * CELL_WIDTH);
    int cell_col = (int)projectile->x;
    if (cell_col >= 0 && cell_col < BOARD_COLS) {
        int cell_x = GRID_LEFT + cell_col * CELL_WIDTH;
        if (x >= cell_x && x < cell_x + 2
            && b->cells[projectile->row][cell_col].type != PLANT_NONE
            && b->cells[projectile->row][cell_col].hp > 0)
            return 0;
    }
    return x >= GRID_LEFT;
}

static void build_board_layout(const Board *b, const ZombieCrowd *crowds,
                               int crowd_count, BoardLayout *layout) {
    int delta_at[BOARD_ROWS][BOARD_WIDTH] = {{0}};

    for (int row = 0; row < BOARD_ROWS; row++) {
        if (b->mowers[row].active && !b->mowers[row].triggered)
            delta_at[row][MOWER_LEFT] = glyph_delta(MOWER_EMOJI, emoji_widths.mower);
        for (int col = 0; col < BOARD_COLS; col++) {
            const Plant *plant = &b->cells[row][col];
            if (plant->type == PLANT_NONE || plant->hp <= 0) continue;
            int terminal_width;
            const wchar_t *glyph = plant_glyph(plant, &terminal_width);
            delta_at[row][GRID_LEFT + col * CELL_WIDTH] = glyph_delta(glyph, terminal_width);
        }
    }
    for (int i = 0; i < crowd_count; i++) {
        const Zombie *zombie = &b->zombies[crowds[i].representative];
        int x = GRID_LEFT + (int)(zombie->x * CELL_WIDTH);
        if (x < GRID_LEFT || x >= BOARD_WIDTH) continue;
        int terminal_width;
        const wchar_t *glyph = zombie_glyph(zombie, &terminal_width);
        delta_at[zombie->row][x] = glyph_delta(glyph, terminal_width);
    }
    for (int i = 0; i < b->effect_count; i++) {
        const CombatEffect *effect = &b->effects[i];
        int x = GRID_LEFT + (int)(effect->x * CELL_WIDTH);
        if (effect->kind == COMBAT_EFFECT_BLAST && x >= 0 && x < BOARD_WIDTH)
            delta_at[effect->row][x] = glyph_delta(EXPLOSION_EMOJI,
                                                   emoji_widths.explosion);
    }

    for (int row = 0; row < BOARD_ROWS; row++) {
        layout->before[row][0] = 0;
        for (int x = 0; x < BOARD_WIDTH; x++)
            layout->before[row][x + 1] = layout->before[row][x] + delta_at[row][x];
    }
}

static int board_x(const BoardLayout *layout, int row, int physical_x) {
    if (physical_x < 0) return physical_x;
    if (physical_x > BOARD_WIDTH) physical_x = BOARD_WIDTH;
    return physical_x + layout->before[row][physical_x];
}

static void clear_grid_area(void) {
    for (int row = 0; row < BOARD_ROWS * CELL_HEIGHT; row++) {
        move(grid_top() + row, 0);
        clrtoeol();
    }
}

static void draw_grid(const Board *b, const BoardLayout *layout,
                      int cursor_row, int cursor_col) {
    clear_grid_area();

    for (int row = 0; row < BOARD_ROWS; row++) {
        int y = grid_top() + row * CELL_HEIGHT;
        mvprintw(y, 0, "%02d ▸", row + 1);

        if (b->mowers[row].active && !b->mowers[row].triggered)
            mvprintw(y, board_x(layout, row, MOWER_LEFT), "%ls", MOWER_EMOJI);

        for (int col = 0; col < BOARD_COLS; col++) {
            int x = board_x(layout, row, GRID_LEFT + col * CELL_WIDTH);
            const Plant *plant = &b->cells[row][col];
            int selected = row == cursor_row && col == cursor_col;

            if (selected) attron(COLOR_PAIR(UI_PAIR_CURSOR) | A_BOLD);
            if (plant->type != PLANT_NONE && plant->hp > 0) {
                int terminal_width;
                const wchar_t *glyph = plant_glyph(plant, &terminal_width);
                (void)terminal_width;
                mvprintw(y, x, "%ls", glyph);
            } else {
                mvprintw(y, x, "%ls", selected ? L"◆" : L"·");
            }
            if (selected) attroff(COLOR_PAIR(UI_PAIR_CURSOR) | A_BOLD);
        }
    }
}

static void draw_crowds(const Board *b, const ZombieCrowd *crowds,
                        int crowd_count, const BoardLayout *layout) {
    for (int i = 0; i < crowd_count; i++) {
        const ZombieCrowd *crowd = &crowds[i];
        const Zombie *zombie = &b->zombies[crowd->representative];
        int physical_x = GRID_LEFT + (int)(zombie->x * CELL_WIDTH);
        if (physical_x < GRID_LEFT || physical_x >= BOARD_WIDTH) continue;

        int pair = crowd->hit ? UI_PAIR_DAMAGE
                 : crowd->count > 1 ? UI_PAIR_CROWD : UI_PAIR_DANGER;
        int y = grid_top() + zombie->row * CELL_HEIGHT;
        int x = board_x(layout, zombie->row, physical_x);
        int terminal_width;
        const wchar_t *glyph = zombie_glyph(zombie, &terminal_width);
        (void)terminal_width;

        attron(A_BOLD | COLOR_PAIR(pair));
        mvprintw(y, x, "%ls", glyph);
        attroff(A_BOLD | COLOR_PAIR(pair));
        if (crowd->count > 1) {
            attron(A_DIM);
            mvaddwstr(y + 1, x, L"×");
            printw("%d", crowd->count);
            attroff(A_DIM);
        }
    }
}

static void draw_projectiles(const Board *b, const BoardLayout *layout) {
    attron(COLOR_PAIR(UI_PAIR_INFO));
    for (int i = 0; i < b->projectile_count; i++) {
        const Projectile *projectile = &b->projectiles[i];
        if (!projectile->alive) continue;

        int y = grid_top() + projectile->row * CELL_HEIGHT;
        int physical_x = GRID_LEFT + (int)(projectile->x * CELL_WIDTH);
        if (projectile_visible(b, projectile))
            mvprintw(y, board_x(layout, projectile->row, physical_x), "·");
    }
    attroff(COLOR_PAIR(UI_PAIR_INFO));
}

static int effect_zombie_width(const CombatEffect *effect) {
    int width = effect->angry ? emoji_widths.angry_zombie
              : emoji_widths.zombies[effect->zombie_type];
    return width > 0 ? width : 1;
}

static void draw_death_effects(const Board *b, const BoardLayout *layout) {
    attron(COLOR_PAIR(UI_PAIR_DEATH_FLASH));
    for (int i = 0; i < b->effect_count; i++) {
        const CombatEffect *effect = &b->effects[i];
        if (effect->kind != COMBAT_EFFECT_DEATH) continue;
        int physical_x = GRID_LEFT + (int)(effect->x * CELL_WIDTH);
        if (physical_x < GRID_LEFT || physical_x >= BOARD_WIDTH) continue;
        int x = board_x(layout, effect->row, physical_x);
        int y = grid_top() + effect->row * CELL_HEIGHT;
        move(y, x);
        for (int col = 0; col < effect_zombie_width(effect); col++) addch(' ');
    }
    attroff(COLOR_PAIR(UI_PAIR_DEATH_FLASH));
}

static void draw_blast_effects(const Board *b, const BoardLayout *layout) {
    attron(COLOR_PAIR(UI_PAIR_DEATH_FLASH));
    for (int i = 0; i < b->effect_count; i++) {
        const CombatEffect *effect = &b->effects[i];
        if (effect->kind != COMBAT_EFFECT_BLAST) continue;
        int physical_x = GRID_LEFT + (int)(effect->x * CELL_WIDTH);
        if (physical_x < GRID_LEFT || physical_x >= BOARD_WIDTH) continue;
        int x = board_x(layout, effect->row, physical_x);
        int y = grid_top() + effect->row * CELL_HEIGHT;
        mvprintw(y, x, "%ls", EXPLOSION_EMOJI);
    }
    attroff(COLOR_PAIR(UI_PAIR_DEATH_FLASH));
}

static void draw_menu(const Game *g) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int top = (rows - 14) / 2;
    int left = cols / 2 - 12;

    attron(A_BOLD | COLOR_PAIR(UI_PAIR_READY));
    mvprintw(top, left,     " _ __  _ ____   _____");
    mvprintw(top + 1, left, "| '_ \\| '_ \\ \\ / /_  /");
    mvprintw(top + 2, left, "| | | | |_) \\ V / / / ");
    mvprintw(top + 3, left, "|_| |_| .__/ \\_/ /___|");
    mvprintw(top + 4, left, "      |_|              ");
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_READY));

    static const char *labels[] = { "LEVEL//5 WAVES", "ENDLESS//NO LIMIT" };
    static const char *descriptions[] = {
        "Clear five waves to win.",
        "Survive accelerating waves."
    };

    for (int i = 0; i < 2; i++) {
        int y = top + 7 + i * 3;
        if (i == g->menu_selection) attron(A_REVERSE | A_BOLD);
        mvprintw(y, cols / 2 - 12, "%c %-20s",
                 i == g->menu_selection ? '>' : ' ', labels[i]);
        if (i == g->menu_selection) attroff(A_REVERSE | A_BOLD);
        attron(A_DIM);
        mvprintw(y + 1, cols / 2 - 12, "  %s", descriptions[i]);
        attroff(A_DIM);
    }

    attron(COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(top + 13, cols / 2 - 21,
             "MOVE//UP DOWN  START//ENTER  QUIT//Q");
    attroff(COLOR_PAIR(UI_PAIR_INFO));
}

static int deck_contains(const Game *g, PlantType type) {
    for (int i = 0; i < g->deck_count; i++) {
        if (g->deck[i] == type) return 1;
    }
    return 0;
}

static void draw_card_select(const Game *g) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;

    attron(A_BOLD | COLOR_PAIR(UI_PAIR_READY));
    mvprintw(1, 2, "CHOOSE YOUR PLANTS");
    attroff(A_BOLD | COLOR_PAIR(UI_PAIR_READY));
    mvprintw(2, 2, "DECK//%d OF %d", g->deck_count, g->max_slots);

    int x = 2;
    int deck_delta = 0;
    for (int i = 0; i < g->max_slots; i++) {
        int draw_x = x + deck_delta;
        if (i < g->deck_count) {
            PlantType type = g->deck[i];
            mvprintw(3, draw_x, "%d:%ls", i + 1, PLANT_DEFS[type].emoji);
            deck_delta += plant_delta(type);
        } else {
            mvprintw(3, draw_x, "[ ]");
        }
        x += 5;
    }

    attron(COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(4, 2, "PLANT LIST");
    mvprintw(4, 40, "PLANT DATA");
    attroff(COLOR_PAIR(UI_PAIR_INFO));

    for (int i = 1; i < PLANT_COUNT; i++) {
        const PlantDef *def = &PLANT_DEFS[i];
        int y = 5 + i - 1;
        int selected = g->card_cursor == i - 1;
        int in_deck = deck_contains(g, (PlantType)i);

        if (selected) attron(A_REVERSE);
        if (in_deck) attron(A_BOLD | COLOR_PAIR(UI_PAIR_READY));
        int curses_width = fallback_width(def->emoji);
        int name_x = 5 + plant_delta((PlantType)i);
        mvprintw(y, 2, "%ls", def->emoji);
        for (int cell = 2 + curses_width; cell < name_x; cell++)
            mvaddch(y, cell, ' ' | A_BOLD);
        mvprintw(y, name_x, "%-12s %3d  %-9s",
                 def->name, def->cost, in_deck ? "IN DECK" : "AVAILABLE");
        if (in_deck) attroff(A_BOLD | COLOR_PAIR(UI_PAIR_READY));
        if (selected) attroff(A_REVERSE);
    }

    const PlantDef *focused = &PLANT_DEFS[g->card_cursor + 1];
    mvprintw(6, 40, "PLANT//%s", focused->name);
    mvprintw(8, 40, "COST   %d SUN", focused->cost);
    mvprintw(9, 40, "HEALTH %d", focused->hp);
    if (focused->shoot_interval > 0)
        mvprintw(10, 40, "ATTACK %d TICKS", focused->shoot_interval);
    else
        mvprintw(10, 40, "ATTACK -");
    if (focused->sun_interval > 0)
        mvprintw(11, 40, "SUN    %d TICKS", focused->sun_interval);
    else
        mvprintw(11, 40, "SUN    -");

    ui_draw_feedback(g, 17);
    if (g->deck_count == 0) {
        attron(A_DIM);
        mvprintw(18, 2, "START//G");
        add_field_separator();
        printw("SELECT AT LEAST ONE PLANT");
        attroff(A_DIM);
    }
    attron(COLOR_PAIR(UI_PAIR_INFO));
    mvprintw(20, (cols - 68) / 2,
             "MOVE//UP DOWN  TOGGLE//ENTER  SLOTS//LEFT RIGHT  START//G  BACK//Q");
    attroff(COLOR_PAIR(UI_PAIR_INFO));
}

void render_frame(const Game *g) {
    int rows, cols, min_rows, min_cols;
    erase();
    getmaxyx(stdscr, rows, cols);
    screen_min_size(g->state, &min_rows, &min_cols);
    if (rows < min_rows || cols < min_cols) {
        draw_size_gate(rows, cols, min_rows, min_cols);
        refresh();
        return;
    }

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

    ZombieCrowd crowds[MAX_ZOMBIES];
    int crowd_count = crowd_build(g->board.zombies, g->board.zombie_count,
                                  emoji_widths.zombies, emoji_widths.angry_zombie,
                                  CELL_WIDTH, crowds);
    BoardLayout layout;
    build_board_layout(&g->board, crowds, crowd_count, &layout);
    ui_draw_hud(g, 0);
    draw_grid(&g->board, &layout, g->cursor_row, g->cursor_col);
    draw_death_effects(&g->board, &layout);
    draw_crowds(&g->board, crowds, crowd_count, &layout);
    draw_projectiles(&g->board, &layout);
    draw_blast_effects(&g->board, &layout);
    ui_draw_game_footer(g, grid_bottom());

    int overlay_center = (grid_top() + grid_bottom() - 1) / 2;
    if (g->state == STATE_WON || g->state == STATE_LOST)
        ui_draw_endscreen(g, overlay_center);
    else if (g->help_visible)
        ui_draw_help(g, overlay_center);
    else if (g->state == STATE_PAUSED)
        ui_draw_pause(overlay_center);

    refresh();
}
