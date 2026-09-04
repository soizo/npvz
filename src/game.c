#include "game.h"
#include "sound.h"
#include <ncurses.h>
#include <stdlib.h>
#include <time.h>

#define SUN_DROP_INTERVAL 200
#define INITIAL_SUN       50

static void spawn_random_zombie(Game *g) {
    if (g->zombies_remaining <= 0) return;

    int row = rand() % BOARD_ROWS;
    ZombieType type = ZOMBIE_NORMAL;

    /* zombie spawning modelled after PvZ1 progression:
       wave 1: normal only
       wave 2: +conehead, +pole vaulter
       wave 3: +newspaper, +buckethead
       wave 4: +screen door, +dancer
       wave 5: +football (elite)
       weights inspired by official point/weight system */
    int roll = rand() % 100;
    if (g->wave >= 5 && roll < 5) {
        type = ZOMBIE_FOOTBALL;
    } else if (g->wave >= 4 && roll < 12) {
        type = ZOMBIE_SCREENDOOR;
    } else if (g->wave >= 4 && roll < 18) {
        type = ZOMBIE_DANCER;
    } else if (g->wave >= 3 && roll < 28) {
        type = ZOMBIE_BUCKETHEAD;
    } else if (g->wave >= 3 && roll < 38) {
        type = ZOMBIE_NEWSPAPER;
    } else if (g->wave >= 2 && roll < 50) {
        type = ZOMBIE_POLEVAULTER;
    } else if (g->wave >= 2 && roll < 65) {
        type = ZOMBIE_CONEHEAD;
    }

    /* endless mode: even harder after wave 10 */
    if (g->mode == MODE_ENDLESS && g->wave >= 10) {
        roll = rand() % 100;
        if (roll < 10) type = ZOMBIE_FOOTBALL;
        else if (roll < 20) type = ZOMBIE_SCREENDOOR;
        else if (roll < 30) type = ZOMBIE_DANCER;
        else if (roll < 40) type = ZOMBIE_POLEVAULTER;
        else if (roll < 50) type = ZOMBIE_BUCKETHEAD;
        else if (roll < 60) type = ZOMBIE_NEWSPAPER;
        else if (roll < 75) type = ZOMBIE_CONEHEAD;
    }

    board_spawn_zombie(&g->board, type, row);
    g->zombies_remaining--;
}

#define FEEDBACK_DURATION_TICKS 45

/* check if a plant type is already in the deck */
static int deck_contains(const Game *g, PlantType type) {
    for (int i = 0; i < g->deck_count; i++) {
        if (g->deck[i] == type) return 1;
    }
    return 0;
}

static void set_feedback(Game *g, GameFeedback feedback) {
    g->feedback = feedback;
    g->feedback_ticks = FEEDBACK_DURATION_TICKS;
}

void game_init(Game *g) {
    srand((unsigned)time(NULL));
    g->state = STATE_MENU;
    g->mode = MODE_LEVEL;
    g->menu_selection = 0;
    board_init(&g->board);
    g->sun = INITIAL_SUN;
    g->level = 1;
    g->wave = 1;
    g->tick = 0;
    g->cursor_row = BOARD_ROWS / 2;
    g->cursor_col = BOARD_COLS / 2;
    g->selected_plant = PLANT_NONE;
    g->shovel_mode = 0;
    g->zombies_remaining = 5 + g->wave * 3;
    g->spawn_timer = 300;
    g->max_slots = 6;
    g->deck_count = 0;
    g->card_cursor = 0;
    g->feedback = FEEDBACK_NONE;
    g->feedback_ticks = 0;
    g->help_visible = 0;

    for (int i = 0; i < PLANT_COUNT; i++) {
        g->card_cooldowns[i] = 0;
        g->deck[i] = PLANT_NONE;
    }
}

/* enter card selection screen before playing */
static void game_enter_card_select(Game *g) {
    g->state = STATE_CARD_SELECT;
    g->deck_count = 0;
    g->card_cursor = 0;
    g->feedback = FEEDBACK_NONE;
    g->feedback_ticks = 0;
    for (int i = 0; i < PLANT_COUNT; i++)
        g->deck[i] = PLANT_NONE;
}

