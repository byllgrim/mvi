/* See LICENSE file for copyright and license details. */
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Macros */
#define VLINES(l) (1 + (l ? l->v/cols : 0))

/* Types */
#define LINSIZ 5 /* TODO 128 */
/* TODO #define ESC_SET_CURSOR_POS "\033[%u;%uH" etc from bb vi */

enum {MODE_NORMAL, MODE_INSERT};

typedef struct Line Line;
struct Line {
	char *c; /* content */
	Line *n; /* next */
	Line *p; /* previous */
	size_t s; /* buffer size */
	size_t l; /* length sans '\0' */
	size_t v; /* visual length */ /* TODO implement calcvlen() */
};

typedef struct {
	Line *l;
	size_t o; /* offset */
} Position; /* TODO rename Textpos? */

/* Function declarations */
static void die(const char *errstr, ...);
static void *ecalloc(size_t nmemb, size_t size);
static void *erealloc(void *ptr, size_t size);

static void init(void);
static void loadfile(void);
static void runcmd(char c);

static void draw(void);
static void clearscreen(void);
static void fillborder(char c, size_t r);
static void setpos(int r, int c);
static void gettermsize(void);

static Position addtext(char *s, Position p);
static char *nudgeright(Position p);
static Line *addline(Line *l);
static void moveright(void);

/* Variables */
static char *filename = NULL;
static Line *fstln; /* first line */
static Line *lstln; /* last line */
static Line *scrln; /* first line on screen */
static Position curpos;
static int mode = MODE_NORMAL;
static size_t rows, cols; /* terminal size */
static size_t crow, ccol; /* current x,y location */
static struct termios attr_saved, attr_mvi;

/* Function definitions */
void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		perror(NULL);

	return p;
}

static void *
erealloc(void *ptr, size_t size) {
	void *p;

	p = realloc(ptr, size);
	if(p == NULL) {
		free(ptr);
		die("Can't realloc.\n");
	}
	return p;
}

void
init(void) /* TODO inittext()? */
{
	Line *l;

	l = ecalloc(1, sizeof(Line));
	l->c = ecalloc(LINSIZ, sizeof(char));
	l->s = LINSIZ;
	l->l = 0;
	l->n = NULL;
	l->p = NULL;

	curpos.l = fstln = lstln = scrln = l;
	curpos.o = 0;

	gettermsize();
	crow = ccol = 0;
	tcgetattr(fileno(stdin), &attr_saved);
	attr_mvi = attr_saved;
	attr_mvi.c_lflag &= (~ICANON & ~ECHO); /* IXON ICRNL ONLCR ? */
	tcsetattr(fileno(stdin), TCSANOW, &attr_mvi);

	/* TODO
	 * Initialize interrupt handling? signal(), sigaction, SIG_IGN, SIG_DFL,
	 *     SIGHUP, SIGINT, SIGWINCH, SIGQUIT, SIGXFSZ, SIGTERM (ex posix).
	 *     Do this first, to ward against precipitous interrupts?
	 * Get terminal modes (tcgetattr)
	 * atexit(). Reset? Save?
	 * Set terminal modes? ECHO|ICANON (raw noecho) see rawmode() in bb vi
	 * Alternate screen buffer write1("\033[?1049h");
	 */
}

void
loadfile(void)
{
	int fd;
	char *buf = NULL;
	ssize_t n;
	Position p = curpos; /* TODO use pointer? */

	if (!filename) /* new file */
		return;

	if ((fd = open(filename, O_RDONLY)) < 0)
		die("mvi: error: %s\n", strerror(errno));

	buf = ecalloc(BUFSIZ + 1, 1);
	while ((n = read(fd, buf, BUFSIZ)) > 0) {
		buf[n] = '\0';
		p = addtext(buf, p);
	}

	if (!lstln->l) { /* posix lines end in '\n' */
		free(lstln->c);
		lstln = lstln->p;
		free(lstln->n);
		lstln->n = NULL;
	}
	close(fd);
	free(buf);
}

