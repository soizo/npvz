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

#define MAX_VFX 64

typedef enum {
    VFX_HIT,         /* zombie hit: gray bg flash */
    VFX_DEATH_SHOT,  /* killed by projectile: white bg */
    VFX_DEATH_BOOM   /* killed by explosion: white bg + 💥 */
} VfxType;

typedef struct {
    VfxType type;
    int row;
    float x;
    int timer;
} Vfx;

typedef struct {
    Plant cells[BOARD_ROWS][BOARD_COLS];
    Zombie zombies[MAX_ZOMBIES];
    int zombie_count;
    Projectile projectiles[MAX_PROJECTILES];
    int projectile_count;
    LawnMower mowers[BOARD_ROWS];
    Vfx vfx[MAX_VFX];
    int vfx_count;
} Board;

/* display characters */
#define CELL_EMPTY  L'\x25A1'   /* □ white square */
#define MOWER_EMOJI L"\U0001F69C" /* 🚜 */

void board_init(Board *b);
void board_place_plant(Board *b, PlantType type, int row, int col);
void board_spawn_zombie(Board *b, ZombieType type, int row);
void board_update(Board *b, int tick, int *sun, int *lives_lost);

#endif