void game_start_playing(Game *g) {
    g->state = STATE_PLAYING;
    board_init(&g->board);
    g->sun = INITIAL_SUN;
    g->wave = 1;
    g->tick = 0;
    g->cursor_row = BOARD_ROWS / 2;
    g->cursor_col = BOARD_COLS / 2;
    g->selected_plant = PLANT_NONE;
    g->shovel_mode = 0;
    g->feedback = FEEDBACK_NONE;
    g->feedback_ticks = 0;
    g->help_visible = 0;
    g->zombies_remaining = 5 + g->wave * 3;
    g->spawn_timer = 300;
    for (int i = 0; i < PLANT_COUNT; i++) {
        g->card_cooldowns[i] = 0;
    }
}

static int handle_menu_input(Game *g, int ch) {
    switch (ch) {
    case KEY_UP: case 'w': case 'k':
        if (g->menu_selection > 0) g->menu_selection--;
        break;
    case KEY_DOWN: case 's': case 'j':
        if (g->menu_selection < 1) g->menu_selection++;
        break;
    case '\n': case '\r': case ' ':
        g->mode = (g->menu_selection == 0) ? MODE_LEVEL : MODE_ENDLESS;
        game_enter_card_select(g);
        break;
    case 'q': case 'Q':
        return 1;
    }
    return 0;
}

static int handle_card_select_input(Game *g, int ch) {
    int plant_count = PLANT_COUNT - 1;

    switch (ch) {
    case KEY_UP: case 'w': case 'k':
        if (g->card_cursor > 0) g->card_cursor--;
        break;
    case KEY_DOWN: case 's': case 'j':
        if (g->card_cursor < plant_count - 1) g->card_cursor++;
        break;
    case KEY_LEFT: case 'a': case 'h':
        if (g->max_slots > 6) {
            g->max_slots--;
            while (g->deck_count > g->max_slots)
                g->deck[--g->deck_count] = PLANT_NONE;
        }
        break;
    case KEY_RIGHT: case 'd': case 'l':
        if (g->max_slots < 9) g->max_slots++;
        break;
    case '\n': case '\r': case ' ': {
        PlantType pt = (PlantType)(g->card_cursor + 1);
        if (deck_contains(g, pt)) {
            int idx = -1;
            for (int i = 0; i < g->deck_count; i++) {
                if (g->deck[i] == pt) { idx = i; break; }
            }
            if (idx >= 0) {
                for (int i = idx; i < g->deck_count - 1; i++)
                    g->deck[i] = g->deck[i + 1];
                g->deck[--g->deck_count] = PLANT_NONE;
            }
        } else if (g->deck_count < g->max_slots) {
            g->deck[g->deck_count++] = pt;
        } else {
            set_feedback(g, FEEDBACK_DECK_FULL);
        }
        break;
    }
    case 'g': case 'G':
        if (g->deck_count > 0) {
            game_start_playing(g);
        } else {
            set_feedback(g, FEEDBACK_EMPTY_DECK);
        }
        break;
    case 'q': case 'Q':
        game_init(g);
        break;
    }
    return 0;
}

static int handle_end_input(Game *g, int ch) {
    if (ch == 'q' || ch == 'Q') {
        return 1;
    } else if (ch == 'r' || ch == 'R') {
        game_enter_card_select(g);
    } else if (ch == 'm' || ch == 'M') {
        game_init(g);
    }
    return 0;
}