void
runcmd(char c) /* it's tricky */
{
	char *s;

	if (mode == MODE_INSERT) { /* TODO prettier sollution? */
		/* TODO what if ESC? change mode */
		s = ecalloc(2, sizeof(char)); /* TODO tidy */
		s[0] = c;
		addtext(s, curpos); /* TODO implement undo */
		moveright();
		draw();
		return;
	}

	switch (c) {
	case 'h':
		/* TODO */
		break;
	case 'i':
		mode = MODE_INSERT;
		break;
	case 'j':
		/* TODO */
		break;
	case 'k':
		/* TODO */
		break;
	case 'l':
		moveright();
	}

	draw(); /* TODO meliorate efficacy */
}

void
draw(void)
{
	Line *l;
	size_t r, c;

	clearscreen();
	setpos(0, 0);
	r = c = 0;
	for (l = scrln; l && r < rows; l = l->n, r++) {
		if (VLINES(l) < rows - r) { /* ample space */ /* TODO <= ? */
			printf("%s", l->c);
			if (r != rows - 1) /* TODO confirm correctness */
				printf("\n");
		} else {
			fillborder('@', r);
			break;
		}
	}
	if (r < rows - 1)
		fillborder('~', r);
	setpos(crow, ccol);
}

void
clearscreen(void)
{
	setpos(0, 0);
	printf("\033[2J");
	/* TODO setpos(crow, ccol) */
}

void
fillborder(char c, size_t r)
{
	setpos(r, 0);
	for (; r < rows - 1; r++)
		printf("%c\n", c);
	putchar(c);
	setpos(crow, ccol);
}

void
setpos(int r, int c)
{
	printf("\033[%u;%uH", ++r, ++c); /* TODO 1-based or 0-based? */
}

void
gettermsize(void) /* TODO YAGNI? */
{
	struct winsize argp;

	ioctl(fileno(stdout), TIOCGWINSZ, &argp); /* TODO open(ttyname(...)) */
	/* TODO error checking */
	rows = argp.ws_row;
	cols = argp.ws_col;
}

Position
addtext(char *s, Position p)
{
	char c;
	int i = 0;
	char *txt = p.l->c;

	for (c = s[0]; c != '\0'; c = s[++i]) {
		if (c == '\n') { /* TODO handle \r ? */
			p.l = addline(p.l);
			p.o = 0;
			txt = p.l->c;

			/* TODO if in the middle of a line? */
		} else {
			txt = nudgeright(p);
			txt[p.o] = c;
			++p.l->l;
			++p.o; /* TODO unicode? */
		}
	}

	return p;
}

char *
nudgeright(Position p)
{
	size_t len = p.l->l + 1;
	size_t size = p.l->s;
	size_t off = p.o;
	char *txt = p.l->c;

	if (len >= size)
		p.l->c = erealloc(p.l->c, (p.l->s += LINSIZ));

	txt = p.l->c + off;
	memmove(txt + 1, txt, len - off);
	return p.l->c;
}

Line *
addline(Line *l)
{
	Line *new;

	new = ecalloc(1, sizeof(Line));
	new->c = ecalloc(LINSIZ, sizeof(char));
	new->c[0] = '\0'; /* TODO remove this? */
	new->s = LINSIZ;
	new->l = 0;
	new->n = l->n;
	new->p = l;
	if (l->n)
		l->n->p = new;
	else
		lstln = new;
	l->n = new;

	return new;
}

void
moveright(void)
{
	if (curpos.o >= curpos.l->l - 1) /* end of line */
		return;

	if (ccol >= cols - 1) { /* end of screen */
		ccol = 0;
		++crow; /* TODO is this safe? */
	} else {
		++ccol;
	}

	++curpos.o; /* TODO unicode */
	setpos(crow, ccol);
}

int
main(int argc, char *argv[])
{
	int editing = 1; /* TODO elevate scope? */

	if (argc > 2)
		die("usage: mvi [file]\n");
	if (argc == 2)
		filename = argv[1];

	init();
	loadfile();
	draw();
	while (editing)
		runcmd(getchar()); /* TODO need char elsewhere? */

	/* TODO cleanup? */
	return EXIT_SUCCESS;
}
