all: fastresize

CC=gcc
RM=rm
STRIP=strip

MAKE_CFLAGS=-O3 -std=gnu99 -Wall -I/usr/include/ImageMagick -Ilibscgi-0.9/ $(CFLAGS)
MAKE_LDFLAGS=-lfcgi -lMagickWand $(LDFLAGS)

fastresize: main.c
	$(CC) $(MAKE_CFLAGS) $(MAKE_LDFLAGS) -o fastresize main.c

clean:
	$(RM) -f fastresize

strip:
	$(STRIP) fastresize