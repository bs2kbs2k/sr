# sr - screenshot utility
# Copyright (C) 2022 ArcNyxx
# see LICENCE file for licensing information

VERSION = 0.9.1

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

WPROFILE = -Wall -Wextra -Wstrict-prototypes -Wmissing-declarations \
-Wunreachable-code -Wcast-align -Wpointer-arith -Wbad-function-cast -Winline \
-Wnested-externs -Wcast-qual -Wshadow -Wwrite-strings -Wno-unused-parameter \
-Wfloat-equal
STD = -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
LIB = -lX11 -lXext -lXcomposite -lXfixes -lXinerama -lImlib2

CFLAGS = $(WPROFILE) $(STD) -Os
LDFLAGS = $(LIB)
