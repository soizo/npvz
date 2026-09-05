#ifndef SOUND_VOICE_H
#define SOUND_VOICE_H

#include "sound.h"
#include <stddef.h>
#include <stdint.h>

#define SOUND_MAX_VOICES 8
#define SOUND_REPEAT_MS 50

typedef struct {
    const int16_t *samples;
    size_t count;
} SoundSample;

typedef struct {
    int active;
    SfxType type;
    size_t cursor;
    uint64_t started_ms;
    uint64_t reusable_ms;
} SoundVoice;

typedef struct {
    SoundVoice voices[SOUND_MAX_VOICES];
    uint64_t last_started[SFX_COUNT];
    unsigned char has_started[SFX_COUNT];
    int capacity;
    int ordinary_limit;
    uint64_t reuse_delay_ms;
} SoundVoicePool;

void sound_voice_pool_init(SoundVoicePool *pool, int capacity,
                           int ordinary_limit, uint64_t reuse_delay_ms);
int sound_voice_acquire(SoundVoicePool *pool, SfxType type, uint64_t now_ms);
void sound_voice_release(SoundVoicePool *pool, int slot, uint64_t now_ms);
void sound_voice_mix(SoundVoicePool *pool,
                     const SoundSample bank[SFX_COUNT],
                     int16_t *output, size_t frames, uint64_t now_ms);

#endif
