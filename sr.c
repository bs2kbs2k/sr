/* sr - screenshot utility
 * Copyright (C) 2022 ArcNyxx
 * see LICENCE file for licensing information */

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Imlib2.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <unistd.h> // TODO

static void die(const char *fmt, ...);
static void parse_constant(char *fmt, int *array);

Display *dpy;
Screen *scr;
Visual *vis;
Window root;

static void
die(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	vfprintf(stderr, fmt, list);
	va_end(list);

	if (fmt[strlen(fmt) - 1] != '\n')
		perror(NULL);

	XCloseDisplay(dpy);
	exit(1);
}

static void
parse_constant(char *fmt, int *array)
{
	char *start = fmt, *end;
	for (int i = 0; i < 4; ++i, start = ++end) {
		if ((end = strchr(start, '.')) == NULL || !isdigit(start[0]))
			die("sr: invalid option: %s\n", fmt);
		array[i] = atoi(start);
	}
}

int
main(int argc, char **argv)
{
	char *csel = NULL;
	bool cur = true, fre = false;
	struct { int x, y, w, h; bool all, mon, win, sel; } opt = { 0 };
	for (argv = &argv[1]; *argv != NULL; argv = &argv[1]) {
		if ((*argv)[0] != '-')
			die("sr: invalid argument: %s\n", *argv);
		switch ((*argv)[1]) {
		case 'a': opt.all = true; break;
		case 'c': cur    = false; break;
		case 'f': fre     = true; break;
		case 'i': csel = *(argv = &argv[1]); break;
		case 'm': opt.mon = true; break;
		case 's': opt.sel = true; break;
		case 'w': opt.win = true; break;
		default: break;
		}
	}
	if ((dpy = XOpenDisplay(NULL)) == NULL)
		die("sr: unable to open display\n");

	scr  = ScreenOfDisplay(dpy, DefaultScreen(dpy));
	root = RootWindow(     dpy, XScreenNumberOfScreen(scr));
	vis  = DefaultVisual(  dpy, XScreenNumberOfScreen(scr));
	imlib_context_set_display(dpy);
	imlib_context_set_drawable(root);
	imlib_context_set_visual(vis);
	imlib_context_set_colormap(DefaultColormap(dpy,
			XScreenNumberOfScreen(scr)));
	imlib_context_set_color_modifier(NULL);
	imlib_context_set_operation(IMLIB_OP_COPY);

	if (fre)
		XGrabServer(dpy);
	if (opt.all)
		opt.x = opt.y = 0, opt.w = scr->width, opt.h = scr->height;
	if (csel != NULL)
		parse_constant(csel, &opt.x);
//	if (opt.select == Select)
//		if (grab(&opt.x, &opt.y, &opt.w, &opt.h))
//			goto clip;
	if (opt.mon) {
		opt.w = opt.h = 1; /* TODO */
	} else {
		Window target = 0;
		XGetInputFocus(dpy, &target, &(int){ 0 });

		XWindowAttributes attrs;
		if (!XGetWindowAttributes(dpy, target, &attrs) ||
				attrs.map_state != IsViewable)
			die("sr: unable to get window\n");
		opt.w = attrs.width, opt.h = attrs.height;
		XTranslateCoordinates(dpy, target, root, 0, 0, &opt.x,
				&opt.y, &(Window){ 0 });

#define BW attrs.border_width
		if (BW > 0) opt.w += BW * 2, opt.h += BW * 2,
				opt.x -= BW, opt.y -= BW;
	}

//clip:
	if (opt.x < 0) opt.w += opt.x, opt.x = 0;
	if (opt.y < 0) opt.h += opt.y, opt.y = 0;
	opt.w = (opt.x + opt.w) <= scr->width  ? opt.w : scr->width;
	opt.h = (opt.y + opt.h) <= scr->height ? opt.h : scr->height;

	Imlib_Image image = imlib_create_image_from_drawable(0,
			opt.x, opt.y, opt.w, opt.h, true);
	if (image == NULL)
		die("sr: unable to grab image\n");
	if (cur) {
		XFixesCursorImage *xcur;
		if ((xcur = XFixesGetCursorImage(dpy)) == NULL)
			die("sr: unable to get cursor image\n");

		Imlib_Image img;
		DATA32 data[xcur->width * xcur->height];
		for (int i = 0; i < (xcur->width * xcur->height); ++i)
			data[i] = xcur->pixels[i];
		if ((img = imlib_create_image_using_data(xcur->width,
				xcur->height, data)) == NULL)
			die("sr: unable to create cursor image\n");

		imlib_context_set_image(img);
		imlib_image_set_has_alpha(true);
		imlib_context_set_image(image);
		imlib_blend_image_onto_image(img, 0, 0, 0, xcur->width, xcur->
				height, xcur->x - xcur->xhot - opt.x, xcur->y -
				xcur->yhot - opt.y, xcur->width, xcur->height);
		imlib_context_set_image(img);
		imlib_free_image();
		XFree(xcur);
	}
	if (fre)
		XUngrabServer(dpy);

	Imlib_Load_Error ret;
	imlib_context_set_image(image);
	imlib_image_set_format("png");
	imlib_save_image_with_error_return("/dev/stdout", &ret);
	imlib_free_image_and_decache();

	XCloseDisplay(dpy);
	return ret != 0;
}
