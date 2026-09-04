#include <assert.h>
#include <ncurses.h>
#include <stdio.h>

#include "../src/game.h"

static void test_exit_is_not_a_loss_state(void) {
    Game g;

    game_init(&g);
    assert(game_handle_input(&g, 'q') == 1);
    assert(g.state == STATE_MENU);

    game_init(&g);
    game_handle_input(&g, '\n');
    assert(game_handle_input(&g, 'q') == 0);
    assert(g.state == STATE_MENU);

    g.state = STATE_PLAYING;
    assert(game_handle_input(&g, 'Q') == 1);
    g.state = STATE_PAUSED;
    assert(game_handle_input(&g, 'q') == 1);
    g.state = STATE_WON;
    assert(game_handle_input(&g, 'q') == 1);
}

static void test_card_selection_contract(void) {
    Game g;

    game_init(&g);
    game_handle_input(&g, '\n');
    assert(g.state == STATE_CARD_SELECT);

    for (int i = 0; i < 12; i++) game_handle_input(&g, KEY_RIGHT);
    assert(g.max_slots == 9);
    for (int i = 0; i < 12; i++) game_handle_input(&g, KEY_LEFT);
    assert(g.max_slots == 6);

    game_handle_input(&g, '\n');
    assert(g.deck_count == 1);
    game_handle_input(&g, '\n');
    assert(g.deck_count == 0);
    game_handle_input(&g, 'g');
    assert(g.state == STATE_CARD_SELECT);

    game_handle_input(&g, '\n');
    game_handle_input(&g, 'g');
    assert(g.state == STATE_PLAYING);
    assert(g.sun == 50);
    assert(g.wave == 1);
}

static void test_end_screen_navigation(void) {
    Game g;

    game_init(&g);
    g.state = STATE_WON;
    game_handle_input(&g, 'r');
    assert(g.state == STATE_CARD_SELECT);

    g.state = STATE_LOST;
    game_handle_input(&g, 'm');
    assert(g.state == STATE_MENU);
}

static void test_placement_contract(void) {
    Game g;

    game_init(&g);
    g.state = STATE_PLAYING;
    g.selected_plant = PLANT_SUNFLOWER;
    g.cursor_row = 0;
    g.cursor_col = 0;
    game_handle_input(&g, ' ');

    assert(g.board.cells[0][0].type == PLANT_SUNFLOWER);
    assert(g.sun == 0);
    assert(g.card_cooldowns[PLANT_SUNFLOWER] == 30);
}

static void test_mode_completion_contract(void) {
    Game g;

    game_init(&g);
    g.state = STATE_PLAYING;
    g.wave = 5;
    g.zombies_remaining = 0;
    game_update(&g);
    assert(g.state == STATE_WON);

    game_init(&g);
    g.state = STATE_PLAYING;
    g.mode = MODE_ENDLESS;
    g.wave = 5;
    g.zombies_remaining = 0;
    game_update(&g);
    assert(g.state == STATE_PLAYING);
    assert(g.wave == 6);
    assert(g.zombies_remaining == 23);
    assert(g.spawn_timer == 90);
}

static void spawn_stationary(Board *b, ZombieType type, int row, float x) {
    board_spawn_zombie(b, type, row);
    Zombie *z = &b->zombies[b->zombie_count - 1];
    z->x = x;
    z->speed = 0.0f;
}

static void test_squash_targets_closest_zombie(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    board_place_plant(&b, PLANT_SQUASH, 2, 4);
    b.cells[2][4].explode_timer = 0;
    spawn_stationary(&b, ZOMBIE_NORMAL, 2, 1.0f);
    spawn_stationary(&b, ZOMBIE_NORMAL, 2, 5.0f);

    board_update(&b, 1, &sun, &lost);

    assert(b.zombie_count == 1);
    assert(b.zombies[0].x == 1.0f);
}

static void test_pole_vaulter_jumps_once(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    board_place_plant(&b, PLANT_WALLNUT, 2, 3);
    board_place_plant(&b, PLANT_WALLNUT, 2, 2);
    spawn_stationary(&b, ZOMBIE_POLEVAULTER, 2, 3.2f);

    board_update(&b, 1, &sun, &lost);
    board_update(&b, 2, &sun, &lost);

    assert(b.zombies[0].x == 2.5f);
    assert(b.zombies[0].type == ZOMBIE_NORMAL);
    assert(b.zombies[0].eating == 1);
    assert(b.zombies[0].has_summoned == 0);
}

static void test_repeater_fires_two_projectiles(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    board_place_plant(&b, PLANT_REPEATER, 0, 0);
    b.cells[0][0].shoot_timer = 0;
    spawn_stationary(&b, ZOMBIE_NORMAL, 0, 8.0f);
    board_update(&b, 1, &sun, &lost);

    assert(b.projectile_count == 2);
}

