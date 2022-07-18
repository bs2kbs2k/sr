/* sr - screenshot utility
 * Copyright (C) 2022 ArcNyxx
 * see LICENCE file for licensing information */

#ifndef OPT_H
#define OPT_H

typedef struct options {
	int lstyle, lwidth, lalpha;
	bool asel, border, freeze, line, ptr, select, focus;
	char *dpy, *lcolour, *lmode;

	union {
		struct {
			int x, y, w, h;
		};
		int arr[4];
	};
} options_t;

void options(int argc, char **argv, options_t *opts);

#endif /* OPT_H */
