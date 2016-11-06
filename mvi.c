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
static void init(void);
static void cmdinsert(void); /* TODO change name */
static void cmdnormal(void); /* TODO change name? */
static void cmdcommand(void);
static void runcmd(char *cmd);
static void draw(void);
static void insertch(int c);
static void insertstr(char *str);
static Line *newline(void); /* TODO arguments next/prev */
static void moveleft(void);
static int prevlen(char *s, int o); /* TODO dont need arguments? */
static int lflen(char *s); /* TODO return size_t? */
static void setstatus(char *fmt, ...);

/* global variables */
static int edit = 1;
static int cury, curx; /* TODO rename y,x or posy,posx */
static Cursor cur; /* TODO be mindful of the stack */
/* TODO firstline? */
static Mode mode = NORMAL;
static char *status = NULL;

/* function definitions */
void
init(void)
{
	setlocale(LC_ALL, ""); /* TODO is this necessary? */
	initscr();
	raw();
	noecho();
	cury = curx = 0;
	cur.o = 0;
	cur.l = newline();
	status = calloc(BUFSIZ+1, sizeof(char)); /* TODO LINSIZ? */
}

void
cmdinsert(void)
{
	int c;

	c = getch();
	/* TODO if backspace prevlen to \0 */
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
	case ':':
		cmdcommand();
		break;
	case 'h':
		moveleft();
		break;
	case 'i':
		mode = INSERT;
		break;
	}
}

void
cmdcommand(void)
{
	char *cmd;
	int i = 0;

	cmd = calloc(BUFSIZ, sizeof(char));
	/* TODO clear status */
	mvprintw(LINES-1, 0, ":");

	while ((cmd[i] = getch()) != '\n' && !ISESC(cmd[i]))
		printw("%c", cmd[i++]);

	runcmd(cmd);
	/* TODO free(cmd) */
	/* TODO fix memleaks all over the place */
}

void
runcmd(char *cmd)
{
	/* TODO more commands. recursive descent? */
	if (cmd[0] == 'q')
		edit = 0;
}

void
draw(void)
{
	mvprintw(0, 0, "%s", cur.l->s); /* TODO print all lines */
	mvprintw(LINES-1, 0, status); /* TODO getmaxy() */
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
	size_t totlen = strlen(src); /* TODO trust strlen? */
	size_t fstlen = lflen(src); /* length until \n or \0 */
	char *ins = cur.l->s + cur.o; /* insert point */

	/* enough space? */
	if ((cur.l->l + fstlen) >= (BUFSIZ * cur.l->m))
		return; /* TODO make more space */

	/* middle of string? */
	if (cur.l->s[cur.o] != '\0')
		memmove(ins + fstlen, ins, cur.l->l - cur.o);

	/* insert new stuff */
	memcpy(ins, src, fstlen);
	cur.l->l += fstlen;
	cur.o += fstlen;

	/* the rest goes in a newline */
	if (fstlen == totlen) /* no newlines */
		return;
	setstatus("newline");
	/* TODO newline(src[totlen-fstlen]) */
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
	return 0; /* TODO -1? */
}

int
lflen(char *s)
{
	char *lf;
	int len;

	if ((lf = strchr(s, '\n'))) /* search for Line Feed */
		len = lf - s;
	else
		len = strlen(s);

	return len;
}

void
setstatus(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(status, BUFSIZ, fmt, ap);
	va_end(ap);
}

int
main()
{
	init();

	while (edit) {
		draw();

		if (mode == INSERT)
			cmdinsert();
		else if (mode == NORMAL)
			cmdnormal();
		else {
			mode = NORMAL; /* TODO think about this */
			setstatus("invalid mode!");
		}
	}

	endwin(); /* TODO atexit() and such? */
	return 0;
}
