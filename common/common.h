/* $Id$ */
/*
 * Copyright (c) 1998, 1999, 2000
 *	Tim Rightnour.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Tim Rightnour.
 * 4. The name of Tim Rightnour may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TIM RIGHTNOUR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL TIM RIGHTNOUR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Headers for common.c, used by dsh-like programs. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum {
	DEFAULT_FANOUT = 64,
	GROUP_MALLOC = 16,
	MAXBUF = 1024
};

#ifndef __P
#define __P(protos) protos
#endif

typedef struct { int fds[2]; } pipe_t;

struct node_data {
	char *name;					/* node name */
	int group;					/* member of this group */
	pipe_t err, out;			/* pipe structures */
	pid_t childpid;				/* pid of the child */
	struct node_data *next;		/* pointer to next node */
	int free;
	double index;
};
typedef struct node_data node_t;

struct group_data {
	char *name;					/* group name */
	int lump;					/* member of this lump */
};
typedef struct group_data group_t;

void bailout __P((int));
char *alignstring __P((char *, size_t));
#ifndef __NetBSD__
char * strsep(char **stringp, const char *delim);
#endif

#ifdef CLUSTERS
void do_showcluster __P((int));
int parse_cluster __P((char **));
node_t *nodealloc __P((char *));

extern char **lumplist;
extern char **rungroup;
extern int exclusion, debug, grouping;
extern group_t *grouplist;
extern node_t *nodelink;
#endif /* CLUSTERS */

extern int debug;
extern char *progname;
