#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/lifecycle.h"

static void assert_signal_is_caught(int signal_number) {
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        lifecycle_install_signal_handlers();
        raise(signal_number);
        raise(SIGINT == signal_number ? SIGTERM : SIGINT);
        assert(lifecycle_signal() == signal_number);
        assert(lifecycle_exit_code(signal_number) == 128 + signal_number);
        _exit(0);
    }

    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
}

int main(void) {
    assert(lifecycle_exit_code(0) == 0);
    assert_signal_is_caught(SIGINT);
    assert_signal_is_caught(SIGTERM);
    assert_signal_is_caught(SIGHUP);
    assert_signal_is_caught(SIGQUIT);
    puts("lifecycle tests passed");
    return 0;
}
