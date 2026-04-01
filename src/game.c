#include "game.h"
#include "sound.h"
#include <ncurses.h>
#include <stdlib.h>
#include <time.h>

#define SUN_DROP_INTERVAL 200
#define INITIAL_SUN       50

/* wave definitions: {zombie_count, types...} */
static void spawn_random_zombie(Game *g) {
    if (g->zombies_remaining <= 0) return;

    int row = rand() % BOARD_ROWS;
    ZombieType type = ZOMBIE_NORMAL;

    /* harder zombies in later waves */
    int roll = rand() % 100;
    if (g->wave >= 3 && roll < 20) {
        type = ZOMBIE_BUCKETHEAD;
    } else if (g->wave >= 2 && roll < 40) {
        type = ZOMBIE_CONEHEAD;
    }

    board_spawn_zombie(&g->board, type, row);
    g->zombies_remaining--;
}

void game_init(Game *g) {
    srand((unsigned)time(NULL));
    g->state = STATE_PLAYING;
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
    g->spawn_timer = 300;  /* ~10 seconds before first zombie */

    for (int i = 0; i < PLANT_COUNT; i++) {
        g->card_cooldowns[i] = 0;
    }
}

void game_handle_input(Game *g, int ch) {
    if (g->state == STATE_WON || g->state == STATE_LOST) {
        if (ch == 'q' || ch == 'Q') {
            g->state = STATE_LOST; /* signal exit */
        } else if (ch == 'r' || ch == 'R') {
            game_init(g);
        }
        return;
    }

    if (ch == 'p' || ch == 'P') {
        g->state = (g->state == STATE_PAUSED) ? STATE_PLAYING : STATE_PAUSED;
        return;
    }

    if (g->state == STATE_PAUSED) return;

    if (ch == 'q' || ch == 'Q') {
        g->state = STATE_LOST; /* will be checked as exit */
        return;
    }

    switch (ch) {
    case KEY_UP:    case 'w': case 'k':
        if (g->cursor_row > 0) g->cursor_row--;
        break;
    case KEY_DOWN:  case 's': case 'j':
        if (g->cursor_row < BOARD_ROWS - 1) g->cursor_row++;
        break;
    case KEY_LEFT:  case 'a': case 'h':
        if (g->cursor_col > 0) g->cursor_col--;
        break;
    case KEY_RIGHT: case 'd': case 'l':
        if (g->cursor_col < BOARD_COLS - 1) g->cursor_col++;
        break;

    /* plant selection */
    case '1': g->selected_plant = PLANT_SUNFLOWER;  g->shovel_mode = 0; break;
    case '2': g->selected_plant = PLANT_PEASHOOTER; g->shovel_mode = 0; break;
    case '3': g->selected_plant = PLANT_WALLNUT;    g->shovel_mode = 0; break;
    case '4': g->selected_plant = PLANT_CHERRYBOMB;  g->shovel_mode = 0; break;
    case '5': g->selected_plant = PLANT_SNOWPEA;     g->shovel_mode = 0; break;

    /* shovel */
    case '0':
        g->shovel_mode = !g->shovel_mode;
        if (g->shovel_mode) g->selected_plant = PLANT_NONE;
        break;

    /* place plant or dig */
    case '\n': case '\r': case ' ':
        if (g->shovel_mode) {
            Plant *p = &g->board.cells[g->cursor_row][g->cursor_col];
            if (p->type != PLANT_NONE && p->hp > 0) {
                p->type = PLANT_NONE;
                p->hp = 0;
                sound_play(SFX_SHOVEL);
            } else {
                sound_play(SFX_DENY);
            }
        } else if (g->selected_plant != PLANT_NONE) {
            const PlantDef *def = &PLANT_DEFS[g->selected_plant];
            if (g->sun >= def->cost
                && g->card_cooldowns[g->selected_plant] <= 0
                && g->board.cells[g->cursor_row][g->cursor_col].type == PLANT_NONE) {
                board_place_plant(&g->board, g->selected_plant,
                                  g->cursor_row, g->cursor_col);
                g->sun -= def->cost;
                g->card_cooldowns[g->selected_plant] = def->cooldown > 0 ? def->cooldown : 30;
                sound_play(SFX_PLANT);
            } else {
                sound_play(SFX_DENY);
            }
        }
        break;
    }
}

void game_update(Game *g) {
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
        if (g->wave > 5) {
            g->state = STATE_WON;
            sound_play(SFX_WIN);
        } else {
            g->zombies_remaining = 5 + g->wave * 3;
            g->spawn_timer = 120;
            sound_play(SFX_WAVE_CLEAR);
        }
    }
}

int game_is_over(Game *g) {
    return (g->state == STATE_WON || g->state == STATE_LOST);
}
