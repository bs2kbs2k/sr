/* sr - screenshot utility
 * Copyright (C) 2022 ArcNyxx
 * see LICENCE file for licensing information */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <Imlib2.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include "util.h"

#define OPTION(key, value) case key: value = true

Display *dpy;
Visual *vis;
Window root;

int
main(int argc, char **argv)
{
	struct {
		int x, y, w, h;
		bool cursor, freeze;
		bool all, constant, monitor, window, select;
	} opt = { 0 };
	for (argv = &argv[1]; *argv != NULL; argv = &argv[1]) {
		if ((*argv)[0] != '-')
			die("sr: invalid argument: %s\n", *argv);
		switch ((*argv)[1]) {
		OPTION('a', opt.all     ); break;
		OPTION('c', opt.cursor  ); break;
		OPTION('f', opt.freeze  ); break;
		OPTION('i', opt.constant);
			char *start = *(argv = &argv[1]), *end;
			for (int i = 0; i < 4; ++i, start = ++end) {
				if ((end = strchr(start, '.')) == NULL ||
						!isdigit(start[0]))
					die("sr: invalid option: %s\n", *argv);
				*(&opt.x + i) = atoi(start);
			}
			break;
		OPTION('m', opt.monitor ); break;
		OPTION('s', opt.select  ); break;
		OPTION('w', opt.window  ); break;
		default: break;
		}
	}
	if ((dpy = XOpenDisplay(NULL)) == NULL)
		die("sr: unable to open display\n");

	Screen *scr = ScreenOfDisplay(dpy, DefaultScreen(dpy));
	root = RootWindow(dpy, XScreenNumberOfScreen(scr));
	vis = DefaultVisual(dpy, XScreenNumberOfScreen(scr));
	imlib_context_set_display(dpy);
	imlib_context_set_drawable(root);
	imlib_context_set_visual(vis);
	imlib_context_set_colormap(DefaultColormap(dpy,
			XScreenNumberOfScreen(scr)));
	imlib_context_set_color_modifier(NULL);
	imlib_context_set_operation(IMLIB_OP_COPY);

	if (opt.freeze)
		XGrabServer(dpy);

	if (opt.all) {
		opt.x = opt.y = 0, opt.w = scr->width, opt.h = scr->height;
	} else {
		if (opt.constant)
			goto out;
	/*	if (opt.select == Select)
			if (select(&opt.x, &opt.y, &opt.w, &opt.h))
				goto out;

		if (opt.select == Monitor) {
			opt.w = opt.h = 1; /* TODO */ /*
		} else {
			Window target = 0;
			XGetInputFocus(dpy, &target, &(int){ });

			XWindowAttributes attrs;
			if (!XGetWindowAttributes(dpy, target, &attrs) ||
					attrs.map_state != IsViewable)
				die("sr: unable to get window\n");
			XTranslateCoordinates(dpy, target, root, 0, 0, &opt.x,
					&opt.y, &(Window){ });
			opt.w = attr.width, opt.h = attr.height;

			const int bw = attrs.border_width;
			if (bw > 0)
				opt.w += bw * 2, opt.h += bw * 2, opt.x -= bw,
					opt.y -= bw;
		} */
out:
		if (opt.x < 0)
			opt.w += opt.x, opt.x = 0;
		if (opt.y < 0)
			opt.h += opt.y, opt.y = 0;
		if ((opt.x + opt.w) > scr->width)
			opt.w = scr->width - opt.x;
		if ((opt.y + opt.h) > scr->height)
			opt.h = scr->height - opt.y;
	}
	Imlib_Image image = imlib_create_image_from_drawable(0,
			opt.x, opt.y, opt.w, opt.h, true);
	if (image == NULL)
		die("sr: unable to grab image\n");
	if (opt.cursor) {
		XFixesCursorImage *cur;
		if ((cur = XFixesGetCursorImage(dpy)) == NULL)
		        die("sr: unable to get cursor image\n");

		Imlib_Image img;
		DATA32 data[cur->width * cur->height];
		for (int i = 0; i < (cur->width * cur->height); ++i)
		        data[i] = cur->pixels[i];
		if ((img = imlib_create_image_using_data(cur->width,
		                cur->height, data)) == NULL)
		        die("sr: unable to create cursor image\n");
		XFree(cur);

		imlib_context_set_image(img);
		imlib_image_set_has_alpha(true);
		imlib_context_set_image(image);
		imlib_blend_image_onto_image(img, 0, 0, 0, cur->width, cur->
				height, cur->x - cur->xhot - opt.x, cur->y -
				cur->yhot - opt.y, cur->width, cur->height);
		imlib_context_set_image(img);
		imlib_free_image();
	}
	if (opt.freeze)
		XUngrabServer(dpy);

	Imlib_Load_Error ret;
	imlib_context_set_image(image);
	imlib_image_set_format("png");
	imlib_save_image_with_error_return("/dev/stdout", &ret);
	imlib_free_image_and_decache();

	XCloseDisplay(dpy);
	return ret != 0;
}
