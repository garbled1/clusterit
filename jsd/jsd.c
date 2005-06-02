/* $Id$ */
/*
 * Copyright (c) 2000
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

/* Semi-intelligent job scheduling daemon.  Intended for heterogenous
 * network load sharing applications.
 *
 */

#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../common/common.h"
#include "../common/sockcommon.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id$");
#endif

/* globals */
int debug, errorflag, exclusion, grouping, iportnum, oportnum;
int nrofrungroups;
group_t *grouplist;
node_t *nodelink;
char **rungroup;
char **lumplist;
pid_t currentchild;
char *progname;

void _log_bailout(int line, char *file);
void do_bench_command(char *argv, int fanout, char *username);
void sig_handler(int i);
void main_loop(void);
void free_node(int sock);

/*
 * Main should handle deciding which group or nodes we run on, running the
 * initial benchmark run, and finally launching the scheduler daemon.
 */

int
main(int argc, char **argv)
{
    extern char	*optarg;
    extern int	optind;

    int someflag, ch, i, fanout, showflag, fanflag;
    char *p, *q, *group, *nodename, *username, *benchcmd;
    char **grouptemp, **exclude;
    struct rlimit limit;
    pid_t pid;

    someflag = showflag = fanflag = 0;
    exclusion = debug = 0;
    iportnum = oportnum = nrofrungroups = 0;
    fanout = DEFAULT_FANOUT;
    nodename = NULL;
    username = NULL;
    group = NULL;
    nodelink = NULL;

    rungroup = malloc(sizeof(char **) * GROUP_MALLOC);
    if (rungroup == NULL)
	bailout();
    exclude = malloc(sizeof(char **) * GROUP_MALLOC);
    if (exclude == NULL)
	bailout();

    progname = p = q = argv[0];
    while (progname != NULL) {
	q = progname;
	progname = (char *)strsep(&p, "/");
    }
    progname = strdup(q);

#if defined(__linux__)
    while ((ch = getopt(argc, argv, "+?diqf:g:l:w:x:p:")) != -1)
#else
    while ((ch = getopt(argc, argv, "?diqf:g:l:w:x:p:")) != -1)
#endif
	switch (ch) {
	case 'd':		/* we want to debug jsd (hidden)*/
	    debug = 1;
	    break;
	case 'i':		/* we want tons of extra info */
	    debug = 1;
	    break;
	case 'l':		/* invoke me as some other user */
	    username = strdup(optarg);
	    break;
	case 'q':		/* just show me some info and quit */
	    showflag = 1;
	    break;
	case 'f':		/* set the fanout size */
	    fanout = atoi(optarg);
	    fanflag = 1;
	    break;
	case 'g':		/* pick a group to run on */
	    i = 0;
	    grouping = 1;
	    for (p = optarg; p != NULL; ) {
		group = (char *)strsep(&p, ",");
		if (group != NULL) {
		    if (((i+1) % GROUP_MALLOC) != 0) {
			rungroup[i++] = strdup(group);
		    } else {
			grouptemp = realloc(rungroup,
					    i*sizeof(char **) +
					    GROUP_MALLOC*sizeof(char *));
			if (grouptemp != NULL)
			    rungroup = grouptemp;
			else
			    bailout();
			rungroup[i++] = strdup(group);
		    }
		}
	    }
	    nrofrungroups = i;
	    group = NULL;
	    break;			
	case 'x':		/* exclude nodes, w overrides this */
	    exclusion = 1;
	    i = 0;
	    for (p = optarg; p != NULL; ) {
		nodename = (char *)strsep(&p, ",");
		if (nodename != NULL) {
		    if (((i+1) % GROUP_MALLOC) != 0) {
			exclude[i++] = strdup(nodename);
		    } else {
			grouptemp = realloc(exclude,
					    i*sizeof(char **) +
					    GROUP_MALLOC*sizeof(char *));
			if (grouptemp != NULL)
			    exclude = grouptemp;
			else
			    bailout();
			exclude[i++] = strdup(nodename);
		    }
		}
	    }
	    break;
	case 'w':		/* perform operation on these nodes */
	    someflag = 1;
	    for (p = optarg; p != NULL; ) {
		nodename = (char *)strsep(&p, ",");
		if (nodename != NULL)
		    (void)nodealloc(nodename);
	    }
	    break;
	case 'o':		/* port to listen to requests on */
	    oportnum = atoi(optarg);
	    break;
	case 'p':		/* port to listen to completions on */
	    iportnum = atoi(optarg);
	    break;
	case '?':		/* you blew it */
	    (void)fprintf(stderr,
		 "usage: jsd [-iq] [-f fanout] [-g rungroup1,...,rungroupN] "
		 "[-l username] [-p port] [-o port] [-w node1,..,nodeN] "
       		 "[-x node1,...,nodeN] [command ...]\n");
	    return(EXIT_FAILURE);
	    /*NOTREACHED*/
	    break;
	default:
	    break;
	}

/* check for a fanout var, and use it if the fanout isn't on the commandline */
    if (!fanflag)
	if (getenv("FANOUT"))
	    fanout = atoi(getenv("FANOUT"));
    if (!iportnum) {
	if (getenv("JSD_IPORT"))
	    iportnum = atoi(getenv("JSD_IPORT"));
	else
	    iportnum = JSDIPORT;
    }
    if (username == NULL)
        if (getenv("RCMD_USER"))
            username = strdup(getenv("RCMD_USER"));
    if (!oportnum) {
	if (getenv("JSD_OPORT"))
	    oportnum = atoi(getenv("JSD_OPORT"));
	else
	    oportnum = JSDOPORT;
    }
    if (!someflag)
	parse_cluster(exclude);

    argc -= optind;
    argv += optind;
    if (showflag) {
	do_showcluster(fanout);
	return(EXIT_SUCCESS);
    }
    openlog("jsd", LOG_PID|LOG_NDELAY, LOG_DAEMON);

    /*
     * set per-process limits for max descriptors, this avoids running
     * out of descriptors and the odd errors that come with that.
     */
    close(STDIN_FILENO);
    pid = fork();
    if (pid == 0) {
	(void)setsid(); /* disassociate from our controlling terminal */
	syslog(LOG_INFO, "Job Scheduling Daemon Started");

	if (getrlimit(RLIMIT_NOFILE, &limit) != 0)
	    bailout();
	if (limit.rlim_cur < fanout * 5) {
	    limit.rlim_cur = fanout * 5;
	    if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
		bailout();
	}
		
	if (getenv("JSD_BENCH_CMD")) {
	    benchcmd = getenv("JSD_BENCH_CMD");
	    do_bench_command(benchcmd, fanout, username);
	} else {
	    syslog(LOG_WARNING, "No JSD_BENCH_CMD environment setting,"
		" assuming homogenus cluster.");
	}

	/* jump to the loop */
	main_loop();
    } else if (pid > 0) {
	(void)printf("%d\n", pid); /* spit the pid out */
	return(EXIT_SUCCESS);
    }
    else if (pid == -1)
	syslog(LOG_CRIT, "Aborting: %m");
    return(EXIT_FAILURE);
}

