/* scrot.c

Copyright 1999-2000 Tom Gilbert <tom@linuxbrit.co.uk,
                                  gilbertt@linuxbrit.co.uk,
                                  scrot_sucks@linuxbrit.co.uk>
Copyright 2009      James Cameron <quozl@us.netrek.org>
Copyright 2010      Ibragimov Rinat <ibragimovrinat@mail.ru>
Copyright 2017      Stoney Sauce <stoneysauce@gmail.com>
Copyright 2019-2022 Daniel T. Borelli <danieltborelli@gmail.com>
Copyright 2019      Jade Auer <jade@trashwitch.dev>
Copyright 2020      blockparole
Copyright 2020      Cungsten Tarbide <ctarbide@tuta.io>
Copyright 2020      Hinigatsu <hinigatsu@protonmail.com>
Copyright 2020      nothub
Copyright 2020      Sean Brennan <zettix1@gmail.com>
Copyright 2021      c0dev0id <sh+github@codevoid.de>
Copyright 2021      Christopher R. Nelson <christopher.nelson@languidnights.com>
Copyright 2021-2022 Guilherme Janczak <guilherme.janczak@yandex.com>
Copyright 2021      IFo Hancroft <contact@ifohancroft.com>
Copyright 2021      Peter Wu <peterwu@hotmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies of the Software and its documentation and acknowledgment shall be
given in the documentation and software packages that this Software was
used.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <Imlib2.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>

#include "imlib.h"
#include "options.h"
#include "scrot.h"

static void uninitXAndImlib(void);
static Imlib_Image scrotGrabFocused(void);
static Bool scrotXEventVisibility(Display *, XEvent *, XPointer);
static Imlib_Image scrotGrabAutoselect(void);
static Imlib_Image scrotGrabShot(void);
static Window scrotGetClientWindow(Display *, Window);
static Window scrotFindWindowByProperty(Display *, const Window,
                                              const Atom);
static int findWindowManagerFrame(Window *const, int *const);

static Imlib_Image scrotGrabFocused(void)
{
    Imlib_Image im = NULL;
    int rx = 0, ry = 0, rw = 0, rh = 0;
    Window target = None;
    int ignored;

    XGetInputFocus(disp, &target, &ignored);
    if (!scrotGetGeometry(target, &rx, &ry, &rw, &rh))
        return NULL;
    scrotNiceClip(&rx, &ry, &rw, &rh);
    im = imlib_create_image_from_drawable(0, rx, ry, rw, rh, 1);
    if (opt.pointer)
        scrotGrabMousePointer(im, rx, ry);
    clientWindow = target;
    return im;
}

static Imlib_Image scrotGrabAutoselect(void)
{
    Imlib_Image im = NULL;
    int rx = opt.autoselectX, ry = opt.autoselectY, rw = opt.autoselectW,
        rh = opt.autoselectH;

    scrotNiceClip(&rx, &ry, &rw, &rh);
    im = imlib_create_image_from_drawable(0, rx, ry, rw, rh, 1);
    if (opt.pointer)
        scrotGrabMousePointer(im, rx, ry);
    return im;
}

/* Clip rectangle nicely */
void scrotNiceClip(int *rx, int *ry, int *rw, int *rh)
{
    if (*rx < 0) {
        *rw += *rx;
        *rx = 0;
    }
    if (*ry < 0) {
        *rh += *ry;
        *ry = 0;
    }
    if ((*rx + *rw) > scr->width)
        *rw = scr->width - *rx;
    if ((*ry + *rh) > scr->height)
        *rh = scr->height - *ry;
}

static int findWindowManagerFrame(Window *const target, int *const frames)
{
    int x, status;
    unsigned int d;
    Window rt, *children, parent;

    status = XGetGeometry(disp, *target, &root, &x, &x, &d, &d, &d, &d);

    if (!status)
        return 0;

    for (;;) {
        status = XQueryTree(disp, *target, &rt, &parent, &children, &d);
        if (status && (children != None))
            XFree(children);
        if (!status || (parent == None) || (parent == rt))
            break;
        *target = parent;
        ++*frames;
    }
    return 1;
}

