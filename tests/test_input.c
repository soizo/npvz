#include <assert.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/input.h"

static void push_wheel(mmask_t state) {
    MEVENT event = { .bstate = state };
    assert(ungetmouse(&event) == OK);
}

int main(void) {
    assert(setenv("TERM", "xterm-256color", 1) == 0);
    FILE *input_file = tmpfile();
    FILE *output_file = tmpfile();
    assert(input_file && output_file);
    SCREEN *screen = newterm(NULL, output_file, input_file);
    assert(screen);
    set_term(screen);

    InputState input;
    input_init(&input);
    assert(ungetch('\n') == OK);
    push_wheel(BUTTON4_PRESSED);
    input_init(&input);
    assert(input_read(&input, 0) == ERR);

    push_wheel(BUTTON4_PRESSED);
    assert(input_read(&input, 0) == KEY_UP);
    push_wheel(BUTTON5_PRESSED);
    assert(input_read(&input, 30) == ERR);
    push_wheel(BUTTON5_PRESSED);
    assert(input_read(&input, 120) == KEY_DOWN);

    input_init(&input);
    assert(ungetch('x') == OK);
    push_wheel(BUTTON4_PRESSED);
    push_wheel(BUTTON4_PRESSED);
    assert(input_read(&input, 0) == KEY_UP);
    assert(input_read(&input, 0) == 'x');
    assert(input_read(&input, 1000) == ERR);

    endwin();
    delscreen(screen);
    fclose(input_file);
    fclose(output_file);
    puts("input tests passed");
    return 0;
}
