#ifndef SOUND_H
#define SOUND_H

typedef enum {
    SFX_PLANT,        /* place plant: rising chirp */
    SFX_SHOVEL,       /* dig: falling square */
    SFX_DENY,         /* can't place: low buzz */
    SFX_HIT,          /* zombie hit: short blip */
    SFX_ZOMBIE_DIE,   /* zombie killed: descending square */
    SFX_EXPLODE,      /* cherry bomb: noise + rumble */
    SFX_MOWER,        /* lawn mower: sawtooth buzz */
    SFX_WAVE_CLEAR,   /* wave complete: C-E-G arpeggio */
    SFX_GAME_OVER,    /* loss: sad descending tone */
    SFX_WIN,          /* victory: C-E-G-C ascending */
    SFX_COUNT
} SfxType;

void sound_init(void);
void sound_play(SfxType type);
void sound_cleanup(void);

#endif
