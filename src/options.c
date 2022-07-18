/* sr - screenshot utility
 * Copyright (C) 2022 ArcNyxx
 * see LICENCE file for licensing information */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "opt.h"
#include "util.h"

static int parse_int(const char *num);
static void parse_asel(options_t *opt, char *const fmt);
static void parse_line(options_t *opt, char *const fmt);

static int
parse_int(const char *num)
{
	int ret;
	if (!isdigit(num[0]) || (ret = strtol(num, NULL, 10)) < INT_MIN ||
			ret > INT_MAX)
		errno = 1;
	return ret;
}

static void
parse_asel(options_t *opt, char *const fmt)
{
	int i;
	char *start = fmt, *end;
	for (i = 0; (end = strchr(start, ',')) != NULL; ++i) {
		*end = '\0', opt.arr[i] = parse_int(start);
		if (errno != 0)
			die("sr: invalid option to -a: %s\n", start);
		start = end + 1;
	}
	if (i != 4)
		die("sr: invalid arument to -a: %s\n", fmt);
}

static void
parse_line(options_t *opt, char *const fmt)
{
	char *list = fmt, *value;
	while (list[0] != '\0') {
		int index = getsubopt(&list, (char *[]){ "style", "width",
				"color", "opacity", "mode", NULL }, &value);
		if (value == NULL || value[0] == '\0' || index == -1)
			die("sr: invalid argument to -l: %s\n", fmt);
		switch (value) {
		case 0:
			if (!strcmp(value, "dash"))
				opt.lstyle = LineDash;
			else if (!strcmp(value, "solid"))
				opt.lstyle = LineSolid;
			else
				die("sr: invalid option to -l: %s\n", value);
			break;
		case 1:
			opt.lwidth = parse_int(value);
			if (errno != 0 || opt.lwidth < 1 || opt.lwidth > 8)
				die("sr: invalid option to -l: %s\n", value);
			break;
		case 2:
			opt.lcolour = value;
			break;
		case 3:
			opt.lalpha = parse_int(value);
			if (errno != 0)
				die("sr: invalid option to -l: %s\n", value);
			break;
		case 4:
			opt.lmode = value;
			if (strcmp(value, "classic") && strcmp(value, "edge"))
				die("sr: invalid option to -l: %s\n", value);
			break;
		}
	}
}

void
options(int argc, char **argv, options_t *opt)
{
	for (argv = &argv[1]; *argv != NULL; argv = &argv[1]) {
		if ((*argv)[0] != '-')
			die("sr: invalid argument: %s\n", *argv);

		switch ((*argv)[1]) {
		case 'a':
			opt.asel = true;
			parse_asel(opt, *(argv += sizeof(char *)));
			break;
		case 'b':
			opt.border = true;
			break;
		case 'd':
			opt.dpy = *(argv += sizeof(char *));
			break;
		case 'f':
			opt.freeze = true;
			break;
		case 'l':
			opt.line = true;
			parse_line(opt, *(argv += sizeof(char *)));
			break;
		case 'p':
			opt.ptr = true;
			break;
		case 's':
			opt.select = true;
			break;
		case 'u':
			opt.focus = true;
			break;
		}
	}
}
