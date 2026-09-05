# Bounded Cross-Platform Audio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace unbounded `afplay` spawning with bounded overlapping audio backends for macOS, Linux, and BSD.

**Architecture:** Keep synthesis and the public sound API in `sound.c`, move bounded voice admission and PCM mixing into a small backend-independent module, and choose SDL2_mixer, Audio Queue, or controlled POSIX playback at compile time. SDL2_mixer is preferred when pkg-config finds it; every backend coalesces 50 ms repeats and enforces a fixed voice ceiling.

**Tech Stack:** C11, SDL2_mixer 2.x when installed, macOS AudioToolbox Audio Queue fallback, POSIX process APIs for Linux/BSD fallback, Make, assert-based tests.

**Spec:** `docs/audio-system-design.md`

## Global Constraints

- Preserve `sound_init`, `sound_play`, and `sound_cleanup` signatures and all existing `SfxType` values.
- Use 22050 Hz mono signed 16-bit PCM and preserve the current synthesized effects.
- Coalesce the same effect within 50 ms; cap SDL and Audio Queue at 8 voices and POSIX at 4.
- Never queue sound requests without a bound or call `waitpid(-1)`.
- Failures disable sound without terminating gameplay.
- Do not add a required dependency: SDL2_mixer remains compile-time optional.
- Do not launch, inspect, or control Ghostty during validation.

---

### Task 1: Bounded Voice Pool and Mixer

**Files:**

- Create: `src/sound_voice.h`
- Create: `src/sound_voice.c`
- Create: `tests/test_sound_voice.c`
- Modify: `Makefile`

**Interfaces:**

- Produces: `SoundSample`, `SoundVoicePool`, `sound_voice_pool_init`, `sound_voice_acquire`, `sound_voice_release`, and `sound_voice_mix`.
- Consumes: `SfxType` from `sound.h` and caller-supplied monotonic milliseconds.

- [ ] **Step 1: Write failing voice-policy and mixer tests**

Create an assert-based test with these cases:

```c
SoundVoicePool pool;
sound_voice_pool_init(&pool, 4, 3, 100);
assert(sound_voice_acquire(&pool, SFX_HIT, 1000) == 0);
assert(sound_voice_acquire(&pool, SFX_HIT, 1049) == -1);
assert(sound_voice_acquire(&pool, SFX_BITE, 1000) == 1);
assert(sound_voice_acquire(&pool, SFX_PLANT, 1000) == 2);
assert(sound_voice_acquire(&pool, SFX_DENY, 1000) == -1);
assert(sound_voice_acquire(&pool, SFX_ZOMBIE_DIE, 1000) == 3);
assert(sound_voice_acquire(&pool, SFX_EXPLODE, 1000) == 0);
```

Also mix two two-sample voices containing `{30000, -30000}` and `{10000, -10000}` and assert output is clamped to `{32767, -32768}`. Release a voice at 1000 ms and assert the same slot is unavailable at 1099 ms and available at 1100 ms.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```sh
make tests/test_sound_voice
```

Expected: compilation fails because `src/sound_voice.h` and its functions do not exist.

- [ ] **Step 3: Implement the fixed-capacity voice module**

Use these concrete types and signatures:

```c
#define SOUND_MAX_VOICES 8

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
    int capacity;
    int ordinary_limit;
    uint64_t reuse_delay_ms;
} SoundVoicePool;

void sound_voice_pool_init(SoundVoicePool *, int capacity,
                           int ordinary_limit, uint64_t reuse_delay_ms);
int sound_voice_acquire(SoundVoicePool *, SfxType, uint64_t now_ms);
void sound_voice_release(SoundVoicePool *, int slot, uint64_t now_ms);
void sound_voice_mix(SoundVoicePool *, const SoundSample bank[SFX_COUNT],
                     int16_t *output, size_t frames, uint64_t now_ms);
```

