/* See LICENSE file */
#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

/* macros */
#define ISESC(c) (c == 27)
/* TODO VLEN use utflen? */

/* types */
typedef enum {
	INSERT,
	NORMAL
} Mode; /* TODO evaluate this way of using enums */

typedef struct Line Line;
struct Line {
	char *s;  /* string content */
	size_t l; /* length excluding \0 */
	size_t v; /* visual length */
	size_t m; /* multiples of BUFSIZ TODO LINSIZ? */
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
static void insertch(int r);
static void insertstr(char *str);

/* global variables */
static int edit = 1;
static Cursor cur; /* TODO be mindful of the stack */
/* TODO firstline? */
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
		insertch(c);
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
	mvprintw(0, 0, "%s", cur.l->s); /* TODO only temporary */
	refresh();
}

void
insertch(int r)
{
	int i;
	char *s;

	s = calloc(5, sizeof(char));
	s[0] = r;
	for (i = 1; i < 4; i++) { /* find utflen */
		if (fullrune(s, i))
			break;
		s[i] = getch();
        }
	insertstr(s);
}

void
insertstr(char *src)
{
	size_t len = strlen(src); /* TODO trust strlen? */
	if ((cur.l->l + len) < (BUFSIZ * cur.l->m)) { /* TODO logic */
		strncat(cur.l->s, src, len);
		/* TODO consider \n inside line */
		cur.l->l += len;
	}
}

int
main()
{
	initscr();
	raw();
	noecho();
	/* TODO init()? */
	cur.o = 0;
	cur.l = calloc(1, sizeof(Line)); /* TODO error checking */
	cur.l->s = calloc(BUFSIZ, sizeof(char));
	cur.l->p = NULL;
	cur.l->n = NULL;
	cur.l->m = 1;
	/* TODO newline() */

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
