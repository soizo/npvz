#ifndef GAME_H
#define GAME_H

#include "board.h"

typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_WON,
    STATE_LOST
} GameState;

typedef struct {
    GameState state;
    Board board;
    int sun;
    int level;
    int wave;
    int tick;
    int cursor_row;
    int cursor_col;
    PlantType selected_plant;
    int shovel_mode;            /* 1 = shovel active */
    int card_cooldowns[PLANT_COUNT];
    int zombies_remaining;  /* zombies left to spawn this wave */
    int spawn_timer;
} Game;

void game_init(Game *g);
void game_handle_input(Game *g, int ch);
void game_update(Game *g);
int  game_is_over(Game *g);

#endif
