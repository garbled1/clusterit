# $Id$
# Makefile for clusterit:  Tim Rightnour

OPSYS!=	uname

CC=	/usr/local/bin/gcc
CFLAGS=	-O2 -Wall

SUBDIR=	dsh pcp

all:
	for dir in ${SUBDIR} ; do \
		(cd $$dir && make CC=${CC} "CFLAGS=${CFLAGS}" OPSYS=${OPSYS}) ;\
	done

clean:
	for dir in ${SUBDIR} ; do \
		(cd $$dir && make clean OPSYS=${OPSYS}) ;\
	done

