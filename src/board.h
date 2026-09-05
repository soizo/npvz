#ifndef BOARD_H
#define BOARD_H

#include "plant.h"
#include "zombie.h"
#include "projectile.h"

#define BOARD_ROWS    5
#define BOARD_COLS    9
#define MAX_ZOMBIES   64
#define MAX_PROJECTILES 128

typedef struct {
    int active;         /* 1 if mower is still on this row */
    float x;            /* position when triggered */
    int triggered;
} LawnMower;

#define MAX_COMBAT_EFFECTS 64

typedef enum {
    COMBAT_EFFECT_DEATH,
    COMBAT_EFFECT_BLAST
} CombatEffectKind;

typedef struct {
    CombatEffectKind kind;
    int row;
    float x;
    int timer;
    ZombieType zombie_type;
    int angry;
} CombatEffect;

typedef struct {
    Plant cells[BOARD_ROWS][BOARD_COLS];
    Zombie zombies[MAX_ZOMBIES];
    int zombie_count;
    Projectile projectiles[MAX_PROJECTILES];
    int projectile_count;
    LawnMower mowers[BOARD_ROWS];
    CombatEffect effects[MAX_COMBAT_EFFECTS];
    int effect_count;
    int plant_flash_ticks[BOARD_ROWS][BOARD_COLS];
} Board;

/* display characters */
#define CELL_EMPTY  L'\x25A1'   /* □ white square */
#define MOWER_EMOJI L"\U0001F69C" /* 🚜 */

void board_init(Board *b);
void board_place_plant(Board *b, PlantType type, int row, int col);
void board_spawn_zombie(Board *b, ZombieType type, int row);
void board_update(Board *b, int tick, int *sun, int *lives_lost);

#endif