/* Get geometry of window and use that */
int scrotGetGeometry(Window target, int *rx, int *ry, int *rw, int *rh)
{
    Window child;
    XWindowAttributes attr;
    int stat, frames = 0;

    /* Get windowmanager frame of window */
    if (target != root) {
        if (findWindowManagerFrame(&target, &frames)) {

            /* Get client window. */
            if (!opt.border)
                target = scrotGetClientWindow(disp, target);
            XRaiseWindow(disp, target);

            /* Give the WM time to update the hidden area of the window.
               Some windows never send the event, a time limit is placed.
            */
            XSelectInput(disp, target, FocusChangeMask);

            struct timespec delay = {0, 10000000L}; // 10ms

            for (short i = 0; i < 30; ++i) {
                if (XCheckIfEvent(disp, &(XEvent){0}, &scrotXEventVisibility, (XPointer)&target))
                    break;
                nanosleep(&delay, NULL);
            }
        }
    }
    stat = XGetWindowAttributes(disp, target, &attr);
    if (!stat || (attr.map_state != IsViewable))
        return 0;
    *rw = attr.width;
    *rh = attr.height;
    XTranslateCoordinates(disp, target, root, 0, 0, rx, ry, &child);

    /* Special case when the TWM emulates the border directly on the window. */
    if (opt.border == 1 && frames < 2 && attr.border_width > 0) {
        *rw += attr.border_width * 2;
        *rh += attr.border_width * 2;
        *rx -= attr.border_width;
        *ry -= attr.border_width;
    }
    return 1;
}

Window scrotGetWindow(Display *display, Window window, int x, int y)
{
    Window source, target;

    int status, xOffset, yOffset;

    source = root;
    target = window;
    if (window == None)
        window = root;
    while (1) {
        status = XTranslateCoordinates(display, source, window, x, y, &xOffset,
            &yOffset, &target);
        if (!status)
            break;
        if (target == None)
            break;
        source = window;
        window = target;
        x = xOffset;
        y = yOffset;
    }
    if (target == None)
        target = window;
    return target;
}


void scrotGrabMousePointer(const Imlib_Image image, const int xOffset,
    const int yOffset)
{
    XFixesCursorImage *xcim = XFixesGetCursorImage(disp);

    if (!xcim) {
        warnx("Failed to get mouse cursor image.");
        return;
    }

    const unsigned short width = xcim->width;
    const unsigned short height = xcim->height;
    const int x = (xcim->x - xcim->xhot) - xOffset;
    const int y = (xcim->y - xcim->yhot) - yOffset;
    DATA32 *pixels = NULL;

#ifdef __i386__
    pixels = (DATA32 *)xcim->pixels;
#else
    DATA32 data[width * height * 4];

    unsigned int i;
    for (i = 0; i < (width * height); i++)
        data[i] = (DATA32)xcim->pixels[i];

    pixels = data;
#endif

    Imlib_Image imcursor = imlib_create_image_using_data(width, height, pixels);

    XFree(xcim);

    if (!imcursor) {
        errx(EXIT_FAILURE,
            "scrotGrabMousePointer: Failed create image using data.");
    }

    imlib_context_set_image(imcursor);
    imlib_image_set_has_alpha(1);
    imlib_context_set_image(image);
    imlib_blend_image_onto_image(imcursor, 0, 0, 0, width, height, x, y, width,
        height);
    imlib_context_set_image(imcursor);
    imlib_free_image();
}

static Imlib_Image scrotGrabShot(void)
{
    Imlib_Image im;

    im = imlib_create_image_from_drawable(0, 0, 0, scr->width,
        scr->height, 1);
    if (opt.pointer)
        scrotGrabMousePointer(im, 0, 0);

    return im;
}

static Bool scrotXEventVisibility(Display *dpy, XEvent *ev, XPointer arg)
{
    (void)dpy; // unused
    Window *win = (Window *)arg;
    return (ev->xvisibility.window == *win);
}

static Window scrotGetClientWindow(Display *display, Window target)
{
    Atom state;
    Atom type = None;
    int format, status;
    unsigned char *data;
    unsigned long after, items;
    Window client;

    state = XInternAtom(display, "WM_STATE", True);
    if (state == None)
        return target;
    status = XGetWindowProperty(display, target, state, 0L, 0L, False,
        AnyPropertyType, &type, &format, &items, &after,
        &data);
    if ((status == Success) && (type != None))
        return target;
    client = scrotFindWindowByProperty(display, target, state);
    if (!client)
        return target;
    return client;
}

