/*
 *	$XConsortium: scrollbar.c,v 1.32 89/12/15 11:45:51 kit Exp $
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
#include <ctype.h>
#include <setjmp.h>
#include <X11/Xatom.h>

#include "ptyx.h"		/* X headers included here. */

#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include <X11/Xaw/Scrollbar.h>

#include "data.h"
#include "error.h"
#include "menu.h"

extern void bcopy();

#ifndef lint
static char rcs_id[] = "$XConsortium: scrollbar.c,v 1.32 89/12/15 11:45:51 kit Exp $";
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
	XtGeometryResult geomreqresult;
	Dimension oldWidth, oldHeight;
	Dimension reqWidth, reqHeight, repWidth, repHeight;
	static Arg argList[] = {
	    {XtNminWidth,	0},
	    {XtNminHeight,	0},
	    /* %%% the next two should move to VTInitialize, VTSetValues */
	    {XtNwidthInc,	0},
	    {XtNheightInc,	0}
	};

#ifndef nothack
	long supp;

	/* %%% gross hack caused by our late processing of geometry
	   (in VTRealize) and setting of size hints there, leaving
	   Shell with insufficient information to do the job properly here.
	   Instead of doing it properly, we save and restore the
	   size hints around Shell.SetValues and Shell.GeometryManager
	 */
	if (!XGetWMNormalHints (screen->display, XtWindow(XtParent(xw)),
				&sizehints, &supp))
	    sizehints.flags = 0;
	sizehints.base_width = min_width;
	sizehints.base_height = min_height;
	sizehints.width_inc = FontWidth(screen);
	sizehints.height_inc = FontHeight(screen);
	sizehints.min_width = sizehints.base_width + sizehints.width_inc;
	sizehints.min_height = sizehints.base_height + sizehints.height_inc;
	sizehints.width =  (screen->max_col + 1) * FontWidth(screen)
				+ min_width;
	sizehints.height = FontHeight(screen) * (screen->max_row + 1)
				+ min_height;
	sizehints.flags |= (PBaseSize|PMinSize|PResizeInc);
#endif

	argList[0].value = (XtArgVal)min_width;
	argList[1].value = (XtArgVal)min_height;
	argList[2].value = (XtArgVal)FontWidth(screen);
	argList[3].value = (XtArgVal)FontHeight(screen);
	XtSetValues( XtParent(xw), argList, XtNumber(argList) );

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

	oldWidth = xw->core.width;
	oldHeight = xw->core.height;
	reqWidth = (screen->max_col + 1) * FontWidth(screen) + min_width;
	reqHeight = FontHeight(screen) * (screen->max_row + 1) + min_height;
	geomreqresult = XtMakeResizeRequest ((Widget)xw, reqWidth, reqHeight,
					     &repWidth, &repHeight);

#ifndef nothack
	XSetWMNormalHints(screen->display, XtWindow(XtParent(xw)), &sizehints);
#endif

	if (oldWidth != reqWidth || oldHeight != reqHeight) {
	    /* wait for a window manager to actually do it */
	    XIfEvent (screen->display, &event, IsEventType, (char *)ConfigureNotify);
	}

	newAttributes.event_mask = oldAttributes.your_event_mask;
	newAttributes.bit_gravity = NorthWestGravity;
	XChangeWindowAttributes( screen->display, TextWindow(screen),
				 CWEventMask|CWBitGravity,
				 &newAttributes );
}

void DoResizeScreen (xw)
    register XtermWidget xw;
{
    int border = 2 * xw->screen.border;
    int sb = (xw->screen.scrollbar ? xw->screen.scrollWidget->core.width : 0);

    ResizeScreen (xw, border + sb, border);
}


