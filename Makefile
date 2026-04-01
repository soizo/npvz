CC      ?= cc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11
LDFLAGS ?=

# macOS ships ncurses with wide-char support built in
UNAME_S := $(shell uname -s)
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
BIN     := npvz

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(NCURSES_LIBS) -lm

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)

.PHONY: all clean install uninstall