static int handle_play_input(Game *g, int ch) {
    if (ch == 'q' || ch == 'Q') return 1;
    if (ch == '?') {
        g->help_visible = !g->help_visible;
        return 0;
    }
    if (g->help_visible) return 0;

    if (ch == 'p' || ch == 'P') {
        g->state = (g->state == STATE_PAUSED) ? STATE_PLAYING : STATE_PAUSED;
        return 0;
    }
    if (g->state == STATE_PAUSED) return 0;

    switch (ch) {
    case KEY_UP: case 'w': case 'k':
        if (g->cursor_row > 0) g->cursor_row--;
        break;
    case KEY_DOWN: case 's': case 'j':
        if (g->cursor_row < BOARD_ROWS - 1) g->cursor_row++;
        break;
    case KEY_LEFT: case 'a': case 'h':
        if (g->cursor_col > 0) g->cursor_col--;
        break;
    case KEY_RIGHT: case 'd': case 'l':
        if (g->cursor_col < BOARD_COLS - 1) g->cursor_col++;
        break;
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': {
        int idx = ch - '1';
        if (idx < g->deck_count) {
            g->selected_plant = g->deck[idx];
            g->shovel_mode = 0;
        }
        break;
    }
    case '0':
        g->shovel_mode = !g->shovel_mode;
        if (g->shovel_mode) g->selected_plant = PLANT_NONE;
        set_feedback(g, g->shovel_mode ? FEEDBACK_SHOVEL_ON : FEEDBACK_SHOVEL_OFF);
        break;
    case '\n': case '\r': case ' ':
        if (g->shovel_mode) {
            Plant *p = &g->board.cells[g->cursor_row][g->cursor_col];
            if (p->type != PLANT_NONE && p->hp > 0) {
                p->type = PLANT_NONE;
                p->hp = 0;
                sound_play(SFX_SHOVEL);
                set_feedback(g, FEEDBACK_REMOVED);
            } else {
                sound_play(SFX_DENY);
                set_feedback(g, FEEDBACK_NOTHING_TO_REMOVE);
            }
        } else if (g->selected_plant == PLANT_NONE) {
            sound_play(SFX_DENY);
            set_feedback(g, FEEDBACK_NO_PLANT);
        } else {
            const PlantDef *def = &PLANT_DEFS[g->selected_plant];
            Plant *p = &g->board.cells[g->cursor_row][g->cursor_col];
            if (g->sun < def->cost) {
                sound_play(SFX_DENY);
                set_feedback(g, FEEDBACK_NEED_SUN);
            } else if (g->card_cooldowns[g->selected_plant] > 0) {
                sound_play(SFX_DENY);
                set_feedback(g, FEEDBACK_COOLDOWN);
            } else if (p->type != PLANT_NONE) {
                sound_play(SFX_DENY);
                set_feedback(g, FEEDBACK_OCCUPIED);
            } else {
                board_place_plant(&g->board, g->selected_plant,
                                  g->cursor_row, g->cursor_col);
                g->sun -= def->cost;
                g->card_cooldowns[g->selected_plant] = def->cooldown > 0 ? def->cooldown : 30;
                sound_play(SFX_PLANT);
                set_feedback(g, FEEDBACK_PLANTED);
            }
        }
        break;
    }
    return 0;
}

int game_handle_input(Game *g, int ch) {
    switch (g->state) {
    case STATE_MENU:
        return handle_menu_input(g, ch);
    case STATE_CARD_SELECT:
        return handle_card_select_input(g, ch);
    case STATE_PLAYING:
    case STATE_PAUSED:
        return handle_play_input(g, ch);
    case STATE_WON:
    case STATE_LOST:
        return handle_end_input(g, ch);
    }
    return 0;
}

void game_update(Game *g) {
    if (g->help_visible) return;
    if (g->feedback_ticks > 0 && --g->feedback_ticks == 0)
        g->feedback = FEEDBACK_NONE;
    if (g->state != STATE_PLAYING) return;

    g->tick++;

    /* passive sun income */
    if (g->tick % SUN_DROP_INTERVAL == 0) {
        g->sun += 25;
    }

    /* cooldown ticks */
    for (int i = 0; i < PLANT_COUNT; i++) {
        if (g->card_cooldowns[i] > 0) g->card_cooldowns[i]--;
    }

    /* spawn zombies */
    if (g->zombies_remaining > 0) {
        g->spawn_timer--;
        if (g->spawn_timer <= 0) {
            spawn_random_zombie(g);
            g->spawn_timer = 40 + rand() % 80;
        }
    }

    /* update board */
    int lives_lost = 0;
    board_update(&g->board, g->tick, &g->sun, &lives_lost);

    if (lives_lost > 0) {
        g->state = STATE_LOST;
        sound_play(SFX_GAME_OVER);
        return;
    }

    /* check wave complete */
    if (g->zombies_remaining <= 0 && g->board.zombie_count == 0) {
        g->wave++;

        if (g->mode == MODE_LEVEL && g->wave > 5) {
            g->state = STATE_WON;
            sound_play(SFX_WIN);
        } else {
            /* next wave — endless never ends */
            g->zombies_remaining = 5 + g->wave * 3;
            /* endless: accelerate spawning */
            if (g->mode == MODE_ENDLESS) {
                int base_timer = 120 - g->wave * 5;
                if (base_timer < 40) base_timer = 40;
                g->spawn_timer = base_timer;
            } else {
                g->spawn_timer = 120;
            }
            sound_play(SFX_WAVE_CLEAR);
        }
    }
}

int game_is_over(Game *g) {
    return (g->state == STATE_WON || g->state == STATE_LOST);
}
