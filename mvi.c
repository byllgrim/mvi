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
} Position;

/* function declarations */
static void init(void);
static void cmdinsert(void); /* TODO change name */
static void cmdnormal(void); /* TODO change name? */
static void cmdcommand(void);
static void runcmd(char *cmd);
static void draw(void);
static void insertch(int c);
static Position insertstr(Position p, char *str);
static Line *newline(Line *p, Line *n);
static void moveleft(void);
static void moveright(void);
static int prevlen(char *s, int o); /* TODO dont need arguments? */
static int lflen(char *s); /* TODO return size_t? */
static void setstatus(char *fmt, ...);

/* global variables */
static int edit = 1;
static int cury, curx; /* TODO rename y,x or posy,posx */
static Position cur; /* TODO be mindful of the stack */
static Line *fstln;
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
	cur.l = fstln = newline(NULL, NULL);
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
	case 'l':
		moveright();
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
	Line *l;

	move(0,0);
	for (l = fstln; l; l = l->n)
		printw("%s\n", l->s); /* TODO consider terminal size */

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
	++curx; /* TODO moveright()? */
	cur = insertstr(cur, s); /* TODO Position from argument? */
	/* TODO free s? */
}

Position
insertstr(Position p, char *src)
{
	Line *l = p.l;
	int o = p.o;
	size_t totlen = strlen(src); /* TODO trust strlen? */
	size_t fstlen = lflen(src); /* length until \n or \0 */
	char *ins = l->s + o; /* insert point */
	Position nxt = p; /* TODO unecessary? or rename new? */

	if ((l->l + fstlen) >= (BUFSIZ * l->m)) /* enough space? */
		return nxt; /* TODO make more space */

	if (l->s[o] != '\0') /* middle of string? */
		memmove(ins + fstlen, ins, l->l - o);

	/* insert new stuff */
	memcpy(ins, src, fstlen);
	nxt.l->l += fstlen;
	nxt.o += fstlen;

	/* the rest goes in a newline */
	if (fstlen == totlen) /* no newlines */
		return nxt;
	curx = 0;
	cury++; /* TODO movedown(); */
	nxt.l = newline(p.l, p.l->n);
	nxt.o = 0;
	nxt = insertstr(nxt, src + (totlen-fstlen));
	nxt = insertstr(nxt, p.l->s + p.o);
	nxt.o -= (strlen(p.l->s)-p.o); /* as above strlen(p.l->s + p.o) ? */
	p.l->s[p.o] = '\0';
	/* TODO clean up this mess. refactor etc */
	return nxt; /* TODO return insert(... */
}

Line *
newline(Line *p, Line *n)
{
	Line *l;

	l = calloc(1, sizeof(Line)); /* TODO error checking */
	l->s = calloc(BUFSIZ, sizeof(char));
	l->l = l->v = 0;
	l->m = 1;
	l->p = p;
	l->n = n;

	if (p) p->n = l;
	if (n) n->p = l;

	return l;
}

void
moveleft(void)
{
	if (!cur.o) /* is at beginning of string */
		return;

	if (!curx) /* is at beginning of termwidth */
		return; /* TODO crawl upwards */

	curx--;
	cur.o -= prevlen(cur.l->s, cur.o);
}

void
moveright(void)
{
	Rune p;

	if (cur.l->s[cur.o + 1] == '\0') /* is at end of string */
		return; /* TODO what about insertch? */

	/* TODO end of termwidth */
	/* TODO end of termheight etc */

	curx++;
	cur.o += chartorune(&p, cur.l->s + cur.o); /* TODO nextlen()? */
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
