/*
 *	@Header: scrollbar.c,v 1.3 88/02/20 15:31:26 swick Exp @
 */

#include <X11/copyright.h>

/*
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital Equipment
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 *
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <stdio.h>
#include <setjmp.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Atoms.h>
#include "ptyx.h"
#include "data.h"
#include <X11/Scroll.h> /* should come from Xaw/Scroll.h at some point */
#include "error.h"

extern void bcopy();

#ifndef lint
static char rcs_id[] = "@Header: scrollbar.c,v 1.3 88/02/20 15:31:26 swick Exp @";
#endif	/* lint */

/* Event handlers */

static void ScrollTextTo();
static void ScrollTextUpDownBy();

static Bool IsEventType( display, event, type )
	Display *display;
	XEvent *event;
	int type;
{
	return (event->type == type);
}


/* resize the text window for a terminal screen, modifying the
 * appropriate WM_SIZE_HINTS and taking advantage of bit gravity.
 */

static void ResizeScreen(xw, min_width, min_height )
	register XtermWidget xw;
	int min_width, min_height;
{
	register TScreen *screen = &xw->screen;
	XSizeHints sizehints;
	XSetWindowAttributes newAttributes;
	XWindowAttributes oldAttributes;
	XEvent event;
        Dimension junk;  /* for values returned by XtMakeResizeRequest */

	XGrabServer(screen->display);
	if (!XGetSizeHints(screen->display, XtWindow(xw->core.parent),
		&sizehints, XA_WM_NORMAL_HINTS))
	    sizehints.flags = 0;
	sizehints.min_width = min_width;
	sizehints.min_height = min_height;
	sizehints.width_inc = FontWidth(screen);
	sizehints.height_inc = FontHeight(screen);
	sizehints.width =  (screen->max_col + 1) * FontWidth(screen)
				+ min_width;
	sizehints.height = FontHeight(screen) * (screen->max_row + 1)
				+ min_height;
	sizehints.flags |= PMinSize|PResizeInc|PSize;
	XSetSizeHints(screen->display,
	 XtWindow(xw->core.parent), &sizehints, XA_WM_NORMAL_HINTS);
	XUngrabServer(screen->display);

	XGetWindowAttributes( screen->display, TextWindow(screen),
			      &oldAttributes );

	newAttributes.event_mask =
	    oldAttributes.your_event_mask | StructureNotifyMask;
	newAttributes.bit_gravity = EastGravity;

        /* The following statement assumes scrollbar is on Left! 
           If we ever have scrollbars on the right, then the
           bit-gravity should be left alone, NOT changed to EastGravity. */
	XChangeWindowAttributes( screen->display, TextWindow(screen),
	     CWEventMask|CWBitGravity, &newAttributes );

	(void) XtMakeResizeRequest((Widget)xw,
	    (unsigned) (screen->max_col + 1) * FontWidth(screen) + min_width,
	    (unsigned) FontHeight(screen) * (screen->max_row + 1) + min_height,
            &junk, &junk);

	/* wait for a window manager to actually do it */
	XIfEvent( screen->display, &event, IsEventType, ConfigureNotify );

	newAttributes.event_mask = oldAttributes.your_event_mask;
	newAttributes.bit_gravity = NorthWestGravity;
	XChangeWindowAttributes( screen->display, TextWindow(screen),
				 CWEventMask|CWBitGravity,
				 &newAttributes );
}


static Widget CreateScrollBar(xw, x, y, height)
	XtermWidget xw;
	int x, y, height;
{
	Widget scrollWidget;
	TScreen *screen = &xw->screen;
	XSetWindowAttributes attr;
	extern char *calloc();

	static Arg argList[] = {
	   {XtNx,		(XtArgVal) 0},
	   {XtNy,		(XtArgVal) 0},
	   {XtNheight,		(XtArgVal) 0},
	   {XtNreverseVideo,	(XtArgVal) 0},
	   {XtNorientation,	(XtArgVal) XtorientVertical},
	   {XtNborderWidth,	(XtArgVal) 1},
	   {XtNwidth,		(XtArgVal) SCROLLBARWIDTH-1},
	};   

	argList[0].value = (XtArgVal) x;
	argList[1].value = (XtArgVal) y;
	argList[2].value = (XtArgVal) height;
	argList[3].value = (XtArgVal) xw->misc.re_verse;


	scrollWidget = XtCreateWidget("scrollbar", scrollbarWidgetClass, 
	  xw, argList, XtNumber(argList));
        XtAddCallback (scrollWidget, XtNscrollProc, ScrollTextUpDownBy, 0);
        XtAddCallback (scrollWidget, XtNthumbProc, ScrollTextTo, 0);
	XtRealizeWidget(scrollWidget);

	attr.do_not_propagate_mask = 
	  LeaveWindowMask | EnterWindowMask | StructureNotifyMask;
	XChangeWindowAttributes(screen->display, XtWindow(scrollWidget), 
	  CWDontPropagate, &attr);

	return(scrollWidget);
}


ScrollBarReverseVideo(scrollWidget)
	register Widget scrollWidget;
{
	Arg argList[1];

	XtSetArg(argList[0], XtNreverseVideo, term->misc.re_verse);
	XtSetValues(scrollWidget, argList, XtNumber(argList));
}



ScrollBarDrawThumb(scrollWidget)
	register Widget scrollWidget;
{
	register TScreen *screen = &term->screen;
	register int thumbTop, thumbHeight, totalHeight;
	
	thumbTop    = screen->topline + screen->savedlines;
	thumbHeight = screen->max_row + 1;
	totalHeight = thumbHeight + screen->savedlines;

	XtScrollBarSetThumb(scrollWidget,
	 ((float)thumbTop) / totalHeight,
	 ((float)thumbHeight) / totalHeight);
	
}

