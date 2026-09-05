#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../src/sound.h"

#define MAX_TEST_PIDS 64

static char fake_player_dir[PATH_MAX];

static void test_path(char *buffer, size_t size, const char *kind, pid_t pid) {
    snprintf(buffer, size, "/tmp/npvz_audio_%s_%ld", kind, (long)pid);
}

static void sleep_ms(long milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = milliseconds % 1000 * 1000000,
    };
    nanosleep(&delay, NULL);
}

static void touch_file(const char *path) {
    FILE *file = fopen(path, "w");
    assert(file);
    fclose(file);
}

static int read_log(pid_t owner, pid_t pids[MAX_TEST_PIDS],
                    char paths[MAX_TEST_PIDS][PATH_MAX]) {
    char log_path[PATH_MAX];
    test_path(log_path, sizeof(log_path), "log", owner);
    FILE *file = fopen(log_path, "r");
    if (!file) return 0;

    int count = 0;
    long pid;
    while (count < MAX_TEST_PIDS &&
           fscanf(file, "%ld %1023s", &pid, paths[count]) == 2) {
        pids[count++] = (pid_t)pid;
    }
    fclose(file);
    return count;
}

static int wait_for_log(pid_t owner, int minimum, pid_t pids[MAX_TEST_PIDS],
                        char paths[MAX_TEST_PIDS][PATH_MAX]) {
    int count = 0;
    for (int attempt = 0; attempt < 100; attempt++) {
        count = read_log(owner, pids, paths);
        if (count >= minimum) return count;
        sleep_ms(10);
    }
    return count;
}

static void remove_test_files(pid_t owner) {
    char path[PATH_MAX];
    const char *kinds[] = {"log", "gate", "fail"};
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        test_path(path, sizeof(path), kinds[i], owner);
        unlink(path);
    }
}

static void stop_fake_players(pid_t owner) {
    pid_t pids[MAX_TEST_PIDS];
    char paths[MAX_TEST_PIDS][PATH_MAX];
    int count = read_log(owner, pids, paths);
    char gate[PATH_MAX];
    test_path(gate, sizeof(gate), "gate", owner);
    unlink(gate);
    for (int i = 0; i < count; i++) kill(pids[i], SIGKILL);
    for (int i = 0; i < count; i++) waitpid(pids[i], NULL, 0);
}

static void install_fake_players(void) {
    static const char *names[] = {
        "afplay", "pw-play", "paplay", "aplay",
        "aucat", "audioplay", "play",
    };
    char cwd[PATH_MAX];
    char script[PATH_MAX];
    char path[PATH_MAX * 2];
    assert(getcwd(cwd, sizeof(cwd)));
    snprintf(script, sizeof(script), "%s/tests/fake_audio_player.sh", cwd);
    snprintf(fake_player_dir, sizeof(fake_player_dir),
             "/tmp/npvz_audio_bin_%ld", (long)getpid());
    assert(mkdir(fake_player_dir, S_IRWXU) == 0);
    const char *old_path = getenv("PATH");
    snprintf(path, sizeof(path), "%s:%s", fake_player_dir,
             old_path ? old_path : "");
    assert(setenv("PATH", path, 1) == 0);

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char link_path[PATH_MAX];
        snprintf(link_path, sizeof(link_path), "%s/%s", fake_player_dir,
                 names[i]);
        assert(symlink(script, link_path) == 0);
    }
}

static void remove_fake_players(void) {
    static const char *names[] = {
        "afplay", "pw-play", "paplay", "aplay",
        "aucat", "audioplay", "play",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char link_path[PATH_MAX];
        snprintf(link_path, sizeof(link_path), "%s/%s", fake_player_dir,
                 names[i]);
        unlink(link_path);
    }
    rmdir(fake_player_dir);
}

static void test_burst_is_bounded_and_cleanup_reaps(void) {
    pid_t owner = getpid();
    pid_t pids[MAX_TEST_PIDS];
    char paths[MAX_TEST_PIDS][PATH_MAX];
    char gate[PATH_MAX];
    remove_test_files(owner);
    test_path(gate, sizeof(gate), "gate", owner);
    touch_file(gate);

    sound_init();
    for (int i = 0; i < 32; i++) sound_play(SFX_HIT);
    sound_play(SFX_BITE);
    sound_play(SFX_PLANT);
    sound_play(SFX_SHOVEL);
    sound_play(SFX_ZOMBIE_DIE);

    int count = wait_for_log(owner, 4, pids, paths);
    sound_cleanup();
    int alive = 0;
    for (int i = 0; i < count; i++)
        if (kill(pids[i], 0) == 0) alive++;

    stop_fake_players(owner);
    remove_test_files(owner);
    assert(count <= 4);
    assert(alive == 0);
}

static void test_failed_player_disables_retries(void) {
    pid_t owner = getpid();
    pid_t pids[MAX_TEST_PIDS];
    char paths[MAX_TEST_PIDS][PATH_MAX];
    char fail[PATH_MAX];
    remove_test_files(owner);
    test_path(fail, sizeof(fail), "fail", owner);
    touch_file(fail);

    sound_init();
    sound_play(SFX_HIT);
    assert(wait_for_log(owner, 1, pids, paths) == 1);
    sleep_ms(50);
    for (int i = 0; i < 32; i++) sound_play(SFX_BITE);
    sleep_ms(50);
    int count = read_log(owner, pids, paths);
    sound_cleanup();

    stop_fake_players(owner);
    remove_test_files(owner);
    assert(count == 1);
}

static void run_one_instance(void) {
    pid_t owner = getpid();
    pid_t pids[MAX_TEST_PIDS];
    char paths[MAX_TEST_PIDS][PATH_MAX];
    char gate[PATH_MAX];
    remove_test_files(owner);
    test_path(gate, sizeof(gate), "gate", owner);
    touch_file(gate);
    sound_init();
    sound_play(SFX_HIT);
    if (wait_for_log(owner, 1, pids, paths) != 1) _exit(2);
    sound_cleanup();
    _exit(0);
}

static pid_t start_and_wait_instance(char output_path[PATH_MAX]) {
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) run_one_instance();

    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    pid_t pids[MAX_TEST_PIDS];
    char paths[MAX_TEST_PIDS][PATH_MAX];
    assert(read_log(child, pids, paths) == 1);
    snprintf(output_path, PATH_MAX, "%s", paths[0]);
    stop_fake_players(child);
    remove_test_files(child);
    return child;
}

static void test_instances_use_distinct_paths(void) {
    char first_path[PATH_MAX];
    char second_path[PATH_MAX];
    pid_t first = start_and_wait_instance(first_path);
    pid_t second = start_and_wait_instance(second_path);

    assert(first != second);
    assert(strcmp(first_path, second_path) != 0);
}

int main(void) {
    install_fake_players();
    test_burst_is_bounded_and_cleanup_reaps();
    test_failed_player_disables_retries();
    test_instances_use_distinct_paths();
    remove_fake_players();
    puts("POSIX sound tests passed");
    return 0;
}
