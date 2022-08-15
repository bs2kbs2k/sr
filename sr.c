/* sr - screenshot utility
 * Copyright (C) 2022 ArcNyxx
 * see LICENCE file for licensing information */

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <Imlib2.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xinerama.h>

Display *dpy = NULL;
Screen *scr;
Window root;

static void die(const char *fmt, ...);
static void pick(int *x, int *y, int *w, int *h);
static void drive(int *x, int *y, int *w, int *h, int opt);

static void
die(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	vfprintf(stderr, fmt, list);
	va_end(list);

	if (fmt[strlen(fmt) - 1] != '\n')
		perror(NULL);
	if (dpy != NULL)
		XCloseDisplay(dpy);
	exit(1);
}

static void
pick(int *x, int *y, int *w, int *h)
{
	Cursor cursor[5];
	static const int names[5] = { XC_cross, XC_ur_angle,
			XC_ul_angle, XC_lr_angle, XC_ll_angle };
	for (int i = 0; i < 5; ++i)
		cursor[i] = XCreateFontCursor(dpy, names[i]);

	XColor col;
	if (!XAllocNamedColor(dpy, XDefaultColormap(dpy, DefaultScreen(dpy)),
			"grey", &col, &(XColor){ 0 }))
		die("sr: unable to allocate colour\n");

	XSetWindowAttributes attrs = { .background_pixel = col.pixel,
			.override_redirect = true };
	Window draw = XCreateWindow(dpy, root, 0, 0, scr->width, scr->height, 0,
			CopyFromParent, InputOutput, CopyFromParent,
			CWBackPixel | CWOverrideRedirect, &attrs);
	Atom atom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", false);
	XChangeProperty(dpy, draw, XInternAtom(dpy, "_NET_WM_WINDOW_TYPE",
			false), XA_ATOM, 32, PropModeReplace,
			(unsigned char *)&atom, 1);
	XSetClassHint(dpy, draw, &(XClassHint){
			(char []){ "sr" }, (char []){ "sr" } });

#define MASK ButtonMotionMask | ButtonPressMask | ButtonReleaseMask
	if (XGrabPointer(dpy, root, false, MASK, GrabModeAsync, GrabModeAsync,
			root, cursor[0], CurrentTime) != GrabSuccess)
		die("sr: unable to grab cursor\n");
	if (XGrabKeyboard(dpy, root, false, GrabModeAsync, GrabModeAsync,
			CurrentTime) != GrabSuccess)
		die("sr: unable to grab keyboard\n");

	XEvent evt; bool press;
	while (!XNextEvent(dpy, &evt)) switch (evt.type) {
	case ButtonPress:
		press = true, *x = evt.xbutton.x, *y = evt.xbutton.y;
		break;
	case ButtonRelease:
		goto done;
	case KeyPress: ;
		/* TODO: rearrange coords when negative w or h */
		KeySym *keysym;
		if ((keysym = XGetKeyboardMapping(dpy, evt.xkey.keycode, 1,
				&(int){ 0 })) == NULL)
			break;
		if (*keysym == XK_Right)
			*x = ++*x <= scr->width ? *x : scr->width;
		else if (*keysym == XK_Left)
			*x = --*x >= 0 ? *x : 0;
		else if (*keysym == XK_Up)
			*y = ++*y <= scr->width ? *y : scr->width;
		else if (*keysym == XK_Down)
			*y = --*y >= 0 ? *y : 0;
		else
			die("sr: key pressed\n");
		XFree(keysym);
		/* FALLTHROUGH */
	case MotionNotify:
		if (!press)
			break;

		Cursor cur;
		if (*x < evt.xbutton.x && *y < evt.xbutton.y)
			cur = cursor[3];
		else if (*x < evt.xbutton.x)
			cur = cursor[1];
		else if (*y < evt.xbutton.y)
			cur = cursor[4];
		else
			cur = cursor[2];
		XChangeActivePointerGrab(dpy, ButtonMotionMask |
				ButtonReleaseMask, cur, CurrentTime);

		if (evt.type == MotionNotify)
			*w = evt.xbutton.x - *x, *h = evt.xbutton.y - *y;
		XRectangle ln[4] = { { *x, *y, 1, *h }, { *x + *w, *y, 1, *h },
				{ *x, *y, *w, 1 }, { *x, *y + *h, *w, 1 } };
		XShapeCombineRectangles(dpy, draw, ShapeBounding, 0, 0, ln, 4,
				ShapeSet, 0);
		XMapWindow(dpy, draw);
	}

done:
	XUngrabKeyboard(dpy, CurrentTime);
	XUngrabPointer(dpy, CurrentTime);
	XSelectInput(dpy, draw, StructureNotifyMask);
	XUnmapWindow(dpy, draw);

	if (*w != 0 || *h != 0)
		do XNextEvent(dpy, &evt);
		while (evt.type != UnmapNotify && evt.xunmap.window != draw);
}

