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

typedef enum {
    FEEDBACK_NONE,
    FEEDBACK_NO_PLANT,
    FEEDBACK_NEED_SUN,
    FEEDBACK_COOLDOWN,
    FEEDBACK_OCCUPIED,
    FEEDBACK_PLANTED,
    FEEDBACK_SHOVEL_ON,
    FEEDBACK_SHOVEL_OFF,
    FEEDBACK_REMOVED,
    FEEDBACK_NOTHING_TO_REMOVE,
    FEEDBACK_DECK_FULL,
    FEEDBACK_EMPTY_DECK
} GameFeedback;

typedef struct {
    GameState state;
    GameMode mode;
    int menu_selection;         /* selected item on the active menu */
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
    int card_focus;             /* 0 = plants, 1 = start, 2 = menu */

    /* transient interface state */
    GameFeedback feedback;
    int feedback_ticks;
    int help_visible;
} Game;

void game_init(Game *g);
void game_start_playing(Game *g);
int  game_handle_input(Game *g, int ch);
int  game_handle_inputs(Game *g, const int *inputs, int count);
void game_update(Game *g);
int  game_is_over(Game *g);

#endif
