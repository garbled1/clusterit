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

#include "../dsh/common.h"
#include "jcommon.h"

#if !defined(lint) && defined(__NetBSD__)
__COPYRIGHT(
"@(#) Copyright (c) 1998, 1999, 2000\n\
        Tim Rightnour.  All rights reserved\n");
__RCSID("$Id$");
#endif

#ifndef __P
#define __P(protos) protos
#endif

extern int errno;

void do_command __P((char **, int, char *));
char *check_node __P((void));
void free_node __P((char *));
void log_bailout __P((int));

/* globals */

int debug, exclusion, grouping, sharedmem;
int errorflag, portnum, semkey;
char **grouplist;
char **rungroup;
char *progname;
node_t *nodelink;

/* 
 * jsh contacts the jsd daemon, and asks for a node to work on.
 */

int
main(argc, argv) 
	int argc;
	char *argv[];
{
	int someflag, ch, allflag, showflag;
	char *p, *q, *group, *nodename, *username;
	node_t *nodeptr;

	extern int debug;
	extern int errorflag;
	extern char *optarg;
	extern int optind;
	extern int semkey;

	sharedmem = 1;
	semkey = 0;
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

	progname = p = q = argv[0];
	while (progname != NULL) {
		q = progname;
		progname = (char *)strsep(&p, "/");
	}
	progname = strdup(q);

	while ((ch = getopt(argc, argv, "?adeil:k:p:")) != -1)
		switch (ch) {
		case 'a':		/* set the allrun flag */
			allflag = 1;
			break;
		case 'd':       /* set the debug flag */
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
		case 'k':		/* the shared memory key, (pid of jsd) */
			semkey = atoi(optarg);
			break;
		case 'p':		/* port to listen on, shuts off SHM */
			sharedmem = 0;
			portnum = atoi(optarg);
			break;
		case '?':		/* you blew it */
			(void)fprintf(stderr,
			    "usage: %s [-aei] [-l username] [-k jsdpid] "
				"[command ...]\n", progname);
			exit(EXIT_FAILURE);
			break;
		default:
			break;
	}

	argc -= optind;
	argv += optind;
	if (sharedmem) {
		if (getenv("JSD_PID"))
			semkey = atoi(getenv("JSD_PID"));
		if (semkey == 0) {
			(void)fprintf(stderr, "%s must set JSD_PID or use -k option\n",
				progname);
			exit(EXIT_FAILURE);
		}
	}
	do_command(argv, allflag, username);
	exit(EXIT_SUCCESS);
}

char *
check_node()
{
	int semid, shmid;
	char *buf, *buf2;

	extern int semkey;

	semid = semget(semkey, 4, NULL);
	if (semid == -1)
		bailout(__LINE__);
	shmid = shmget(semkey, MAXBUF * sizeof(char), NULL);
	if (shmid == -1)
		bailout(__LINE__);

	buf = shmat(shmid, NULL, 0);
	if ((int)buf == -1)
		bailout(__LINE__);

	kill((pid_t)semkey, SIGUSR1); /* alert jsd that we are coming in */

	shm_wait_deeplock(semid);
	shm_take_mainlock(semid);
	printf("Locked jsd\n");

	shm_take_deeplock(semid);
	buf2 = strdup(buf);
	printf("buf=%s buf2=%s\n", buf, buf2);
	memcpy(buf, "\0", MAXBUF * sizeof(char)); /* reinit the buffer */
	printf("buf=%s buf2=%s\n", buf, buf2);

	shm_leave_deeplock(semid);
	shm_leave_mainlock(semid);

	return(buf2);
}

void
free_node(nodename)
	char *nodename;
{
	int semid, shmid;
	char *buf;

	extern int semkey;

	semid = semget(semkey, 4, NULL);
	if (semid == -1)
		bailout(__LINE__);
	shmid = shmget(semkey, MAXBUF * sizeof(char), NULL);
	if (shmid == -1)
		bailout(__LINE__);

	buf = shmat(shmid, NULL, 0);
	if ((int)buf == -1)
		bailout(__LINE__);

	kill((pid_t)semkey, SIGUSR2); /* alert jsd that we are coming in */
	shm_take_freelock(semid);
	shm_wait_seclock(semid);
	bcopy(nodename, buf, MAXBUF * sizeof(char));
	printf("freeing node %s\n", buf);
	shm_leave_freelock(semid);
}

