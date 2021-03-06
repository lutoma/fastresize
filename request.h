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

#include <fcgiapp.h>

static inline void http_error(FCGX_Request* request, int num) {
	FCGX_FPrintF(request->out,
		"Status: %d %s\r\n"
		"Content-Type: text/html\r\n"
		"\r\n"
		"<h1>Error %d</h1>"
		"<hr /><a href='https://github.com/lutoma/fastresize'>fastresize</a>",
		num, num == 404 ? "Not found" : "Error",	num);
}

#define http_error_c(num) { http_error(request, num); return; }
#define http_sendfile(file) { FCGX_FPrintF(request->out, "X-Accel-Redirect: /asset-send/%s\r\n\r\n", file); }

void handle_request(FCGX_Request* request, char* root, char* thumbnail_root);