#include "board.h"
#include "sound.h"
#include <string.h>
#include <math.h>

#define PLANT_FLASH_TICKS 3

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

static void board_add_effect(Board *b, CombatEffectKind kind,
                             const Zombie *zombie, int duration) {
    if (b->effect_count >= MAX_COMBAT_EFFECTS) return;
    CombatEffect *effect = &b->effects[b->effect_count++];
    effect->kind = kind;
    effect->row = zombie->row;
    effect->x = zombie->x;
    effect->timer = duration;
    effect->zombie_type = zombie->type;
    effect->angry = zombie->type == ZOMBIE_NEWSPAPER && zombie->armor_hp <= 0;
}

static void board_add_projectile(Board *b, ProjType type, int row, int col) {
    if (b->projectile_count >= MAX_PROJECTILES) return;
    projectile_init(&b->projectiles[b->projectile_count], type, row, col);
    b->projectile_count++;
}

static void flash_plant(Board *b, int row, int col) {
    b->plant_flash_ticks[row][col] = PLANT_FLASH_TICKS;
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

static void update_plants(Board *b, int tick, int *sun) {
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            Plant *p = &b->cells[r][c];
            if (p->type == PLANT_NONE || p->hp <= 0) continue;

            plant_update(p, tick);

            if (p->type == PLANT_SUNFLOWER && p->sun_timer <= 0) {
                *sun += 25;
                p->sun_timer = PLANT_DEFS[PLANT_SUNFLOWER].sun_interval;
                flash_plant(b, r, c);
            }

            if ((p->type == PLANT_PEASHOOTER || p->type == PLANT_SNOWPEA
                 || p->type == PLANT_REPEATER)
                && p->shoot_timer <= 0
                && row_has_zombie_ahead(b, r, c)) {
                ProjType pt = (p->type == PLANT_SNOWPEA) ? PROJ_SNOWPEA : PROJ_PEA;
                board_add_projectile(b, pt, r, c + 1);
                if (p->type == PLANT_REPEATER) {
                    board_add_projectile(b, PROJ_PEA, r, c + 1);
                    if (b->projectile_count > 0)
                        b->projectiles[b->projectile_count - 1].x += 0.4f;
                }
                p->shoot_timer = PLANT_DEFS[p->type].shoot_interval;
                flash_plant(b, r, c);
            }

            if (p->type == PLANT_CHERRYBOMB && p->explode_timer <= 0) {
                for (int i = 0; i < b->zombie_count; i++) {
                    Zombie *z = &b->zombies[i];
                    if (!z->alive) continue;
                    if (z->row >= r - 1 && z->row <= r + 1
                        && z->x > c - 0.7f && z->x < c + 1.7f) {
                        board_add_effect(b, COMBAT_EFFECT_BLAST, z, 5);
                        z->alive = 0;
                        z->hp = 0;
                    }
                }
                flash_plant(b, r, c);
                p->type = PLANT_NONE;
                p->hp = 0;
                sound_play(SFX_EXPLODE);
            }

            if (p->type == PLANT_JALAPENO && p->explode_timer <= 0) {
                for (int i = 0; i < b->zombie_count; i++) {
                    Zombie *z = &b->zombies[i];
                    if (!z->alive || z->row != r) continue;
                    board_add_effect(b, COMBAT_EFFECT_BLAST, z, 5);
                    z->alive = 0;
                    z->hp = 0;
                }
                flash_plant(b, r, c);
                p->type = PLANT_NONE;
                p->hp = 0;
                sound_play(SFX_EXPLODE);
            }

            if (p->type == PLANT_CHOMPER && p->chomp_timer <= 0) {
                for (int i = 0; i < b->zombie_count; i++) {
                    Zombie *z = &b->zombies[i];
                    if (!z->alive || z->row != r) continue;
                    int zc = (int)z->x;
                    if (zc == c || zc == c + 1) {
                        board_add_effect(b, COMBAT_EFFECT_DEATH, z, 5);
                        z->alive = 0;
                        z->hp = 0;
                        p->chomp_timer = 180;
                        flash_plant(b, r, c);
                        sound_play(SFX_EXPLODE);
                        break;
                    }
                }
            }

            if (p->type == PLANT_POTATOMINE && p->explode_timer <= 0) {
                for (int i = 0; i < b->zombie_count; i++) {
                    Zombie *z = &b->zombies[i];
                    if (!z->alive || z->row != r) continue;
                    if ((int)z->x == c) {
                        board_add_effect(b, COMBAT_EFFECT_BLAST, z, 5);
                        z->alive = 0;
                        z->hp = 0;
                        flash_plant(b, r, c);
                        p->type = PLANT_NONE;
                        p->hp = 0;
                        sound_play(SFX_EXPLODE);
                        break;
                    }
                }
            }

            if (p->type == PLANT_SQUASH && p->explode_timer <= 0) {
                float nearest_distance = (float)(BOARD_COLS + 1);
                int nearest_idx = -1;
                for (int i = 0; i < b->zombie_count; i++) {
                    Zombie *z = &b->zombies[i];
                    if (!z->alive || z->row != r) continue;
                    float distance = fabsf(z->x - (float)c);
                    if (distance < nearest_distance) {
                        nearest_distance = distance;
                        nearest_idx = i;
                    }
                }
                if (nearest_idx >= 0) {
                    Zombie *z = &b->zombies[nearest_idx];
                    board_add_effect(b, COMBAT_EFFECT_DEATH, z, 5);
                    z->alive = 0;
                    z->hp = 0;
                    flash_plant(b, r, c);
                    p->type = PLANT_NONE;
                    p->hp = 0;
                    sound_play(SFX_EXPLODE);
                }
            }
        }
    }
}

