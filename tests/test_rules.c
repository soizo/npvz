#include <assert.h>
#include <ncurses.h>
#include <stdio.h>

#include "../src/game.h"
#include "../src/crowd.h"
#include "../src/sound.h"

void sound_test_reset(void);
int sound_test_count(SfxType type);

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

static void test_placement_feedback_contract(void) {
    Game g;

    game_init(&g);
    g.state = STATE_PLAYING;

    game_handle_input(&g, ' ');
    assert(g.feedback == FEEDBACK_NO_PLANT);

    g.selected_plant = PLANT_PEASHOOTER;
    g.sun = 0;
    game_handle_input(&g, ' ');
    assert(g.feedback == FEEDBACK_NEED_SUN);

    g.sun = PLANT_DEFS[PLANT_PEASHOOTER].cost;
    g.card_cooldowns[PLANT_PEASHOOTER] = 5;
    game_handle_input(&g, ' ');
    assert(g.feedback == FEEDBACK_COOLDOWN);

    g.card_cooldowns[PLANT_PEASHOOTER] = 0;
    plant_init(&g.board.cells[g.cursor_row][g.cursor_col],
               PLANT_SUNFLOWER, g.cursor_row, g.cursor_col);
    game_handle_input(&g, ' ');
    assert(g.feedback == FEEDBACK_OCCUPIED);

    g.board.cells[g.cursor_row][g.cursor_col].type = PLANT_NONE;
    g.board.cells[g.cursor_row][g.cursor_col].hp = 0;
    game_handle_input(&g, ' ');
    assert(g.feedback == FEEDBACK_PLANTED);
    assert(g.feedback_ticks > 0);
}

static void test_shovel_and_deck_feedback_contract(void) {
    Game g;

    game_init(&g);
    g.state = STATE_PLAYING;

    game_handle_input(&g, '0');
    assert(g.shovel_mode == 1);
    assert(g.feedback == FEEDBACK_SHOVEL_ON);
    game_handle_input(&g, ' ');
    assert(g.feedback == FEEDBACK_NOTHING_TO_REMOVE);

    plant_init(&g.board.cells[g.cursor_row][g.cursor_col],
               PLANT_WALLNUT, g.cursor_row, g.cursor_col);
    game_handle_input(&g, ' ');
    assert(g.feedback == FEEDBACK_REMOVED);

    game_init(&g);
    game_handle_input(&g, '\n');
    game_handle_input(&g, 'g');
    assert(g.state == STATE_CARD_SELECT);
    assert(g.feedback == FEEDBACK_EMPTY_DECK);

    g.max_slots = 6;
    g.deck_count = 6;
    for (int i = 0; i < 6; i++) g.deck[i] = (PlantType)(i + 1);
    g.card_cursor = 6;
    game_handle_input(&g, '\n');
    assert(g.feedback == FEEDBACK_DECK_FULL);
}

static void test_help_freezes_and_restores_play(void) {
    Game g;

    game_init(&g);
    g.state = STATE_PLAYING;
    int tick = g.tick;
    int col = g.cursor_col;

    game_handle_input(&g, '?');
    assert(g.help_visible == 1);
    game_update(&g);
    assert(g.tick == tick);

    game_handle_input(&g, KEY_RIGHT);
    assert(g.cursor_col == col);

    game_handle_input(&g, '?');
    assert(g.help_visible == 0);
    game_update(&g);
    assert(g.tick == tick + 1);
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
    assert(b.effect_count == 1);
    assert(b.effects[0].kind == COMBAT_EFFECT_DEATH);
    assert(b.plant_flash_ticks[2][4] > 0);
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
    assert(b.plant_flash_ticks[0][0] > 0);
}

static void test_sunflower_and_snowpea_actions_flash(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    board_place_plant(&b, PLANT_SUNFLOWER, 0, 0);
    b.cells[0][0].sun_timer = 0;
    board_place_plant(&b, PLANT_SNOWPEA, 1, 1);
    b.cells[1][1].shoot_timer = 0;
    spawn_stationary(&b, ZOMBIE_NORMAL, 1, 8.0f);
    board_update(&b, 1, &sun, &lost);

    assert(sun == 25);
    assert(b.plant_flash_ticks[0][0] > 0);
    assert(b.plant_flash_ticks[1][1] > 0);
    for (int tick = 2; tick <= 10; tick++) board_update(&b, tick, &sun, &lost);
    assert(b.plant_flash_ticks[0][0] == 0);
    assert(b.plant_flash_ticks[1][1] == 0);
}

