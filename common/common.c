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

/*
 * these are routines that are used in all the dsh programs, and therefore
 * rather than having to fix them in n places, they are kept here.
 */

#include "common.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id$");
#endif


/*
 * This routine just rips open the various arrays and prints out information
 * about what the command would have done, and the topology of your cluster.
 * Invoked via the -q switch.
 */

void
do_showcluster(fanout)
	int fanout;
{
	node_t *nodeptr;
	int i, j, l, n;
	char *group;

	i = l = 0;
	for (nodeptr = nodelink; nodeptr->next != NULL;
		 nodeptr = nodeptr->next)
		i++; /* just count the nodes */
		;
	j = i / fanout;
      /* how many times do I have to run in order to reach them all */
	if (i % fanout)
		j++;

	if (rungroup[0] != NULL) {
		(void)printf("Rungroup:");
		for (i=0; rungroup[i] != NULL; i++) {
			if (!(i % 4) && i > 0)
				(void)printf("\n");
			(void)printf("\t%s", rungroup[i]);
		}
		if (i % 4)
			(void)printf("\n");
	}

	nodeptr = nodelink;

	if (getenv("CLUSTER"))
		(void)printf("Cluster file: %s\n", getenv("CLUSTER"));
	(void)printf("Fanout size: %d\n", fanout);
	for (n=0; n <= j; n++) {
		for (i=0; (i < fanout && nodeptr != NULL); i++) {
			l++;
			group = NULL;
			if (nodeptr->group > 0)
					group = strdup(grouplist[nodeptr->group]);
			if (group == NULL)
				(void)printf("Node: %3d  Fangroup: %3d  Rungroup: None"
					"            Host: %-15s\n", l, n + 1, nodeptr->name);
			else
				(void)printf("Node: %3d  Fangroup: %3d  Rungroup: %-15s"
					"  Host: %-15s\n", l, n + 1, group,
					nodeptr->name);
			nodeptr = nodeptr->next;
		}
	}
}


/*
 * A routine to parse the command arguments, and prepare a nodelist for use
 * returns the number of groups in the list.
 */

int
parse_cluster(exclude)
	char **exclude;
{
	FILE *fd;
	char *clusterfile, *p, *nodename;
	char **grouptemp;
	int i, j, g, fail, gfail;
	char	buf[MAXBUF];
	extern int errno;
	struct node_data *nodeptr;

	g = -1;

	grouplist = (char **)malloc(GROUP_MALLOC * sizeof(char **));
	if (grouplist == NULL)
		bailout(__LINE__);

    /* if -w wasn't specified, we need to parse the cluster file */
	clusterfile = getenv("CLUSTER");
	if (clusterfile == NULL) {
		(void)fprintf(stderr,
			"%s: must use -w flag without CLUSTER environment setting.\n",
			progname);
		exit(EXIT_FAILURE);
	}
	fd = fopen(clusterfile, "r");
	if (NULL == fd) {
		(void)fprintf(stderr, "%s: open of clusterfile failed:%s\n",
			progname, strerror(errno));
		exit(EXIT_FAILURE);
	}
	i = 0;
	while ((nodename = fgets(buf, sizeof(buf), fd))) {
		p = (char *)strsep(&nodename, "\n");
		if (strcmp(p, "") != 0) {
			if (exclusion || grouping) { /* this handles the -x,g option */
				fail = 0;
				for (j = 0; exclude[j] != NULL; j++)
					if (strcmp(p, exclude[j]) == 0)
						fail = 1;
				gfail = 1;
				for (j = 0; (rungroup[j] != NULL && gfail == 1); j++)
					if (strcmp(grouplist[g], rungroup[j]) == 0)
						gfail = 0;

				if (!fail) {
					if (strstr(p, "GROUP") != NULL) {
						strsep(&p, ":");
						if (((g+1) % GROUP_MALLOC) != 0 && g > 0) {
						    grouplist[++g] = strdup(p);
						} else {
							grouptemp = realloc(grouplist,
								(g+1)*sizeof(char **) +
								GROUP_MALLOC * sizeof(char **));
							if (grouptemp != NULL)
								grouplist = grouptemp;
							else
								bailout(__LINE__);
							grouplist[++g] = strdup(p);
						}
					} else if (!gfail) {
						nodeptr = nodealloc(strdup(p));
						if (g >= 0)
							nodeptr->group = g;
					}
				}
			} else {
				if (strstr(p, "GROUP") != NULL) {
					strsep(&p, ":");
					if (((g+1) % GROUP_MALLOC) != 0 || g < 1) {
						grouplist[++g] = strdup(p);
					} else {
						grouptemp = (char **)realloc(grouplist,
							(g+1)*sizeof(char **) +
							GROUP_MALLOC * sizeof(char **));
						if (grouptemp != NULL)
							grouplist = grouptemp;
						else
							bailout(__LINE__);
						grouplist[++g] = strdup(p);
					}
				} else {
					nodeptr = nodealloc(strdup(p));
					if (g >= 0)
						nodeptr->group = g;
				}
			} /* exlusion */
		} /* strcmp */
	} /* while nodename */
	fclose(fd);
	return(g);
}

/* return a string, followed by n - strlen spaces */

char *
alignstring(string, n)
	char *string;
	size_t n;
{
	size_t i;
	char *newstring;

	newstring = strdup(string);
	for (i=1; i <= n - strlen(string); i++)
		newstring = strcat(newstring, " ");

	return(newstring);
}


/* 
 * Simple error handling routine, needs severe work.
 * Its almost totally useless.
 */

/*ARGSUSED*/
void
bailout(line) 
	int line;
{
	extern int errno;
	
	if (debug)
		(void)fprintf(stderr, "%s: Failed on line %d: %s %d\n",
			progname, line,	strerror(errno), errno);
	else
		(void)fprintf(stderr, "%s: Internal error, aborting: %s\n",
			progname, strerror(errno));

	_exit(EXIT_FAILURE);
}

/* allocates a new/first node, and returns a pointer to the user */
struct node_data *
nodealloc(nodename)
	char * nodename;
{
	struct node_data *nodeptr, *nodex;

	if (nodelink == NULL) {
		nodelink = malloc((size_t)sizeof(node_t));
		nodelink->name = strdup(nodename);
	    nodelink->group = 0;
		nodelink->err.fds[0] = NULL;
		nodelink->err.fds[1] = NULL;
		nodelink->out.fds[0] = NULL;
		nodelink->out.fds[1] = NULL;
		nodelink->childpid = NULL;
		nodelink->next = NULL;
		return(nodelink);
	}
	nodex = malloc(sizeof(node_t));
	if (NULL == nodex)
		bailout(__LINE__);
	
	for (nodeptr = nodelink; nodeptr->next != NULL;
		 nodeptr = nodeptr->next)
		;
	
	nodeptr->next = nodex;
	nodex->name = strdup(nodename);
	nodex->group = 0;
	nodex->err.fds[0] = NULL;
	nodex->err.fds[1] = NULL;
	nodex->out.fds[0] = NULL;
	nodex->out.fds[1] = NULL;
	nodex->childpid = NULL;
	nodex->next = NULL;							
	return(nodex);
}
