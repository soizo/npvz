#ifndef PLANT_H
#define PLANT_H

#include <wchar.h>

typedef enum {
    PLANT_NONE = 0,
    PLANT_SUNFLOWER,
    PLANT_PEASHOOTER,
    PLANT_WALLNUT,
    PLANT_CHERRYBOMB,
    PLANT_SNOWPEA,
    PLANT_COUNT
} PlantType;

typedef struct {
    PlantType type;
    int hp;
    int row;
    int col;
    int sun_timer;      /* sunflower: ticks until next sun */
    int shoot_timer;    /* peashooter/snowpea: ticks until next shot */
    int explode_timer;  /* cherrybomb: ticks until detonation */
} Plant;

typedef struct {
    const wchar_t *emoji;
    const char *name;
    int cost;
    int hp;
    int cooldown;       /* card cooldown in ticks */
    int shoot_interval; /* ticks between shots, 0 if non-shooter */
    int sun_interval;   /* ticks between sun production, 0 if none */
} PlantDef;

extern const PlantDef PLANT_DEFS[PLANT_COUNT];

void plant_init(Plant *p, PlantType type, int row, int col);
void plant_update(Plant *p, int tick);

#endif