static void test_each_zombie_bite_plays_crunch(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    board_place_plant(&b, PLANT_WALLNUT, 2, 3);
    spawn_stationary(&b, ZOMBIE_NORMAL, 2, 3.0f);
    b.zombies[0].eat_timer = 1;
    sound_test_reset();

    board_update(&b, 1, &sun, &lost);
    assert(sound_test_count(SFX_BITE) == 1);
    board_update(&b, 2, &sun, &lost);
    assert(sound_test_count(SFX_BITE) == 1);
    b.zombies[0].eat_timer = 1;
    board_update(&b, 3, &sun, &lost);
    assert(sound_test_count(SFX_BITE) == 2);
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
    for (int i = 0; i < MAX_COMBAT_EFFECTS; i++) b.effects[i].timer = 2;
    b.effect_count = MAX_COMBAT_EFFECTS;
    projectile_init(&b.projectiles[0], PROJ_PEA, 0, 4);
    b.projectiles[0].speed = 0.0f;
    b.projectile_count = 1;
    spawn_stationary(&b, ZOMBIE_NORMAL, 0, 4.0f);
    b.zombies[0].hp = 1;
    board_update(&b, 1, &sun, &lost);
    assert(b.effect_count == MAX_COMBAT_EFFECTS);
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
    assert(b.effect_count == 1);
    assert(b.effects[0].kind == COMBAT_EFFECT_BLAST);
    assert(b.plant_flash_ticks[2][4] > 0);
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
    assert(b.plant_flash_ticks[2][4] > 0);
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
    assert(b.effect_count == 1);
    assert(b.effects[0].kind == COMBAT_EFFECT_BLAST);
    assert(b.plant_flash_ticks[2][4] > 0);
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

static void fire_stationary_projectile(Board *b, int row, int col) {
    projectile_init(&b->projectiles[0], PROJ_PEA, row, col);
    b->projectiles[0].speed = 0.0f;
    b->projectile_count = 1;
}

static void test_projectile_hit_and_death_effects_are_exclusive(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    spawn_stationary(&b, ZOMBIE_NORMAL, 0, 4.0f);
    fire_stationary_projectile(&b, 0, 4);
    board_update(&b, 1, &sun, &lost);
    assert(b.zombie_count == 1);
    assert(b.zombies[0].hit_ticks > 0);
    assert(b.effect_count == 0);

    board_init(&b);
    spawn_stationary(&b, ZOMBIE_NORMAL, 0, 4.0f);
    b.zombies[0].hp = 1;
    fire_stationary_projectile(&b, 0, 4);
    board_update(&b, 1, &sun, &lost);
    assert(b.zombie_count == 0);
    assert(b.effect_count == 1);
    assert(b.effects[0].kind == COMBAT_EFFECT_DEATH);
}

static void test_kill_effects_match_attack_type(void) {
    Board b;
    int sun = 0;
    int lost = 0;

    board_init(&b);
    board_place_plant(&b, PLANT_CHERRYBOMB, 2, 4);
    b.cells[2][4].explode_timer = 0;
    spawn_stationary(&b, ZOMBIE_NORMAL, 2, 4.0f);
    board_update(&b, 1, &sun, &lost);
    assert(b.effect_count == 1);
    assert(b.effects[0].kind == COMBAT_EFFECT_BLAST);
    assert(b.plant_flash_ticks[2][4] > 0);

    board_init(&b);
    board_place_plant(&b, PLANT_CHOMPER, 2, 4);
    spawn_stationary(&b, ZOMBIE_NORMAL, 2, 5.0f);
    board_update(&b, 1, &sun, &lost);
    assert(b.effect_count == 1);
    assert(b.effects[0].kind == COMBAT_EFFECT_DEATH);
    assert(b.plant_flash_ticks[2][4] > 0);

    for (int tick = 2; tick <= 5; tick++)
        board_update(&b, tick, &sun, &lost);
    assert(b.effect_count == 0);
}

static void fill_zombie_widths(int widths[ZOMBIE_TYPE_COUNT]) {
    for (int i = 0; i < ZOMBIE_TYPE_COUNT; i++) widths[i] = 2;
}

static void test_crowd_groups_same_cell_and_half_overlaps(void) {
    Zombie zombies[4];
    ZombieCrowd groups[MAX_ZOMBIES];
    int widths[ZOMBIE_TYPE_COUNT];
    fill_zombie_widths(widths);

    zombie_init(&zombies[0], ZOMBIE_NORMAL, 0);
    zombies[0].x = 4.0f;
    zombie_init(&zombies[1], ZOMBIE_CONEHEAD, 0);
    zombies[1].x = 4.5f;
    zombie_init(&zombies[2], ZOMBIE_FOOTBALL, 0);
    zombies[2].x = 4.75f;
    zombie_init(&zombies[3], ZOMBIE_NORMAL, 0);
    zombies[3].x = 5.0f;
    zombies[1].hit_ticks = 2;

    int count = crowd_build(zombies, 4, widths, 2, 4, groups);
    assert(count == 1);
    assert(groups[0].count == 4);
    assert(groups[0].representative == 2);
    assert(groups[0].hit == 1);
}

static void test_crowd_representative_priority(void) {
    Zombie zombies[3];
    ZombieCrowd groups[MAX_ZOMBIES];
    int widths[ZOMBIE_TYPE_COUNT];
    fill_zombie_widths(widths);

    zombie_init(&zombies[0], ZOMBIE_NORMAL, 0);
    zombie_init(&zombies[1], ZOMBIE_BUCKETHEAD, 0);
    zombie_init(&zombies[2], ZOMBIE_DANCER, 0);
    for (int i = 0; i < 3; i++) zombies[i].x = 4.0f;
    zombies[0].hp = 1000;
    zombies[1].hp = 500;
    zombies[1].armor_hp = 500;
    zombies[2].hp = 1;
    assert(crowd_build(zombies, 3, widths, 2, 4, groups) == 1);
    assert(groups[0].representative == 2);

    zombie_init(&zombies[0], ZOMBIE_BACKUP, 0);
    zombie_init(&zombies[1], ZOMBIE_SCREENDOOR, 0);
    zombies[0].x = zombies[1].x = 4.0f;
    zombies[0].hp = 500;
    zombies[1].hp = 100;
    zombies[1].armor_hp = 0;
    assert(crowd_build(zombies, 2, widths, 2, 4, groups) == 1);
    assert(groups[0].representative == 0);

    zombie_init(&zombies[0], ZOMBIE_DANCER, 0);
    zombie_init(&zombies[1], ZOMBIE_FOOTBALL, 0);
    zombies[0].x = 3.75f;
    zombies[1].x = 4.0f;
    zombies[0].hp = zombies[1].hp = 100;
    zombies[0].armor_hp = zombies[1].armor_hp = 0;
    assert(crowd_build(zombies, 2, widths, 2, 4, groups) == 1);
    assert(groups[0].representative == 0);
}

int main(void) {
    test_exit_is_not_a_loss_state();
    test_card_selection_contract();
    test_end_screen_navigation();
    test_placement_contract();
    test_placement_feedback_contract();
    test_shovel_and_deck_feedback_contract();
    test_help_freezes_and_restores_play();
    test_mode_completion_contract();
    test_squash_targets_closest_zombie();
    test_pole_vaulter_jumps_once();
    test_repeater_fires_two_projectiles();
    test_sunflower_and_snowpea_actions_flash();
    test_each_zombie_bite_plays_crunch();
    test_dancer_summons_once();
    test_mower_stops_a_breach();
    test_breach_without_mower_loses();
    test_entity_capacity_is_bounded();
    test_jalapeno_only_clears_its_row();
    test_chomper_eats_one_then_digests();
    test_potato_mine_requires_arming();
    test_armor_break_rules();
    test_projectile_hit_and_death_effects_are_exclusive();
    test_kill_effects_match_attack_type();
    test_crowd_groups_same_cell_and_half_overlaps();
    test_crowd_representative_priority();
    puts("rule tests passed");
    return 0;
}
