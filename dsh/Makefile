# $Id$
# dsh Makefile: Tim Rightnour

CC=	/usr/local/bin/gcc
CFLAGS=	-Wall -O2
PROG= dsh

.if ${OPSYS} != NetBSD
SRCS= dsh.c strsep.c
.else
SRCS= dsh.c
.endif

MAN= dsh.1

all: dsh

${PROG}: ${SRCS:.c=.o}
	${CC} ${CFLAGS} -o $@ ${SRCS:.c=.o}

install: ${PROG}
	/usr/bin/install -c -s -o bin -g bin -m 0755 dsh ${PREFIX}/bin
	/usr/bin/install -c -o bin -g bin -m 0444 dsh.1 ${PREFIX}/man/man1

clean:
	rm -f *.o ${PROG}
