# $Id$
# Makefile for dsh:  Tim Rightnour

PROG=		pcp

.if (${OPSYS} != NetBSD)
SRCS= pcp.c strsep.c
.else
SRCS= pcp.c
.endif

SCRIPTS=	prm pdf dshbak
MAN=		pcp.1 prm.1 pdf.1

all: ${PROG} scripts

${PROG}: ${SRCS:.c=.o}
	${CC} -O2 -Wall -o $@ ${SRCS:.c=.o}

scripts: ${SCRIPTS}

clean:
	rm -f ${PROG} ${SCRIPTS} *.o
