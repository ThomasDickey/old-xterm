/*
 *	@Source: /orpheus/u1/X11/clients/xterm/RCS/scrollbar.c,v @
 *	@Header: scrollbar.c,v 1.20 87/08/17 19:38:44 swick Exp @
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
#include <X11/Xtlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "ptyx.h"
#include "data.h"
#include "error.h"

extern void bcopy();

#ifndef lint
static char rcs_id[] = "@Header: scrollbar.c,v 1.20 87/08/17 19:38:44 swick Exp @";
#endif	lint

/* Event handlers */
extern EventDoNothing();

extern ScrollTextTo();
extern ScrollTextUpDownBy();

static Bool IsEventType( display, event, type )
	Display *display;
	XEvent *event;
	int type;
{
	if (event->type == type)
	    return( True );
	else
	    return( False );
}


/* resize the text window for a terminal screen, modifying the
 * appropriate WM_SIZE_HINTS and taking advantage of bit gravity.
 */

static void ResizeScreen( screen, min_width, min_height )
	register TScreen *screen;
	int min_width, min_height;
{
	XSizeHints sizehints;
	XSetWindowAttributes newAttributes;
	XWindowAttributes oldAttributes;
	XEvent event;

	XGrabServer(screen->display);
	if (!XGetSizeHints(screen->display, VWindow(screen),
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
	 TextWindow(screen), &sizehints, XA_WM_NORMAL_HINTS);
	XUngrabServer(screen->display);

	XGetWindowAttributes( screen->display, TextWindow(screen),
			      &oldAttributes );

	newAttributes.event_mask =
	    oldAttributes.your_event_mask | StructureNotifyMask;
	/* assume that the scrollbar always goes on the left! */
	newAttributes.bit_gravity = EastGravity;

	XChangeWindowAttributes( screen->display, TextWindow(screen),
				 CWEventMask|CWBitGravity,
				 &newAttributes );
	XResizeWindow(
	    screen->display,
	    TextWindow(screen),
	    (unsigned) (screen->max_col + 1) * FontWidth(screen) + min_width,
	    (unsigned) FontHeight(screen) * (screen->max_row + 1)
		      + min_height );

	/* wait for a window manager to actually do it */
	XIfEvent( screen->display, &event, IsEventType, ConfigureNotify );

	newAttributes.event_mask = oldAttributes.your_event_mask;
	newAttributes.bit_gravity = NorthWestGravity;
	XChangeWindowAttributes( screen->display, TextWindow(screen),
				 CWEventMask|CWBitGravity,
				 &newAttributes );
}


Window CreateScrollBar(w, x, y, height)
	Window w;
	int x, y, height;
{
	Window scrollWindow;
	TScreen *screen = &term.screen;
	XSetWindowAttributes attr;
	extern char *calloc();

	static Arg argList[] = {
	   {XtNbackground,	(caddr_t) 0},
	   {XtNborder,		(caddr_t) 0},
	   {XtNx,		(caddr_t) 0},
	   {XtNy,		(caddr_t) 0},
	   {XtNheight,		(caddr_t) 0},
	   {XtNorientation,	(caddr_t) XtorientVertical},
	   {XtNscrollUpDownProc,(caddr_t) ScrollTextUpDownBy},
	   {XtNthumbProc,	(caddr_t) ScrollTextTo},
	   {XtNborderWidth,	(caddr_t) 1},
	   {XtNwidth,		(caddr_t) SCROLLBARWIDTH-1},
	};   

	argList[0].value = (caddr_t) screen->background;
	argList[1].value = (caddr_t) screen->bordercolor;
	argList[2].value = (caddr_t) x;
	argList[3].value = (caddr_t) y;
	argList[4].value = (caddr_t) height;

	scrollWindow = XtScrollBarCreate(screen->display, w, 
	  argList, XtNumber(argList));

	attr.do_not_propagate_mask = 
	  LeaveWindowMask | EnterWindowMask | StructureNotifyMask;
	XChangeWindowAttributes(screen->display, scrollWindow, 
	  CWDontPropagate, &attr);

	return(scrollWindow);
}


RedrawScrollBar(scrollWindow)
	Window scrollWindow;
{

	TScreen *screen = &term.screen;
	XtSendExpose(screen->display, scrollWindow);
}

ScrollBarReverseVideo(scrollWindow)
	register Window scrollWindow;
{
	register TScreen *screen = &term.screen;
	register caddr_t temp;

	static Arg argList[] = {
/* Don't muck with the order */
	   {XtNforeground,	(caddr_t) NULL},	
	   {XtNbackground,	(caddr_t) NULL},
	   {XtNborder,		(caddr_t) NULL},
	};   

	XtScrollBarGetValues(
		screen->display, scrollWindow, argList, XtNumber(argList));

	/* Swap background and foreground */
	temp   		 = argList[0].value;
	argList[0].value = argList[1].value;
	argList[1].value = temp;

	/* Should really only do this if the old bordertile is the same as
	   the scrollbar's current bordertile |||  LGR: ???*/
	argList[2].value = (caddr_t) screen->bordercolor;

	XtScrollBarGetValues(
		screen->display, scrollWindow, argList, XtNumber(argList));

}


ScrollBarDrawThumb(scrollWindow)
	register Window scrollWindow;
{
	register TScreen *screen = &term.screen;
	register int thumbTop, thumbHeight, totalHeight;
	
	thumbTop    = screen->topline + screen->savedlines;
	thumbHeight = screen->max_row + 1;
	totalHeight = thumbHeight + screen->savedlines;

	XtScrollBarSetThumb(screen->display, scrollWindow,
	 ((float)thumbTop) / totalHeight,
	 ((float)thumbHeight) / totalHeight);
	
}

ResizeScrollBar(scrollWindow, x, y, height)
	register Window scrollWindow;
	int x, y;
	unsigned height;
{
	register TScreen *screen = &term.screen;

	XMoveResizeWindow(
	    screen->display, scrollWindow,
	    x, y, 
	    (SCROLLBARWIDTH - 1), height);
	ScrollBarDrawThumb(scrollWindow);
}

WindowScroll(screen, top)
	register TScreen *screen;
	int top;
{
	register int i, lines;
	register int scrolltop, scrollheight, refreshtop;

	if (top < -screen->savedlines)
		top = -screen->savedlines;
	else if (top > 0)
		top = 0;
	if((i = screen->topline - top) == 0) {
		ScrollBarDrawThumb(screen->scrollWindow);
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
		    (int) screen->border + screen->scrollbar,
		    (int) scrolltop * FontHeight(screen) + screen->border, 
		    (unsigned) Width(screen),
		    (unsigned) scrollheight * FontHeight(screen),
		    (int) screen->border + screen->scrollbar,
		    (int) (scrolltop + i) * FontHeight(screen)
			+ screen->border);
	}
	screen->topline = top;
	XClearArea(
	    screen->display,
	    TextWindow(screen), 
	    (int) screen->border + screen->scrollbar, 
	    (int) refreshtop * FontHeight(screen) + screen->border, 
	    (unsigned) Width(screen),
	    (unsigned) lines * FontHeight(screen),
	    FALSE);
	ScrnRefresh(screen, refreshtop, 0, lines, screen->max_col + 1);

	ScrollBarDrawThumb(screen->scrollWindow);
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
	if(!screen->scrollWindow) {
		if((screen->scrollWindow = CreateScrollBar(TextWindow(screen),
		 -1, - 1, Height(screen) + border)) == NULL) {
			Bell();
			return;
		}
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
	screen->scrollbar = SCROLLBARWIDTH;
	ScrollBarDrawThumb(screen->scrollWindow);
	if (!init) ResizeScreen( screen, border + SCROLLBARWIDTH, border );
	/* map afterwards so BitGravity can be used profitably */
	XMapWindow(screen->display, screen->scrollWindow);
}

ScrollBarOff(screen)
	register TScreen *screen;
{
	register int border = 2 * screen->border;

	if(!screen->scrollbar)
		return;
	screen->scrollbar = 0;
	XUnmapWindow(screen->display, screen->scrollWindow);
	ResizeScreen( screen, border, border );
}

/*ARGSUSED*/
ScrollTextTo(dpy, scrollbarWindow, clientWindow, topPercent)
	Display *dpy;
	Window scrollbarWindow, clientWindow;
	float topPercent;
{
	register TScreen *screen = &term.screen;
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
ScrollTextUpDownBy(dpy, scrollbarWindow, clientWindow, pixels)
	Display *dpy;
	Window scrollbarWindow, clientWindow;
	int pixels;
{
	register TScreen *screen = &term.screen;
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
