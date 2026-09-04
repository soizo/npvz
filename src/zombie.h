#ifndef ZOMBIE_H
#define ZOMBIE_H

#include <wchar.h>

#ifndef BOARD_COLS
#define BOARD_COLS 9
#endif

typedef enum {
    ZOMBIE_NORMAL = 0,
    ZOMBIE_CONEHEAD,
    ZOMBIE_BUCKETHEAD,
    ZOMBIE_DANCER,
    ZOMBIE_BACKUP,
    ZOMBIE_POLEVAULTER,
    ZOMBIE_NEWSPAPER,
    ZOMBIE_FOOTBALL,
    ZOMBIE_SCREENDOOR,
    ZOMBIE_TYPE_COUNT
} ZombieType;

typedef struct {
    ZombieType type;
    int hp;
    int row;
    float x;            /* fractional column position */
    float speed;        /* columns per tick */
    int eating;         /* 1 if currently eating a plant */
    int eat_timer;      /* ticks until next bite */
    int alive;
    int exploding;      /* >0: show 💥, counts down to death */
    int armor_hp;       /* headgear hp: cone/bucket, 0 for normal */
    int summon_timer;   /* dancer: ticks until summon backup dancers */
    int has_summoned;   /* dancer: 1 if already summoned */
    int has_vaulted;    /* pole vaulter: 1 after jumping a plant */
} Zombie;

typedef struct {
    const wchar_t *emoji;
    const char *name;
    int hp;
    float speed;
    int damage;         /* damage per bite */
} ZombieDef;

extern const ZombieDef ZOMBIE_DEFS[ZOMBIE_TYPE_COUNT];

void zombie_init(Zombie *z, ZombieType type, int row);
void zombie_update(Zombie *z, int tick);
void zombie_take_damage(Zombie *z, int damage);

#endif
