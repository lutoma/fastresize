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
#include <wand/MagickWand.h>

#include "global.h"
#include "request.h"

void usage(char* argv[])
{
	fprintf(stderr, "Usage: %s [root] [listen_addr] [user] [group] [num_workers]\n", argv[0]);
	syslog(LOG_ERR, "Invalid command line arguments\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[], char* envp[])
{
	if(argc < 6)
		usage(argv);

	char* root = argv[1];
	char* listen_addr = argv[2];
	int num_workers = atoi(argv[5]);

	if(num_workers < 1)
		usage(argv);

	struct passwd* pwd = getpwnam(argv[3]);
	if (pwd == NULL)
			error_errno("getpwnam_r failed", EXIT_FAILURE);

	struct group* grp = getgrnam(argv[4]);
	if (grp == NULL)
			error_errno("getgrnam_r failed", EXIT_FAILURE);

	int userid = pwd->pw_uid;
	int groupid = grp->gr_gid;

	if(root[strlen(root) - 1] != '/')
		error("Did you forget the ending slash in the root directory path?", EXIT_FAILURE);

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

	// Fork worker processes and exit main process (to daemonize)
	bool worker = false;
	syslog(LOG_INFO, "Forking workers\n");
	for(int i = 1; i <= num_workers; i++)
		if((worker = fork() == 0)) break;
	
	if(!worker)
	{
		syslog(LOG_INFO, "Now listening for requests on 127.0.0.1:9000\n");
		printf("Successfull startup, see syslog for details\n");

		// Change the filemode mask
		umask(0);

		// Close stdin, -out, -err
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		exit(EXIT_SUCCESS);
	}

	while(FCGX_Accept_r(&request) == 0)
		handle_request(&request, root);

	// Cleanup & exit
	MagickWandTerminus();
	exit(EXIT_SUCCESS);
}