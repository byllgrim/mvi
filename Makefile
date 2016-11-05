CPPFLAGS = -D_POSIX_C_SOURCE=200112L
CFLAGS = -std=c89 -pedantic-errors -Wall -Wextra -Os ${CPPFLAGS}
LDFLAGS = -lncursesw -lutf

all:
	cc -o mvi ${CFLAGS} mvi.c ${LDFLAGS}