static Widget CreateScrollBar(xw, x, y, height)
	XtermWidget xw;
	int x, y, height;
{
	Widget scrollWidget;

	static Arg argList[] = {
	   {XtNx,		(XtArgVal) 0},
	   {XtNy,		(XtArgVal) 0},
	   {XtNheight,		(XtArgVal) 0},
	   {XtNreverseVideo,	(XtArgVal) 0},
	   {XtNorientation,	(XtArgVal) XtorientVertical},
	   {XtNborderWidth,	(XtArgVal) 1},
	};   

	argList[0].value = (XtArgVal) x;
	argList[1].value = (XtArgVal) y;
	argList[2].value = (XtArgVal) height;
	argList[3].value = (XtArgVal) xw->misc.re_verse;

	scrollWidget = XtCreateWidget("scrollbar", scrollbarWidgetClass, 
	  xw, argList, XtNumber(argList));
        XtAddCallback (scrollWidget, XtNscrollProc, ScrollTextUpDownBy, 0);
        XtAddCallback (scrollWidget, XtNjumpProc, ScrollTextTo, 0);
	return (scrollWidget);
}

static void RealizeScrollBar (sbw, screen)
    Widget sbw;
    TScreen *screen;
{
    XtRealizeWidget (sbw);
}


ScrollBarReverseVideo(scrollWidget)
	register Widget scrollWidget;
{
	Arg args[4];
	int nargs = XtNumber(args);
	unsigned long bg, fg, bdr;
	Pixmap bdpix;

	XtSetArg(args[0], XtNbackground, &bg);
	XtSetArg(args[1], XtNforeground, &fg);
	XtSetArg(args[2], XtNborderColor, &bdr);
	XtSetArg(args[3], XtNborderPixmap, &bdpix);
	XtGetValues (scrollWidget, args, nargs);
	args[0].value = (XtArgVal) fg;
	args[1].value = (XtArgVal) bg;
	nargs--;				/* don't set border_pixmap */
	if (bdpix == XtUnspecifiedPixmap) {	/* if not pixmap then pixel */
	    args[2].value = args[1].value;	/* set border to new fg */
	} else {				/* ignore since pixmap */
	    nargs--;				/* don't set border pixel */
	}
	XtSetValues (scrollWidget, args, nargs);
}



ScrollBarDrawThumb(scrollWidget)
	register Widget scrollWidget;
{
	register TScreen *screen = &term->screen;
	register int thumbTop, thumbHeight, totalHeight;
	
	thumbTop    = screen->topline + screen->savedlines;
	thumbHeight = screen->max_row + 1;
	totalHeight = thumbHeight + screen->savedlines;

	XawScrollbarSetThumb(scrollWidget,
	 ((float)thumbTop) / totalHeight,
	 ((float)thumbHeight) / totalHeight);
	
}

ResizeScrollBar(scrollWidget, x, y, height)
	register Widget scrollWidget;
	int x, y;
	unsigned height;
{
	XtConfigureWidget(scrollWidget, x, y, scrollWidget->core.width,
	    height, scrollWidget->core.border_width);
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

	ScrollSelection(screen, i);

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
	ScrnRefresh(screen, refreshtop, 0, lines, screen->max_col + 1, False);

	ScrollBarDrawThumb(screen->scrollWidget);
}


ScrollBarOn (xw, init, doalloc)
    XtermWidget xw;
    int init, doalloc;
{
	register TScreen *screen = &xw->screen;
	register int border = 2 * screen->border;
	register int i;
	Char *realloc(), *calloc();

	if(screen->scrollbar)
		return;

	if (init) {			/* then create it only */
	    if (screen->scrollWidget) return;

	    /* make it a dummy size and resize later */
	    if ((screen->scrollWidget = CreateScrollBar (xw, -1, - 1, 5))
		== NULL) {
		Bell();
		return;
	    }

	    return;

	}

	if (!screen->scrollWidget) {
	    Bell ();
	    Bell ();
	    return;
	}

	if (doalloc && screen->allbuf) {
	    if((screen->allbuf =
		(ScrnBuf) realloc((char *) screen->buf,
				  (unsigned) 2*(screen->max_row + 2 +
						screen->savelines) *
				  sizeof(char *)))
	       == NULL)
	      Error (ERROR_SBRALLOC);
	    screen->buf = &screen->allbuf[2 * screen->savelines];
	    bcopy ((char *)screen->allbuf, (char *)screen->buf,
		   2 * (screen->max_row + 2) * sizeof (char *));
	    for(i = 2 * screen->savelines - 1 ; i >= 0 ; i--)
	      if((screen->allbuf[i] =
		  calloc((unsigned) screen->max_col + 1, sizeof(char))) ==
		 NULL)
		Error (ERROR_SBRALLOC2);
	}

	ResizeScrollBar (screen->scrollWidget, -1, -1, 
			 Height (screen) + border);
	RealizeScrollBar (screen->scrollWidget, screen);
	screen->scrollbar = screen->scrollWidget->core.width;

	ScrollBarDrawThumb(screen->scrollWidget);
	DoResizeScreen (xw);
	/* map afterwards so BitGravity can be used profitably */
	XMapWindow(screen->display, XtWindow(screen->scrollWidget));
	update_scrollbar ();
}

