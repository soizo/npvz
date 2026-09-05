#include "game.h"
#include "lifecycle.h"
#include "render.h"
#include "sound.h"
#include <ncurses.h>
#include <stdio.h>

#define FRAME_DELAY_MS 33  /* ~30 fps */

int main(void) {
    lifecycle_install_signal_handlers();
    puts("npvz: starting");
    fflush(stdout);

    sound_init();
    render_init();

    Game game;
    game_init(&game);

    int quit = 0;
    while (!quit) {
        int ch = getch();
        if (ch != ERR) quit = game_handle_input(&game, ch);
        if (quit || lifecycle_signal()) break;

        game_update(&game);
        render_frame(&game);
        napms(FRAME_DELAY_MS);
    }

    render_cleanup();
    sound_cleanup();

    int signal_number = lifecycle_signal();
    if (signal_number)
        printf("npvz: interrupted by signal %d\n", signal_number);
    else
        puts("npvz: goodbye");
    return lifecycle_exit_code(signal_number);
}
