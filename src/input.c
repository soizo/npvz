#include "input.h"
#include <ncurses.h>

void input_init(void) {
    mousemask(0, NULL);
    flushinp();
}

int input_read(void) {
    return getch();
}
