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

void handle_request(FCGX_Request* request, char* root)
{
	char* basename = FCGX_GetParam("BASENAME", request->envp);
	char* extension = FCGX_GetParam("EXTENSION", request->envp);
	char* size_str = FCGX_GetParam("SIZE", request->envp);
	char* mode = FCGX_GetParam("MODE", request->envp);

	if(!size_str || !basename || !extension)
		http_error_c(500);

	// Build filename
	char* req_file = calloc(strlen(basename) + strlen(size_str) + strlen(extension) + 3, sizeof(char));
	sprintf(req_file, "%s_%s.%s", basename, size_str, extension);

	// Build absolute file path
	char* req_path = calloc(strlen(root) +  strlen(req_file) + 1, sizeof(char));
	sprintf(req_path, "%s%s", root, req_file);

	// Check if the requested file exists already
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

	// Convert size to an integer
	size_t size = atoi(size_str);

	if(size <= 0)
		http_error_c(404);

	// Get source path
	char* path = calloc(strlen(root) +  strlen(basename) + strlen(extension) + 2, sizeof(char));
	sprintf(path, "%s%s.%s", root, basename, extension);

	// Read the image, resize it and write it
	int resize_status = resize_image(path, req_path, size, mode);

	if(resize_status == 1)
	{
		FCGX_FPrintF(request->out, "Location: ../%s.%s\r\n\r\n", basename, extension);
		free(path);
		free(req_path);
		return;
	} else if(resize_status < 0)
	{
		free(path);
		free(req_path);
		http_error_c(404);
	}

	http_sendfile(req_file, "image/png");
	syslog(LOG_INFO, "[200] %s - Generated from %s.%s.\n", req_file, basename, extension);

	free(path);
	free(req_path);
}