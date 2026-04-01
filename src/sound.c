#include "sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define SR 22050  /* sample rate */

static char sfx_paths[SFX_COUNT][64];

/* ---- WAV writer ---- */

static void write_wav(const char *path, const int16_t *samples, int count) {
    FILE *f = fopen(path, "wb");
    if (!f) return;

    int32_t data_size = count * 2;
    int32_t riff_size = 36 + data_size;
    int32_t fmt_size  = 16;
    int16_t pcm       = 1;
    int16_t mono      = 1;
    int32_t sr        = SR;
    int32_t byte_rate = SR * 2;
    int16_t align     = 2;
    int16_t bits      = 16;

    fwrite("RIFF", 1, 4, f);
    fwrite(&riff_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fwrite(&fmt_size, 4, 1, f);
    fwrite(&pcm, 2, 1, f);
    fwrite(&mono, 2, 1, f);
    fwrite(&sr, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&align, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);
    fwrite(samples, 2, count, f);

    fclose(f);
}

/* ---- waveform generators ---- */

static float env_adsr(int i, int n, int attack, int release) {
    if (i < attack) return (float)i / attack;
    if (i > n - release) return (float)(n - i) / release;
    return 1.0f;
}

/* sine/square chirp from f0 to f1 */
static void gen_chirp(int16_t *buf, int n, float f0, float f1,
                      float vol, int square) {
    float phase = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        float freq = f0 + (f1 - f0) * t;
        phase += 2.0f * (float)M_PI * freq / SR;
        float val = square ? (sinf(phase) > 0 ? 1.0f : -1.0f) : sinf(phase);
        float e = env_adsr(i, n, 150, 400);
        buf[i] = (int16_t)(val * vol * e * 32767);
    }
}

/* noise burst with decay */
static void gen_noise(int16_t *buf, int n, float vol) {
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        float e = (1.0f - t);
        float val = ((float)rand() / RAND_MAX * 2.0f - 1.0f);
        buf[i] = (int16_t)(val * vol * e * e * 32767);
    }
}

/* sawtooth wave */
static void gen_saw(int16_t *buf, int n, float freq, float vol) {
    float phase = 0;
    for (int i = 0; i < n; i++) {
        phase += freq / SR;
        if (phase > 1.0f) phase -= 1.0f;
        float val = 2.0f * phase - 1.0f;
        float e = env_adsr(i, n, 200, 400);
        buf[i] = (int16_t)(val * vol * e * 32767);
    }
}

/* arpeggio: play a sequence of notes (sine) */
static void gen_arpeggio(int16_t *buf, int n, const float *notes,
                         int note_count, float vol) {
    float phase = 0;
    int seg = n / note_count;
    for (int i = 0; i < n; i++) {
        int idx = i / seg;
        if (idx >= note_count) idx = note_count - 1;
        phase += 2.0f * (float)M_PI * notes[idx] / SR;
        float local = (float)(i % seg);
        float e = 1.0f;
        if (local < 80) e = local / 80;
        if (local > seg - 200) e = (seg - local) / 200;
        buf[i] = (int16_t)(sinf(phase) * vol * e * 32767);
    }
}

/* ---- init / play / cleanup ---- */

void sound_init(void) {
    int16_t buf[SR]; /* max 1 second */
    int n;

    for (int i = 0; i < SFX_COUNT; i++)
        snprintf(sfx_paths[i], sizeof(sfx_paths[i]),
                 "/tmp/npvz_sfx_%d.wav", i);

    /* SFX_PLANT: rising sine chirp 400→900Hz, 80ms */
    n = (int)(0.08f * SR);
    gen_chirp(buf, n, 400, 900, 0.30f, 0);
    write_wav(sfx_paths[SFX_PLANT], buf, n);

    /* SFX_SHOVEL: falling square 500→200Hz, 70ms */
    n = (int)(0.07f * SR);
    gen_chirp(buf, n, 500, 200, 0.25f, 1);
    write_wav(sfx_paths[SFX_SHOVEL], buf, n);

    /* SFX_DENY: short low buzz 100Hz square, 60ms */
    n = (int)(0.06f * SR);
    gen_chirp(buf, n, 100, 90, 0.20f, 1);
    write_wav(sfx_paths[SFX_DENY], buf, n);

    /* SFX_HIT: very short square blip 350Hz, 35ms */
    n = (int)(0.035f * SR);
    gen_chirp(buf, n, 350, 300, 0.20f, 1);
    write_wav(sfx_paths[SFX_HIT], buf, n);

    /* SFX_ZOMBIE_DIE: descending square 600→100Hz, 120ms */
    n = (int)(0.12f * SR);
    gen_chirp(buf, n, 600, 100, 0.25f, 1);
    write_wav(sfx_paths[SFX_ZOMBIE_DIE], buf, n);

    /* SFX_EXPLODE: noise + low rumble, 200ms */
    n = (int)(0.2f * SR);
    gen_noise(buf, n, 0.35f);
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        buf[i] += (int16_t)(sinf(2.0f * (float)M_PI * 60 * i / SR)
                            * 0.30f * (1.0f - t) * 32767);
    }
    write_wav(sfx_paths[SFX_EXPLODE], buf, n);

    /* SFX_MOWER: sawtooth buzz 120Hz, 250ms */
    n = (int)(0.25f * SR);
    gen_saw(buf, n, 120, 0.25f);
    write_wav(sfx_paths[SFX_MOWER], buf, n);

    /* SFX_WAVE_CLEAR: C5-E5-G5 arpeggio, 300ms */
    n = (int)(0.3f * SR);
    {
        float notes[] = {523.25f, 659.25f, 783.99f};
        gen_arpeggio(buf, n, notes, 3, 0.30f);
    }
    write_wav(sfx_paths[SFX_WAVE_CLEAR], buf, n);

    /* SFX_GAME_OVER: slow descending sine 400→80Hz, 500ms */
    n = (int)(0.5f * SR);
    gen_chirp(buf, n, 400, 80, 0.30f, 0);
    write_wav(sfx_paths[SFX_GAME_OVER], buf, n);

    /* SFX_WIN: C5-E5-G5-C6 ascending, 400ms */
    n = (int)(0.4f * SR);
    {
        float notes[] = {523.25f, 659.25f, 783.99f, 1046.50f};
        gen_arpeggio(buf, n, notes, 4, 0.30f);
    }
    write_wav(sfx_paths[SFX_WIN], buf, n);
}

void sound_play(SfxType type) {
    if (type < 0 || type >= SFX_COUNT) return;

    /* reap finished children */
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    pid_t pid = fork();
    if (pid == 0) {
        /* child: silence stdout/stderr, detach from terminal input */
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp("afplay", "afplay", sfx_paths[type], (char *)NULL);
        _exit(1);
    }
    /* parent continues immediately */
}

void sound_cleanup(void) {
    for (int i = 0; i < SFX_COUNT; i++)
        unlink(sfx_paths[i]);
    /* reap any remaining children */
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}
