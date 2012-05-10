all: fastresize

CC=gcc
RM=rm
STRIP=strip

MAKE_CFLAGS=-O3 -std=gnu99 -Wall -I/usr/include/ImageMagick $(CFLAGS)
MAKE_LDFLAGS=-lfcgi -lMagickWand $(LDFLAGS)

fastresize: main.c resize.c request.c resize.h global.h request.h
	$(CC) $(MAKE_CFLAGS) $(MAKE_LDFLAGS) -o fastresize main.c request.c resize.c

.PHONY: clean
clean:
	$(RM) -f fastresize

.PHONY: strip
strip:
	$(STRIP) fastresize

.PHONY: install
install: fastresize fastresize-init
	install -g root -o root -m 755 --strip fastresize /usr/bin/fastresize
	install -g root -o root -m 755 fastresize-init /etc/init.d/fastresize

.PHONY: uninstall
uninstall:
	rm -f /usr/bin/fastresize
	rm -f /etc/init.d/fastresize

.PHONY: count
count:
	find -L . -type f -iregex "^.*\.\(c\|h\)" | xargs wc | tail -n 1 2> /dev/null 