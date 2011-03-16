#include "../autoconfig.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char		*progname;
static const key_t	key_astribanks = 0xAB11A0;
static int		debug;
static int		verbose;
static int		timeout_seconds = 60;

/* If libc provides no timeout variant: try to do without it: */
#ifndef HAVE_SEMTIMEDOP
#define  semtimedop(sem, ops, n, timeout)    semop(sem, ops, n)
#endif

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-d] [-t <seconds>] [-a|-r|-w]\n", progname);
	exit(1);
}

static int absem_get(int createit)
{
	int	flags = (createit) ? IPC_CREAT | 0644 : 0;
	int	absem;

	if((absem = semget(key_astribanks, 1, flags)) < 0)
		absem = -errno;
	return absem;
}

static int absem_touch(void)
{
	int		absem;

	if((absem = absem_get(1)) < 0) {
		perror(__FUNCTION__);
		return absem;
	}
	if(semctl(absem, 0, SETVAL, 0) < 0) {
		perror("SETVAL");
		return -errno;
	}
	if(debug)
		fprintf(stderr, "%s: touched absem\n", progname);
	if(verbose)
		printf("Astribanks initialization is starting\n");
	return 0;
}

static int absem_remove(void)
{
	int	absem;

	if((absem = absem_get(0)) < 0) {
		if(absem == -ENOENT) {
			if(debug)
				fprintf(stderr, "%s: absem already removed\n", progname);
			return 0;
		}
		perror(__FUNCTION__);
		return absem;
	}
	if(semctl(absem, 0, IPC_RMID, 0) < 0) {
		perror("RMID");
		return -errno;
	}
	if(debug)
		fprintf(stderr, "%s: removed absem\n", progname);
	if(verbose)
		printf("Astribanks initialization is done\n");
	return 0;
}

static int absem_wait(void)
{
	int		absem;
	struct sembuf	sops;
	long		now;
	long		start_wait;
	struct timespec	timeout;

	if((absem = absem_get(0)) < 0) {
		perror(__FUNCTION__);
		return absem;
	}
	sops.sem_num = 0;
	sops.sem_op = -1;
	sops.sem_flg = 0;
	start_wait = time(NULL);
	timeout.tv_sec = timeout_seconds;
	timeout.tv_nsec = 0;
	if(semtimedop(absem, &sops, 1, &timeout) < 0) {
		switch(errno) {
		case EIDRM:	/* Removed -- OK */
			break;
		case EAGAIN:	/* Timeout -- Report */
			fprintf(stderr, "Astribanks waiting timed out\n");
			return -errno;
		default:	/* Unexpected errors */
			perror("semop");
			return -errno;
		}
		/* fall-thgough */
	}
	now = time(NULL);
	if(debug)
		fprintf(stderr, "%s: waited on absem %ld seconds\n", progname, now - start_wait);
	if(verbose)
		printf("Finished after %ld seconds\n", now - start_wait);
	return 0;
}

static int absem_detected(void)
{
	int	absem;

	if((absem = absem_get(0)) < 0) {
		if(debug)
			fprintf(stderr, "%s: absem does not exist\n", progname);
		return absem;
	}
	if(debug)
		fprintf(stderr, "%s: absem exists\n", progname);
	if(verbose)
		printf("Astribanks are initializing...\n");
	return 0;
}

int main(int argc, char *argv[])
{
	const char	options[] = "dvarwt:h";
	int		val;

	progname = argv[0];
	while (1) {
		int	c;
		int	t;

		c = getopt (argc, argv, options);
		if (c == -1)
			break;

		switch (c) {
			case 'd':
				debug++;
				break;
			case 'v':
				verbose++;
				break;
			case 't':
				t = atoi(optarg);
				if(t <= 0) {
					fprintf(stderr,
						"%s: -t expect a positive number of seconds: '%s'\n",
						progname, optarg);
					usage();
				}
				timeout_seconds = t;
				break;
			case 'a':
				if((val = absem_touch()) < 0) {
					fprintf(stderr, "%s: Add failed: %d\n", progname, val);
					return 1;
				}
				return 0;
			case 'r':
				if((val = absem_remove()) < 0) {
					fprintf(stderr, "%s: Remove failed: %d\n", progname, val);
					return 1;
				}
				return 0;
			case 'w':
				if((val = absem_wait()) < 0) {
					fprintf(stderr, "%s: Wait failed: %d\n", progname, val);
					return 1;
				}
				return 0;
			case 'h':
			default:
				fprintf(stderr, "Unknown option '%c'\n", c);
				usage();
		}
	}
	val = absem_detected();
	return (val == 0) ? 0 : 1;
}
