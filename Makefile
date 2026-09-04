CC      ?= cc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11
LDFLAGS ?=

# macOS ships ncurses with wide-char support built in
UNAME_S := $(shell uname -s)
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

SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:.c=.o)
HDR     := $(wildcard src/*.h)
BIN     := npvz
TEST_BIN := tests/test_rules
TEST_SRC := tests/test_rules.c tests/sound_stub.c src/game.c src/board.c src/crowd.c src/plant.c src/zombie.c src/projectile.c

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(NCURSES_LIBS) -lm

src/%.o: src/%.c $(HDR)
	$(CC) $(CFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(HDR)
	$(CC) $(CFLAGS) $(NCURSES_CFLAGS) -Isrc -o $@ $(TEST_SRC) -lm

clean:
	rm -f $(OBJ) $(BIN) $(TEST_BIN)

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)

.PHONY: all test clean install uninstall
