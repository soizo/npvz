#include "game.h"
#include "render.h"
#include "sound.h"
#include <ncurses.h>

#define FRAME_DELAY_MS 33  /* ~30 fps */
#define INPUTS_PER_FRAME 16

int main(void) {
    sound_init();
    render_init();

    Game game;
    game_init(&game);

    int quit = 0;
    while (!quit) {
        int inputs[INPUTS_PER_FRAME];
        int input_count = 0;
        int ch;
        while (input_count < INPUTS_PER_FRAME && (ch = getch()) != ERR)
            inputs[input_count++] = ch;
        quit = game_handle_inputs(&game, inputs, input_count);
        if (quit) break;

        game_update(&game);
        render_frame(&game);
        napms(FRAME_DELAY_MS);
    }

    render_cleanup();
    sound_cleanup();
    return 0;
}
