CPPFLAGS = -D_POSIX_C_SOURCE=200112L
CFLAGS = -std=c89 -pedantic-errors -Wall -Wextra -Os ${CPPFLAGS}
LDFLAGS = -lncursesw -lutf
PREFIX = /usr/local

all:
	cc -o mvi ${CFLAGS} mvi.c ${LDFLAGS}

clean:
	rm -f mvi

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f mvi ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/mvi

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/mvi
