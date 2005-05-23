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

#include <errno.h>
#include <signal.h>
#include <poll.h>
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

void do_command(char **argv, int allrun, char *username);
char *check_node(void);
void free_node(char *nodename);
void _log_bailout(int line, char *file);

/* globals */

int debug, exclusion, grouping;
int errorflag, iportnum, oportnum;
char **rungroup;
char **lumplist;
char *progname, *jsd_host;
group_t *grouplist;
node_t *nodelink;

/* 
 * jsh contacts the jsd daemon, and asks for a node to work on.
 */

int
main(int argc, char **argv)
{
    int someflag, ch, allflag, showflag;
    char *p, *q, *group, *nodename, *username;
    node_t *nodeptr;

    extern char *optarg;
    extern int optind;

    iportnum = oportnum = 0;
    someflag = 0;
    showflag = 0;
    exclusion = 0;
    debug = 0;
    errorflag = 0;
    allflag = 0;
    grouping = 0;
    username = NULL;
    nodename = NULL;
    group = NULL;
    nodeptr = NULL;
    nodelink = NULL;
    jsd_host = NULL;

    progname = p = q = argv[0];
    while (progname != NULL) {
	q = progname;
	progname = (char *)strsep(&p, "/");
    }
    progname = strdup(q);

#if defined(__linux__)
    while ((ch = getopt(argc, argv, "+?adeil:k:p:")) != -1)
#else
    while ((ch = getopt(argc, argv, "?adeil:k:p:")) != -1)
#endif
	switch (ch) {
	case 'a':		/* set the allrun flag */
	    allflag = 1;
	    break;
	case 'd':               /* set the debug flag */
	    debug = 1;
	    break;
	case 'e':		/* we want stderr to be printed */
	    errorflag = 1;
	    break;
	case 'i':		/* we want tons of extra info */
	    debug = 1;
	    break;
	case 'l':		/* invoke me as some other user */
	    username = strdup(optarg);
	    break;
	case 'o':		/* port to get nodes from jsd on */
	    oportnum = atoi(optarg);
	    break;
	case 'p':		/* port to release nodes to jsd on */
	    iportnum = atoi(optarg);
	    break;
	case 'h':		/* host to connect to jsd on */
	    jsd_host = strdup(optarg);
	    break;			
	case '?':		/* you blew it */
	    (void)fprintf(stderr,
			  "usage: %s [-aei] [-l username] [-p port] [-o port] "
			  "[-h hostname] [command ...]\n", progname);
	    exit(EXIT_FAILURE);
	    break;
	default:
	    break;
	}

    argc -= optind;
    argv += optind;

    if (!iportnum) {
	if (getenv("JSD_IPORT"))
	    iportnum = atoi(getenv("JSD_IPORT"));
	else
	    iportnum = JSDIPORT;
    }

    if (!oportnum) {
	if (getenv("JSD_OPORT"))
	    oportnum = atoi(getenv("JSD_OPORT"));
	else
	    oportnum = JSDOPORT;
    }

    if (jsd_host == NULL) {
	if (getenv("JSD_HOST"))
	    jsd_host = strdup(getenv("JSD_HOST"));
	else
	    jsd_host = strdup("localhost");
    }
    if (username == NULL)
        if (getenv("RCMD_USER"))
            username = strdup(getenv("RCMD_USER"));

    if (jsd_host == NULL)
	bailout();

    do_command(argv, allflag, username);
    exit(EXIT_SUCCESS);
}

char *
check_node(void)
{
    char *buf;
    int sock, i;
    struct sockaddr_in name;
    struct hostent *hostinfo;

    /* create socket */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
	bailout();

    name.sin_family = AF_INET;
    name.sin_port = htons(oportnum);
    hostinfo = gethostbyname(jsd_host);

    if (hostinfo == NULL) {
	(void)fprintf(stderr, "Unknown host %s.\n", jsd_host);
	exit(EXIT_FAILURE);
    }

    name.sin_addr = *(struct in_addr *)hostinfo->h_addr;

    if (connect(sock, (struct sockaddr *)&name, sizeof(name)) < 0)
	bailout();

    i = read_from_client(sock, &buf); /* get a node */
    buf[i] = '\0';
    if (debug)
	printf("Got node %s\n", buf);
    (void)close(sock);

    return(buf);
}

