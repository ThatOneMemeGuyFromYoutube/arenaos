# NT31 - Windows NT 3.1, recreated from memory in portable C.
#
# Targets:
#   make            build nt31            (cc, any platform)
#   make CC=musl-gcc build with musl      (Linux: apt install musl-tools)
#   make CC=clang   build with clang      (macOS / Linux)
#   make strict     build with -std=c11 + strict POSIX feature test set
#                   (exercises the same code paths musl provides)
#   make stub-win   syntax-check the _WIN32 code path against a stub windows.h
#   make run        build and run
#   make clean

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
LDFLAGS ?=

SRC := src/main.c src/term.c src/kernel.c src/win.c src/apps.c
BIN := nt31

.PHONY: all strict stub-win run clean

all: $(BIN)

$(BIN): $(SRC) src/nt.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

strict: $(SRC) src/nt.h
	$(CC) -std=c11 -D_POSIX_C_SOURCE=200809L -Wpedantic -Werror -O2 \
	    -o $(BIN).strict $(SRC) $(LDFLAGS)
	@echo "strict C11/POSIX build ok: $(BIN).strict"

stub-win: $(SRC) src/nt.h
	$(CC) -std=c11 -Wall -Wextra -Werror -D_WIN32 -Itest/win_stub \
	    -fsyntax-only $(SRC)
	@echo "windows branch syntax ok"

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN) $(BIN).strict nt31_shot.txt