/*
 * This performs the bulk of the program's purpose.  The program enters this
 * loop, and either watches the shared memory, and talks with jsh processes,
 * or listens on the jsd port, and listens for jsh processes asking for a
 * node handout.
 *
 * The theory of operation is simple:
 * a jsh process is invoked to run a command, similar to run.  jsh contacts
 * the jsd asking for an available node.  jsd locks the database, assigns a
 * node, marks that node as in use, and unlocks the db.  Eventually the
 * jsh process will complete it's task, and report back to jsd.  Jsd will
 * then free up the node for further processing.
 */

void main_loop(void)
{
    char *buf;
    node_t *nodeptr, *fastnode;
    double topspeed;
    int osock, isock, new;
    struct sockaddr_in clientname;
    size_t size;
    fd_set node_fd_set, free_fd_set, full_fd_set;
    struct timeval timeout;
    struct sigaction signaler;

    buf = NULL;

    signaler.sa_handler = sig_handler;
    signaler.sa_flags = SA_RESTART;
    sigemptyset(&signaler.sa_mask);
    if (sigaction(SIGTERM, &signaler, NULL) != 0)
	log_bailout();
	
    osock = make_socket(oportnum);
    isock = make_socket(iportnum);
#ifdef HAVE_SETPRIORITY
       /* not implemented in Cygwin */
       /* read: http://www.cygwin.com/ml/cygwin/2003-10/msg00664.html */
    setpriority(PRIO_PROCESS, 0, 20); /* nice ourselves */
#endif /* HAVE_SETPRIORITY */

    if (listen(osock, SOMAXCONN) < 0)
	log_bailout();
    if (listen(isock, SOMAXCONN) < 0)
	log_bailout();


    for (;;) { /* loop */
	FD_ZERO(&node_fd_set);
	FD_ZERO(&full_fd_set);
	FD_ZERO(&free_fd_set);
	FD_SET(osock, &node_fd_set);
	FD_SET(isock, &free_fd_set);
	FD_SET(osock, &full_fd_set);
	FD_SET(isock, &full_fd_set);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (debug)
	    syslog(LOG_DEBUG, "Entering main loop");
	if (select(FD_SETSIZE, &full_fd_set, &full_fd_set, NULL, NULL) < 0)
	    log_bailout();
	if (debug)
	    syslog(LOG_DEBUG, "We have a connection");
	if (select(FD_SETSIZE, &free_fd_set, &free_fd_set, NULL, &timeout)) {
	    /* jsh wants to free a node */
	    if (debug)
		syslog(LOG_DEBUG, "Someone wants to free a node");
	    free_node(isock);
	} else if (select(FD_SETSIZE, &node_fd_set, NULL, NULL, &timeout)) {
	    /* jsh wants a node */
	    if (debug)
		syslog(LOG_DEBUG, "Someone wants a node");
	    new = accept(osock, (struct sockaddr *) &clientname, &size);
	    if (new < 0)
		log_bailout();
	    topspeed = 0.0;
	    fastnode = nodeptr = NULL;
	    while (fastnode == NULL)
		for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next) {
		    FD_ZERO(&free_fd_set);
		    FD_SET(isock, &free_fd_set);
		    if (nodeptr->index > topspeed && nodeptr->free)
			fastnode = nodeptr;
		    else if (select(FD_SETSIZE, &free_fd_set, &free_fd_set,
				 NULL, &timeout)) {
			free_node(isock);
		    }
		}
	    fastnode->free = 0;
	    if (debug)
		syslog(LOG_DEBUG, "Handing out node %s to a jsh process",
		    fastnode->name);
	    write_to_client(new, fastnode->name);
	    close(new);
	}
    }
}

