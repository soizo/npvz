#include "plant.h"

const PlantDef PLANT_DEFS[PLANT_COUNT] = {
    [PLANT_NONE]       = { L"",   "none",        0,   0,  0,  0,  0 },
    [PLANT_SUNFLOWER]  = { L"🌻", "Sunflower",  50, 100,  0,  0, 120 },
    [PLANT_PEASHOOTER] = { L"🫛", "Peashooter",100, 100,  0, 60,   0 },
    [PLANT_WALLNUT]    = { L"🪨", "Wall-nut",   50, 400,  0,  0,   0 },
    [PLANT_CHERRYBOMB] = { L"🍒", "CherryBomb",150,  50,  0,  0,   0 },
    [PLANT_SNOWPEA]    = { L"🧊", "Snow Pea",  175, 100,  0, 60,   0 },
    [PLANT_JALAPENO]   = { L"🌶\uFE0F", "Jalapeno", 125,  50,  0,  0,   0 },
    [PLANT_REPEATER]   = { L"🪴", "Repeater",  200, 100,  0, 60,   0 },
    [PLANT_CHOMPER]    = { L"🌺", "Chomper",   150, 100,  0,  0,   0 },
    [PLANT_POTATOMINE] = { L"🥔", "PotatoMine", 25,  50,  0,  0,   0 },
    [PLANT_SQUASH]     = { L"🍈", "Squash",     50, 100,  0,  0,   0 },
};

void plant_init(Plant *p, PlantType type, int row, int col) {
    p->type = type;
    p->hp = PLANT_DEFS[type].hp;
    p->row = row;
    p->col = col;
    p->sun_timer = PLANT_DEFS[type].sun_interval;
    p->shoot_timer = PLANT_DEFS[type].shoot_interval;
    p->explode_timer = (type == PLANT_CHERRYBOMB || type == PLANT_JALAPENO) ? 15
                     : (type == PLANT_POTATOMINE) ? 120
                     : (type == PLANT_SQUASH) ? 30 : 0;
    p->chomp_timer = 0;
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
    if ((p->type == PLANT_CHERRYBOMB || p->type == PLANT_JALAPENO
         || p->type == PLANT_POTATOMINE || p->type == PLANT_SQUASH)
        && p->explode_timer > 0) {
        p->explode_timer--;
    }
    if (p->type == PLANT_CHOMPER && p->chomp_timer > 0) {
        p->chomp_timer--;
    }
}