static void
drive(int *x, int *y, int *w, int *h, int opt)
{
	if ((opt & 2) != 0)
		pick(x, y, w, h);
	if (*w != 0 && *h != 0)
		return;

	if ((opt & 8) != 0) {
		if ((opt & 2) != 0)
			XQueryPointer(dpy, root, &(Window){ 0 },
					&(Window){ 0 }, x, y, &(int){ 0 },
					&(int){ 0 }, &(unsigned int){ 0 });

		int num;
		XineramaScreenInfo *si = XineramaQueryScreens(dpy, &num), *sel;
		for (sel = si; sel != NULL; sel = &sel[1])
			if (*x >= sel->x_org && *y >= sel->y_org &&
					*x <= sel->x_org + sel->width &&
					*y <= sel->y_org + sel->height)
				break;
		if (sel == NULL)
			*x = *y = 0, *w = scr->width, *h = scr->height;
		else
			*x = sel->x_org, *y = sel->y_org, *w = sel->width,
					*h = sel->height;
		if (si != NULL)
			XFree(si);
		return;
	}

	Window target = root, new = 0;
	if ((opt & 2) != 0)
		while (XQueryPointer(dpy, target, &(Window){ 0 }, &new,
				x, y, &(int){ 0 }, &(int){ 0 },
				&(unsigned int){ 0 }) && new != 0)
			target = new;
	else
		XGetInputFocus(dpy, &target, &(int){ 0 });

	XWindowAttributes attrs;
	if (!XGetWindowAttributes(dpy, target, &attrs) ||
			attrs.map_state != IsViewable)
		die("sr: unable to get window\n");
	*w = attrs.width, *h = attrs.height;
	XTranslateCoordinates(dpy, target, root, 0, 0, x, y, &(Window){ 0 });

#define BW attrs.border_width
	if (BW > 0)
		*w += BW * 2, *h += BW * 2, *x -= BW, *y -= BW;
}

int
main(int argc, char **argv)
{
	int x = 0, y = 0, w = 0, h = 0, opt = 1;
	char *optsel = NULL;
	bool optcur = true, optfre = false;

	for (argv = &argv[1]; *argv != NULL; argv = &argv[1]) {
		if ((*argv)[0] != '-')
			die("sr: invalid argument: %s\n", *argv);
		switch ((*argv)[1]) {
		case 'c': optcur = !optcur; break;
		case 'f': optfre = !optfre; break;

		case 'a': opt = 1; break;
		case 'i': opt = 0, optsel = *(argv = &argv[1]); break;

		case 's': opt = (opt & (4 | 8)) | 2; break;
		case 'm': opt = (opt & 2) | 8; break;
		case 'w': opt = (opt & 2) | 4; break;
		}
	}

	if ((dpy = XOpenDisplay(NULL)) == NULL)
		die("sr: unable to open display\n");
	scr  = ScreenOfDisplay(dpy, DefaultScreen(dpy));
	root = RootWindow(dpy, XScreenNumberOfScreen(scr));

	imlib_context_set_display(dpy);
	imlib_context_set_drawable(root);
	imlib_context_set_visual(
			DefaultVisual(dpy, XScreenNumberOfScreen(scr)));
	imlib_context_set_colormap(
			DefaultColormap(dpy, XScreenNumberOfScreen(scr)));
	imlib_context_set_color_modifier(NULL);
	imlib_context_set_operation(IMLIB_OP_COPY);

	if (optfre)
		XGrabServer(dpy);
	if ((opt & 1) != 0) {
		w = scr->width, h = scr->height;
	} else if (optsel != NULL) {
		char *start = optsel, *end;
		for (int i = 0; i < 3; ++i, start = ++end) {
			((int *)&x)[i] = strtoul(start, &end, 10);
			if (*end != ',' || !isdigit(*start))
				die("sr: invalid option: %s\n", optsel);
		}
		((int *)&x)[3] = strtoul(start, &end, 10);
		if (*end != '\0' || !isdigit(*start))
			die("sr: invalid option: %s\n", optsel);
	} else {
		drive(&x, &y, &w, &h, opt);
	}

	if (x < 0) w += x, x = 0;
	if (y < 0) h += y, y = 0;
	w = (x + w) <= scr->width  ? w : scr->width - x;
	h = (y + h) <= scr->height ? h : scr->height - y;

	Imlib_Image image;
	if ((image = imlib_create_image_from_drawable(0,
			x, y, w, h, true)) == NULL)
		die("sr: unable to grab image\n");
	if (optcur) {
		XFixesCursorImage *cur;
		if ((cur = XFixesGetCursorImage(dpy)) == NULL)
			die("sr: unable to get cursor image\n");

		DATA32 *data;
		Imlib_Image img;
		if ((data = malloc(cur->width * cur->height * 4)) == NULL)
			die("sr: unable to allocate memory: ");
		for (int i = 0; i < cur->width * cur->height; ++i)
			data[i] = cur->pixels[i];
		if ((img = imlib_create_image_using_data(cur->width,
				cur->height, data)) == NULL)
			die("sr: unable to create cursor image\n");

		imlib_context_set_image(img);
		imlib_image_set_has_alpha(true);
		imlib_context_set_image(image);
		imlib_blend_image_onto_image(img, 0, 0, 0, cur->width,
				cur->height, cur->x - cur->xhot - x, cur->y -
				cur->yhot - y, cur->width, cur->height);
		imlib_context_set_image(img);
		imlib_free_image();
		free(data); XFree(cur);
	}
	if (optfre)
		XUngrabServer(dpy);

	Imlib_Load_Error ret;
	imlib_context_set_image(image);
	imlib_image_set_format("png");
	imlib_save_image_with_error_return("/dev/stdout", &ret);
	imlib_free_image_and_decache();

	if (ret != 0)
		die("sr: unable to write png data\n");
	XCloseDisplay(dpy);
}
