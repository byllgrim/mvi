CPPFLAGS = -D_POSIX_C_SOURCE=200112L
CFLAGS = -std=c89 -pedantic-errors -Wall -Wextra -Os ${CPPFLAGS}
LDFLAGS = -lcurses

all:
	cc -o mvi ${CFLAGS} ${LDFLAGS} mvi.c