/*
* Free up a node for use by other requestors
*/

void free_node(int sock)
{
    int new, i;
    struct sockaddr_in clientname;
    size_t size;
    char *buf;
    node_t *nodeptr;

    if (debug)
	syslog(LOG_DEBUG, "Entered free_node");
    new = accept(sock, (struct sockaddr *) &clientname, &size);
    if (debug)
	syslog(LOG_DEBUG, "accepted new connection");
    if (new < 0)
	log_bailout();
    write_to_client(new, "1");
    i = read_from_client(new, &buf);
    buf[i] = '\0';
    if (debug)
	syslog(LOG_DEBUG, "got node %s from client", buf);
    for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next)
	if (strcmp(buf, nodeptr->name) == 0) {
	    nodeptr->free = 1;
	    if (debug)
		syslog(LOG_DEBUG, "freeing node %s", nodeptr->name);
	}
    close(new);
}

/*
 * Note that while the below is nearly identical, it has but one purpose in
 * life, and that is to populate the index of machine speeds.
 */

void
do_bench_command(char *argv, int fanout, char *username)
{
    FILE *fd, *in;
    char pipebuf[2048], buf[MAXBUF];
    int status, i, j, n, g;
    char *q, *rsh, *cd;
    node_t *nodeptr, *nodehold;

    j = i = 0;
    in = NULL;
    q = NULL;
    cd = pipebuf;

    if (debug) {
	if (username != NULL)
	    syslog(LOG_DEBUG, "As User: %s", username);
	syslog(LOG_DEBUG, "On nodes:");
    }
    for (nodeptr = nodelink; nodeptr; nodeptr = nodeptr->next) {
	if (debug) {
	    q = (char *)malloc(MAXBUF * sizeof(char));
	    if (q == NULL)
		log_bailout();
	    memset(q, 0, MAXBUF * sizeof(char));
	    if (!(j % 4) && j > 0)
		strcat(q, "\n");
	    strcat(q, nodeptr->name);
	    strcat(q, "\t");
	}
	j++;
    }
    if (debug)
	syslog(LOG_DEBUG, "%s", q);

    i = j; /* side effect of above */
    j = i / fanout;
    if (i % fanout)
	j++; /* compute the # of rungroups */

    if (debug) {
	syslog(LOG_DEBUG, "\nDo Command: %s", argv);
	syslog(LOG_DEBUG, "Fanout: %d Groups:%d", fanout, j);
    }

    rsh = getenv("RCMD_CMD");
    if (rsh == NULL)
	rsh = strdup("rsh");
    if (rsh == NULL)
	bailout();
    g = 0;
    nodeptr = nodelink;
    for (n=0; n <= j; n++) {
	nodehold = nodeptr;
	for (i=0; (i < fanout && nodeptr != NULL); i++) {
	    g++;
	    if (debug)
		syslog(LOG_DEBUG, "Working node: %d, fangroup %d,"
		    " fanout part: %d", g, n, i);
/*
 * we set up pipes for each node, to prepare for the oncoming barrage of data.
 * Close on exec must be set here, otherwise children spawned after other
 * children, inherit the open file descriptors, and cause the pipes to remain
 * open forever.
 */
	    if (pipe(nodeptr->out.fds) != 0)
		log_bailout();
	    if (fcntl(nodeptr->out.fds[0], F_SETFD, 1) == -1)
		log_bailout();
	    if (fcntl(nodeptr->out.fds[1], F_SETFD, 1) == -1)
		log_bailout();
	    nodeptr->childpid = fork();
	    switch (nodeptr->childpid) {
		/* its the ol fork and switch routine eh? */
	    case -1:
		log_bailout();
		break;
	    case 0:
		/* remove from parent group to avoid kernel
		 * passing signals to children.
		 */
		(void)setsid();
		if (dup2(nodeptr->out.fds[1], STDOUT_FILENO) 
		    != STDOUT_FILENO) 
		    log_bailout();
		if (close(nodeptr->out.fds[0]) != 0)
		    log_bailout();

		if (username != NULL)
		    (void)sprintf(buf, "%s@%s", username, nodeptr->name);
		else
		    (void)sprintf(buf, "%s", nodeptr->name);
		if (debug)
		    syslog(LOG_DEBUG, "%s %s %s", rsh, buf, argv);
		execlp(rsh, rsh, buf, argv, (char *)0);
		log_bailout();
		break;
	    default:
		break;
	    } /* switch */
	    nodeptr = nodeptr->next;
	} /* for i */
	nodeptr = nodehold;
	for (i=0; (i < fanout && nodeptr != NULL); i++) {
	    if (debug)
		syslog(LOG_DEBUG, "Printing node: %d, fangroup %d,"
		    " fanout part: %d", g-fanout+i, n, i);
	    currentchild = nodeptr->childpid;
	    /* now close off the useless stuff, and read the goodies */
	    if (close(nodeptr->out.fds[1]) != 0)
		log_bailout();
	    fd = fdopen(nodeptr->out.fds[0], "r"); /* stdout */
	    if (fd == NULL)
		log_bailout();
	    while ((cd = fgets(pipebuf, sizeof(pipebuf), fd))) {
		if (cd != NULL) {
		    nodeptr->index = atof(cd);
		    syslog(LOG_DEBUG, "recorded speed %f for node %s",
			nodeptr->index, nodeptr->name);
		}
	    }
	    fclose(fd);
	    (void)wait(&status);
	    nodeptr = nodeptr->next;
	} /* for i */			
    } /* for n */
}

/*ARGSUSED*/
void
_log_bailout(int line, char *file)
{
    syslog(LOG_CRIT, "%s: Failed in %s line %d: %m %d", progname, file, line,
	errno);

    _exit(EXIT_FAILURE);
}

void
sig_handler(int i)
{

    switch (i) {
    case SIGTERM:
	_exit(EXIT_SUCCESS);
	break;
    default:
	log_bailout();
	break;
    }
}