ScrollBarOff(screen)
	register TScreen *screen;
{
	if(!screen->scrollbar)
		return;
	screen->scrollbar = 0;
	XUnmapWindow(screen->display, XtWindow(screen->scrollWidget));
	DoResizeScreen (term);
	update_scrollbar ();
}

/*ARGSUSED*/
static void ScrollTextTo(scrollbarWidget, closure, topPercent)
	Widget scrollbarWidget;
	caddr_t closure;
	float *topPercent;
{
	register TScreen *screen = &term->screen;
	int thumbTop;	/* relative to first saved line */
	int newTopLine;

/*
   screen->savedlines : Number of offscreen text lines,
   screen->maxrow + 1 : Number of onscreen  text lines,
   screen->topline    : -Number of lines above the last screen->max_row+1 lines
*/

	thumbTop = *topPercent * (screen->savedlines + screen->max_row+1);
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


/*
 * assume that b is lower case and allow plural
 */
static int specialcmplowerwiths (a, b)
    char *a, *b;
{
    register char ca, cb;

    if (!a || !b) return 0;

    while (1) {
	ca = *a;
	cb = *b;
	if (isascii(ca) && isupper(ca)) {		/* lowercasify */
#ifdef _tolower
	    ca = _tolower (ca);
#else
	    ca = tolower (ca);
#endif
	}
	if (ca != cb || ca == '\0') break;  /* if not eq else both nul */
	a++, b++;
    }
    if (cb == '\0' && (ca == '\0' || (ca == 's' && a[1] == '\0')))
      return 1;

    return 0;
}

static int params_to_pixels (screen, params, n)
    TScreen *screen;
    String *params;
    int n;
{
    register mult = 1;
    register char *s;

    switch (n > 2 ? 2 : n) {
      case 2:
	s = params[1];
	if (specialcmplowerwiths (s, "page")) {
	    mult = (screen->max_row + 1) * FontHeight(screen);
	} else if (specialcmplowerwiths (s, "halfpage")) {
	    mult = ((screen->max_row + 1) * FontHeight(screen)) >> 1;
	} else if (specialcmplowerwiths (s, "pixel")) {
	    mult = 1;
	} /* else assume that it is Line */
	mult *= atoi (params[0]);
	break;
      case 1:
	mult = atoi (params[0]) * FontHeight(screen);	/* lines */
	break;
      default:
	mult = screen->scrolllines * FontHeight(screen);
	break;
    }

    return mult;
}


/*ARGSUSED*/
void HandleScrollForward (gw, event, params, nparams)
    Widget gw;
    XEvent *event;
    String *params;
    Cardinal *nparams;
{
    XtermWidget w = (XtermWidget) gw;
    register TScreen *screen = &w->screen;

    ScrollTextUpDownBy (gw, (Opaque) NULL,
			params_to_pixels (screen, params, (int) *nparams));
    return;
}


/*ARGSUSED*/
void HandleScrollBack (gw, event, params, nparams)
    Widget gw;
    XEvent *event;
    String *params;
    Cardinal *nparams;
{
    XtermWidget w = (XtermWidget) gw;
    register TScreen *screen = &w->screen;

    ScrollTextUpDownBy (gw, (Opaque) NULL,
			-params_to_pixels (screen, params, (int) *nparams));
    return;
}


