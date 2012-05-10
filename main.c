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
#include <sys/stat.h>
#include <wand/MagickWand.h>
#include <fcgiapp.h>
#include <syslog.h>

#define error(desc, ret) do { syslog(LOG_ERR, "Error: %s\n",  desc); exit(ret); } while(0);
#define error_errno(desc, ret) do { syslog(LOG_ERR, "Error: %s (%s)\n",  desc, strerror(errno)); exit(ret); } while(0);
#define http_error(num) { FCGX_FPrintF(request->out, "\r\n\r\nError %d\n", num); }
#define http_error_c(num) { http_error(num); return; }
#define http_sendfile(file, mime) { FCGX_FPrintF(request->out, "Content-type: %s\r\nX-Sendfile: %s\r\n\r\n", mime, file); }

void handle_request(FCGX_Request* request, char* root, char* http_uri)
{
	int uri_splice = strlen(http_uri);

	// Get request URI (+1 to strip off the leading slash)
	char* req_file = FCGX_GetParam("REQUEST_URI", request->envp);

	// Check if the request URI is too short
	if(strlen(req_file) < uri_splice + 1)
		http_error_c(404);
	
	req_file += uri_splice;

	// Look for directory traversal attacks
	for(int i = 0; i < strlen(req_file); i++)
	{
		if(*(req_file + i) != '.')
			continue;
		
		if(*(req_file + i + 1) == '.' && *(req_file + i + 2) == '/')
			http_error_c(403);
	}

	// Check if the requested file exists already
	char* req_path = calloc(strlen(root) +  strlen(req_file) + 1, sizeof(char));
	sprintf(req_path, "%s%s", root, req_file);

	struct stat check_stat;
	if(stat(req_path, &check_stat) == 0)
	{
		if(check_stat.st_mode & S_IFREG)
		{
			http_sendfile(req_path, "image/png");
			syslog(LOG_INFO, "[200] %s - Already exists in file system\n", req_file);
			return;
		} else {
			// Directory, pipe, symlink or similar.
			http_error_c(403);
		}
	}

	int dot = -1;
	int underscore = -1;

	// Backwards scan for size and filename
	for(int i = strlen(req_file); i >= 0 && underscore == -1; i--)
	{
		switch(*(req_file + i))
		{
			case '.': dot = i + 1; break;
			case '_': underscore = i + 1; break;
		}
	}

	// Check for invalid filenames
	if(dot == -1 || underscore == -1 || dot <= underscore || underscore == dot - 1)
		http_error_c(404);

	// Convert size to an integer
	char* size_str = calloc(dot - underscore, sizeof(char));
	if(size_str == NULL)
	{
		syslog(LOG_ERR, "Could not allocate.\n");
		http_error_c(500);
	}

	memcpy(size_str, req_file + underscore, dot - underscore - 1);
	size_t size = atoi(size_str);
	free(size_str);

	if(size <= 0)
		http_error_c(404);

	// Get source picture
	char* source = calloc(strlen(req_file), sizeof(char));
	if(source == NULL)
	{
		syslog(LOG_ERR, "Could not allocate.\n");
		http_error_c(500);
	}

	memcpy(source, req_file, underscore - 1);
	strcpy(source + underscore - 1, req_file + dot - 1);

	// Create new image
	MagickWand* magick_wand = NewMagickWand();
	if((uintptr_t)magick_wand < 1)
		error("Could not initialize ImageMagick (magick_wand is NULL)", EXIT_FAILURE);

	// Read the Image
	char* path = calloc(strlen(root) +  strlen(source) + 1, sizeof(char));
	sprintf(path, "%s%s", root, source);

	bool status = MagickReadImage(magick_wand, path);
	if (status == false)
	{
		free(source);
		free(path);
		free(req_path);
		http_error_c(404);
	}

	size_t width = MagickGetImageWidth(magick_wand);
	size_t height = MagickGetImageHeight(magick_wand);

	// If the size is bigger than both height and width, redirect to original
	if(size >= width && size >= height)
	{
		FCGX_FPrintF(request->out, "Location: %s%s\r\n\r\n", http_uri, source);
		return;
	}

	size_t new_height = size;
	size_t new_width = size;

	bool square = false; // FIXME

	// Unless a square image is requested, calculcate proper aspect ratio
	if(!square)
	{
		if(width >= height)
			new_height = height * size / width;
		else
			new_width = width * size / height;
	}

	// Turn the image into a thumbnail sequence (for animated GIFs)
	MagickResetIterator(magick_wand);
	while (MagickNextImage(magick_wand) != false)
		MagickResizeImage(magick_wand, new_width, new_height, LanczosFilter, 1);

	// Write the image
	status = MagickWriteImages(magick_wand, req_path, true);
	if (status == MagickFalse)
		error("Could not write image", EXIT_FAILURE)

	http_sendfile(req_path, "image/png");
	syslog(LOG_INFO, "[200] %s - Generated from %s.\n", req_file, source);

	free(source);
	free(path);
	free(req_path);
	DestroyMagickWand(magick_wand);
}

void usage(char* argv[])
{
	fprintf(stderr, "Usage: %s [root] [http_uri] [listen_addr] [group] [user] [num_workers]\n", argv[0]);
	syslog(LOG_ERR, "Invalid command line arguments\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[], char* envp[])
{
	if(argc < 7)
		usage(argv);

	char* root = argv[1];
	char* http_uri = argv[2];
	char* listen_addr = argv[3];
	int groupid = atoi(argv[4]);
	int userid = atoi(argv[5]);
	int num_workers = atoi(argv[6]);

	if(groupid <= 0 || userid <= 0 || num_workers <= 0)
		usage(argv);

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

	// Fork worker processes
	bool worker = false;
	syslog(LOG_INFO, "Forking workers\n");
	for(int i = 1; i < num_workers; i++)
		if((worker = fork() == 0)) break;
	
	if(!worker)
		syslog(LOG_INFO, "Now listening for requests on 127.0.0.1:9000\n");
	
	while(FCGX_Accept_r(&request) == 0)
		handle_request(&request, root, http_uri);

	// Cleanup & exit
	MagickWandTerminus();
	exit(EXIT_SUCCESS);
}