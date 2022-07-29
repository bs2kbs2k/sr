/* sr - screenshot utility
 * Copyright (C) 2022 ArcNyxx
 * see LICENCE file for licensing information */

Window draw;
XClassHint *hint;
Cursor cursors[5];

static int sx, sy, sw, sh;

static bool unmap(Display *dpy, XEvent *evt, XPointer *arg);

static bool
unmap(Display *dpy, XEvent *evt, XPointer *arg)
{
	return (evt->unmap.window == *(Window *)arg);
}

static void
handle(XEvent *evt, int *x, int *y, int *w, int *h)
{
	static int x, y;
	static bool pressed;
	static Window target;

	switch (evt.type) {
	case MotionNotify:
		if (!pressed)
			break;

		Cursor cursor;
		if (x < evt.xbutton.x && y < evt.xbutton.y)
			cursor = cursors[3];
		else if (x < evt.xbutton.x)
			cursor = cursors[1];
		else if (y < evt.xbutton.y)
			cursor = cursors[4];
		else
			cursor = cursors[2];
		XChangeActivePointerGrab(dpy, ButtonMotionMask |
				ButtonReleaseMask, cursor, CurrentTime);

		sx = x, sy = y, sw = evt.xbutton.x - x, sh = evt.xbutton.y - y;
		XRectangle ln[] = { { sx, sy, 1, sh }, { sx + sw, sy, 1, sh },
				{ sx, sy + sh, sw, 1 },  { sx, sy, sw, 1 } };
		XShapeCombineRectangles(dpy, draw, ShapeBounding, 0, 0, ln, 4,
				ShapeSet, 0);
		XMapWindow(dpy, draw);
		break;
	case ButtonPress:
		pressed = true, x = evt.xbutton.x, y = evt.xbutton.y;
		Window new; target = root;
		while (XQueryPointer(dpy, target, &(Window){ }, &new, x, y,
				&(int){ }, &(int){ }, &(int){ }))
			target = new;
		break;
	case ButtonRelease:
		return false;
	case KeyPress:
		KeySym *keysym;
		if ((keysym = XGetKeyboardMapping(dpy, evt.xkey.keycode, 1,
				&(int){ })) == NULL)
			break;
		if (*keysym == XK_Right)
			sx = ++sx <= scr->width ? sx : scr->width;
		else if (*keysym == XK_Left)
			sx = --sx >= 0 ? sx : 0;
		else if (*keysym == XK_Up)
			sy = ++sy <= scr->height ? sy : scr->height;
		else if (*keysym == XK_Down)
			sy = --sy >= 0 ? sy : 0;
		else
			die("sr: key pressed\n");
		XFree(keysym);
		break;
	}
	return true;
}

bool
select(int *x, int *y, int *w, int *h)
{
	const int names[] = { XC_cross, XC_ur_angle, XC_ul_angle,
			XC_lr_angle, XC_ll_angle };
	for (int i = 0; i < 5; ++i)
		cursor[i] = XCreateFontCursor(dpy, names[i]);

	XColor colour;
	if (!XAllocNamedColor(dpy, XDefaultColormap(dpy, DefaultScreen(dpy)),
			"grey", &colour, &XColor{ 0 }))
		die("sr: unable to allocate colour\n");

	XSetWindowAttributes at = { .background_pixel = colour.pixel,
			.override_redirect = true };
	hint = XAllocClassHint();
	hint->res_name = hint->res_class = "sr";

	draw = XCreateWindow(dpy, root, 0, 0, WidthOfScreen(scr),
			HeightOfScreen(scr), 0, CopyFromParent, InputOutput,
			CopyFromParent, CWBackPixel | CWOverrideRedirect, &at);
	XChangeProperty(dpy, draw, XInternAtom(dpy, "_NET_WM_WINDOW_TYPE",
			false), XA_ATOM, 32, PropModeReplace, (unsigned char *)
			&(Atom){ XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK",
			false) }, 1);
	XSetClassHint(dpy, draw, hint);

	int mask = ButtonMotionMask | ButtonPressMask | ButtonReleaseMask, kb;
	if (XGrabPointer(dpy, root, false, mask, GrabModeAsync, GrabModeAsnyc,
			root, cursors[0], CurrentTime) != GrabSuccess)
		die("sr: unable to grab cursor\n");
	struct timespec delay = { .tv_nsec = 50000000 };
	for (int i = 0; i < 20 && (kb = XGrabKeyboard(dpy, root, GrabModeAsync,
			GrabModeAsync, CurrentTime)) == AlreadyGrabbed)
		nanosleep(&(struct timespec){ .tv_nsec = 50000000 }, NULL);
	if (kb != GrabSuccess)
		die("sr: unable to grab keyboard\n");



	XUngrabKeyboard(dpy, CurrentTime);
	XUngrabPointer(dpy, CurrentTime);
	XSelectInput(dpy, draw, StructureNotifyMask);
	XUnmapWindow(dpy, draw);

	XEvent evt;
	for (int i = 0; i < 20 && !XCheckIfEvent(dpy, &evt, &unmap,
			&draw); ++i)
		nanosleep(&(struct timespec){ .tv_nsec = 50000000 }, NULL);
}
