#include "input.h"
#include <ncurses.h>
#include <time.h>

#define WHEEL_INTERVAL_MS 120

void input_init(InputState *input) {
    input->next_wheel_ms = 0;
    mousemask(BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
    mouseinterval(0);
    flushinp();
}

int input_read(InputState *input, int64_t now_ms) {
    int ch = getch();
    if (ch != KEY_MOUSE) return ch;

    MEVENT event;
    if (getmouse(&event) == ERR) return ERR;
    int wheel_key = event.bstate & BUTTON4_PRESSED ? KEY_UP
                  : event.bstate & BUTTON5_PRESSED ? KEY_DOWN : ERR;

    while ((ch = getch()) == KEY_MOUSE) getmouse(&event);
    if (ch != ERR) ungetch(ch);

    if (wheel_key == ERR || now_ms < input->next_wheel_ms) return ERR;
    input->next_wheel_ms = now_ms + WHEEL_INTERVAL_MS;
    return wheel_key;
}

int64_t input_now_ms(void) {
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
