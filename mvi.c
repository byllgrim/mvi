/* See LICENSE file */
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

/* macros */
#define ISBS(c) (c == 127)
#define ISESC(c) (c == 27)
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define CURLINE getcury(stdscr) /* omitted () to match curses' LINES */
#define CURCOL getcurx(stdscr)

/* types */
enum {INSERT, NORMAL};

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
	size_t o;  /* offset */
} Position;

/* function declarations */
static void die(char *fmt, ...);
static void loadfile(char *name); /* TODO return values */
static void savefile(char *name);
static void init(void);
static void cmdinsert(void); /* TODO change names?*/
static void cmdnormal(void);
static void cmdcommand(void);
static void runcmd(char *cmd);
static void draw(void);
static void calcdrw(void);
static void insertch(int c);
static Position insertstr(Position p, char *str);
static Position backspace(Position p);
static Line *newline(Line *p, Line *n);
static void rmline(Line *l);
static void moveleft(void);
static void moveright(void);
static void moveup(void);
static void movedown(void);
static int prevlen(char *s, int o);
static size_t lflen(char *s); /* length til first \n */
static void setstatus(char *fmt, ...);
static void printstatus(void);

/* global variables */
static int edit = 1;
static int touched = 0;
static Position cur;
static Position drw; /* first to be drawn on screen */
static Line *fstln;
static int mode = NORMAL;
static char *status = NULL;
static char *filename = NULL;

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
	Position p;

	if (!(f = fopen(name, "r")))
		die("loadfile:"); /* TODO graceful handling */

	if (!(buf = calloc(BUFSIZ, sizeof(char))))
		die("loadfile:"); /* TODO graceful handling */

	p = cur;
	while (fread(buf, sizeof(char), BUFSIZ/2, f))
		p = insertstr(p, buf);

	if (p.l->s[0] == '\0')
		rmline(p.l);

	strncpy(filename, name, BUFSIZ); /* TODO LINSIZ? */
	fclose(f);
}

void
savefile(char *name)
{
	FILE *f;
	Line *l;

	if (name && name[0] != '\0')
		strncpy(filename, name, BUFSIZ); /* TODO LINSIZ? */

	if (!(f = fopen(filename, "w"))) {
		setstatus("savefile: %s", strerror(errno));
		return;
	}

	for (l = fstln; l; l = l->n)
		fprintf(f, "%s\n", l->s);

	setstatus("\"%s\"", filename);
	touched = 0;
	fclose(f);
}

void
init(void)
{
	setlocale(LC_ALL, ""); /* TODO is this necessary? */
	initscr();
	raw();
	noecho();
	cur.o = drw.o = 0;
	cur.l = drw.l = fstln = newline(NULL, NULL);
	status = calloc(BUFSIZ+1, sizeof(char)); /* TODO LINSIZ? */
	filename = calloc(BUFSIZ+1, sizeof(char));
}

void
cmdinsert(void)
{
	int c;

	c = getch();
	if (ISESC(c)) {
		mode = NORMAL;
		moveleft();
	} else if (ISBS(c)) {
		cur = backspace(cur);
		touched = 1;
	} else {
		insertch(c);
		touched = 1;
	}
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
	size_t i;

	cmd = calloc(BUFSIZ, sizeof(char)); /* TODO paranoid checking */

	setstatus(":");
	printstatus();

	i = 0;
	while ((cmd[i] = getch()) != '\n' && !ISESC(cmd[i])) {
		if (!isprint(cmd[i]))
			cmd[i--] = '\0';
		else
			printw("%c", cmd[i++]);
	}
	cmd[i] = '\0';

	runcmd(cmd);
	/* TODO free(cmd) */
	/* TODO fix memleaks all over the place */
}

void
runcmd(char *cmd)
{
	if (cmd[0] == 'q') {
		if (!touched || (cmd[1] == '!'))
			edit = 0;
		else
			setstatus("unsaved changes; q! to override");
	} else if (cmd[0] == 'w') {
		if ((cmd[1] == ' ') && (cmd[2] != '\0'))
			savefile(cmd + 2);
		else
			savefile(NULL);
	} else if (cmd[0] == 'd') {
		if (cur.l->n) {
			if (cur.l == fstln)
				fstln = cur.l->n;
			if (cur.l == drw.l) {
				drw.l = cur.l->n;
				drw.o = 0;
			}
			cur.l = cur.l->n;
			cur.o = 0;
			rmline(cur.l->p);
		} else if (cur.l->p) {
			cur.l = cur.l->p;
			cur.o = 0;
			rmline(cur.l->n);
		}
		touched = 1;
		/* TODO refactor into separate function */
	}
}

void
draw(void)
{
	Line *l;
	size_t x, y, o, i;

	calcdrw();
	move(0,0);
	for (l = drw.l, o = drw.o; l; l = l->n, o = 0) {
		if (l == cur.l) {
			y = CURLINE;
			y += utfnlen(l->s + o, cur.o - o) / COLS;
		}
		for (i = o; l->s[i] && LINES - CURLINE - 1; i++)
			printw("%c", l->s[i]);
		printw("\n");
	}
	while (CURLINE < LINES - 1)
		printw("~\n");

	printstatus();
	x = utfnlen(cur.l->s, cur.o) % COLS;
	move(y, x);
	refresh();
}

