fastresize
==========

fastresize is a small FastCGI daemon that resizes images on the fly. It has a 
very low memory footprint and uses almost no CPU time (except for the actual
resizing that ImageMagick does, of course).

Dependencies
------------

* ImageMagick
* libmagickwand
* libfcgi

Usage
-----

fastresize was built to be used with nginx and makes use of some nginx-specific
features like the X-Accel-Redirect. Altough it might be possible to get it to
run with other webservers, it is untested (Early version have also been used
with lighttpd).

Please see nginx-sample.conf for an example of how to configure your nginx to
correctly pass requests to fastresize.

Launching fastresize
---------------------

The syntax to launch fastresize is

	fastresize [root] [listen_addr] [group] [user] [num_workers]

root
  The directory assets should be served from. Don't forget the trailing slash.

listen_addr
  The socket address to listen on, for example 127.0.0.1:9000

group
  Group ID to run under

user
  User ID to run under

num_workers
  A number larger or equal to 1 that defines how many worker processes will be
  spawned. Usually, n+1 is a good choice (n = CPU cores).

