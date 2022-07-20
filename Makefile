# sr - screenshot utility
# Copyright (C) 2022 ArcNyxx
# see LICENCE file for licensing information

.POSIX:

include config.mk

SRC = sr.c sel.c util.c
HEAD = sel.h util.h
OBJ = $(SRC:.c=.o)

all: sr

$(OBJ): $(HEAD) config.mk

.c.o:
	$(CC) $(CFLAGS) -c $<

sr: $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -f sr $(OBJ) sr-$(VERSION).tar.gz

dist: clean
	mkdir -p sr-$(VERSION)
	cp -R README LICENCE Makefile config.mk sr.1 $(SRC) sr-$(VERSION)
	tar -cf - sr-$(VERSION) | gzip -c > sr-$(VERSION).tar.gz
	rm -rf sr-$(VERSION)

install: all
	mkdir -p $(PREFIX)/bin $(MANPREFIX)/man1
	cp -f sr $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/sr
	sed 's/VERSION/$(VERSION)/g' < sr.1 > $(MANPREFIX)/man1/sr.1
	chmod 644 $(MANPREFIX)/man1/sr.1

uninstall:
	rm -f $(PREFIX)/bin/sr $(MANPREFIX)/man1/sr.1

.PHONY: all clean dist install uninstall
