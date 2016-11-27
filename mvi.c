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
static void die(char *fmt, ...);
static void loadfile(char *name);
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
static void moveup(void);
static void movedown(void);
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
die(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1); /* TODO handle curses settings */
}

void
loadfile(char *name)
{
	FILE *f;
	char *buf;

	if (!(f = fopen(name, "r")))
		die("loadfile:"); /* TODO graceful handling */

	if (!(buf = calloc(BUFSIZ, sizeof(char))))
		die("loadfile:"); /* TODO graceful handling */

	while (fread(buf, sizeof(char), BUFSIZ/2, f))
		cur = insertstr(cur, buf);

	fclose(f);
	/* TODO remove last newline */
}

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
	case 'j':
		movedown();
		break;
	case 'k':
		moveup();
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
	if (cmd[0] == 'q') /* TODO save guard */
		edit = 0;
}

void
draw(void)
{
	Line *l;

	move(0,0);
	for (l = fstln; l; l = l->n)
		printw("%s\n", l->s); /* TODO consider terminal size */
	while (getcury(stdscr) < (getmaxy(stdscr) - 1))
		printw("~\n");

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
	cur = insertstr(cur, s);
	/* TODO free s? */
}

Position
insertstr(Position p, char *src)
{
	/* Takes a string and inserts it at the given position.
	 * If there isn't enough space in dest, more will be allocated.
	 * The previous text must be shifted to make room.
	 * Upon NL, a new line is made and filled with the remainding text.
	 * It returns the position after the newly inserted text.
	 * TODO remove this comment when confirmed functionality.
	 */

	size_t len;
	char *ins;

	len = lflen(src);
	if ((p.l->l + len) >= BUFSIZ*p.l->m) /* enough space? */
		return p; /* TODO allocate more room */

	ins = p.l->s + p.o;
	memmove(ins + len, ins, strlen(ins)); /* make room TODO only if needed*/
	memmove(ins, src, len); /* insert text */
	p.o += len; /* update offset */

	if (len < strlen(src)) { /* TODO safe string handling */
		p.l = newline(p.l, p.l->n);
		p.o = 0;
		p = insertstr(p, src + len + 1);
		insertstr(p, ins + len);
		ins[len] = '\0';
	}

	return p;
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
	/* TODO rethink movement system */
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

void
moveup(void)
{
	if (!cur.l->p) /* is there anything above? */
		return;

	/* TODO beginning of termheight etc */

	cur.o = MIN(utfnlen(cur.l->s, cur.o), cur.l->p->l - 1); /* TODO utf */
	cur.l = cur.l->p;
	cury--;
	curx = utfnlen(cur.l->s, cur.o); /* TODO scale for looong lines */
}

void
movedown(void)
{
	if (!cur.l->n) /* theres nothing below */
		return;

	/* TODO if nextline is shorter */
	/* TODO end of termheight etc */

	cur.l = cur.l->n;
	cury++;
}

int
prevlen(char *s, int o)
{
	int max, i, n;
	Rune p; /* TODO name should be r */

	max = MIN(UTFmax, o);
	for (i = max; i > 0; i--) {
		n = charntorune(&p, s+o-i, i);
		if (n == i)
			return n;
	}
	return 0; /* TODO -1? */
}

int
lflen(char *s) /* length until line feed */
{
	char *lf;
	int len;

	if ((lf = strchr(s, '\n'))) /* search for Line Feed */
		len = lf - s;
	else
		len = strlen(s); /* TODO trust strlen? */

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
main(int argc, char *argv[])
{
	if (argc > 2)
		die("usage: %s [file]", argv[0]);

	init();

	if (argc == 2)
		loadfile(argv[1]);

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
