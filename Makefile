include config.mk

SRC = mvi.c util.c
OBJ = ${SRC:.c=.o}

all: mvi

mvi: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

.c.o:
	${CC} -c ${CFLAGS} $<

clean:
	rm -f mvi ${OBJ}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f mvi ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/mvi

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/mvi