void
calcdrw(void)
{ /* TODO utf lengths */
	Line *l;
	size_t len, rows, o;

	len = utfnlen(cur.l->s, cur.o);
	if (drw.l == cur.l && cur.o < drw.o) {
		drw.o = (len / COLS) * COLS;
		return;
		/* TODO is this redundant? */
	}

	if (cur.l == drw.l->p) {
		drw.l = drw.l->p;
		drw.o = (len / COLS) * COLS;
		return; /* TODO reconsider this optimization */
	}

	/* TODO precalc vlen */
	l = drw.l;
	o = drw.o;
	for (rows = 0; l && l != cur.l; l = l->n, o = 0) {
		len = utflen(l->s + o);
		rows += len / COLS + 1;
	}
	rows += utfnlen(cur.l->s, cur.o) / COLS + 1;
	while (rows >= (size_t)LINES) {
		len = utflen(drw.l->s);
		if (drw.o + COLS < len) { /* TODO utf */
			drw.o += COLS;
			rows--;
		} else {
			drw.l = drw.l->n;
			drw.o = 0;
			rows--;
		}
	}
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
	cur = insertstr(cur, s);
	/* TODO free s? */
}

Position
insertstr(Position p, char *src)
{
	size_t len;
	char *ins;

	len = lflen(src);
	if ((p.l->l + len) >= BUFSIZ*p.l->m) /* enough space? */
		return p; /* TODO allocate more room */

	ins = p.l->s + p.o;
	memmove(ins + len, ins, strlen(ins)); /* make room TODO only if needed*/
	memmove(ins, src, len);
	p.o += len;

	if (len < strlen(src)) { /* TODO safe string handling */
		p.l = newline(p.l, p.l->n);
		p.o = 0;
		p = insertstr(p, src + len + 1);
		insertstr(p, ins + len);
		ins[len] = '\0';
	}

	return p;
}

Position
backspace(Position p)
{
	char *dest, *src;
	size_t n;

	if (!p.o)
		return p;

	src = cur.l->s + cur.o;
	dest = src - 1; /* TODO prevlen */
	n = strlen(src);
	memcpy(dest, src, n);
	src[n - 1] = '\0'; /* TODO prevlen */
	p.o -= 1; /* TODO prevlen */

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
rmline(Line *l)
{
	if (l->p)
		l->p->n = l->n;
	if (l->n)
		l->n->p = l->p;
	free(l->s); /* TODO if (l->s) */
	/* TODO free(l) */
}

void
moveleft(void)
{
	if (!cur.o)
		return;

	cur.o -= prevlen(cur.l->s, cur.o);
}

void
moveright(void)
{
	Rune p;

	if (cur.l->s[cur.o] == '\0')
		return;

	cur.o += chartorune(&p, cur.l->s + cur.o); /* TODO nextlen()? */
}

void
moveup(void)
{
	size_t hlen, o;

	hlen = utfnlen(cur.l->s, cur.o);

	if (!cur.l->p && hlen < (size_t)COLS)
		return;

	if (hlen >= (size_t)COLS) {
		cur.o -= COLS; /* TODO saved offset? */
	} else {
		/* TODO proper utf vlen to offset */
		cur.l = cur.l->p;
		o = cur.o;
		cur.o = (utflen(cur.l->s) / COLS) * COLS;
		cur.o += MIN(o, utflen(cur.l->s + cur.o) - 1);
		/* TODO clean up this logical mess */
	}
}

void
movedown(void)
{
	size_t taillen, idlecol;

	taillen = utflen(cur.l->s + cur.o);
	idlecol = COLS - CURCOL;

	if (!cur.l->n && taillen <= idlecol)
		return;

	if (taillen > idlecol) {
		if (taillen > (size_t)COLS)
			cur.o += COLS; /* TODO utf */
		else
			cur.o = strlen(cur.l->s) - 1;
	} else {
		/* TODO proper utf vlen to offset */
		cur.o = MIN((size_t)CURCOL, utflen(cur.l->n->s)-1);
		cur.l = cur.l->n;
	}
}

int
prevlen(char *s, int o)
{
	int max, i, n;
	Rune r;

	max = MIN(UTFmax, o);
	for (i = max; i > 0; i--) {
		n = charntorune(&r, s+o-i, i);
		if (n == i)
			return n;
	}
	return 0; /* TODO -1? */
}

size_t
lflen(char *s)
{
	char *lf;

	if ((lf = strchr(s, '\n'))) /* fuck CRLF */
		return lf - s;
	else
		return strlen(s); /* TODO trust strlen? */
}

void
setstatus(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(status, BUFSIZ, fmt, ap);
	va_end(ap);
}

void
printstatus(void)
{
	size_t i;

	move(LINES - 1, 0);
	for (i = 0; i < (size_t)COLS; i++)
		printw(" ");
	mvprintw(LINES - 1, 0, status);
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
	}

	endwin(); /* TODO atexit() and such? */
	return 0;
}
