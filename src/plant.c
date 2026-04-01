#include "plant.h"

const PlantDef PLANT_DEFS[PLANT_COUNT] = {
    [PLANT_NONE]       = { L"",   "none",        0,   0,  0,  0,  0 },
    [PLANT_SUNFLOWER]  = { L"🌻", "Sunflower",  50, 100,  0,  0, 120 },
    [PLANT_PEASHOOTER] = { L"🫛", "Peashooter",100, 100,  0, 60,   0 },
    [PLANT_WALLNUT]    = { L"🪨", "Wall-nut",   50, 400,  0,  0,   0 },
    [PLANT_CHERRYBOMB] = { L"🍒", "CherryBomb",150,  50,  0,  0,   0 },
    [PLANT_SNOWPEA]    = { L"🧊", "Snow Pea",  175, 100,  0, 60,   0 },
};

void plant_init(Plant *p, PlantType type, int row, int col) {
    p->type = type;
    p->hp = PLANT_DEFS[type].hp;
    p->row = row;
    p->col = col;
    p->sun_timer = PLANT_DEFS[type].sun_interval;
    p->shoot_timer = PLANT_DEFS[type].shoot_interval;
    p->explode_timer = (type == PLANT_CHERRYBOMB) ? 15 : 0;
}

void plant_update(Plant *p, int tick) {
    (void)tick;
    if (p->type == PLANT_NONE || p->hp <= 0) return;

    if (p->sun_timer > 0) {
        p->sun_timer--;
    }
    if (p->shoot_timer > 0) {
        p->shoot_timer--;
    }
    if (p->type == PLANT_CHERRYBOMB && p->explode_timer > 0) {
        p->explode_timer--;
    }
}
