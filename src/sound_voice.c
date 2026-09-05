#include "sound_voice.h"
#include <limits.h>
#include <string.h>

static int priority(SfxType type) {
    switch (type) {
    case SFX_WIN:
    case SFX_GAME_OVER:
    case SFX_WAVE_CLEAR:
    case SFX_MOWER:
    case SFX_EXPLODE:
        return 2;
    case SFX_ZOMBIE_DIE:
        return 1;
    default:
        return 0;
    }
}

void sound_voice_pool_init(SoundVoicePool *pool, int capacity,
                           int ordinary_limit, uint64_t reuse_delay_ms) {
    memset(pool, 0, sizeof(*pool));
    pool->capacity = capacity > SOUND_MAX_VOICES ? SOUND_MAX_VOICES : capacity;
    pool->ordinary_limit = ordinary_limit > pool->capacity
                               ? pool->capacity
                               : ordinary_limit;
    pool->reuse_delay_ms = reuse_delay_ms;
}

int sound_voice_acquire(SoundVoicePool *pool, SfxType type, uint64_t now_ms) {
    if (type < 0 || type >= SFX_COUNT) return -1;
    if (pool->has_started[type] && now_ms >= pool->last_started[type] &&
        now_ms - pool->last_started[type] < SOUND_REPEAT_MS)
        return -1;

    int request_priority = priority(type);
    int limit = request_priority ? pool->capacity : pool->ordinary_limit;
    int slot = -1;

    for (int i = 0; i < limit; i++) {
        if (!pool->voices[i].active && now_ms >= pool->voices[i].reusable_ms) {
            slot = i;
            break;
        }
    }

    if (slot < 0 && request_priority) {
        uint64_t oldest = UINT64_MAX;
        for (int i = 0; i < pool->capacity; i++) {
            SoundVoice *voice = &pool->voices[i];
            if (voice->active && priority(voice->type) < request_priority &&
                voice->started_ms < oldest) {
                slot = i;
                oldest = voice->started_ms;
            }
        }
    }

    if (slot < 0) return -1;

    pool->voices[slot] = (SoundVoice){
        .active = 1,
        .type = type,
        .started_ms = now_ms,
        .reusable_ms = pool->voices[slot].reusable_ms,
    };
    pool->last_started[type] = now_ms;
    pool->has_started[type] = 1;
    return slot;
}

void sound_voice_release(SoundVoicePool *pool, int slot, uint64_t now_ms) {
    if (slot < 0 || slot >= pool->capacity) return;
    pool->voices[slot].active = 0;
    pool->voices[slot].cursor = 0;
    pool->voices[slot].reusable_ms = now_ms + pool->reuse_delay_ms;
}

void sound_voice_mix(SoundVoicePool *pool,
                     const SoundSample bank[SFX_COUNT],
                     int16_t *output, size_t frames, uint64_t now_ms) {
    for (size_t frame = 0; frame < frames; frame++) {
        int32_t mixed = 0;
        for (int slot = 0; slot < pool->capacity; slot++) {
            SoundVoice *voice = &pool->voices[slot];
            if (!voice->active) continue;
            const SoundSample *sample = &bank[voice->type];
            if (voice->cursor >= sample->count) {
                sound_voice_release(pool, slot, now_ms);
                continue;
            }
            mixed += sample->samples[voice->cursor++];
            if (voice->cursor == sample->count)
                sound_voice_release(pool, slot, now_ms);
        }
        if (mixed > INT16_MAX) mixed = INT16_MAX;
        if (mixed < INT16_MIN) mixed = INT16_MIN;
        output[frame] = (int16_t)mixed;
    }
}
