/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Types */
#define LINSIZ 5 /* TODO 128 */
/* TODO #define ESC_SET_CURSOR_POS "\033[%u;%uH" etc from bb vi */

typedef struct Line Line;
struct Line {
	char *c; /* content */
	Line *n; /* next */
	Line *p; /* previous */
	size_t s; /* buffer size */
	size_t l; /* length sans '\0' */
};

typedef struct {
	Line *l;
	size_t o; /* offset */
} Position;

/* Function declarations */
static void die(const char *errstr, ...);
static void *ecalloc(size_t nmemb, size_t size);
static void *erealloc(void *ptr, size_t size);

static void init(void);
static void loadfile(void);

static Position addtext(char *s, Position p);
static char *nudgeright(Position p);
static Line *addline(Line *l);

/* Variables */
static char *filename = NULL;
static Line *fstln; /* first line */
static Line *lstln; /* last line */
static Position curpos;

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

	curpos.l = fstln = lstln = l;
	curpos.o = 0;

	/* TODO
	 * Initialize interrupt handling? signal(), sigaction, SIG_IGN, SIG_DFL,
	 *     SIGHUP, SIGINT, SIGWINCH, SIGQUIT, SIGXFSZ, SIGTERM.
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
		buf[n] = '\n';
		p = addtext(buf, p);
	}

	close(fd);
	free(buf);
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
			++p.o;
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

int
main(int argc, char *argv[])
{
	if (argc > 2)
		die("usage: mvi [file]\n");
	if (argc == 2)
		filename = argv[1];

	init();
	loadfile();
	/* edit() */

	return EXIT_SUCCESS;
}
