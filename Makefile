CFLAGS = -std=c89 -pedantic-errors -Wall -Wextra -Os

all:
	cc -o mvi ${CFLAGS} mvi.c
