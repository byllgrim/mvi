/* See LICENSE file */
#include <curses.h>
#include <stdlib.h>

/* macros */
#define ISESC(c) (c == 27)

/* types */
typedef enum {
	INSERT,
	NORMAL
} Mode; /* TODO evaluate this way of using enums */

typedef struct Line Line;
struct Line {
	char *s;  /* string content */
	int l;    /* length TODO including \0? */
	int v;    /* visual length */
	int m;    /* multiples of BUFSIZ TODO LINSIZ? */
	Line *p;  /* previous line */
	Line *n;  /* next line */
};

typedef struct {
	Line *l;
	int o;  /* offset */
	        /* TODO visual offset? */
} Cursor;

/* function declarations */
static void cmdinsert(void);
static void cmdnormal(void); /* TODO change name? */
static void draw(void);

/* global variables */
static int edit = 1;
static Cursor cur; /* TODO be mindful of the stack */
static Mode mode = NORMAL;

/* function definitions */
void
cmdinsert(void)
{
	int c;

	c = getch();
	if (ISESC(c))
		mode = NORMAL;
	else
		addch(c); /* TODO this is temporary */
}

void
cmdnormal(void)
{
	switch(getch()) {
	case 'i':
		mode = INSERT;
		break;
	case 'q': /* TODO this is temporary */
		edit = 0;
		break;
	}
}

void
draw(void)
{
	/* TODO print all lines */
	refresh();
}

int
main()
{
	initscr();
	raw();
	noecho();
	cur.l = calloc(1, sizeof(Line)); /* TODO error checking */
	cur.o = 0;
	/* TODO init()? */

	while (edit) {
		draw();
		if (mode == INSERT)
			cmdinsert();
		else if (mode == NORMAL)
			cmdnormal();
		else {
			mvprintw(LINES-3, 0, "invalid mode");
			getch();
			mode = NORMAL; /* TODO think about this */
			/* TODO setstatus()? */
		}
	}

	endwin();
	return 0;
}
