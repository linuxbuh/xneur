/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Copyright (C) 2006-2016 XNeur Team
 *
 */

#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "defines.h"
#include "keymap.h"

#include "types.h"
#include "log.h"

#include "window.h"

#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5

typedef struct {
	int flags;
	int functions;
	int decorations;
	int input_mode;
	int status;
} MWMHints;

static int error_handler(Display *d, XErrorEvent *e)
{
	if (d || e) {}
        return FALSE;
}

/// Check "_NET_SUPPORTED" atom support
static int is_has_net_supported(Display *display, Window window)
{
	Atom request      = XInternAtom(display, "_NET_SUPPORTED", False);
	Atom feature_atom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);

	unsigned long nitems = 0L;
	int result = FALSE;
	Atom *results = (Atom *) get_win_prop(display, window, request, &nitems);
	if (results != NULL) {
		for (unsigned long i = 0L; i < nitems; ++i) {
			if (results[i] == feature_atom) {
				result = TRUE;
				break;
			}
		}
		XFree(results);
	}
	return result;
}

static void window_uninit(struct _window *p)
{
	if (p->keymap != NULL)
		p->keymap->uninit(p->keymap);

	free(p);

	log_message(DEBUG, _("Window is freed"));
}

struct _window* window_init(struct _xneur_handle *handle)
{
	XSetErrorHandler(error_handler);

	Display *display = XOpenDisplay(NULL);
	if (!display)
	{
		log_message(ERROR, _("Can't connect to XServer"));
		return NULL;
	}

	Window root = DefaultRootWindow(display);
	// Create Main Window
	Window window = XCreateSimpleWindow(display, root, 0, 0, 100, 100, 0, 0, 0);
	if (!window)
	{
		log_message(ERROR, _("Can't create program window"));
		XCloseDisplay(display);
		return NULL;
	}

	// Create flag window
	XSetWindowAttributes attrs;
	attrs.override_redirect = True;

	Window flag_window = XCreateWindow(display, root, 0, 0, 1, 1,0, CopyFromParent, CopyFromParent, CopyFromParent, CWOverrideRedirect, &attrs);
	if (!flag_window)
	{
		log_message(ERROR, _("Can't create flag window"));
		XCloseDisplay(display);
		return NULL;
	}

	// Set no border mode to flag window
	MWMHints mwmhints;
	memset(&mwmhints, 0, sizeof(mwmhints));
	mwmhints.flags		= MWM_HINTS_DECORATIONS;
	mwmhints.decorations	= 0;

	Atom motif_prop = XInternAtom(display, "_MOTIF_WM_HINTS", False);

	XChangeProperty(display, flag_window, motif_prop, motif_prop, 32, PropModeReplace, (unsigned char *) &mwmhints, PROP_MWM_HINTS_ELEMENTS);

	XWMHints wmhints;
	memset(&wmhints, 0, sizeof(wmhints));
	wmhints.flags = InputHint;
	wmhints.input = 0;

	Atom win_prop = XInternAtom(display, "_WIN_HINTS", False);

	XChangeProperty(display, flag_window, win_prop, win_prop, 32, PropModeReplace, (unsigned char *) &mwmhints, sizeof (XWMHints) / 4);

	int _NET_SUPPORTED = is_has_net_supported(display, root);

	log_message(LOG, _("Main window with id %d created"), window);

	XSynchronize(display, TRUE);
	XFlush(display);

	struct _keymap* keymap = keymap_init(handle, display);
	if (keymap == NULL) {
		XCloseDisplay(display);
		return NULL;
	}

	struct _window *p = (struct _window *) malloc(sizeof(struct _window));
	memset(p, 0, sizeof(struct _window));

	p->keymap  = keymap;
	p->display = display;
	p->window  = window;
	p->_NET_SUPPORTED = _NET_SUPPORTED;

	// Function mapping
	p->uninit = window_uninit;

	return p;
}
