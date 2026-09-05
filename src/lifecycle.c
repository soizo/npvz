#include "lifecycle.h"
#include <signal.h>
#include <stddef.h>

static volatile sig_atomic_t caught_signal;

static void catch_signal(int signal_number) {
    if (!caught_signal) caught_signal = signal_number;
}

void lifecycle_install_signal_handlers(void) {
    static const int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT};
    struct sigaction action = {0};
    action.sa_handler = catch_signal;
    sigemptyset(&action.sa_mask);
    for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
        sigaddset(&action.sa_mask, signals[i]);

    caught_signal = 0;
    for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
        sigaction(signals[i], &action, NULL);
}

int lifecycle_signal(void) {
    return caught_signal;
}

int lifecycle_exit_code(int signal_number) {
    return signal_number ? 128 + signal_number : 0;
}
