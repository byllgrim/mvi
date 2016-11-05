/* See LICENSE file */
#include <curses.h>
#include <locale.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

/* macros */
#define ISESC(c) (c == 27)
/* TODO VLEN use utflen? */
#define MIN(A, B) ((A) < (B) ? (A) : (B))

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
static void cmdinsert(void); /* TODO change name */
static void cmdnormal(void); /* TODO change name? */
static void draw(void);
static void insertch(int c);
static void insertstr(char *str);
static Line *newline(void); /* TODO arguments next/prev */
static void moveleft(void);
static int prevlen(char *s, int o); /* TODO dont need arguments? */
static void tmpstatus(char *fmt, ...); /* TODO temporary? */

/* global variables */
static int edit = 1;
static int cury, curx; /* TODO rename y,x or posy,posx */
static Cursor cur; /* TODO be mindful of the stack */
/* TODO firstline? */
static Mode mode = NORMAL;

/* function definitions */
void
cmdinsert(void)
{
	int c;

	c = getch();
	if (ISESC(c)) {
		mode = NORMAL;
		moveleft();
	} else
		insertch(c);
}

void
cmdnormal(void)
{
	switch(getch()) {
	case 'h':
		moveleft();
		break;
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
	mvprintw(0, 0, "%s", cur.l->s); /* TODO print all lines */
	move(cury, curx);
	refresh();
}

void
insertch(int c) /* TODO change variable name */
{
	int i;
	char *s;

	if (KEY_MIN <= c && c <= KEY_MAX) /* curses special keys */
		return; /* TODO is it necessary? */

	s = calloc(5, sizeof(char));
	s[0] = (char)c;
	for (i = 1; i < 4; i++) { /* find utf char */
		if (fullrune(s, i))
			break;
		s[i] = getch();
        }
	insertstr(s);
	++curx;
}

void
insertstr(char *src)
{
	size_t len = strlen(src); /* TODO trust strlen? */
	/* TODO char *ins = cur.l->s + cur.o */

	if ((cur.l->l + len) >= (BUFSIZ * cur.l->m)) /* not enough space */
		return; /* TODO make more space */

	if (cur.l->s[cur.o] != '\0') /* not at end of string */
		memmove(cur.l->s+len+cur.o, cur.l->s+cur.o, cur.l->l - cur.o);

	memcpy(cur.l->s + cur.o, src, len);
	/* TODO consider \n inside line */
	cur.l->l += len;
	cur.o += len;
}

Line *
newline(void)
{
	Line *l;

	l = calloc(1, sizeof(Line)); /* TODO error checking */
	l->s = calloc(BUFSIZ, sizeof(char));
	l->p = NULL;
	l->n = NULL;
	l->m = 1;

	return l;
}

void
moveleft(void)
{
	if (!cur.o) /* is at beginning of string */
		return;

	if (!curx) /* is at beginning of termwidth */
		return; /* TODO crawl upwards */

	--curx;
	cur.o -= prevlen(cur.l->s, cur.o);
}

int
prevlen(char *s, int o)
{
	int max, i, n;
	Rune p; /* TODO NULL */

	max = MIN(UTFmax, o);
	for (i = max; i > 0; i--) {
		n = charntorune(&p, s+o-i, i);
		if (n == i)
			return n;
	}
	return 0; /* TODO -1 */
}

void
tmpstatus(char *fmt, ...)
{
	char *msg;
	int len;
	va_list ap;

	len = getmaxx(stdscr);
	msg = calloc(len+1, sizeof(char));

	va_start(ap, fmt);
	vsnprintf(msg, len, fmt, ap);
	va_end(ap);

	mvprintw(LINES-3, 0, msg);
}

int
main()
{
	setlocale(LC_ALL, ""); /* TODO is this necessary? */
	initscr();
	raw();
	noecho();
	cury = curx = 0;
	cur.o = 0;
	cur.l = newline();
	/* TODO init()? */
	/* TODO setlocale? */

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