Priority is `2` for win, game-over, wave-clear, mower, and explosion; `1` for zombie death; `0` otherwise. Ordinary requests may use only `ordinary_limit` slots. Higher-priority requests may use all slots and replace the oldest active lower-priority voice. Mix through an `int32_t` accumulator and clamp once per frame.

- [ ] **Step 4: Run RED-to-GREEN verification**

Run:

```sh
make tests/test_sound_voice && ./tests/test_sound_voice
```

Expected: `sound voice tests passed` and exit 0.

- [ ] **Step 5: Commit the independent voice engine**

```sh
git add Makefile src/sound_voice.c src/sound_voice.h tests/test_sound_voice.c
git commit -m "feat: add bounded sound voice engine"
```

---

### Task 2: SDL2_mixer Primary Backend

**Files:**

- Modify: `src/sound.c`
- Modify: `Makefile`
- Create: `tests/test_sound_sdl.c`

**Interfaces:**

- Consumes: `SoundSample` and `SoundVoicePool` from Task 1.
- Produces: the unchanged public API through the `NPVZ_SOUND_SDL` backend.

- [ ] **Step 1: Add a failing dummy-driver integration test**

Set `SDL_AUDIODRIVER=dummy`, call `sound_init`, assert `Mix_QuerySpec` reports 22050 Hz, mono output and eight allocated channels, issue all `SFX_COUNT` effects repeatedly, and assert `Mix_Playing(-1) <= 8`. Call `sound_cleanup` twice and assert `Mix_QuerySpec` reports a closed device.

- [ ] **Step 2: Verify the old backend fails the SDL contract**

Run:

```sh
make tests/test_sound_sdl && SDL_AUDIODRIVER=dummy ./tests/test_sound_sdl
```

Expected: FAIL because the current implementation never opens SDL2_mixer.

- [ ] **Step 3: Make SDL2_mixer compile-time optional**

In `Makefile`, detect `pkg-config SDL2_mixer`. For `SOUND_BACKEND=auto`, define `NPVZ_SOUND_SDL` and append pkg-config flags when found; otherwise select `NPVZ_SOUND_AUDIOQUEUE` on Darwin and `NPVZ_SOUND_POSIX` elsewhere. Accept explicit `SOUND_BACKEND=sdl`, `audioqueue`, or `posix` so every backend can be built in CI.

- [ ] **Step 4: Replace temporary-file-first synthesis with an in-memory bank**

Keep static storage sized for each effect's existing maximum of half a second:

```c
#define SR 22050
#define MAX_SAMPLES (SR / 2)
static int16_t sample_storage[SFX_COUNT][MAX_SAMPLES];
static SoundSample sample_bank[SFX_COUNT];
```

Populate `sample_bank[type].samples` and `.count` while preserving every existing generator parameter. Remove the old fixed `/tmp/npvz_sfx_N.wav` initialization from the SDL path.

- [ ] **Step 5: Implement SDL lifecycle and bounded playback**

Open one `AUDIO_S16SYS`, mono, 22050 Hz device; allocate eight channels; create each chunk with `Mix_QuickLoad_RAW`; initialize `SoundVoicePool` with capacity 8, ordinary limit 6, and zero slot reuse delay. Before admission, release channels for which `Mix_Playing(channel)` is false. Halt a selected occupied channel before priority replacement, then call `Mix_PlayChannel(slot, chunk, 0)`. On any initialization failure, close partial resources and leave sound disabled.

Track whether npvz initialized `SDL_INIT_AUDIO`; cleanup must halt channels, free chunks, close the mixer, and call `SDL_QuitSubSystem(SDL_INIT_AUDIO)` only when owned.

- [ ] **Step 6: Verify SDL GREEN and existing rules**

Run:

```sh
make tests/test_sound_sdl && SDL_AUDIODRIVER=dummy ./tests/test_sound_sdl
make test
```

Expected: SDL integration and existing rule tests pass without audible output.

- [ ] **Step 7: Commit the primary backend**

