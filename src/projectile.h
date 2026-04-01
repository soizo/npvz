#ifndef PROJECTILE_H
#define PROJECTILE_H

typedef enum {
    PROJ_PEA = 0,
    PROJ_SNOWPEA,
    PROJ_TYPE_COUNT
} ProjType;

typedef struct {
    ProjType type;
    int row;
    float x;            /* fractional column position */
    float speed;        /* columns per tick */
    int damage;
    int alive;
    int slow;           /* 1 if this projectile slows zombies */
} Projectile;

/* display character: middle dot · (U+00B7) */
#define PROJ_CHAR L'\x00B7'

void projectile_init(Projectile *p, ProjType type, int row, int col);
void projectile_update(Projectile *p);

#endif
