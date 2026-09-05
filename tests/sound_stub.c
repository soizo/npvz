#include "../src/sound.h"

static int play_counts[SFX_COUNT];

void sound_play(SfxType type) {
    if (type >= 0 && type < SFX_COUNT) play_counts[type]++;
}

void sound_test_reset(void) {
    for (int i = 0; i < SFX_COUNT; i++) play_counts[i] = 0;
}

int sound_test_count(SfxType type) {
    return type >= 0 && type < SFX_COUNT ? play_counts[type] : 0;
}