void
free_node(char *nodename)
{
    int sock;
    char *buf;
    struct sockaddr_in name;
    struct hostent *hostinfo;

    /* create socket */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
	bailout();

    name.sin_family = AF_INET;
    name.sin_port = htons(iportnum);
    hostinfo = gethostbyname(jsd_host);

    if (hostinfo == NULL) {
	(void)fprintf(stderr, "Unknown host %s.\n", jsd_host);
	exit(EXIT_FAILURE);
    }

    name.sin_addr = *(struct in_addr *)hostinfo->h_addr;

    if (connect(sock, (struct sockaddr *)&name, sizeof(name)) < 0)
	bailout();

    if (debug)
	printf("Freeing node %s\n", nodename);
    read_from_client(sock, &buf);
    if (write_to_client(sock, nodename) != 0)
	bailout();
    if (debug)
	printf("freed node %s\n", nodename);
    (void)close(sock);
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void
do_command(char **argv, int allrun, char *username)
{
    FILE *fd, *fda, *in;
    char buf[MAXBUF], pipebuf[2048];
    char *nodename, *p, *command, *rsh, *cd;
    int status, i, piping, pollret;
    pipe_t out, err;
    pid_t childpid;
    struct pollfd fds[2];

    i = 0;
    piping = 0;
    in = NULL;
    nodename = NULL;

    if (debug)
	if (username != NULL)
	    (void)printf("As User: %s\n", username);

    /* construct the command from the remains of argv */
    command = (char *)malloc(MAXBUF * sizeof(char));
    memset(command, 0, MAXBUF * sizeof(char));
    for (p = *argv; p != NULL; p = *++argv ) {
	strcat(command, p);
	strcat(command, " ");
    }
    if (debug) {
	(void)printf("\nDo Command: %s\n", command);
    }
    if (strcmp(command,"") == 0) {
	piping = 1;
	/* are we a terminal?  then go interactive! */
	if (isatty(STDIN_FILENO) && piping)
	    (void)printf("%s>", progname);
	in = fdopen(STDIN_FILENO, "r");
	/* start reading stuff from stdin and process */
	command = fgets(buf, sizeof(buf), in);
	if (command != NULL)
	    if (strcmp(command,"\n") == 0)
		command = NULL;
    } else {
	close(STDIN_FILENO);
	if (open("/dev/null", O_RDONLY, NULL) != 0)
	    bailout();
    }
    rsh = getenv("RCMD_CMD");
    if (rsh == NULL)
	rsh = strdup("rsh");
    if (rsh == NULL)
	bailout();

    if (allrun)
	nodename = check_node();

    while (command != NULL) {
	if (!allrun)
	    nodename = check_node();
	if (debug)
	    printf("Working node: %s\n", nodename);
	/* we set up pipes for each node, to prepare
	 * for the oncoming barrage of data.
	 */
	if (pipe(out.fds) != 0)
	    bailout();
	if (pipe(err.fds) != 0)
	    bailout();
	/* its the ol fork and switch routine eh? */
	childpid = fork();
	switch (childpid) {
	case -1:
	    bailout();
	    break;
	case 0:
	    if (piping)
		close(STDIN_FILENO);
	    /* stupid unix tricks vol 1 */
	    if (dup2(out.fds[1], STDOUT_FILENO) != STDOUT_FILENO)
		bailout();
	    if (dup2(err.fds[1], STDERR_FILENO) != STDERR_FILENO)
		bailout();
	    if (close(out.fds[0]) != 0)
		bailout();
	    if (close(err.fds[0]) != 0)
		bailout();
	    if (username != NULL)
		(void)sprintf(buf, "%s@%s", username, nodename);
	    else
		(void)sprintf(buf, "%s", nodename);
	    if (debug)
		(void)printf("%s %s %s\n", rsh, buf, command);
	    execlp(rsh, rsh, buf, command, (char *)0);
	    bailout();
	} /* end switch */
	/* now close off the useless stuff, and read the goodies */
	if (close(out.fds[1]) != 0)
	    bailout();
	if (close(err.fds[1]) != 0)
	    bailout();

	fda = fdopen(out.fds[0], "r"); /* stdout */
	if (fda == NULL)
	    bailout();
	fd = fdopen(err.fds[0], "r"); /* stderr */
	if (fd == NULL)
	    bailout();
	fds[0].fd = out.fds[0];
	fds[1].fd = err.fds[0];
	fds[0].events = POLLIN|POLLPRI;
	fds[1].events = POLLIN|POLLPRI;
	pollret = 1;
	
	while (pollret >= 0) {
	    int gotdata;
	    
	    pollret = poll(fds, 2, 5);
	    gotdata = 0;
	    if ((fds[0].revents&POLLIN) == POLLIN ||
		(fds[0].revents&POLLPRI) == POLLPRI) {
		cd = fgets(pipebuf, sizeof(pipebuf), fda);
		if (cd != NULL) {
		    (void)printf("%s: %s", nodename, cd);
		    gotdata++;
		}
	    }
	    if ((fds[1].revents&POLLIN) == POLLIN ||
		(fds[1].revents&POLLPRI) == POLLPRI) {
		cd = fgets(pipebuf, sizeof(pipebuf), fd);
		if (errorflag && cd != NULL) {
		    (void)printf("%s: %s", nodename, cd);
		    gotdata++;
		} else if (!errorflag && cd != NULL)
		    gotdata++;
	    }
	    if (!gotdata)
		if (((fds[0].revents&POLLHUP) == POLLHUP ||
		     (fds[0].revents&POLLERR) == POLLERR ||
		     (fds[0].revents&POLLNVAL) == POLLNVAL) &&
		    ((fds[1].revents&POLLHUP) == POLLHUP ||
		     (fds[1].revents&POLLERR) == POLLERR ||
		     (fds[1].revents&POLLNVAL) == POLLNVAL))
		    break;
	}
	fclose(fda);
	fclose(fd);
	(void)wait(&status);

	/* yes, this is code repetition, no need to adjust your monitor */
	if (piping) {
	    if (isatty(STDIN_FILENO) && piping)
		(void)printf("%s>", progname);
	    command = fgets(buf, sizeof(buf), in);
	    if (command != NULL)
		if (strcmp(command,"\n") == 0)
		    command = NULL;
	} else
	    command = NULL;
	if (!allrun)
	    free_node(nodename);
    } /* while loop */
    if (allrun)
	free_node(nodename);
    if (piping) {  /* I learned this the hard way */
	fflush(in);
	fclose(in);
    }
}

void
_log_bailout(int line, char *file)
{
    _bailout(line, file);
}
