/* A small FastCGI server that generates image thumbnails on the fly. Originally
 * written for soup.io.
 *
 * Copyright © 2012 euphoria GmbH
 * Author: Lukas Martini <lutoma@phoria.eu>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <fcgiapp.h>
#include <sys/stat.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <wand/MagickWand.h>

#include "global.h"
#include "request.h"

int worker_id = -1;

void usage(char* argv[])
{
	fprintf(stderr, "Usage: %s [root] [thumbnail_root] [listen_addr] [user] [group] [num_workers]\n", argv[0]);
	syslog(LOG_ERR, "Invalid command line arguments\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[], char* envp[])
{
	if(argc < 7)
		usage(argv);

	char* root = argv[1];
	char* thumbnail_root = argv[2];
	char* listen_addr = argv[3];
	int num_workers = atoi(argv[6]);

	if(num_workers < 1)
		usage(argv);

	struct passwd* pwd = getpwnam(argv[4]);
	if (pwd == NULL)
			error_errno("getpwnam_r failed", EXIT_FAILURE);

	struct group* grp = getgrnam(argv[5]);
	if (grp == NULL)
			error_errno("getgrnam_r failed", EXIT_FAILURE);

	int userid = pwd->pw_uid;
	int groupid = grp->gr_gid;

	if(root[strlen(root) - 1] != '/' || thumbnail_root[strlen(thumbnail_root) - 1] != '/')
		error("Did you forget the ending slash in the (thumbnail) root directory path?", EXIT_FAILURE);

	// TODO Some more error checking

	// Initialize syslog
	syslog(LOG_INFO, "Starting up\n");
	#if defined(DEBUG)
		setlogmask(LOG_UPTO(LOG_DEBUG));
		openlog("fastresize", LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);
	#else
		setlogmask(LOG_UPTO(LOG_INFO));
		openlog("fastresize", LOG_CONS, LOG_USER);
	#endif

	// Change the working directory
	if((chdir(root)) < 0)
		error("Couldn't change working directory\n", EXIT_FAILURE);

	// Initialize FastCGI
	syslog(LOG_INFO, "Initializing FastCGI\n");
	if(FCGX_Init())
		error("Could not initialize FastCGI (during FCGX_Init())", EXIT_FAILURE);

	int listen_socket = FCGX_OpenSocket(listen_addr, 400);
	if(listen_socket < 0)
		error("Couldn't bind to FastCGI socket", EXIT_FAILURE);

	// Now that we've got our socket, drop root privileges
	if (getuid() == 0) {
		if (setgid(groupid) != 0)
			error_errno("setgid: Unable to drop group privileges", 1);
		if (setuid(userid) != 0)
			error_errno("setuid: Unable to drop user privileges", 1);
	}

	FCGX_Request request;
	if(FCGX_InitRequest(&request, listen_socket, 0))
		error("Couldn't initialize FastCGI request handler", EXIT_FAILURE);

	// Initialize ImageMagick
	syslog(LOG_INFO, "Initializing ImageMagick\n");
	MagickWandGenesis();
	atexit(MagickWandTerminus);

	/* Fork a new master process to daemonize and exit the old one. We use
	 * _Exit here to not trigger the atexit that terminates ImageMagick.
	 */
	if(fork())
		_Exit(EXIT_SUCCESS);


	syslog(LOG_INFO, "Forking workers\n");

	// Fork worker processes
	pid_t* worker_pids = calloc(num_workers, sizeof(pid_t));
	int worker_id;
	for(worker_id = 0; worker_id <= num_workers; worker_id++)
	{
		worker_pids[worker_id] = fork();

		// Exit the loop if we're the forked process
		if(worker_pids[worker_id] == 0)
			break;

		syslog(LOG_INFO, "Forked worker with PID %d\n", worker_pids[worker_id]);
	}

	// The following code is only executed in the master process.
	if(worker_id > num_workers)
	{
		printf("Successfull startup, see syslog for details\n");

		// Sleep a little until we get a SIG{TERM,HUP,INT}.
		sigset_t mask;
		sigfillset(&mask);
		sigdelset(&mask, SIGTERM);
		sigdelset(&mask, SIGINT);
		sigdelset(&mask, SIGHUP);
		sigsuspend(&mask);

		syslog(LOG_INFO, "Catched one of SIG{TERM,INT, HUP}, killing workers!\n");

		exit(EXIT_SUCCESS);
	}

	syslog(LOG_INFO, "Worker #%d is now listening for requests on 127.0.0.1:9000\n", worker_id);

	while(FCGX_Accept_r(&request) == 0)
		handle_request(&request, root, thumbnail_root);

	// Exit
	exit(EXIT_SUCCESS);
}