/* 
 * Do the actual dirty work of the program, now that the arguments
 * have all been parsed out.
 */

void
do_command(argv, allrun, username)
	char **argv;
	char *username;
	int allrun;
{
	FILE *fd, *in;
	char buf[MAXBUF];
	char *nodename;
	int status, i, piping;
	char *p, *command, *rsh;
	pipe_t out, err;
	pid_t childpid;

	extern int debug;

	i = 0;
	piping = 0;
	in = NULL;
	nodename = NULL;

	if (debug)
		if (username != NULL)
			(void)printf("As User: %s\n", username);

	/* construct the command from the remains of argv */
	command = (char *)malloc(MAXBUF * sizeof(char));
	memcpy(command, "\0", MAXBUF * sizeof(char));
	for (p = *argv; p != NULL; p = *++argv ) {
		strcat(command, p);
		strcat(command, " ");
	}
	if (debug) {
		(void)printf("\nDo Command: %s\n", command);
	}
	if (strcmp(command,"") == 0) {
		piping = 1;
		if (isatty(STDIN_FILENO) && piping)
/* are we a terminal?  then go interactive! */
			(void)printf("%s>", progname);
		in = fdopen(STDIN_FILENO, "r");
		command = fgets(buf, sizeof(buf), in);
/* start reading stuff from stdin and process */
		if (command != NULL)
			if (strcmp(command,"\n") == 0)
				command = NULL;
	} else {
		close(STDIN_FILENO);
	}
	if (allrun)
		nodename = check_node();
	while (command != NULL) {
		if (!allrun)
			nodename = check_node();
		if (debug)
			printf("Working node: %s\n", nodename);
		if (pipe(out.fds) != 0)
			bailout(__LINE__);
		if (pipe(err.fds) != 0)
			bailout(__LINE__);
/* we set up pipes for each node, to prepare
 * for the oncoming barrage of data.
 */
		childpid = fork();
		switch (childpid) {
/* its the ol fork and switch routine eh? */
			case -1:
				bailout(__LINE__);
				break;
			case 0:
				if (piping)
					close(STDIN_FILENO);
				if (dup2(out.fds[1], STDOUT_FILENO) != STDOUT_FILENO)
/* stupid unix tricks vol 1 */
					bailout(__LINE__);
				if (dup2(err.fds[1], STDERR_FILENO) != STDERR_FILENO)
					bailout(__LINE__);
				if (close(out.fds[0]) != 0)
					bailout(__LINE__);
				if (close(err.fds[0]) != 0)
					bailout(__LINE__);
				rsh = getenv("RCMD_CMD");
				if (rsh == NULL)
					rsh = "rsh";
				if (debug)
					printf("%s %s %s\n", rsh, nodename, command);
				if (username != NULL)
/* interestingly enough, this -l thing works great with ssh */
					execlp(rsh, rsh, "-l", username, nodename,
						command, (char *)0);
				else
					execlp(rsh, rsh, nodename, command, (char *)0);
				bailout(__LINE__);
		} /* end switch */
		if (close(out.fds[1]) != 0)
/* now close off the useless stuff, and read the goodies */
			bailout(__LINE__);
		if (close(err.fds[1]) != 0)
			bailout(__LINE__);
		fd = fdopen(out.fds[0], "r"); /* stdout */
		while ((p = fgets(buf, sizeof(buf), fd)))
			(void)printf("%s: %s", nodename, p);
		fclose(fd);
		fd = fdopen(err.fds[0], "r"); /* stderr */
		while ((p = fgets(buf, sizeof(buf), fd)))
			if (errorflag)
				(void)printf("%s: %s", nodename, p);
		fclose(fd);
		(void)wait(&status);
		if (piping) {
			if (isatty(STDIN_FILENO) && piping)
/* yes, this is code repetition, no need to adjust your monitor */
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
log_bailout(line)
{
	bailout(line);
}