static Window scrotFindWindowByProperty(Display *display, const Window window,
    const Atom property)
{
    Atom type = None;
    int format, status;
    unsigned char *data;
    unsigned int i, numberChildren;
    unsigned long after, numberItems;
    Window child = None, *children, parent, rootReturn;

    status = XQueryTree(display, window, &rootReturn, &parent, &children,
        &numberChildren);
    if (!status)
        return None;
    for (i = 0; (i < numberChildren) && (child == None); i++) {
        status = XGetWindowProperty(display, children[i], property, 0L, 0L, False,
            AnyPropertyType, &type, &format,
            &numberItems, &after, &data);
        if (data)
            XFree(data);
        if ((status == Success) && type)
            child = children[i];
    }
    for (i = 0; (i < numberChildren) && (child == None); i++)
        child = scrotFindWindowByProperty(display, children[i], property);
    if (children != None)
        XFree(children);
    return (child);
}

/* sr - screenshot utility
 * Copyright (C) 2022 ArcNyxx
 * see LICENCE file for licensing information */

#include <Imlib2.h>
#include <X11/Xlib.h>

#include "util.h"

Display *dpy;
Visual *vis;
Window root;

static void
clip(int *x, int *y, int *w, int *h)
{
	if (*x < 0)
		*w += *x, *x = 0;
	if (*y < 0)
		*h += *y, *y = 0;
	if ((*x + *w) > scr->width)
		*w = scr->width - *x;
	if ((*y + *h) > scr->height)
		*h = scr->height - *y;
}

static void
dest(void)
{
	XCloseDisplay(dpy);
}

void
geom(Window target, int *x, int *y, int *w, int *h)
{
	if (target != root) {
	}
}

int
main(int argc, char **argv)
{
	typedef enum select {
		Select; All; Monitor; Window; Constant;
	} select_t;
	struct opt {
		int x, y, w, h;
		select_t select;
		bool cursor, freeze;
	} opt = { 0 };
	for (argv = &argv[1]; *argv != NULL; argv = &argv[1]) {
		if ((*argv)[0] != '-')
			die("sr: invalid argument: %s\n", *argv);
		switch ((*argv)[0]) {
		case 'a': opt.select = All; break;
		case 'c': opt.cursor = true; break;
		case 'f': opt.freeze = true; break;
		case 'i': opt.select = Constant;
			char *start = *(argv = &argv[1]), end;
			for (int i = 0; i < 4; ++i, start = end + 1) {
				if ((end = strchr(start, ',')) == NULL ||
						!isdigit(start[0]))
					die("sr: invalid option: %s\n", *argv);
				*end = '\0', *((&opt.x)[i]) = atoi(start);
			}
			break;
		case 'm': opt.select = Monitor; break;
		case 's': opt.select = Select; break;
		case 'w': opt.window = Window; break;
		}
	}

	if ((dpy = XOpenDisplay(NULL)) == NULL)
		die("sr: unable to open display\n");
	atexit(dest);

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

	Imlib_Image image;
	if (opt.select == Constant) {
		clip(&opt.x, &opt.y, &opt.w, &opt.h);
	} else if (opt.Select == All) {
		opt.w = scr->width, opt.h = scr->height;
	} else {
		if (opt.select == Select)
			if (select(&opt.x, &opt.y, &opt.w, &opt.h))
				goto out;

		if (opt.select == Monitor) {

		} else {
			Window target = 0;
			XGetInputFocus(dpy, &target, &(int){ });
			if (!geom(target, &opt.x, &opt.y, &opt.w, &opt.h))
				die("sr: unable to get window\n");
		}
out:
		clip(&opt.x, &opt.y, &opt.w, &opt.h);
	}
	image = imlib_create_image_from_drawable(0, x, y, w, h, true);
	if (opt.ptr)
		mouse(image, x, y);
	if (image == NULL)
		die("sr: unable to grab image\n");

	if (opt.freeze)
		XUngrabServer(dpy);

	Imlib_Load_Error ret;
	imlib_context_set_image(image);
	imlib_image_set_format("png");
	imlib_image_attach_data_value("quality", NULL, 100, NULL);
	imlib_save_image_with_error_return("/dev/stdout", &ret);
	imlib_free_image_and_decache();
	return ret != 0;
}
