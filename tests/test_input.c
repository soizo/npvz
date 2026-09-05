#include <assert.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/input.h"

int main(void) {
    assert(setenv("TERM", "xterm-256color", 1) == 0);
    FILE *input_file = tmpfile();
    FILE *output_file = tmpfile();
    assert(input_file && output_file);
    SCREEN *screen = newterm(NULL, output_file, input_file);
    assert(screen);
    set_term(screen);

    mousemask(ALL_MOUSE_EVENTS, NULL);
    assert(ungetch('\n') == OK);

    input_init();

    mmask_t old_mask;
    mousemask(0, &old_mask);
    assert(old_mask == 0);
    assert(input_read() == ERR);

    assert(ungetch('x') == OK);
    assert(input_read() == 'x');

    endwin();
    delscreen(screen);
    fclose(input_file);
    fclose(output_file);
    puts("input tests passed");
    return 0;
}
