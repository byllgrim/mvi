/* See LICENSE file for copyright and license details. */
#include <sys/ioctl.h>

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Macros */
#define VLINES(l) (1 + (l ? l->v/cols : 0))
#define ISESC(c) (c == 27)
#define LASTINLINE (curpos.o >= curpos.l->l - 1)

/* Types */
#define LINSIZ 5 /* TODO 128 */

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
static void savefile(void);
static void runcmd(char c);
static void excmd(void);
static void quit(void);

static void draw(void);
static void clearscreen(void);
static void fillborder(char c, size_t r);
static void showstatus(void);
static void setpos(int r, int c);
static void gettermsize(void);

static Position addtext(char *s, Position p);
static char *nudgeright(Position p);
static Line *addline(Line *l);
static void moveleft(void);
static void moveright(void);
static void moveup(void);
static void movedown(void);

/* Variables */
static char *filename = NULL;
static Line *fstln; /* first line */
static Line *lstln; /* last line */
static Line *scrln; /* first line on screen */
static Position curpos;
static int mode = MODE_NORMAL;
static int unsaved = 0;
static size_t rows, cols; /* terminal size */
static size_t crow, ccol; /* current x,y location */
static size_t saved_offset; /* used for moving up/down lines */
static struct termios attr_saved, attr_mvi;
static char status[BUFSIZ] = "";
static struct sigaction *signalignore;

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
init(void) /* TODO rename inittext()? */
{
	Line *l;

	signalignore = ecalloc(1, sizeof(struct sigaction));
	signalignore->sa_handler = SIG_IGN;
	sigaction(SIGINT, signalignore, NULL); /* ward from precepitous ints */
	/* TODO other signals */

	l = ecalloc(1, sizeof(Line));
	l->c = ecalloc(LINSIZ, sizeof(char));
	l->s = LINSIZ;
	l->l = 0;
	l->n = NULL;
	l->p = NULL;

	curpos.l = fstln = lstln = scrln = l;
	curpos.o = saved_offset= 0;

	gettermsize();
	crow = ccol = 0;
	tcgetattr(fileno(stdin), &attr_saved);
	attr_mvi = attr_saved;
	attr_mvi.c_lflag &= (~ICANON & ~ECHO); /* IXON ICRNL ONLCR ? */
	tcsetattr(fileno(stdin), TCSANOW, &attr_mvi);

	/* TODO atexit(). Reset? Save? */
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

	buf = ecalloc(BUFSIZ + 1, 1); /* TODO alloca */
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
savefile(void)
{
	char nofile[] = "No current filename";
	FILE *file;
	Position p;

	if (!filename) {
		strncpy(status, nofile, strlen(nofile)+1); /* TODO prettify */
		return;
	}

	if (!(file = fopen(filename, "w"))) {
		strncpy(status, strerror(errno), strlen(strerror(errno))+1);
		return;
	}

	for (p.l = fstln; p.l; p.l = p.l->n)
		fprintf(file, "%s\n", p.l->c);

	strncpy(status, "saved file", strlen("saved file")+1);
	/* TODO "file" x lines, y characters*/
	unsaved = 0;
	fclose(file);
}

void
runcmd(char c) /* it's tricky */
{
	char *s;

	if (mode == MODE_INSERT) { /* TODO prettify */
		if (ISESC(c)) {
			moveleft();
			mode = MODE_NORMAL;
			goto normal;
		}
		s = ecalloc(2, sizeof(char));
		s[0] = c; /* TODO stringmychar(c); */
		addtext(s, curpos); /* TODO implement undo */
		unsaved = 1;
		free(s);
		moveright();
		goto done;
	}

normal:
	/* TODO while (s[i]) */
	switch (c) {
	case ':':
		excmd();
		break;
	case 'a':
		moveright();
		mode = MODE_INSERT;
		break;
	case 'h':
		moveleft();
		break;
	case 'i':
		mode = MODE_INSERT;
		break;
	case 'j':
		movedown();
		break;
	case 'k':
		moveup();
		break;
	case 'l':
		if (!LASTINLINE)
			moveright();
		break;
	}

done:
	draw(); /* TODO meliorate efficacy */
}

void
excmd(void)
{
	char c;
	size_t i;
	char *s = alloca(cols); /* TODO error handling */

	strncpy(status, ":", 2);
	showstatus();
	c = getchar();
	for (i = 0; i < cols - 2 && c != '\n'; ++i) { /* -2 for : and \0 */
		if (ISESC(c))
			return;
		putchar(c);
		s[i] = c;
		c = getchar();
	}
	s[++i] = '\0';

	switch (s[0]) {
	case 'q':
		if (unsaved && s[1] != '!')
			strncpy(status, "unsaved changes", 16);
		else
			quit();
		break;
	case 'w':
		savefile();
		break;
	}
}

void
quit(void)
{
	status[0] = '\0';
	showstatus();
	exit(EXIT_SUCCESS); /* TODO restore attr_saved? */
}

void
draw(void) /* TODO files longer than screen size */
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
	showstatus();
	setpos(crow, ccol);
}

void
clearscreen(void)
{
	setpos(0, 0);
	printf("\033[2J");
	/* TODO setpos(crow, ccol)? No? */
}

void
fillborder(char c, size_t r)
{
	setpos(r, 0);
	for (; r < rows - 2; r++) /* room for status bar */
		printf("%c\n", c);
	putchar(c);
	setpos(crow, ccol);
}

void
showstatus(void)
{
	size_t i;

	setpos(rows - 1, 0);
	for (i = 0; i < cols; i++)
		putchar(' ');

	if (strlen(status) > cols)
		status[cols] = '\0';
	setpos(rows - 1, 0);
	printf(status);
	fflush(stdout);
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
moveleft(void)
{
	if (!curpos.o)
		return;

	if (!ccol)
		printf("up a line");
	else
		--ccol;

	saved_offset = --curpos.o; /* TODO unicode */
	setpos(crow, ccol);
}

void
moveright(void)
{
	if (ccol >= cols - 1) { /* end of screen */
		ccol = 0;
		++crow; /* TODO is this safe? */
	} else {
		++ccol;
	}

	saved_offset = ++curpos.o; /* TODO unicode */
	setpos(crow, ccol);
}

void
moveup(void)
{
	if (!curpos.l->p)
		return;
	if (saved_offset > curpos.o) /* TODO assure correct logic */
		curpos.o = saved_offset;

	curpos.l = curpos.l->p;
	if (curpos.l->l - 1 < curpos.o) /* TODO assure logic */
		curpos.o = curpos.l->l -1;

	--crow; ccol = curpos.o; /* TODO this is just a quick fix */
	setpos(crow, ccol);
}

void
movedown(void)
{
	if (!curpos.l->n)
		return;
	if (saved_offset > curpos.o) /* TODO assure correct logic */
		curpos.o = saved_offset;

	curpos.l = curpos.l->n;
	if (curpos.l->l - 1 < curpos.o)
		curpos.o = curpos.l->l - 1;

	++crow; ccol = curpos.o; /* TODO this is just a quick fix */
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
