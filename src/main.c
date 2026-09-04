#define _XOPEN_SOURCE_EXTENDED 1
#include "game.h"
#include "render.h"
#include "sound.h"
#include <ncurses.h>
#include <unistd.h>

#define FRAME_DELAY_US  33333  /* ~30 fps */

int main(void) {
    sound_init();
    render_init();

    Game game;
    game_init(&game);

    int quit = 0;
    while (!quit) {
        int ch = getch();
        if (ch != ERR) quit = game_handle_input(&game, ch);
        if (quit) break;

        game_update(&game);
        render_frame(&game);
        usleep(FRAME_DELAY_US);
    }

    render_cleanup();
    sound_cleanup();
    return 0;
}
