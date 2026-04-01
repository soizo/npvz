#include "projectile.h"

void projectile_init(Projectile *p, ProjType type, int row, int col) {
    p->type = type;
    p->row = row;
    p->x = (float)col;
    p->speed = 0.15f;
    p->damage = 20;
    p->alive = 1;
    p->slow = (type == PROJ_SNOWPEA) ? 1 : 0;
}

void projectile_update(Projectile *p) {
    if (!p->alive) return;
    p->x += p->speed;
}
