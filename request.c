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
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "request.h"
#include "resize.h"

static int do_mkdir(const char *path, mode_t mode)
{
	struct stat st;
	int status = 0;

	if (stat(path, &st) != 0)
	{
		/* Directory does not exist */
		if (mkdir(path, mode) != 0)
			status = -1;
	}
	else if (!S_ISDIR(st.st_mode))
	{
		errno = ENOTDIR;
		status = -1;
	}

	return status;
}


int mkpath(const char *path, mode_t mode)
{
	char* pp;
	char* sp;
	int status;
	char* copypath = strdup(path);

	status = 0;
	pp = copypath;
	while (status == 0 && (sp = strchr(pp, '/')) != 0)
	{
		if (sp != pp)
		{
			// Neither root nor double slash in path
			*sp = 0;
			status = do_mkdir(copypath, mode);
			*sp = '/';
		}
		pp = sp + 1;
	}

	if(status == 0)
		status = do_mkdir(path, mode);

	free(copypath);
	return status;
}

void handle_request(FCGX_Request* request, char* root, char* thumbnail_root)
{
	char* basename = FCGX_GetParam("BASENAME", request->envp);
	char* extension = FCGX_GetParam("EXTENSION", request->envp);
	char* size_str = FCGX_GetParam("SIZE", request->envp);
	char* mode = FCGX_GetParam("MODE", request->envp);
	char* req_file = FCGX_GetParam("FILENAME", request->envp);

	if(!size_str || !basename || !extension || !req_file)
	{
		syslog(LOG_CRIT, "[500] %s - Invalid FastCGI parameters from server!\n", req_file ? req_file : "");
		http_error_c(500);
	}

	if(!mode)
		mode = "default";

	// Build absolute file path
	char* req_path = calloc(strlen(thumbnail_root) +  strlen(req_file) + 1, sizeof(char));
	if(!req_path)
	{
		syslog(LOG_EMERG, "[500] %s - Allocation failure for req_path!\n", req_file);
		http_error_c(500);
	}

	sprintf(req_path, "%s%s", thumbnail_root, req_file);

	// Check if the requested file exists already
	struct stat check_stat;
	if(stat(req_path, &check_stat) == 0)
	{
		free(req_path);
		if(check_stat.st_mode & S_IFREG)
		{
			http_sendfile(req_file);
			syslog(LOG_INFO, "[200] %s - Already exists in file system\n", req_file);
			return;
		} else {
			// Directory, pipe, symlink or similar.
			syslog(LOG_WARNING, "[403] %s - Request for existing file of wrong type (directory, symlink, ...)\n", req_file);
			http_error_c(403);
		}
	}

	// Get directory path
	char* dir_req_path = calloc(strlen(req_path) + 1, sizeof(char));
	if(!req_path)
	{
		syslog(LOG_EMERG, "[500] %s - Allocation failure for dir_req_path!\n", req_file);
		free(req_path);
		http_error_c(500);
	}

	strcpy(dir_req_path, req_path);

	for(int i = strlen(dir_req_path) - 1; i > 0; i--)
	{
		if(*(dir_req_path + i) == '/')
		{
			*(dir_req_path + i + 1) = 0;
			break;
		}
	}

	// Create thumbnail directory if neccessary
	if(stat(dir_req_path, &check_stat) != 0 && errno == ENOENT)
		if(mkpath(dir_req_path, S_IREAD | S_IWRITE | S_IEXEC | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0)
		{
			syslog(LOG_ERR, "[500] %s - Couldn't mkpath() %s\n", req_path, dir_req_path);
			free(req_path);
			free(dir_req_path);
			http_error_c(500);
		}

	free(dir_req_path);

	// Convert size to an integer
	size_t size = atoi(size_str);

	if(size <= 0)
	{
		free(req_path);
		http_error_c(404);
	}

	// Get source path
	char* path = calloc(strlen(root) +  strlen(basename) + strlen(extension) + 2, sizeof(char));
	if(!req_path)
	{
		syslog(LOG_EMERG, "[500] %s - Allocation failure for path!\n", req_file);
		free(req_path);
		http_error_c(500);
	}

	sprintf(path, "%s%s.%s", root, basename, extension);

	// Read the image, resize it and write it
	int resize_status = resize_image(path, req_path, size, mode);

	if(resize_status == 1)
	{
		FCGX_FPrintF(request->out, "Location: ../%s.%s\r\n\r\n", basename, extension);
		free(path);
		free(req_path);
		syslog(LOG_INFO, "[203] %s - Requested image size bigger than original - redirecting\n", req_file);
		return;
	} else if(resize_status < 0)
	{
		free(path);
		free(req_path);
		syslog(LOG_WARNING, "[404] %s - File not found\n", req_file);
		http_error_c(404);
	}

	http_sendfile(req_file);
	syslog(LOG_INFO, "[200] %s - Generated from %s.%s.\n", req_file, basename, extension);

	free(path);
	free(req_path);
}