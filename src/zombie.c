#include "zombie.h"

const ZombieDef ZOMBIE_DEFS[ZOMBIE_TYPE_COUNT] = {
    [ZOMBIE_NORMAL]     = { L"🧟", "Zombie",      100, 0.005f, 10 },
    [ZOMBIE_CONEHEAD]   = { L"🪖", "Conehead",    100, 0.005f, 10 },
    [ZOMBIE_BUCKETHEAD] = { L"🪣", "Buckethead",  100, 0.004f, 10 },
};

/* armor HP by type */
static const int ZOMBIE_ARMOR[ZOMBIE_TYPE_COUNT] = {
    [ZOMBIE_NORMAL]     = 0,
    [ZOMBIE_CONEHEAD]   = 100,
    [ZOMBIE_BUCKETHEAD] = 300,
};

void zombie_init(Zombie *z, ZombieType type, int row) {
    z->type = type;
    z->hp = ZOMBIE_DEFS[type].hp;
    z->row = row;
    z->x = (float)(BOARD_COLS);  /* start just off right edge */
    z->speed = ZOMBIE_DEFS[type].speed;
    z->eating = 0;
    z->eat_timer = 0;
    z->alive = 1;
    z->exploding = 0;
    z->armor_hp = ZOMBIE_ARMOR[type];
}

void zombie_take_damage(Zombie *z, int damage) {
    if (!z->alive) return;

    /* damage goes to armor first */
    if (z->armor_hp > 0) {
        z->armor_hp -= damage;
        if (z->armor_hp <= 0) {
            /* armor destroyed — leftover damage hits body */
            int overflow = -z->armor_hp;
            z->armor_hp = 0;
            z->type = ZOMBIE_NORMAL;  /* downgrade to 🧟 */
            z->hp -= overflow;
        }
    } else {
        z->hp -= damage;
    }

    if (z->hp <= 0) {
        z->alive = 0;
    }
}

void zombie_update(Zombie *z, int tick) {
    (void)tick;
    if (!z->alive) return;

    if (!z->eating) {
        z->x -= z->speed;
    }
    /* eating logic is handled entirely in board_update */
}
