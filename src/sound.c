#include "sound.h"
#include "sound_voice.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(NPVZ_SOUND_SDL)
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#elif defined(NPVZ_SOUND_AUDIOQUEUE)
#include <AudioToolbox/AudioQueue.h>
#include <pthread.h>
#include <stdatomic.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define SR 22050
#define MAX_SAMPLES (SR / 2)

static int16_t sample_storage[SFX_COUNT][MAX_SAMPLES];
static SoundSample sample_bank[SFX_COUNT];

static float env_adsr(int i, int n, int attack, int release) {
    if (i < attack) return (float)i / attack;
    if (i > n - release) return (float)(n - i) / release;
    return 1.0f;
}

static void gen_chirp(int16_t *buf, int n, float f0, float f1,
                      float volume, int square) {
    float phase = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        float frequency = f0 + (f1 - f0) * t;
        phase += 2.0f * (float)M_PI * frequency / SR;
        float value = square ? (sinf(phase) > 0 ? 1.0f : -1.0f)
                             : sinf(phase);
        float envelope = env_adsr(i, n, 150, 400);
        buf[i] = (int16_t)(value * volume * envelope * 32767);
    }
}

static void gen_noise(int16_t *buf, int n, float volume) {
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        float envelope = 1.0f - t;
        float value = (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
        buf[i] = (int16_t)(value * volume * envelope * envelope * 32767);
    }
}

static void gen_saw(int16_t *buf, int n, float frequency, float volume) {
    float phase = 0;
    for (int i = 0; i < n; i++) {
        phase += frequency / SR;
        if (phase > 1.0f) phase -= 1.0f;
        float value = 2.0f * phase - 1.0f;
        float envelope = env_adsr(i, n, 200, 400);
        buf[i] = (int16_t)(value * volume * envelope * 32767);
    }
}

static void gen_arpeggio(int16_t *buf, int n, const float *notes,
                         int note_count, float volume) {
    float phase = 0;
    int segment = n / note_count;
    for (int i = 0; i < n; i++) {
        int index = i / segment;
        if (index >= note_count) index = note_count - 1;
        phase += 2.0f * (float)M_PI * notes[index] / SR;
        float local = (float)(i % segment);
        float envelope = 1.0f;
        if (local < 80) envelope = local / 80;
        if (local > segment - 200) envelope = (segment - local) / 200;
        buf[i] = (int16_t)(sinf(phase) * volume * envelope * 32767);
    }
}

static int16_t clamp_sample(int value) {
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return (int16_t)value;
}

static int16_t *begin_sample(SfxType type, int count) {
    sample_bank[type] = (SoundSample){sample_storage[type], (size_t)count};
    return sample_storage[type];
}

static void generate_samples(void) {
    int n = (int)(0.08f * SR);
    gen_chirp(begin_sample(SFX_PLANT, n), n, 400, 900, 0.30f, 0);

    n = (int)(0.07f * SR);
    gen_chirp(begin_sample(SFX_SHOVEL, n), n, 500, 200, 0.25f, 1);

    n = (int)(0.06f * SR);
    gen_chirp(begin_sample(SFX_DENY, n), n, 100, 90, 0.20f, 1);

    n = (int)(0.035f * SR);
    gen_chirp(begin_sample(SFX_HIT, n), n, 350, 300, 0.20f, 1);

    n = (int)(0.045f * SR);
    gen_noise(begin_sample(SFX_BITE, n), n, 0.28f);

    n = (int)(0.12f * SR);
    gen_chirp(begin_sample(SFX_ZOMBIE_DIE, n), n, 600, 100, 0.25f, 1);

    n = (int)(0.2f * SR);
    int16_t *explosion = begin_sample(SFX_EXPLODE, n);
    gen_noise(explosion, n, 0.35f);
    for (int i = 0; i < n; i++) {
        float t = (float)i / n;
        int rumble = (int)(sinf(2.0f * (float)M_PI * 60 * i / SR) *
                           0.30f * (1.0f - t) * 32767);
        explosion[i] = clamp_sample(explosion[i] + rumble);
    }

    n = (int)(0.25f * SR);
    gen_saw(begin_sample(SFX_MOWER, n), n, 120, 0.25f);

    static const float clear_notes[] = {523.25f, 659.25f, 783.99f};
    n = (int)(0.3f * SR);
    gen_arpeggio(begin_sample(SFX_WAVE_CLEAR, n), n, clear_notes, 3, 0.30f);

    n = (int)(0.5f * SR);
    gen_chirp(begin_sample(SFX_GAME_OVER, n), n, 400, 80, 0.30f, 0);

    static const float win_notes[] = {523.25f, 659.25f, 783.99f, 1046.50f};
    n = (int)(0.4f * SR);
    gen_arpeggio(begin_sample(SFX_WIN, n), n, win_notes, 4, 0.30f);
}

