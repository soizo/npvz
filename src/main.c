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

        if (ch == 'q' || ch == 'Q') {
            if (game.state == STATE_PLAYING || game.state == STATE_PAUSED) {
                quit = 1;
                break;
            }
        }

        if (ch != ERR) {
            game_handle_input(&game, ch);
        }

        game_update(&game);
        render_frame(&game);

        if (game.state == STATE_WON || game.state == STATE_LOST) {
            /* wait for R or Q on end screen */
            nodelay(stdscr, FALSE);
            while (1) {
                ch = getch();
                if (ch == 'q' || ch == 'Q') { quit = 1; break; }
                if (ch == 'r' || ch == 'R') {
                    game_init(&game);
                    nodelay(stdscr, TRUE);
                    break;
                }
            }
        }

        usleep(FRAME_DELAY_US);
    }

    render_cleanup();
    sound_cleanup();
    return 0;
}