static void test_dancer_summons_once(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    spawn_stationary(&b, ZOMBIE_DANCER, 2, 5.0f);
    b.zombies[0].summon_timer = 1;
    board_update(&b, 1, &sun, &lost);
    assert(b.zombie_count == 3);
    assert(b.zombies[1].type == ZOMBIE_BACKUP);
    assert(b.zombies[1].row == 1);
    assert(b.zombies[2].type == ZOMBIE_BACKUP);
    assert(b.zombies[2].row == 3);

    board_update(&b, 2, &sun, &lost);
    assert(b.zombie_count == 3);
}

static void test_mower_stops_a_breach(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    spawn_stationary(&b, ZOMBIE_NORMAL, 0, -0.1f);
    board_update(&b, 1, &sun, &lost);
    assert(b.zombie_count == 0);
    assert(lost == 0);
    assert(b.mowers[0].active == 0);
}

static void test_breach_without_mower_loses(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    b.mowers[0].active = 0;
    spawn_stationary(&b, ZOMBIE_NORMAL, 0, -0.1f);
    board_update(&b, 1, &sun, &lost);
    assert(b.zombie_count == 0);
    assert(lost == 1);
}

static void test_entity_capacity_is_bounded(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    for (int i = 0; i <= MAX_ZOMBIES; i++)
        board_spawn_zombie(&b, ZOMBIE_NORMAL, i % BOARD_ROWS);
    assert(b.zombie_count == MAX_ZOMBIES);

    board_init(&b);
    for (int i = 0; i < MAX_PROJECTILES; i++)
        projectile_init(&b.projectiles[i], PROJ_PEA, 0, 0);
    b.projectile_count = MAX_PROJECTILES;
    board_place_plant(&b, PLANT_REPEATER, 1, 0);
    b.cells[1][0].shoot_timer = 0;
    spawn_stationary(&b, ZOMBIE_NORMAL, 1, 8.0f);
    board_update(&b, 1, &sun, &lost);
    assert(b.projectile_count == MAX_PROJECTILES);

    board_init(&b);
    for (int i = 0; i < MAX_VFX; i++) b.vfx[i].timer = 2;
    b.vfx_count = MAX_VFX;
    projectile_init(&b.projectiles[0], PROJ_PEA, 0, 4);
    b.projectiles[0].speed = 0.0f;
    b.projectile_count = 1;
    spawn_stationary(&b, ZOMBIE_NORMAL, 0, 4.0f);
    board_update(&b, 1, &sun, &lost);
    assert(b.vfx_count == MAX_VFX);
}

static void test_jalapeno_only_clears_its_row(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    board_place_plant(&b, PLANT_JALAPENO, 2, 4);
    b.cells[2][4].explode_timer = 0;
    spawn_stationary(&b, ZOMBIE_NORMAL, 2, 7.0f);
    spawn_stationary(&b, ZOMBIE_NORMAL, 1, 7.0f);
    board_update(&b, 1, &sun, &lost);
    assert(b.zombie_count == 1);
    assert(b.zombies[0].row == 1);
}

static void test_chomper_eats_one_then_digests(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    board_place_plant(&b, PLANT_CHOMPER, 2, 4);
    spawn_stationary(&b, ZOMBIE_NORMAL, 2, 5.0f);
    board_update(&b, 1, &sun, &lost);
    assert(b.zombie_count == 0);
    assert(b.cells[2][4].chomp_timer == 180);
}

static void test_potato_mine_requires_arming(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    board_place_plant(&b, PLANT_POTATOMINE, 2, 4);
    spawn_stationary(&b, ZOMBIE_NORMAL, 2, 4.0f);
    board_update(&b, 1, &sun, &lost);
    assert(b.zombie_count == 1);

    b.cells[2][4].explode_timer = 0;
    board_update(&b, 2, &sun, &lost);
    assert(b.zombie_count == 0);
}

static void test_armor_break_rules(void) {
    Zombie z;

    zombie_init(&z, ZOMBIE_NEWSPAPER, 0);
    zombie_take_damage(&z, 100);
    assert(z.armor_hp == 0);
    assert(z.hp == 80);
    assert(z.speed == 0.008f);

    zombie_init(&z, ZOMBIE_FOOTBALL, 0);
    zombie_take_damage(&z, 100);
    assert(z.armor_hp == 100);
    assert(z.hp == 200);
    zombie_take_damage(&z, 150);
    assert(z.armor_hp == 0);
    assert(z.hp == 150);

    zombie_init(&z, ZOMBIE_SCREENDOOR, 0);
    zombie_take_damage(&z, 100);
    assert(z.armor_hp == 150);
    assert(z.hp == 100);
}

int main(void) {
    test_exit_is_not_a_loss_state();
    test_card_selection_contract();
    test_end_screen_navigation();
    test_placement_contract();
    test_mode_completion_contract();
    test_squash_targets_closest_zombie();
    test_pole_vaulter_jumps_once();
    test_repeater_fires_two_projectiles();
    test_dancer_summons_once();
    test_mower_stops_a_breach();
    test_breach_without_mower_loses();
    test_entity_capacity_is_bounded();
    test_jalapeno_only_clears_its_row();
    test_chomper_eats_one_then_digests();
    test_potato_mine_requires_arming();
    test_armor_break_rules();
    puts("rule tests passed");
    return 0;
}
