#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <ncurses.h>

#ifndef BUTTON5_PRESSED
#define BUTTON5_PRESSED NCURSES_MOUSE_MASK(5, NCURSES_BUTTON_PRESSED)
#endif

typedef struct {
    int64_t next_wheel_ms;
} InputState;

void input_init(InputState *input);
int input_read(InputState *input, int64_t now_ms);
int64_t input_now_ms(void);

#endif
