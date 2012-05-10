fastresize
==========

fastresize is a small FastCGI daemon that resizes images on the fly. It has a 
very low memory footprint and uses almost no CPU time (except for the actual
resizing that ImageMagick does, of course).

URL format
----------

As fastresize was originally written for soup.io, it is very closely tied to
the URL format used for assets there:

	http://example.org/<subdirectory>/<imagename>-<size>.<extension>

Both the subdirectopry part (i.e. you can also serve your assets from / if
that's what you want to) and the -<size> path can be left out.

Some valid examples:

	http://example.org/asset/test.png
	http://example.org/asset/test-200.png
	http://example.org/test-200.png

If the requested size is larger than the original image, it redirects to the
URL of the original image for caching purposes.

Dependencies
------------

* ImageMagick
* libmagickwand
* libfcgi

Usage
-----

The syntax to launch fastresize is

	fastresize [root] [http_uri] [listen_addr] [group] [user] [num_workers]

root
  The directory assets should be served from. Don't forget the trailing slash.

http_uri
  The http base URI. For example, if you want your assets to be served from
  http://example.org/assets/, set this to "/assets/".

listen_addr
  The socket address to listen on, for example 127.0.0.1:9000

group
  Group ID to run under

user
  User ID to run under

num_workers
  A number larger or equal to 1 that defines how many worker processes will be
  spawned. Usually, n+1 is a good choice (n = CPU cores).

