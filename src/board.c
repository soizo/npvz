#include "board.h"
#include "sound.h"
#include <string.h>
#include <math.h>

void board_init(Board *b) {
    memset(b, 0, sizeof(Board));
    for (int r = 0; r < BOARD_ROWS; r++) {
        b->mowers[r].active = 1;
        b->mowers[r].x = -1.0f;
        b->mowers[r].triggered = 0;
        for (int c = 0; c < BOARD_COLS; c++) {
            b->cells[r][c].type = PLANT_NONE;
        }
    }
}

void board_place_plant(Board *b, PlantType type, int row, int col) {
    if (row < 0 || row >= BOARD_ROWS || col < 0 || col >= BOARD_COLS) return;
    if (b->cells[row][col].type != PLANT_NONE) return;
    plant_init(&b->cells[row][col], type, row, col);
}

void board_spawn_zombie(Board *b, ZombieType type, int row) {
    if (b->zombie_count >= MAX_ZOMBIES) return;
    zombie_init(&b->zombies[b->zombie_count], type, row);
    b->zombie_count++;
}

static void board_add_vfx(Board *b, VfxType type, int row, float x, int duration) {
    if (b->vfx_count >= MAX_VFX) return;
    Vfx *v = &b->vfx[b->vfx_count++];
    v->type = type;
    v->row = row;
    v->x = x;
    v->timer = duration;
}

static void board_add_projectile(Board *b, ProjType type, int row, int col) {
    if (b->projectile_count >= MAX_PROJECTILES) return;
    projectile_init(&b->projectiles[b->projectile_count], type, row, col);
    b->projectile_count++;
}

/* check if any zombie is in the given row at or beyond the given column */
static int row_has_zombie_ahead(const Board *b, int row, int col) {
    for (int i = 0; i < b->zombie_count; i++) {
        const Zombie *z = &b->zombies[i];
        if (z->alive && z->row == row && z->x >= (float)col) {
            return 1;
        }
    }
    return 0;
}

void board_update(Board *b, int tick, int *sun, int *lives_lost) {
    *lives_lost = 0;

    /* update plants */
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            Plant *p = &b->cells[r][c];
            if (p->type == PLANT_NONE || p->hp <= 0) continue;

            plant_update(p, tick);

            /* sunflower produces sun */
            if (p->type == PLANT_SUNFLOWER && p->sun_timer <= 0) {
                *sun += 25;
                p->sun_timer = PLANT_DEFS[PLANT_SUNFLOWER].sun_interval;
            }

            /* shooters fire peas if zombies ahead */
            if ((p->type == PLANT_PEASHOOTER || p->type == PLANT_SNOWPEA)
                && p->shoot_timer <= 0
                && row_has_zombie_ahead(b, r, c)) {
                ProjType pt = (p->type == PLANT_SNOWPEA) ? PROJ_SNOWPEA : PROJ_PEA;
                board_add_projectile(b, pt, r, c + 1);
                p->shoot_timer = PLANT_DEFS[p->type].shoot_interval;
            }

            /* cherry bomb explodes */
            if (p->type == PLANT_CHERRYBOMB && p->explode_timer <= 0) {
                for (int i = 0; i < b->zombie_count; i++) {
                    Zombie *z = &b->zombies[i];
                    if (!z->alive) continue;
                    int zc = (int)z->x;
                    if (z->row >= r - 1 && z->row <= r + 1
                        && zc >= c - 1 && zc <= c + 1) {
                        board_add_vfx(b, VFX_DEATH_BOOM, z->row, z->x, 5);
                        z->alive = 0;
                        z->hp = 0;
                    }
                }
                p->type = PLANT_NONE;
                p->hp = 0;
                sound_play(SFX_EXPLODE);
            }
        }
    }

    /* update projectiles */
    for (int i = 0; i < b->projectile_count; i++) {
        Projectile *pr = &b->projectiles[i];
        if (!pr->alive) continue;
        projectile_update(pr);

        /* off screen */
        if (pr->x > (float)(BOARD_COLS + 1)) {
            pr->alive = 0;
            continue;
        }

        /* collision with zombies */
        for (int j = 0; j < b->zombie_count; j++) {
            Zombie *z = &b->zombies[j];
            if (!z->alive || z->exploding || z->row != pr->row) continue;
            if (fabsf(z->x - pr->x) < 0.5f) {
                zombie_take_damage(z, pr->damage);
                if (pr->slow) {
                    z->speed = ZOMBIE_DEFS[z->type].speed * 0.5f;
                }
                board_add_vfx(b, VFX_HIT, z->row, z->x, 3);
                sound_play(SFX_HIT);
                if (!z->alive) {
                    board_add_vfx(b, VFX_DEATH_SHOT, z->row, z->x, 5);
                    sound_play(SFX_ZOMBIE_DIE);
                }
                pr->alive = 0;
                break;
            }
        }
    }

    /* compact dead projectiles */
    int wp = 0;
    for (int i = 0; i < b->projectile_count; i++) {
        if (b->projectiles[i].alive) {
            if (wp != i) b->projectiles[wp] = b->projectiles[i];
            wp++;
        }
    }
    b->projectile_count = wp;

    /* update zombies */
    for (int i = 0; i < b->zombie_count; i++) {
        Zombie *z = &b->zombies[i];
        if (!z->alive) continue;

        /* check if eating a plant */
        int zc = (int)z->x;
        z->eating = 0;
        if (zc >= 0 && zc < BOARD_COLS) {
            Plant *p = &b->cells[z->row][zc];
            if (p->type != PLANT_NONE && p->hp > 0) {
                z->eating = 1;
                if (z->eat_timer <= 0) {
                    z->eat_timer = 10;
                }
                z->eat_timer--;
                if (z->eat_timer <= 0) {
                    p->hp -= ZOMBIE_DEFS[z->type].damage;
                    z->eat_timer = 10;
                    if (p->hp <= 0) {
                        p->type = PLANT_NONE;
                    }
                }
            }
        }

        zombie_update(z, tick);

        /* zombie reached the left edge */
        if (z->x < 0.0f) {
            /* try lawn mower */
            LawnMower *m = &b->mowers[z->row];
            if (m->active && !m->triggered) {
                m->triggered = 1;
                sound_play(SFX_MOWER);
                /* mower kills all zombies in this row */
                for (int j = 0; j < b->zombie_count; j++) {
                    if (b->zombies[j].alive && b->zombies[j].row == z->row) {
                        b->zombies[j].alive = 0;
                        b->zombies[j].hp = 0;
                    }
                }
                m->active = 0;
            } else {
                /* no mower — lost a life */
                z->alive = 0;
                (*lives_lost)++;
            }
        }
    }

    /* update vfx timers */
    int wv = 0;
    for (int i = 0; i < b->vfx_count; i++) {
        b->vfx[i].timer--;
        if (b->vfx[i].timer > 0) {
            if (wv != i) b->vfx[wv] = b->vfx[i];
            wv++;
        }
    }
    b->vfx_count = wv;

    /* compact dead zombies */
    int wz = 0;
    for (int i = 0; i < b->zombie_count; i++) {
        if (b->zombies[i].alive) {
            if (wz != i) b->zombies[wz] = b->zombies[i];
            wz++;
        }
    }
    b->zombie_count = wz;
}