ResizeScrollBar(scrollWidget, x, y, height)
	register Widget scrollWidget;
	int x, y;
	unsigned height;
{
	XtConfigureWidget(scrollWidget, x, y, (SCROLLBARWIDTH - 1), height,
	    scrollWidget->core.border_width);
	ScrollBarDrawThumb(scrollWidget);
}

WindowScroll(screen, top)
	register TScreen *screen;
	int top;
{
	register int i, lines;
	register int scrolltop, scrollheight, refreshtop;
	register int x = 0;

	if (top < -screen->savedlines)
		top = -screen->savedlines;
	else if (top > 0)
		top = 0;
	if((i = screen->topline - top) == 0) {
		ScrollBarDrawThumb(screen->scrollWidget);
		return;
	}

	ScrollSelection(i);

	if(screen->cursor_state)
		HideCursor();
	lines = i > 0 ? i : -i;
	if(lines > screen->max_row + 1)
		lines = screen->max_row + 1;
	scrollheight = screen->max_row - lines + 1;
	if(i > 0)
		refreshtop = scrolltop = 0;
	else {
		scrolltop = lines;
		refreshtop = scrollheight;
	}
	x = screen->scrollbar +	screen->border;
	if(scrollheight > 0) {
		if (screen->multiscroll && scrollheight == 1 &&
		 screen->topline == 0 && screen->top_marg == 0 &&
		 screen->bot_marg == screen->max_row) {
			if (screen->incopy < 0 && screen->scrolls == 0)
				CopyWait (screen);
			screen->scrolls++;
		} else {
			if (screen->incopy)
				CopyWait (screen);
			screen->incopy = -1;
		}
		XCopyArea(
		    screen->display, 
		    TextWindow(screen), TextWindow(screen),
		    screen->normalGC,
		    (int) x,
		    (int) scrolltop * FontHeight(screen) + screen->border, 
		    (unsigned) Width(screen),
		    (unsigned) scrollheight * FontHeight(screen),
		    (int) x,
		    (int) (scrolltop + i) * FontHeight(screen)
			+ screen->border);
	}
	screen->topline = top;
	XClearArea(
	    screen->display,
	    TextWindow(screen), 
	    (int) x,
	    (int) refreshtop * FontHeight(screen) + screen->border, 
	    (unsigned) Width(screen),
	    (unsigned) lines * FontHeight(screen),
	    FALSE);
	ScrnRefresh(screen, refreshtop, 0, lines, screen->max_col + 1);

	ScrollBarDrawThumb(screen->scrollWidget);
}

ScrollBarOn(screen, init)
	register TScreen *screen;
	int init;
{
	register int border = 2 * screen->border;
	register int i;
	char *realloc(), *calloc();

	if(screen->scrollbar)
		return;
	if(!screen->scrollWidget) {
		if((screen->scrollWidget = CreateScrollBar(term,
		 -1, - 1, Height(screen) + border)) == NULL) {
			Bell();
			return;
		}
		if (screen->allbuf) {
		    if((screen->allbuf = (ScrnBuf) realloc((char *) screen->buf,
			(unsigned) 2*(screen->max_row + 2 + screen->savelines) * sizeof(char *)))
		            == NULL)
			       Error (ERROR_SBRALLOC);
		    screen->buf = &screen->allbuf[2 * screen->savelines];
		    bcopy ((char *)screen->allbuf, (char *)screen->buf,
			   2 * (screen->max_row + 2) * sizeof (char *));
		    for(i = 2 * screen->savelines - 1 ; i >= 0 ; i--)
			if((screen->allbuf[i] =
			 calloc((unsigned) screen->max_col + 1, sizeof(char))) == NULL)
				Error (ERROR_SBRALLOC2);
		    }
	}
	screen->scrollbar = SCROLLBARWIDTH;
	ScrollBarDrawThumb(screen->scrollWidget);
	if (!init) ResizeScreen(term, border + SCROLLBARWIDTH, border );
	/* map afterwards so BitGravity can be used profitably */
	XMapWindow(screen->display, XtWindow(screen->scrollWidget));
}

ScrollBarOff(screen)
	register TScreen *screen;
{
	register int border = 2 * screen->border;

	if(!screen->scrollbar)
		return;
	screen->scrollbar = 0;
	XUnmapWindow(screen->display, XtWindow(screen->scrollWidget));
	ResizeScreen( term, border, border );
}

/*ARGSUSED*/
static void ScrollTextTo(scrollbarWidget, closure, topPercent)
	Widget scrollbarWidget;
	Opaque closure;
	float topPercent;
{
	register TScreen *screen = &term->screen;
	int thumbTop;	/* relative to first saved line */
	int newTopLine;

/*
   screen->savedlines : Number of offscreen text lines,
   screen->maxrow + 1 : Number of onscreen  text lines,
   screen->topline    : -Number of lines above the last screen->max_row+1 lines
*/

	thumbTop = topPercent * (screen->savedlines + screen->max_row+1);
	newTopLine = thumbTop - screen->savedlines;
	WindowScroll(screen, newTopLine);
}

/*ARGSUSED*/
static void ScrollTextUpDownBy(scrollbarWidget, closure, pixels)
	Widget scrollbarWidget;
	Opaque closure;
	int pixels;
{
	register TScreen *screen = &term->screen;
	register int rowOnScreen, newTopLine;

	rowOnScreen = pixels / FontHeight(screen);
	if (rowOnScreen == 0) {
		if (pixels < 0)
			rowOnScreen = -1;
		else if (pixels > 0)
			rowOnScreen = 1;
	}
	newTopLine = screen->topline + rowOnScreen;
	WindowScroll(screen, newTopLine);
}
