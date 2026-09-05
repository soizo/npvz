# Audio System Redesign

## Goal

Prevent high-frequency sound effects from creating unbounded audio clients or leaving playback resources behind, while preserving overlapping game sounds on macOS, Linux, and BSD.

## Public contract

The existing API remains unchanged:

```c
void sound_init(void);
void sound_play(SfxType type);
void sound_cleanup(void);
```

Callers in `game.c`, `board.c`, and `main.c` require no changes. Initialization and cleanup remain non-fatal: if no backend can start, gameplay continues silently.

## Shared sound data

Keep the existing synthesized effects and sample rate. Generate each effect once into process-owned PCM memory during `sound_init`. Backends consume those samples directly or write per-process temporary WAV files when an external player requires them.

Every backend has a fixed voice limit and coalesces repeats of the same effect received within 50 ms. Win, game-over, wave-clear, mower, and explosion sounds have high priority; zombie death has medium priority; all other effects are ordinary. When full, a higher-priority request replaces the oldest lower-priority voice; otherwise the new sound is dropped. Sound requests are never queued without a bound.

## Backend selection

The Makefile selects one backend at compile time:

1. Use SDL2_mixer when `pkg-config SDL2_mixer` succeeds.
2. Without SDL2_mixer on macOS, use Audio Queue Services from AudioToolbox.
3. Without SDL2_mixer on Linux or BSD, use the bounded POSIX player backend.

The selected backend is reported through compile definitions. No backend is selected dynamically after the program starts.

### SDL2_mixer

Open one mono `AUDIO_S16SYS` device at 22050 Hz and allocate eight channels. Load the generated PCM without copying it into a second custom mixer. Six channels handle ordinary effects; two channels are reserved for important effects so hit bursts cannot hide win, loss, wave, mower, or explosion feedback.

`Mix_PlayChannel` starts playback asynchronously. Exhausted ordinary channels drop the new ordinary effect. Cleanup halts channels, frees chunks, closes the mixer device, and releases only the SDL audio subsystem initialized by npvz.

### macOS Audio Queue fallback

Create one Audio Queue output and mix up to eight active voices into each output buffer. The callback sums samples in a wider accumulator and clamps once to signed 16-bit output, preventing overflow distortion. Access to voice state is synchronized between the game thread and Audio Queue callback.

The queue is the only system audio client. Cleanup stops and disposes it synchronously before freeing sample storage.

### Linux/BSD POSIX fallback

Maintain four child slots and launch only tracked children. Linux tries `pw-play`, `paplay`, then `aplay`; BSD tries `aucat`, `audioplay`, then `play`. Missing or failed playback disables this backend for the rest of the process, preventing repeated failed launches.

Different effects may occupy separate slots and overlap. A slot cannot launch a second player until 100 ms after its previous launch, preventing rapid short-effect process churn. Cleanup sends TERM to tracked children, waits up to 200 ms, then sends KILL and performs a blocking reap if necessary. It never calls `waitpid(-1)`.

Temporary WAV names include the npvz PID, so concurrent game instances do not overwrite or unlink each other's files. Cleanup removes only its own files.

## Failure handling

- Invalid effect types are ignored.
- Sample allocation, file creation, device initialization, or player startup failure disables sound rather than terminating the game.
- Partial initialization is cleaned up by `sound_cleanup`.
- Repeated `sound_init` or `sound_cleanup` calls do not leak resources.

## Verification

A regression test forces the POSIX backend to use a silent fake player and proves:

- a high-frequency burst never exceeds four child processes;
- repeated identical effects are coalesced;
- cleanup leaves no child alive;
- a missing or failed player disables retries;
- concurrent npvz instances use different temporary paths.

Backend-independent tests cover voice limits, priority replacement, sample mixing/clamping, and repeated lifecycle calls. macOS validation builds both the installed SDL2_mixer path and the forced Audio Queue fallback without playing sound through the user's terminal.

Before completion, run:

```sh
make test
make clean && make
git diff --check
```

Terminal rendering is unchanged, so VHS/Ghostty validation is outside this change.
