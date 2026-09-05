#include <assert.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include "../src/sound.h"

int main(void) {
    int frequency = 0;
    int channels = 0;
    Uint16 format = 0;

    sound_init();

    assert(Mix_QuerySpec(&frequency, &format, &channels));
    assert(frequency == 22050);
    assert(format == AUDIO_S16SYS);
    assert(channels == 1);
    assert(Mix_AllocateChannels(-1) == 8);

    for (int repeat = 0; repeat < 4; repeat++)
        for (int type = 0; type < SFX_COUNT; type++)
            sound_play((SfxType)type);
    assert(Mix_Playing(-1) <= 8);

    sound_cleanup();
    sound_cleanup();
    assert(!Mix_QuerySpec(NULL, NULL, NULL));
    puts("SDL sound tests passed");
    return 0;
}
