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

#pragma once

#define error(desc, ret) do { syslog(LOG_ERR, "Error: %s\n",  desc); exit(ret); } while(0);
#define error_errno(desc, ret) do { syslog(LOG_ERR, "Error: %s (%s)\n",  desc, strerror(errno)); exit(ret); } while(0);
#define log_request(status, level, text, args...) { syslog(level, "%d - #%d - %s - " text "\n", status, worker_id, req_file ? req_file : "unknown", ## args); }

extern int worker_id;