```sh
git add Makefile src/sound.c tests/test_sound_sdl.c
git commit -m "fix: replace afplay fanout with SDL audio"
```

---

### Task 3: macOS Audio Queue Fallback

**Files:**

- Modify: `src/sound.c`
- Modify: `Makefile`
- Create: `tests/test_audioqueue_build.sh`

**Interfaces:**

- Consumes: `SoundSample`, `SoundVoicePool`, and `sound_voice_mix` from Task 1.
- Produces: the unchanged public API through `NPVZ_SOUND_AUDIOQUEUE`.

- [ ] **Step 1: Add a failing forced-fallback build check**

The script runs:

```sh
make clean
make SOUND_BACKEND=audioqueue
nm -u npvz | grep -Eq '(_fork|_execl|_posix_spawn)' && exit 1
```

It must skip with exit 0 off Darwin. On Darwin, the old implementation fails because it imports `fork` and `execlp`.

- [ ] **Step 2: Verify RED**

Run:

```sh
sh tests/test_audioqueue_build.sh
```

Expected: FAIL on Darwin because the existing binary imports process-launch symbols.

- [ ] **Step 3: Implement one Audio Queue output**

Configure `AudioStreamBasicDescription` for 22050 Hz, one channel, signed native-endian packed 16-bit PCM. Create one output queue, allocate three 512-frame buffers, fill and enqueue them, then start the queue. Initialize a voice pool with capacity 8, ordinary limit 6, and zero reuse delay.

The output callback locks a `pthread_mutex_t`, calls `sound_voice_mix` into the returned buffer, unlocks, sets `mAudioDataByteSize`, and re-enqueues while enabled. `sound_play` acquires the same lock and admits a voice using monotonic milliseconds.

- [ ] **Step 4: Implement synchronous cleanup**

Set the enabled flag false, call `AudioQueueStop(queue, true)`, then `AudioQueueDispose(queue, true)`. Only after disposal returns, reset the pool. Keep the mutex statically initialized so repeated init/cleanup remains safe.

- [ ] **Step 5: Link and verify the fallback without playback**

For `SOUND_BACKEND=audioqueue`, add `-framework AudioToolbox -framework CoreFoundation` and do not add SDL flags. Run:

```sh
sh tests/test_audioqueue_build.sh
make test
```

Expected: forced fallback builds, has no process-launch imports, and all non-audible tests pass.

- [ ] **Step 6: Commit the macOS fallback**

```sh
git add Makefile src/sound.c tests/test_audioqueue_build.sh
git commit -m "feat: add bounded Audio Queue fallback"
```

---

### Task 4: Linux/BSD Controlled Player Fallback

**Files:**

- Modify: `src/sound.c`
- Modify: `Makefile`
- Create: `tests/test_sound_posix.c`
- Create: `tests/fake_audio_player.sh`

**Interfaces:**

- Consumes: `SoundSample` and `SoundVoicePool` from Task 1.
- Produces: the unchanged public API through `NPVZ_SOUND_POSIX`.

- [ ] **Step 1: Add a failing fake-player stress test**

The fake player appends its PID and WAV argument to paths named by test environment variables, then waits while a gate file exists. The C test prepends a temporary directory containing fake `pw-play`, `paplay`, `aplay`, `aucat`, `audioplay`, and `play` links; initializes sound; sends repeated identical hits and four distinct effects; and asserts no more than four recorded live PIDs.

After `sound_cleanup`, assert `kill(pid, 0)` fails with `ESRCH`. Run a second phase where the fake player exits 127: after the first failed child is reaped, 32 further requests must leave the launch count at one. Fork two short-lived test parents and assert recorded WAV paths contain two different parent PIDs.

- [ ] **Step 2: Verify RED against the unbounded implementation**

Run:

```sh
make tests/test_sound_posix
./tests/test_sound_posix
```

Expected: FAIL because a 32-call burst starts more than four fake players and cleanup leaves them alive.

