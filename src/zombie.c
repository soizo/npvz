#include "zombie.h"

const ZombieDef ZOMBIE_DEFS[ZOMBIE_TYPE_COUNT] = {
    [ZOMBIE_NORMAL]     = { L"🧟\uFE0F", "Zombie",      100, 0.005f, 10 },
    [ZOMBIE_CONEHEAD]   = { L"🪖\uFE0F", "Conehead",    100, 0.005f, 10 },
    [ZOMBIE_BUCKETHEAD] = { L"🪣\uFE0F", "Buckethead",  100, 0.004f, 10 },
    [ZOMBIE_DANCER]     = { L"🕺\uFE0F", "Dancer",      150, 0.006f, 10 },
    [ZOMBIE_BACKUP]     = { L"👯\uFE0F", "Backup",       80, 0.007f,  8 },
    [ZOMBIE_POLEVAULTER]= { L"🏌\uFE0F", "Pole Vaulter",150, 0.008f, 10 },
    [ZOMBIE_NEWSPAPER]  = { L"📰\uFE0F", "Newspaper",   100, 0.003f, 10 },
    [ZOMBIE_FOOTBALL]   = { L"🏈\uFE0F", "Football",    200, 0.008f, 12 },
    [ZOMBIE_SCREENDOOR] = { L"🚪\uFE0F", "Screen Door", 100, 0.004f, 10 },
};

/* armor HP by type */
static const int ZOMBIE_ARMOR[ZOMBIE_TYPE_COUNT] = {
    [ZOMBIE_NORMAL]     = 0,
    [ZOMBIE_CONEHEAD]   = 100,
    [ZOMBIE_BUCKETHEAD] = 300,
    [ZOMBIE_DANCER]     = 0,
    [ZOMBIE_BACKUP]     = 0,
    [ZOMBIE_POLEVAULTER]= 0,
    [ZOMBIE_NEWSPAPER]  = 80,
    [ZOMBIE_FOOTBALL]   = 200,
    [ZOMBIE_SCREENDOOR] = 250,
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
    z->summon_timer = (type == ZOMBIE_DANCER) ? 180 : 0;
    z->has_summoned = 0;
    z->has_vaulted = 0;
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
            z->hp -= overflow;
            if (z->type == ZOMBIE_NEWSPAPER) {
                /* newspaper zombie rages: keep type for 😡 emoji */
                z->speed = 0.008f;
            } else {
                z->type = ZOMBIE_NORMAL;  /* downgrade to 🧟 */
            }
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
