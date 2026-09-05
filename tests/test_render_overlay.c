#define NCURSES_WIDECHAR 1
#include <assert.h>
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/board.h"
#include "../src/render.h"
#include "../src/ui.h"

static int physical_updates;

int render_refresh(void) {
    physical_updates++;
    return wrefresh(stdscr);
}

int render_stage(void) {
    return wnoutrefresh(stdscr);
}

int render_commit(void) {
    physical_updates++;
    return doupdate();
}

static void assert_updates(const Game *game, int expected) {
    physical_updates = 0;
    render_frame(game);
    assert(physical_updates == expected);
}

int main(void) {
    assert(setenv("TERM", "xterm-256color", 1) == 0);
    setlocale(LC_ALL, "");
    FILE *input = tmpfile();
    FILE *output = tmpfile();
    assert(input && output);
    SCREEN *screen = newterm(NULL, output, input);
    assert(screen);
    set_term(screen);
    assert(resizeterm(24, 100) == OK);

    Game game = {0};
    board_init(&game.board);
    game.state = STATE_PAUSED;
    game.wave = 1;

    assert_updates(&game, 1);
    assert(mvwinch(stdscr, 0, 0) & A_DIM);
    cchar_t cell;
    wchar_t text[CCHARW_MAX];
    attr_t attrs;
    short pair;
    assert(mvin_wch(1, 10, &cell) == OK);
    assert(getcchar(&cell, text, &attrs, &pair, NULL) == OK);
    assert(text[0] != L'q');
    assert(mvin_wch(5, 15, &cell) == OK);
    assert(getcchar(&cell, text, &attrs, &pair, NULL) == OK);
    assert(text[0] == L' ');
    assert(pair == UI_PAIR_CROWD);
    assert_updates(&game, 0);
    game.menu_selection = 1;
    assert_updates(&game, 1);
    assert_updates(&game, 0);

    game.help_visible = 1;
    assert_updates(&game, 1);
    assert_updates(&game, 0);

    game.help_visible = 0;
    game.state = STATE_WON;
    assert_updates(&game, 1);
    assert_updates(&game, 0);

    game.state = STATE_LOST;
    assert_updates(&game, 1);
    assert_updates(&game, 0);

    assert(resizeterm(25, 100) == OK);
    assert_updates(&game, 1);

    endwin();
    delscreen(screen);
    fclose(input);
    fclose(output);
    puts("overlay render tests passed");
    return 0;
}
