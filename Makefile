CC       ?= cc
CPPFLAGS ?=
CFLAGS   ?= -Wall -Wextra -O2 -std=c11
LDFLAGS  ?=

SDL_MIXER_AVAILABLE := $(shell pkg-config --exists SDL2_mixer 2>/dev/null && echo yes)
SDL_MIXER_CFLAGS := $(shell pkg-config --cflags SDL2_mixer 2>/dev/null)
SDL_MIXER_LIBS   := $(shell pkg-config --libs SDL2_mixer 2>/dev/null)

# macOS ships ncurses with wide-char support built in
UNAME_S := $(shell uname -s)
SOUND_BACKEND ?= auto

# pi-lens-ignore: SC1072, SC1064, SC1065, SC1073
ifeq ($(SOUND_BACKEND),auto)
    ifeq ($(SDL_MIXER_AVAILABLE),yes)
        SELECTED_SOUND_BACKEND := sdl
    else ifeq ($(UNAME_S),Darwin)
        SELECTED_SOUND_BACKEND := audioqueue
    else
        SELECTED_SOUND_BACKEND := posix
    endif
else
    SELECTED_SOUND_BACKEND := $(SOUND_BACKEND)
endif

ifeq ($(SELECTED_SOUND_BACKEND),sdl)
    ifneq ($(SDL_MIXER_AVAILABLE),yes)
        $(error SOUND_BACKEND=sdl requires pkg-config SDL2_mixer)
    endif
    SOUND_CPPFLAGS := -DNPVZ_SOUND_SDL $(SDL_MIXER_CFLAGS)
    SOUND_LIBS := $(SDL_MIXER_LIBS)
else ifeq ($(SELECTED_SOUND_BACKEND),audioqueue)
    ifneq ($(UNAME_S),Darwin)
        $(error SOUND_BACKEND=audioqueue is supported only on macOS)
    endif
    SOUND_CPPFLAGS := -DNPVZ_SOUND_AUDIOQUEUE
    SOUND_LIBS := -framework AudioToolbox -framework CoreFoundation
else ifeq ($(SELECTED_SOUND_BACKEND),posix)
    SOUND_CPPFLAGS := -DNPVZ_SOUND_POSIX \
        $(if $(filter Linux,$(UNAME_S)),-DNPVZ_SOUND_LINUX)
else
    $(error SOUND_BACKEND must be auto, sdl, audioqueue, or posix)
endif

# pi-lens-ignore: SC1072, SC1064, SC1065, SC1073
ifeq ($(UNAME_S),Darwin)
    NCURSES_CFLAGS :=
    NCURSES_LIBS   := -lncurses
else
    NCURSES_CFLAGS := $(shell pkg-config --cflags ncursesw 2>/dev/null)
    NCURSES_LIBS   := $(shell pkg-config --libs ncursesw 2>/dev/null || echo -lncursesw)
endif

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/npvz

SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:.c=.o)
HDR     := $(wildcard src/*.h)
BIN     := npvz
TEST_BIN := tests/test_rules
TEST_SRC := tests/test_rules.c tests/sound_stub.c src/game.c src/board.c src/crowd.c src/plant.c src/zombie.c src/projectile.c
VOICE_TEST_BIN := tests/test_sound_voice
SDL_TEST_BIN := tests/test_sound_sdl
POSIX_TEST_BIN := tests/test_sound_posix
LIFECYCLE_TEST_BIN := tests/test_lifecycle
RENDER_TEST_BIN := tests/test_render_overlay
RENDER_TEST_OBJ := tests/render_test.o
INPUT_TEST_BIN := tests/test_input
ASCIIART := asciiart/newspaper-zombie.txt

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(NCURSES_LIBS) $(SOUND_LIBS) -lm

src/lifecycle.o src/sound.o: CPPFLAGS += -D_POSIX_C_SOURCE=200809L
src/render.o: CPPFLAGS += -D_XOPEN_SOURCE_EXTENDED=1

src/%.o: src/%.c $(HDR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NCURSES_CFLAGS) $(SOUND_CPPFLAGS) \
		-DNPVZ_DATA_DIR=\"$(DATADIR)\" -c -o $@ $<

SDL_TESTS :=
ifeq ($(SDL_MIXER_AVAILABLE),yes)
    SDL_TESTS := $(SDL_TEST_BIN)
endif

test: $(TEST_BIN) $(VOICE_TEST_BIN) $(SDL_TESTS) $(POSIX_TEST_BIN) \
		$(LIFECYCLE_TEST_BIN) $(RENDER_TEST_BIN) $(INPUT_TEST_BIN)
	./$(TEST_BIN)
	./$(VOICE_TEST_BIN)
	./$(LIFECYCLE_TEST_BIN)
	./$(RENDER_TEST_BIN)
	./$(INPUT_TEST_BIN)
ifeq ($(SDL_MIXER_AVAILABLE),yes)
	SDL_AUDIODRIVER=dummy ./$(SDL_TEST_BIN)
endif
	./$(POSIX_TEST_BIN)
	./tests/test_audioqueue_build.sh

$(TEST_BIN): $(TEST_SRC) $(HDR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NCURSES_CFLAGS) -Isrc -o $@ $(TEST_SRC) -lm

$(VOICE_TEST_BIN): tests/test_sound_voice.c src/sound_voice.c src/sound_voice.h src/sound.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc -o $@ tests/test_sound_voice.c src/sound_voice.c

$(SDL_TEST_BIN): tests/test_sound_sdl.c src/sound.c src/sound_voice.c $(HDR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_POSIX_C_SOURCE=200809L \
		-DNPVZ_SOUND_SDL $(SDL_MIXER_CFLAGS) \
		-Isrc -o $@ tests/test_sound_sdl.c src/sound.c src/sound_voice.c \
		$(SDL_MIXER_LIBS) -lm

$(POSIX_TEST_BIN): tests/test_sound_posix.c src/sound.c src/sound_voice.c $(HDR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_POSIX_C_SOURCE=200809L \
		-DNPVZ_SOUND_POSIX -Isrc -o $@ tests/test_sound_posix.c \
		src/sound.c src/sound_voice.c -lm

$(LIFECYCLE_TEST_BIN): tests/test_lifecycle.c src/lifecycle.c src/lifecycle.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -D_POSIX_C_SOURCE=200809L \
		-Isrc -o $@ tests/test_lifecycle.c src/lifecycle.c

$(RENDER_TEST_OBJ): src/render.c $(HDR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NCURSES_CFLAGS) \
		-DNPVZ_RENDER_TEST -D_XOPEN_SOURCE_EXTENDED=1 \
		-DNPVZ_DATA_DIR=\"$(DATADIR)\" -c -o $@ src/render.c

$(RENDER_TEST_BIN): tests/test_render_overlay.c $(RENDER_TEST_OBJ) $(HDR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NCURSES_CFLAGS) -Isrc \
		-DNPVZ_DATA_DIR=\"$(DATADIR)\" -o $@ tests/test_render_overlay.c \
		$(RENDER_TEST_OBJ) tests/sound_stub.c src/ui.c src/board.c \
		src/crowd.c src/plant.c src/zombie.c src/projectile.c \
		$(NCURSES_LIBS) -lm

$(INPUT_TEST_BIN): tests/test_input.c src/input.c src/input.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(NCURSES_CFLAGS) -Isrc -o $@ \
		tests/test_input.c src/input.c $(NCURSES_LIBS)

clean:
	rm -f $(OBJ) $(BIN) $(TEST_BIN) $(VOICE_TEST_BIN) $(SDL_TEST_BIN) \
		$(POSIX_TEST_BIN) $(LIFECYCLE_TEST_BIN) $(RENDER_TEST_BIN) \
		$(RENDER_TEST_OBJ) $(INPUT_TEST_BIN)

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(DATADIR)/asciiart
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)
	install -m 644 $(ASCIIART) $(DESTDIR)$(DATADIR)/asciiart/

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN) \
		$(DESTDIR)$(DATADIR)/asciiart/newspaper-zombie.txt

.PHONY: all test clean install uninstall
