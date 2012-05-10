/* Copyright © 2012 euphoria GmbH
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
#include <syslog.h>
#include <fcgiapp.h>
#include <sys/stat.h>
#include <string.h>

#include "global.h"
#include "request.h"
#include "resize.h"

void handle_request(FCGX_Request* request, char* root, char* http_uri)
{
	int uri_splice = strlen(http_uri);

	// Get request URI (+1 to strip off the leading slash)
	char* req_file = FCGX_GetParam("REQUEST_URI", request->envp);
	
	if(!req_file)
		return;

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
			http_sendfile(req_file, "image/png");
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

	char* path = calloc(strlen(root) +  strlen(source) + 1, sizeof(char));
	sprintf(path, "%s%s", root, source);

	// Read the image, resize it and write it
	int resize_status = resize_image(path, req_path, size);

	if(resize_status == 1)
	{
		FCGX_FPrintF(request->out, "Location: %s%s\r\n\r\n", http_uri, source);
		return;
	} else if(resize_status < 0)
	{
		free(source);
		free(path);
		free(req_path);
		http_error_c(404);
	}

	http_sendfile(req_file, "image/png");
	syslog(LOG_INFO, "[200] %s - Generated from %s.\n", req_file, source);

	free(source);
	free(path);
	free(req_path);
}