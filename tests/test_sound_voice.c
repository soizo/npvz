#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/sound_voice.h"

static void test_repeat_coalescing(void) {
    SoundVoicePool pool;

    sound_voice_pool_init(&pool, 4, 4, 0);
    assert(sound_voice_acquire(&pool, SFX_HIT, 1000) == 0);
    assert(sound_voice_acquire(&pool, SFX_HIT, 1049) == -1);
    assert(sound_voice_acquire(&pool, SFX_HIT, 1050) == 1);
}

static void test_capacity_and_priority(void) {
    SoundVoicePool pool;

    sound_voice_pool_init(&pool, 4, 3, 0);
    assert(sound_voice_acquire(&pool, SFX_HIT, 1000) == 0);
    assert(sound_voice_acquire(&pool, SFX_BITE, 1000) == 1);
    assert(sound_voice_acquire(&pool, SFX_PLANT, 1000) == 2);
    assert(sound_voice_acquire(&pool, SFX_DENY, 1000) == -1);
    assert(sound_voice_acquire(&pool, SFX_ZOMBIE_DIE, 1000) == 3);
    assert(sound_voice_acquire(&pool, SFX_EXPLODE, 1000) == 0);
    assert(pool.voices[0].type == SFX_EXPLODE);
}

static void test_slot_reuse_delay(void) {
    SoundVoicePool pool;

    sound_voice_pool_init(&pool, 1, 1, 100);
    assert(sound_voice_acquire(&pool, SFX_HIT, 1000) == 0);
    sound_voice_release(&pool, 0, 1000);
    assert(sound_voice_acquire(&pool, SFX_BITE, 1099) == -1);
    assert(sound_voice_acquire(&pool, SFX_BITE, 1100) == 0);
}

static void test_mixing_clamps_and_releases(void) {
    static const int16_t first[] = {30000, -30000};
    static const int16_t second[] = {10000, -10000};
    SoundSample bank[SFX_COUNT] = {0};
    SoundVoicePool pool;
    int16_t output[2];

    bank[SFX_HIT] = (SoundSample){first, 2};
    bank[SFX_BITE] = (SoundSample){second, 2};
    sound_voice_pool_init(&pool, 2, 2, 0);
    assert(sound_voice_acquire(&pool, SFX_HIT, 1000) == 0);
    assert(sound_voice_acquire(&pool, SFX_BITE, 1000) == 1);

    sound_voice_mix(&pool, bank, output, 2, 1000);

    assert(output[0] == 32767);
    assert(output[1] == -32768);
    assert(!pool.voices[0].active);
    assert(!pool.voices[1].active);
}

int main(void) {
    test_repeat_coalescing();
    test_capacity_and_priority();
    test_slot_reuse_delay();
    test_mixing_clamps_and_releases();
    puts("sound voice tests passed");
    return 0;
}