static uint64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}

#if defined(NPVZ_SOUND_SDL)

static Mix_Chunk *chunks[SFX_COUNT];
static SoundVoicePool voices;
static int enabled;
static int mixer_open;
static int sdl_audio_owned;

void sound_init(void) {
    sound_cleanup();
    generate_samples();

    sdl_audio_owned = !(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO);
    if (sdl_audio_owned && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    if (Mix_OpenAudio(SR, AUDIO_S16SYS, 1, 512) != 0) {
        sound_cleanup();
        return;
    }
    mixer_open = 1;
    Mix_AllocateChannels(SOUND_MAX_VOICES);
    sound_voice_pool_init(&voices, SOUND_MAX_VOICES, 6, 0);

    for (int i = 0; i < SFX_COUNT; i++) {
        chunks[i] = Mix_QuickLoad_RAW((Uint8 *)sample_bank[i].samples,
                                      (Uint32)(sample_bank[i].count * 2));
        if (!chunks[i]) {
            sound_cleanup();
            return;
        }
    }
    enabled = 1;
}

void sound_play(SfxType type) {
    if (!enabled || type < 0 || type >= SFX_COUNT) return;
    uint64_t now = monotonic_ms();
    for (int i = 0; i < voices.capacity; i++)
        if (voices.voices[i].active && !Mix_Playing(i))
            sound_voice_release(&voices, i, now);

    int slot = sound_voice_acquire(&voices, type, now);
    if (slot < 0) return;
    if (Mix_Playing(slot)) Mix_HaltChannel(slot);
    if (Mix_PlayChannel(slot, chunks[type], 0) < 0)
        sound_voice_release(&voices, slot, now);
}

void sound_cleanup(void) {
    enabled = 0;
    if (mixer_open) Mix_HaltChannel(-1);
    for (int i = 0; i < SFX_COUNT; i++) {
        if (chunks[i]) Mix_FreeChunk(chunks[i]);
        chunks[i] = NULL;
    }
    if (mixer_open) Mix_CloseAudio();
    mixer_open = 0;
    if (sdl_audio_owned) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    sdl_audio_owned = 0;
    memset(&voices, 0, sizeof(voices));
}

#elif defined(NPVZ_SOUND_AUDIOQUEUE)

#define AUDIO_QUEUE_BUFFERS 3
#define AUDIO_QUEUE_FRAMES 512

static AudioQueueRef audio_queue;
static SoundVoicePool voices;
static pthread_mutex_t voice_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int enabled;

static void fill_audio_buffer(void *context, AudioQueueRef queue,
                              AudioQueueBufferRef buffer) {
    (void)context;
    if (!atomic_load(&enabled)) return;

    size_t frames = buffer->mAudioDataBytesCapacity / sizeof(int16_t);
    pthread_mutex_lock(&voice_lock);
    sound_voice_mix(&voices, sample_bank, buffer->mAudioData, frames,
                    monotonic_ms());
    pthread_mutex_unlock(&voice_lock);
    buffer->mAudioDataByteSize = (UInt32)(frames * sizeof(int16_t));
    if (atomic_load(&enabled))
        AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

void sound_init(void) {
    sound_cleanup();
    generate_samples();
    sound_voice_pool_init(&voices, SOUND_MAX_VOICES, 6, 0);

    AudioStreamBasicDescription format = {0};
    format.mSampleRate = SR;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                          kAudioFormatFlagIsPacked |
                          kAudioFormatFlagsNativeEndian;
    format.mBytesPerPacket = sizeof(int16_t);
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = sizeof(int16_t);
    format.mChannelsPerFrame = 1;
    format.mBitsPerChannel = 16;

    if (AudioQueueNewOutput(&format, fill_audio_buffer, NULL, NULL, NULL, 0,
                            &audio_queue) != noErr)
        return;

    atomic_store(&enabled, 1);
    for (int i = 0; i < AUDIO_QUEUE_BUFFERS; i++) {
        AudioQueueBufferRef buffer;
        if (AudioQueueAllocateBuffer(audio_queue,
                                     AUDIO_QUEUE_FRAMES * sizeof(int16_t),
                                     &buffer) != noErr) {
            sound_cleanup();
            return;
        }
        memset(buffer->mAudioData, 0, buffer->mAudioDataBytesCapacity);
        buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
        if (AudioQueueEnqueueBuffer(audio_queue, buffer, 0, NULL) != noErr) {
            sound_cleanup();
            return;
        }
    }
    if (AudioQueueStart(audio_queue, NULL) != noErr) sound_cleanup();
}

void sound_play(SfxType type) {
    if (!atomic_load(&enabled) || type < 0 || type >= SFX_COUNT) return;
    pthread_mutex_lock(&voice_lock);
    sound_voice_acquire(&voices, type, monotonic_ms());
    pthread_mutex_unlock(&voice_lock);
}

void sound_cleanup(void) {
    atomic_store(&enabled, 0);
    if (audio_queue) {
        AudioQueueStop(audio_queue, true);
        AudioQueueDispose(audio_queue, true);
        audio_queue = NULL;
    }
    pthread_mutex_lock(&voice_lock);
    memset(&voices, 0, sizeof(voices));
    pthread_mutex_unlock(&voice_lock);
}

#else

#define POSIX_VOICES 4
#define SHUTDOWN_TIMEOUT_MS 200

static char sfx_paths[SFX_COUNT][80];
static pid_t player_pids[POSIX_VOICES];
static SoundVoicePool voices;
static int enabled;
static int initialized;

static int write_wav(const char *path, const SoundSample *sample) {
    FILE *file = fopen(path, "wb");
    if (!file) return 0;

    int32_t data_size = (int32_t)(sample->count * 2);
    int32_t riff_size = 36 + data_size;
    int32_t format_size = 16;
    int16_t pcm = 1;
    int16_t mono = 1;
    int32_t sample_rate = SR;
    int32_t byte_rate = SR * 2;
    int16_t alignment = 2;
    int16_t bits = 16;

    fwrite("RIFF", 1, 4, file);
    fwrite(&riff_size, 4, 1, file);
    fwrite("WAVE", 1, 4, file);
    fwrite("fmt ", 1, 4, file);
    fwrite(&format_size, 4, 1, file);
    fwrite(&pcm, 2, 1, file);
    fwrite(&mono, 2, 1, file);
    fwrite(&sample_rate, 4, 1, file);
    fwrite(&byte_rate, 4, 1, file);
    fwrite(&alignment, 2, 1, file);
    fwrite(&bits, 2, 1, file);
    fwrite("data", 1, 4, file);
    fwrite(&data_size, 4, 1, file);
    fwrite(sample->samples, 2, sample->count, file);
    int ok = !ferror(file) && fclose(file) == 0;
    if (!ok) unlink(path);
    return ok;
}

static void wait_for_pid(pid_t pid) {
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
}

static void reap_players(uint64_t now) {
    for (int slot = 0; slot < POSIX_VOICES; slot++) {
        if (player_pids[slot] <= 0) continue;
        int status = 0;
        pid_t result = waitpid(player_pids[slot], &status, WNOHANG);
        if (result == player_pids[slot]) {
            player_pids[slot] = 0;
            sound_voice_release(&voices, slot, now);
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) enabled = 0;
        } else if (result < 0 && errno == ECHILD) {
            player_pids[slot] = 0;
            sound_voice_release(&voices, slot, now);
        }
    }
}

static void exec_player(const char *path) {
#if defined(NPVZ_SOUND_LINUX)
    execlp("pw-play", "pw-play", path, (char *)NULL);
    if (errno != ENOENT) _exit(126);
    execlp("paplay", "paplay", path, (char *)NULL);
    if (errno != ENOENT) _exit(126);
    execlp("aplay", "aplay", "-q", path, (char *)NULL);
#else
    execlp("aucat", "aucat", "-i", path, (char *)NULL);
    if (errno != ENOENT) _exit(126);
    execlp("audioplay", "audioplay", path, (char *)NULL);
    if (errno != ENOENT) _exit(126);
    execlp("play", "play", "-q", path, (char *)NULL);
#endif
    _exit(errno == ENOENT ? 127 : 126);
}

void sound_init(void) {
    sound_cleanup();
    generate_samples();
    sound_voice_pool_init(&voices, POSIX_VOICES, 3, 100);
    initialized = 1;

    for (int i = 0; i < SFX_COUNT; i++) {
        snprintf(sfx_paths[i], sizeof(sfx_paths[i]),
                 "/tmp/npvz_sfx_%ld_%d.wav", (long)getpid(), i);
        if (!write_wav(sfx_paths[i], &sample_bank[i])) {
            sound_cleanup();
            return;
        }
    }
    enabled = 1;
}

void sound_play(SfxType type) {
    if (!enabled || type < 0 || type >= SFX_COUNT) return;
    uint64_t now = monotonic_ms();
    reap_players(now);
    if (!enabled) return;

    int slot = sound_voice_acquire(&voices, type, now);
    if (slot < 0) return;
    if (player_pids[slot] > 0) {
        kill(player_pids[slot], SIGKILL);
        wait_for_pid(player_pids[slot]);
        player_pids[slot] = 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        sound_voice_release(&voices, slot, now);
        return;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        exec_player(sfx_paths[type]);
    }
    player_pids[slot] = pid;
}

void sound_cleanup(void) {
    enabled = 0;
    for (int slot = 0; slot < POSIX_VOICES; slot++)
        if (player_pids[slot] > 0) kill(player_pids[slot], SIGTERM);

    uint64_t deadline = monotonic_ms() + SHUTDOWN_TIMEOUT_MS;
    int remaining;
    do {
        remaining = 0;
        for (int slot = 0; slot < POSIX_VOICES; slot++) {
            if (player_pids[slot] <= 0) continue;
            pid_t result = waitpid(player_pids[slot], NULL, WNOHANG);
            if (result == player_pids[slot] ||
                (result < 0 && errno == ECHILD)) {
                player_pids[slot] = 0;
            } else {
                remaining++;
            }
        }
        if (remaining && monotonic_ms() < deadline) {
            struct timespec delay = {.tv_nsec = 10000000};
            nanosleep(&delay, NULL);
        }
    } while (remaining && monotonic_ms() < deadline);

    for (int slot = 0; slot < POSIX_VOICES; slot++) {
        if (player_pids[slot] <= 0) continue;
        kill(player_pids[slot], SIGKILL);
        wait_for_pid(player_pids[slot]);
        player_pids[slot] = 0;
    }
    if (initialized) {
        for (int i = 0; i < SFX_COUNT; i++) {
            if (sfx_paths[i][0]) unlink(sfx_paths[i]);
            sfx_paths[i][0] = '\0';
        }
    }
    initialized = 0;
    memset(&voices, 0, sizeof(voices));
}

#endif
