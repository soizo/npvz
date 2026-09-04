#ifndef GAME_H
#define GAME_H

#include "board.h"

typedef enum {
    STATE_MENU,
    STATE_CARD_SELECT,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_WON,
    STATE_LOST
} GameState;

typedef enum {
    MODE_LEVEL,
    MODE_ENDLESS
} GameMode;

typedef struct {
    GameState state;
    GameMode mode;
    int menu_selection;         /* 0 = level, 1 = endless */
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

    /* card slot selection */
    int max_slots;              /* 6-9, chosen at card select screen */
    PlantType deck[PLANT_COUNT]; /* selected plants for this game */
    int deck_count;             /* how many plants in deck */
    int card_cursor;            /* cursor in card selection screen */
} Game;

void game_init(Game *g);
void game_start_playing(Game *g);
int  game_handle_input(Game *g, int ch);
void game_update(Game *g);
int  game_is_over(Game *g);

#endif
