#include "crowd.h"

static int find_root(int parent[MAX_ZOMBIES], int index) {
    while (parent[index] != index) {
        parent[index] = parent[parent[index]];
        index = parent[index];
    }
    return index;
}

static int display_width(const Zombie *zombie,
                         const int widths[ZOMBIE_TYPE_COUNT], int angry_width) {
    int width = zombie->type == ZOMBIE_NEWSPAPER && zombie->armor_hp <= 0
              ? angry_width : widths[zombie->type];
    return width > 0 ? width : 1;
}

static int overlaps(const Zombie *a, const Zombie *b,
                    const int widths[ZOMBIE_TYPE_COUNT], int angry_width,
                    int cell_width) {
    if (a->row != b->row) return 0;
    if ((int)a->x == (int)b->x) return 1;

    int a_left = (int)(a->x * cell_width);
    int b_left = (int)(b->x * cell_width);
    int a_right = a_left + display_width(a, widths, angry_width);
    int b_right = b_left + display_width(b, widths, angry_width);
    return a_left < b_right && b_left < a_right;
}

static int display_tier(ZombieType type) {
    if (type == ZOMBIE_NORMAL) return 0;
    if (type == ZOMBIE_CONEHEAD || type == ZOMBIE_BUCKETHEAD
        || type == ZOMBIE_BACKUP || type == ZOMBIE_SCREENDOOR)
        return 1;
    return 2;
}

static int preferred(const Zombie *candidate, int candidate_index,
                     const Zombie *current, int current_index) {
    int candidate_tier = display_tier(candidate->type);
    int current_tier = display_tier(current->type);
    if (candidate_tier != current_tier) return candidate_tier > current_tier;

    int candidate_hp = candidate->hp + candidate->armor_hp;
    int current_hp = current->hp + current->armor_hp;
    if (candidate_hp != current_hp) return candidate_hp > current_hp;
    if (candidate->x != current->x) return candidate->x < current->x;
    return candidate_index < current_index;
}

int crowd_build(const Zombie *zombies, int zombie_count,
                const int widths[ZOMBIE_TYPE_COUNT], int angry_width,
                int cell_width, ZombieCrowd groups[MAX_ZOMBIES]) {
    int parent[MAX_ZOMBIES];
    int roots[MAX_ZOMBIES];
    int group_count = 0;
    if (zombie_count > MAX_ZOMBIES) zombie_count = MAX_ZOMBIES;

    for (int i = 0; i < zombie_count; i++) parent[i] = zombies[i].alive ? i : -1;
    for (int i = 0; i < zombie_count; i++) {
        if (parent[i] < 0) continue;
        for (int j = i + 1; j < zombie_count; j++) {
            if (parent[j] < 0 || !overlaps(&zombies[i], &zombies[j], widths,
                                           angry_width, cell_width))
                continue;
            int a = find_root(parent, i);
            int b = find_root(parent, j);
            if (a != b) parent[b] = a;
        }
    }

    for (int i = 0; i < zombie_count; i++) {
        if (parent[i] < 0) continue;
        int root = find_root(parent, i);
        int group = -1;
        for (int j = 0; j < group_count; j++) {
            if (roots[j] == root) {
                group = j;
                break;
            }
        }
        if (group < 0) {
            group = group_count++;
            roots[group] = root;
            groups[group] = (ZombieCrowd){
                .representative = i,
                .count = 0,
                .hit = 0
            };
        }

        groups[group].count++;
        if (zombies[i].hit_ticks > 0) groups[group].hit = 1;
        int current = groups[group].representative;
        if (preferred(&zombies[i], i, &zombies[current], current))
            groups[group].representative = i;
    }

    return group_count;
}