- [ ] **Step 3: Write per-process WAV files only for this backend**

Change `write_wav` to return success. Generate paths with:

```c
snprintf(paths[i], sizeof(paths[i]), "/tmp/npvz_sfx_%ld_%d.wav",
         (long)getpid(), i);
```

If any write fails, unlink files already written and disable the backend.

- [ ] **Step 4: Implement four tracked child slots**

Initialize a pool with capacity 4, ordinary limit 3, and 100 ms reuse delay. Before each request, call `waitpid(slot_pid, &status, WNOHANG)` only for occupied slots. Release a completed slot; if its player exited nonzero, disable future playback.

In the child, redirect standard streams to `/dev/null` and try Linux commands in `pw-play`, `paplay`, `aplay` order or BSD commands in `aucat`, `audioplay`, `play` order. Continue to the next command only when `exec` returns `ENOENT`; otherwise exit 126. Exit 127 if none exists. In the parent, retain only the returned PID in the selected slot.

For priority replacement, send SIGKILL to the lower-priority tracked PID and reap that exact PID before reuse. On fork failure, release the selected slot without affecting gameplay.

- [ ] **Step 5: Implement bounded shutdown**

Send SIGTERM to every tracked PID. For up to 200 ms, poll only those PIDs with `waitpid(pid, ..., WNOHANG)` and sleep 10 ms between passes. SIGKILL and blocking-wait any PID still present. Reset slots and unlink only PID-qualified WAV files. A second cleanup call performs no process or file operation.

- [ ] **Step 6: Verify POSIX GREEN and all backend builds**

Run:

```sh
make tests/test_sound_posix && ./tests/test_sound_posix
make clean && make SOUND_BACKEND=posix
make clean && make SOUND_BACKEND=sdl
make clean && make SOUND_BACKEND=audioqueue
```

Expected: stress test passes; each backend build supported on the host exits 0; unsupported explicit backends fail with a clear Make error rather than a compiler error.

- [ ] **Step 7: Commit the POSIX fallback**

```sh
git add Makefile src/sound.c tests/test_sound_posix.c tests/fake_audio_player.sh
git commit -m "feat: bound Unix audio player processes"
```

---

### Task 5: Documentation and Full Verification

**Files:**

- Modify: `README.md`
- Modify: `Makefile`

**Interfaces:**

- Consumes: all three completed backends.
- Produces: documented build selection and one reproducible validation entry point.

- [ ] **Step 1: Update platform requirements**

Document that SDL2_mixer is used automatically when available; macOS otherwise uses AudioToolbox; Linux supports `pw-play`, `paplay`, or `aplay`; BSD supports `aucat`, `audioplay`, or `play`; and missing playback support degrades to silent gameplay.

- [ ] **Step 2: Make `make test` run every safe check**

Include rule tests, voice tests, SDL dummy-driver tests when SDL2_mixer is installed, POSIX fake-player stress tests, and the Darwin Audio Queue build check. Keep every test silent.

- [ ] **Step 3: Run proactive diagnostics**

Run LSP diagnostics on `src/sound.c`, `src/sound_voice.c`, all new tests, and `Makefile`. Fix all blocking findings in changed files.

- [ ] **Step 4: Run fresh project verification**

Run exactly:

```sh
make test
make clean && make
git diff --check
```

Expected: every command exits 0 with no test failures, build errors, or whitespace errors.

- [ ] **Step 5: Inspect the final diff and repository state**

Confirm no debug markers, temporary audio files, generated binaries, object files, or unrelated paths are staged. Confirm `git status --short` contains only intended source, test, build, and README changes.

- [ ] **Step 6: Commit documentation and verification wiring**

```sh
git add README.md Makefile
git commit -m "docs: document cross-platform audio backends"
```

- [ ] **Step 7: Record final evidence**

Report all local commit hashes, the exact verification commands and exit results, the selected local backend, and final `git status --short`. Do not push.