static void update_projectiles(Board *b) {
    for (int i = 0; i < b->projectile_count; i++) {
        Projectile *pr = &b->projectiles[i];
        if (!pr->alive) continue;
        projectile_update(pr);

        if (pr->x > (float)(BOARD_COLS + 1)) {
            pr->alive = 0;
            continue;
        }

        for (int j = 0; j < b->zombie_count; j++) {
            Zombie *z = &b->zombies[j];
            if (!z->alive || z->row != pr->row) continue;
            if (fabsf(z->x - pr->x) < 0.5f) {
                zombie_take_damage(z, pr->damage);
                if (pr->slow) z->speed = ZOMBIE_DEFS[z->type].speed * 0.3f;
                sound_play(SFX_HIT);
                if (z->alive) {
                    z->hit_ticks = 3;
                } else {
                    board_add_effect(b, COMBAT_EFFECT_DEATH, z, 5);
                    sound_play(SFX_ZOMBIE_DIE);
                }
                pr->alive = 0;
                break;
            }
        }
    }
}

static void summon_dancers(Board *b) {
    int current_count = b->zombie_count;
    for (int i = 0; i < current_count; i++) {
        Zombie *z = &b->zombies[i];
        if (!z->alive || z->type != ZOMBIE_DANCER || z->has_summoned) continue;

        z->summon_timer--;
        if (z->summon_timer > 0) continue;

        z->has_summoned = 1;
        int rows[] = { z->row - 1, z->row + 1 };
        for (int j = 0; j < 2; j++) {
            int row = rows[j];
            if (row < 0 || row >= BOARD_ROWS || b->zombie_count >= MAX_ZOMBIES) continue;
            zombie_init(&b->zombies[b->zombie_count], ZOMBIE_BACKUP, row);
            b->zombies[b->zombie_count].x = z->x;
            b->zombie_count++;
        }
    }
}

static void update_zombies(Board *b, int tick, int *lives_lost) {
    for (int i = 0; i < b->zombie_count; i++) {
        Zombie *z = &b->zombies[i];
        if (!z->alive) continue;

        int col = (int)z->x;
        z->eating = 0;
        if (col >= 0 && col < BOARD_COLS) {
            Plant *p = &b->cells[z->row][col];
            if (p->type != PLANT_NONE && p->hp > 0) {
                if (z->type == ZOMBIE_POLEVAULTER && !z->has_vaulted && col > 0) {
                    z->has_vaulted = 1;
                    z->x = (float)(col - 1) + 0.5f;
                    z->speed = 0.005f;
                    z->type = ZOMBIE_NORMAL;
                    continue;
                }
                z->eating = 1;
                if (z->eat_timer <= 0) z->eat_timer = 10;
                z->eat_timer--;
                if (z->eat_timer <= 0) {
                    p->hp -= ZOMBIE_DEFS[z->type].damage;
                    z->eat_timer = 10;
                    sound_play(SFX_BITE);
                    if (p->hp <= 0) p->type = PLANT_NONE;
                }
            }
        }

        zombie_update(z, tick);
        if (z->x >= 0.0f) continue;

        LawnMower *mower = &b->mowers[z->row];
        if (mower->active && !mower->triggered) {
            mower->triggered = 1;
            sound_play(SFX_MOWER);
            for (int j = 0; j < b->zombie_count; j++) {
                if (b->zombies[j].alive && b->zombies[j].row == z->row) {
                    b->zombies[j].alive = 0;
                    b->zombies[j].hp = 0;
                }
            }
            mower->active = 0;
        } else {
            z->alive = 0;
            (*lives_lost)++;
        }
    }
}

static void update_effects(Board *b) {
    for (int i = 0; i < b->effect_count; i++) b->effects[i].timer--;
    for (int row = 0; row < BOARD_ROWS; row++)
        for (int col = 0; col < BOARD_COLS; col++)
            if (b->plant_flash_ticks[row][col] > 0)
                b->plant_flash_ticks[row][col]--;
}

static void compact_entities(Board *b) {
    int write = 0;
    for (int i = 0; i < b->projectile_count; i++) {
        if (!b->projectiles[i].alive) continue;
        if (write != i) b->projectiles[write] = b->projectiles[i];
        write++;
    }
    b->projectile_count = write;

    write = 0;
    for (int i = 0; i < b->zombie_count; i++) {
        if (!b->zombies[i].alive) continue;
        if (write != i) b->zombies[write] = b->zombies[i];
        write++;
    }
    b->zombie_count = write;

    write = 0;
    for (int i = 0; i < b->effect_count; i++) {
        if (b->effects[i].timer <= 0) continue;
        if (write != i) b->effects[write] = b->effects[i];
        write++;
    }
    b->effect_count = write;
}

void board_update(Board *b, int tick, int *sun, int *lives_lost) {
    *lives_lost = 0;
    update_plants(b, tick, sun);
    update_projectiles(b);
    summon_dancers(b);
    update_zombies(b, tick, lives_lost);
    update_effects(b);
    compact_entities(b);
}
