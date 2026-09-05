#ifndef CROWD_H
#define CROWD_H

#include "board.h"

typedef struct {
    int representative;
    int count;
    int hit;
} ZombieCrowd;

int crowd_build(const Zombie *zombies, int zombie_count,
                const int widths[ZOMBIE_TYPE_COUNT], int angry_width,
                int cell_width, ZombieCrowd groups[MAX_ZOMBIES]);

#endif
