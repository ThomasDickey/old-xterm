/*
 * $XConsortium: Tekproc.c,v 1.47 89/01/04 12:01:34 jim Exp $
 *
 * Warning, there be crufty dragons here.
 */


#include <X11/copyright.h>

/*
 * Copyright 1988 Massachusetts Institute of Technology
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

/* Tekproc.c */

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include "ptyx.h"
#include "Tekparse.h"
#include <stdio.h>
#ifdef mips			/* !defined(mips) || !defined(SYSTYPE_SYSV) */
# ifndef SYSTYPE_SYSV
# include <sgtty.h>
# endif /* not SYSTYPE_SYSV */
#else
# include <sgtty.h>
#endif /* mips */
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <pwd.h>
#include "data.h"
#include "error.h"

#ifdef macII
#undef FIOCLEX					/* redefined from sgtty.h */
#undef FIONCLEX					/* redefined from sgtty.h */
#include <sys/ioctl.h>				/* to get FIONREAD */
#endif /* macII */

#ifdef MODEMENU
#include "menu.h"
#endif	/* MODEMENU */

extern void exit();
extern long time();

#define TekColormap DefaultColormap( screen->display, \
				    DefaultScreen(screen->display) )
#define DefaultGCID DefaultGC(screen->display, DefaultScreen(screen->display))->gid

/* Tek defines */

#define	BEL		07
#define	CANCEL		030
#define	DOTDASHEDLINE	2
#define	DOTTEDLINE	1
#define	EAST		01
#define	ETX		03
#define	LARGEFONT	0
#define	LARGEFONTNAME	"9x15"
#define	LINEMASK	07
#define	LONGDASHEDLINE	4
#define	MARGIN1		0
#define	MARGIN2		1
#define MAX_PTS		150
#define MAX_VTX		300
#define	NAK		025
#define	NORTH		04
#define	PENDOWN		1
#define	PENUP		0
#define	SHORTDASHEDLINE	3
#define	SMALLFONT	3
#define	SMALLFONTNAME	"6x10"
#define	SOLIDLINE	0
#define	SOUTH		010
#define	TEKBOTTOMPAD	23
#define	TEKDEFHEIGHT	565
#define	TEKDEFWIDTH	750
#define	TEKHEIGHT	3072
#define	TEKHOME		((TekChar[screen->page.fontsize].nlines - 1)\
			 * TekChar[screen->page.fontsize].vsize)
#define	TEKMINHEIGHT	452
#define	TEKMINWIDTH	600
#define	TEKTOPPAD	34
#define	TEKWIDTH	4096
#define	TEXT_BUF_SIZE	256
#define	THREEFONT	2
#define	THREEFONTNAME	"6x13"
#define	TWOFONT		1
#define	TWOFONTNAME	"8x13"
#define	WEST		02

#define	TekMove(x,y)	screen->cur_X = x; screen->cur_Y = y
#define	input()		Tinput()
#define	unput(c)	*Tpushback++ = c

#ifndef lint
static char rcs_id[] = "$XConsortium: Tekproc.c,v 1.47 89/01/04 12:01:34 jim Exp $";
#endif	/* lint */

static XPoint *T_box[TEKNUMFONTS] = {
	T_boxlarge,
	T_box2,
	T_box3,
	T_boxsmall,
};
static struct Tek_Char {
	int hsize;	/* in Tek units */
	int vsize;	/* in Tek units */
	int charsperline;
	int nlines;
} TekChar[TEKNUMFONTS] = {
	{56, 88, 74, 35},	/* large */
	{51, 82, 81, 38},	/* #2 */
	{34, 53, 121, 58},	/* #3 */
	{31, 48, 133, 64},	/* small */
};

static Cursor GINcursor;
static XSegment *line_pt;
static int nplot;
static TekLink Tek0;
static jmp_buf Tekjump;
static TekLink *TekRecord;
static XSegment *Tline;

extern int Talptable[];
extern int Tbestable[];
extern int Tbyptable[];
extern int Tesctable[];
extern int Tipltable[];
extern int Tplttable[];
extern int Tpttable[];
extern int Tspttable[];

static int *curstate = Talptable;
static int *Tparsestate = Talptable;

/* event handlers */
extern void HandleKeyPressed();
extern void HandleStringEvent();
extern void HandleEnterWindow();
extern void HandleLeaveWindow();
extern void HandleFocusChange();
extern void HandleSecure();
extern void TekButtonPressed();

static char defaultTranslations[] = 
    "<KeyPress>: insert()";

static XtActionsRec actionsList[] = { 
    { "string",	HandleStringEvent },
    { "insert",	HandleKeyPressed },
    { "secure", HandleSecure }
};

static Dimension defOne = 1;

static XtResource resources[] = {
    {XtNwidth, XtCWidth, XtRDimension, sizeof(Dimension),
	 XtOffset(Widget, core.width), XtRDimension, (caddr_t)&defOne},
    {XtNheight, XtCHeight, XtRDimension, sizeof(Dimension),
	 XtOffset(Widget, core.height), XtRDimension, (caddr_t)&defOne},
};

static void TekInitialize(), TekRealize(), TekConfigure();
void TekExpose();

WidgetClassRec tekClassRec = {
  {
/* core_class fields */	
    /* superclass	  */	(WidgetClass) &widgetClassRec,
    /* class_name	  */	"Tek4014",
    /* widget_size	  */	sizeof(TekWidgetRec),
    /* class_initialize   */    NULL,
    /* class_part_initialize */ NULL,
    /* class_inited       */	FALSE,
    /* initialize	  */	TekInitialize,
    /* initialize_hook    */    NULL,				
    /* realize		  */	TekRealize,
    /* actions		  */	actionsList,
    /* num_actions	  */	XtNumber(actionsList),
    /* resources	  */	resources,
    /* num_resources	  */	XtNumber(resources),
    /* xrm_class	  */	NULLQUARK,
    /* compress_motion	  */	TRUE,
    /* compress_exposure  */	TRUE,
    /* compress_enterleave */   TRUE,
    /* visible_interest	  */	FALSE,
    /* destroy		  */	NULL,
    /* resize		  */	TekConfigure,
    /* expose		  */	TekExpose,
    /* set_values	  */	NULL,
    /* set_values_hook    */    NULL,
    /* set_values_almost  */    NULL,
    /* get_values_hook    */    NULL,
    /* accept_focus	  */	NULL,
    /* version            */    XtVersion,
    /* callback_offsets   */    NULL,
    /* tm_table           */    defaultTranslations,
    /* query_geometry     */    XtInheritQueryGeometry,
    /* display_accelerator*/    XtInheritDisplayAccelerator,
    /* extension          */    NULL
  }
};
#define tekWidgetClass ((WidgetClass)&tekClassRec)

static Boolean Tfailed = FALSE;

TekWidget CreateTekWidget ()
{
    Widget tekshellwidget;
    extern Arg ourTopLevelShellArgs[];
    extern int number_ourTopLevelShellArgs;

    /* this causes the Initialize method to be called */
    tekshellwidget = XtCreateApplicationShell ("tektronix",
					       topLevelShellWidgetClass,
					       ourTopLevelShellArgs, 
					       number_ourTopLevelShellArgs);
    /* this causes the Realize method to be called */
    tekWidget = (TekWidget) XtCreateManagedWidget ("tek4014", tekWidgetClass,
						   tekshellwidget, NULL, 0);
    return (tekWidget);
}


int TekInit ()
{
    if (Tfailed) return (0);
    if (tekWidget) return (1);
    if (CreateTekWidget()) {
	return (1);
    }
    return (0);
}

Tekparse()
{
	register TScreen *screen = &term->screen;
	register int c, x, y;
	char ch;
	int arg;
	int Tinput();

	for( ; ; )
		switch(Tparsestate[c = input()]) {
		 case CASE_REPORT:
			/* report address */
			if(screen->TekGIN) {
				TekGINoff();
				TekEnqMouse(0);
			} else {
				c = 064;	/* has hard copy unit */
				if(screen->margin == MARGIN2)
					c |= 02;
				TekEnq(c, screen->cur_X, screen->cur_Y);
			}
			TekRecord->ptr[-1] = NAK; /* remove from recording */
			Tparsestate = curstate;
			break;

		 case CASE_VT_MODE:
			/* special return to vt102 mode */
			Tparsestate = curstate;
			TekRecord->ptr[-1] = NAK; /* remove from recording */
			if(screen->logging) {
				FlushLog(screen);
				screen->logstart = buffer;
			}
			return;

		 case CASE_SPT_STATE:
			/* Enter Special Point Plot mode */
			if(screen->TekGIN)
				TekGINoff();
			Tparsestate = curstate = Tspttable;
			break;

		 case CASE_GIN:
			/* Do Tek GIN mode */
			screen->TekGIN = &TekRecord->ptr[-1];
				/* Set cross-hair cursor raster array */
			if(GINcursor = make_tcross(
			    screen->mousecolor,
			    term->core.background_pixel))
				XDefineCursor(
				    screen->display,
				    TShellWindow,
				    GINcursor);
			Tparsestate = Tbyptable;	/* Bypass mode */
			break;

		 case CASE_BEL:
			/* BEL */
			if(screen->TekGIN)
				TekGINoff();
			if(!TekRefresh)
				Bell();
			Tparsestate = curstate;	/* clear bypass condition */
			break;

		 case CASE_BS:
			/* BS */
			if(screen->TekGIN)
				TekGINoff();
			Tparsestate = curstate;	/* clear bypass condition */
			TCursorBack();
			break;

		 case CASE_PT_STATE:
			/* Enter Tek Point Plot mode */
			if(screen->TekGIN)
				TekGINoff();
			Tparsestate = curstate = Tpttable;
			break;

		 case CASE_PLT_STATE:
			/* Enter Tek Plot mode */
			if(screen->TekGIN)
				TekGINoff();
			Tparsestate = curstate = Tplttable;
			if((c = input()) == BEL)
				screen->pen = PENDOWN;
			else {
				unput(c);
				screen->pen = PENUP;
			}
			break;

		 case CASE_TAB:
			/* HT */
			if(screen->TekGIN)
				TekGINoff();
			Tparsestate = curstate;	/* clear bypass condition */
			TCursorForward();
			break;

		 case CASE_IPL_STATE:
			/* Enter Tek Incremental Plot mode */
			if(screen->TekGIN)
				TekGINoff();
			Tparsestate = curstate = Tipltable;
			break;

		 case CASE_ALP_STATE:
			/* Enter Tek Alpha mode from any other mode */
			if(screen->TekGIN)
				TekGINoff();
			/* if in one of graphics states, move alpha cursor */
			if(nplot > 0)	/* flush line Tbuffer */
				TekFlush();
			Tparsestate = curstate = Talptable;
			break;

		 case CASE_UP:
			/* cursor up */
			if(screen->TekGIN)
				TekGINoff();
			Tparsestate = curstate;	/* clear bypass condition */
			TCursorUp();
			break;

		 case CASE_COPY:
			/* make copy */
			if(screen->TekGIN)
				TekGINoff();
			TekCopy();
			TekRecord->ptr[-1] = NAK; /* remove from recording */
			Tparsestate = curstate;	/* clear bypass condition */
			break;

		 case CASE_PAGE:
			/* Page Function */
			if(screen->TekGIN)
				TekGINoff();
			TekPage();	/* clear bypass condition */
			break;

		 case CASE_BES_STATE:
			/* Byp: an escape char */
			Tparsestate = Tbestable;
			break;

		 case CASE_BYP_STATE:
			/* set bypass condition */
			Tparsestate = Tbyptable;
			break;

		 case CASE_IGNORE:
			/* Esc: totally ignore CR, ESC, LF, ~ */
			break;

		 case CASE_ASCII:
			/* Select ASCII char set */
			/* ignore for now */
			Tparsestate = curstate;
			break;

		 case CASE_APL:
			/* Select APL char set */
			/* ignore for now */
			Tparsestate = curstate;
			break;

		 case CASE_CHAR_SIZE: 
			/* character size selector */
		        TekSetGCFont (screen->cur.fontsize = (c & 03));
			Tparsestate = curstate;
			break;

		 case CASE_BEAM_VEC:
			/* beam and vector selector */
			/* only line types */
			if((c &= LINEMASK) != screen->cur.linetype) {
				if(nplot > 0)
					TekFlush();
				screen->cur.linetype = c;
			}
			Tparsestate = curstate;
			break;

		 case CASE_CURSTATE:
			Tparsestate = curstate;
			break;

		 case CASE_PENUP:
			/* Ipl: penup */
			screen->pen = PENUP;
			break;

		 case CASE_PENDOWN:
			/* Ipl: pendown */
			screen->pen = PENDOWN;
			break;

		 case CASE_IPL_POINT:
			/* Ipl: point */
			x = screen->cur_X;
			y = screen->cur_Y;
			if(c & NORTH)
				y++;
			else if(c & SOUTH)
				y--;
			if(c & EAST)
				x++;
			else if(c & WEST)
				x--;
			if(screen->pen == PENDOWN)
				TekDraw(x, y);
			else
				TekMove(x, y);
			break;

		 case CASE_PLT_VEC:
			/* Plt: vector */
			unput(c);
			if(getpoint()) {
				if(screen->pen == PENDOWN)
					TekDraw(screen->cur.x, screen->cur.y);
				else
					TekMove(screen->cur.x, screen->cur.y);
				screen->pen = PENDOWN;
			}
			break;

		 case CASE_PT_POINT:
			/* Pt: point */
			unput(c);
			if(getpoint()) {
				TekMove(screen->cur.x, screen->cur.y);
				TekDraw(screen->cur.x, screen->cur.y);
			}
			break;

		 case CASE_SPT_POINT:
			/* Spt: point */
			/* ignore intensity character in c */
			if(getpoint()) {
				TekMove(screen->cur.x, screen->cur.y);
				TekDraw(screen->cur.x, screen->cur.y);
			}
			break;

		 case CASE_CR:
			/* CR */
			if(screen->TekGIN)
				TekGINoff();
			if(nplot > 0)	/* flush line Tbuffer */
				TekFlush();
			screen->cur_X = screen->margin == MARGIN1 ? 0 :
			 TEKWIDTH / 2;
			Tparsestate = curstate = Talptable;
			break;

		 case CASE_ESC_STATE:
			/* ESC */
			Tparsestate = Tesctable;
			break;

		 case CASE_LF:
			/* LF */
			if(screen->TekGIN)
				TekGINoff();
			TCursorDown();
			if(!TekRefresh && (screen->display->qlen > 0 ||
			 (ioctl(screen->display->fd, FIONREAD, (char *)&arg), arg) > 0))
				xevents();
			break;

		 case CASE_SP:
			/* SP */
			TCursorForward();
			break;

		 case CASE_PRINT:
			/* printable character */
			ch = c;
			c = screen->cur.fontsize;

			XDrawString(
			    screen->display,
			    TWindow(screen), 
			    screen->TnormalGC,
			    (int)(screen->cur_X * TekScale(screen)) + screen->border,
			    (int)((TEKHEIGHT + TEKTOPPAD - screen->cur_Y) * TekScale(screen)) + screen->border,
			    &ch,
			    1);
			TCursorForward();
			break;
		 case CASE_OSC:
			/* do osc escape */
			do_osc(Tinput);
			Tparsestate = curstate;
			break;
		}
}			

TekSetGCFont (size)
    int size;  /* one of SMALLFONT, TWOFONT, THREEFONT, or LARGEFONT */
    {
    register TScreen *screen = &term->screen;
    Font fid = screen->Tfont[size]->fid;
    if (fid == DefaultGCID) 
       /* we didn't succeed in opening a real font
	  for this size.  Instead, use server default. */
       XCopyGC (screen->display,
	  DefaultGC(screen->display, DefaultScreen(screen->display)),
	  GCFont, screen->TnormalGC);
   else
       XSetFont (screen->display, screen->TnormalGC, fid);
    }

static int rcnt;
static char *rptr;
static int Tselect_mask;

Tinput()
{
	register TScreen *screen = &term->screen;
	register char *cp;
	register int i;
	register TekLink *tek;
	extern char *malloc();

	if(Tpushback > Tpushb)
		return(*--Tpushback);
	if(TekRefresh) {
		if(rcnt-- > 0)
			return(*rptr++);
		if(tek = TekRefresh->next) {
			TekRefresh = tek;
			rcnt = tek->count - 1;
			rptr = tek->data;
			return(*rptr++);
		}
		TekRefresh = (TekLink *)0;
		longjmp(Tekjump, 1);
	}
again:
	if(Tbcnt-- <= 0) {
		if(nplot > 0)	/* flush line Tbuffer */
			TekFlush();
		Tselect_mask = pty_mask;	/* force a read */
		for( ; ; ) {
			if(Tselect_mask & pty_mask) {
				if(screen->logging)
					FlushLog(screen);
				if((Tbcnt = read(screen->respond,
				 Tbptr = Tbuffer, BUF_SIZE)) < 0) {
					if(errno == EIO)
						Cleanup (0);
					else if(errno != EWOULDBLOCK)
						Panic(
				 "Tinput:read returned unexpected error (%d)\n",
						 errno);
				} else if(Tbcnt == 0)
					Panic("input: read returned zero\n", 0);
				else {
					/* strip parity bit */
					for(i = Tbcnt, cp = Tbptr ; i > 0 ; i--)
						*cp++ &= CHAR;
					break;
				}
			}
			if (Ttoggled && curstate == Talptable) {
				TCursorToggle(TOGGLE);
				Ttoggled = FALSE;
			}
			if(QLength(screen->display))
				Tselect_mask = X_mask;
			else {
				XFlush(screen->display);
				Tselect_mask = Select_mask;
				if((i = select(max_plus1, &Tselect_mask,
					(int *)NULL, (int *)NULL,
					(struct timeval *)NULL)) < 0){
					if (errno != EINTR)
						SysError(ERROR_TSELECT);
					continue;
				}
			}
			if(Tselect_mask & X_mask) {
				xevents();
				if(Tbcnt > 0)
					goto again;
			}
		}
		Tbcnt--;
		if (!Ttoggled && curstate == Talptable) {
			TCursorToggle(TOGGLE);
			Ttoggled = TRUE;
		}
	}
	if((tek = TekRecord)->count >= TEK_LINK_BLOCK_SIZE) {
		if((TekRecord = tek->next = (TekLink *)malloc(sizeof(TekLink)))
		 == (TekLink *)0)
			Panic("Tinput: malloc error (%d)\n", errno);
		tek = tek->next;
		tek->next = (TekLink *)0;
		tek->count = 0;
		tek->ptr = tek->data;
	}
	tek->count++;
	return(*tek->ptr++ = *Tbptr++);
}

/* this should become the Tek Widget's Resize proc */
static void TekConfigure(w)
    Widget w;
{
    register TScreen *screen = &term->screen;
    register int border = 2 * screen->border;
    register double d;

    if (TWindow(screen)) XClearWindow(screen->display, TWindow(screen));
    TWidth(screen) = w->core.width - border;
    THeight(screen) = w->core.height - border;
    TekScale(screen) = (double)TWidth(screen) / TEKWIDTH;
    if((d = (double)THeight(screen) / (TEKHEIGHT + TEKTOPPAD + TEKBOTTOMPAD))
       < TekScale(screen))
      TekScale(screen) = d;
    TFullWidth(screen) = w->core.width;
    TFullHeight(screen) = w->core.height;
}

/* this should become the Tek Widget's Expose proc */
/* need to use compress_events = TRUE so you don't need to 
   look at the "count" in the exposure event ! */
/*ARGSUSED*/
void TekExpose(w, event, region)
Widget w;
XExposeEvent *event;
Region region;
{
	register TScreen *screen = &term->screen;

#ifdef lint
	region = region;
#endif
	if(!Ttoggled)
	    TCursorToggle(CLEAR);
	Ttoggled = TRUE;
	Tpushback = Tpushb;
	screen->cur_X = 0;
	screen->cur_Y = TEKHOME;
	screen->cur = screen->page;
        TekSetGCFont (screen->cur.fontsize);
	screen->margin = MARGIN1;
	if(screen->TekGIN) {
		screen->TekGIN = NULL;
		TekGINoff();
	}
	TekRefresh = &Tek0;
	rptr = TekRefresh->data;
	rcnt = TekRefresh->count;
	Tparsestate = curstate = Talptable;
	if(!screen->waitrefresh)
		dorefresh();
}

dorefresh()
{
	register TScreen *screen = &term->screen;
	static Cursor wait_cursor = None;

	if (wait_cursor == None)
            wait_cursor = make_wait(screen->mousecolor, term->core.background_pixel);
        XDefineCursor(screen->display, TShellWindow, wait_cursor);
	XFlush(screen->display);
	if(!setjmp(Tekjump))
		Tekparse();
	XDefineCursor(screen->display, TShellWindow,
	 (screen->TekGIN && GINcursor) ? GINcursor : screen->arrow);
}

TekPage()
{
	register TScreen *screen = &term->screen;
	register TekLink *tek, *tek2;

	XClearWindow(screen->display, TWindow(screen));
	screen->cur_X = 0;
	screen->cur_Y = TEKHOME;
	screen->margin = MARGIN1;
	screen->page = screen->cur;
	if(screen->TekGIN)
		TekGINoff();
	tek = TekRecord = &Tek0;
	tek->count = 0;
	tek->ptr = tek->data;
	if(tek = tek->next)
		do {
			tek2 = tek->next;
			free((char *)tek);
		} while(tek = tek2);
	TekRecord->next = (TekLink *)0;
	TekRefresh = (TekLink *)0;
	Ttoggled = TRUE;
	Tparsestate = curstate = Talptable;	/* Tek Alpha mode */
}

#define	EXTRABITS	017
#define	FIVEBITS	037
#define	HIBITS		(FIVEBITS << SHIFTHI)
#define	LOBITS		(FIVEBITS << SHIFTLO)
#define	SHIFTHI		7
#define	SHIFTLO		2
#define	TWOBITS		03

getpoint()
{
	register int c, x, y, e, lo_y = 0;
	register TScreen *screen = &term->screen;

	x = screen->cur.x;
	y = screen->cur.y;
	for( ; ; ) {
		if((c = input()) < ' ') {	/* control character */
			unput(c);
			return(0);
		}
		if(c < '@') {	/* Hi X or Hi Y */
			if(lo_y) {	/* seen a Lo Y, so this must be Hi X */
				x &= ~HIBITS;
				x |= (c & FIVEBITS) << SHIFTHI;
				continue;
			}
			/* else Hi Y */
			y &= ~HIBITS;
			y |= (c & FIVEBITS) << SHIFTHI;
			continue;
		}
		if(c < '`') {	/* Lo X */
			x &= ~LOBITS;
			x |= (c & FIVEBITS) << SHIFTLO;
			screen->cur.x = x;
			screen->cur.y = y;
			return(1);	/* OK */
		}
		/* else Lo Y */
		if(lo_y) {	/* seen a Lo Y, so other must be extra bits */
			e = (y >> SHIFTLO) & EXTRABITS;
			x &= ~TWOBITS;
			x |= e & TWOBITS;
			y &= ~TWOBITS;
			y |= (e >> SHIFTLO) & TWOBITS;
		}
		y &= ~LOBITS;
		y |= (c & FIVEBITS) << SHIFTLO;
		lo_y++;
	}
}

TCursorBack()
{
	register TScreen *screen = &term->screen;
	register struct Tek_Char *t;
	register int x, l;

	x = ( screen->cur_X -=
		(t = &TekChar[screen->cur.fontsize])->hsize
	    );

	if(screen->margin == MARGIN1 && x < 0 || screen->margin == MARGIN2
	 && x < TEKWIDTH / 2) {
		if((l = (screen->cur_Y + (t->vsize - 1)) / t->vsize + 1) >=
		 t->nlines) {
			screen->margin = !screen->margin;
			l = 0;
		}
		screen->cur_Y = l * t->vsize;
		screen->cur_X = (t->charsperline - 1) * t->hsize;
	}
}

TCursorForward()
{
	register TScreen *screen = &term->screen;
	register struct Tek_Char *t;
	register int l;

	if( ( screen->cur_X +=
		( t = &TekChar[screen->cur.fontsize])->hsize
	    ) > TEKWIDTH
	  ) {
		if((l = screen->cur_Y / t->vsize - 1) < 0) {
			screen->margin = !screen->margin;
			l = t->nlines - 1;
		}
		screen->cur_Y = l * t->vsize;
		screen->cur_X = screen->margin == MARGIN1 ? 0 : TEKWIDTH / 2;
	}
}

TCursorUp()
{
	register TScreen *screen = &term->screen;
	register struct Tek_Char *t;
	register int l;

	t = &TekChar[screen->cur.fontsize];

	if((l = (screen->cur_Y + (t->vsize - 1)) / t->vsize + 1) >= t->nlines) {
		l = 0;
		if((screen->margin = !screen->margin) != MARGIN1) {
			if(screen->cur_X < TEKWIDTH / 2)
				screen->cur_X += TEKWIDTH / 2;
		} else if(screen->cur_X >= TEKWIDTH / 2)
			screen->cur_X -= TEKWIDTH / 2;
	}
	screen->cur_Y = l * t->vsize;
}

TCursorDown()
{
	register TScreen *screen = &term->screen;
	register struct Tek_Char *t;
	register int l;

	t = &TekChar[screen->cur.fontsize];

	if((l = screen->cur_Y / t->vsize - 1) < 0) {
		l = t->nlines - 1;
		if((screen->margin = !screen->margin) != MARGIN1) {
			if(screen->cur_X < TEKWIDTH / 2)
				screen->cur_X += TEKWIDTH / 2;
		} else if(screen->cur_X >= TEKWIDTH / 2)
			screen->cur_X -= TEKWIDTH / 2;
	}
	screen->cur_Y = l * t->vsize;
}

TekDraw (x, y)
int x, y;
{
	register TScreen *screen = &term->screen;

	if(nplot == 0 || T_lastx != screen->cur_X || T_lasty != screen->cur_Y) {
		/*
		 * We flush on each unconnected line segment if the line
		 * type is not solid.  This solves a bug in X when drawing
		 * points while the line type is not solid.
		 */
		if(nplot > 0 && screen->cur.linetype != SOLIDLINE)
			TekFlush();
	}
	AddToDraw(screen->cur_X, screen->cur_Y, x, y);
	T_lastx = screen->cur_X = x;
	T_lasty = screen->cur_Y = y;
}

AddToDraw(x1, y1, x2, y2)
int x1, y1, x2, y2;
{
	register TScreen *screen = &term->screen;
	register XSegment *lp;

	if(nplot >= MAX_PTS) {
		TekFlush();
	}
	lp = line_pt++;
	lp->x1 = x1 = x1 * TekScale(screen) + screen->border;
	lp->y1 = y1 = (TEKHEIGHT + TEKTOPPAD - y1) * TekScale(screen) +
	 screen->border;
	lp->x2 = x2 = x2 * TekScale(screen) + screen->border;
	lp->y2 = y2 = (TEKHEIGHT + TEKTOPPAD - y2) * TekScale(screen) +
	 screen->border;
	nplot++;
}

TekFlush ()
{
	register TScreen *screen = &term->screen;

	XDrawSegments(screen->display, TWindow(screen), 
		((screen->cur.linetype == SOLIDLINE)?  screen->TnormalGC :
		 screen->linepat[screen->cur.linetype - 1]),
		 Tline, nplot);
	nplot = 0;
	line_pt = Tline;
}

TekGINoff()
{
	register TScreen *screen = &term->screen;
	
	XDefineCursor(screen->display, TShellWindow, screen->arrow);
	if(GINcursor)
		XFreeCursor(screen->display, GINcursor);
	if(screen->TekGIN) {
		*screen->TekGIN = CANCEL;	/* modify recording */
		screen->TekGIN = NULL;
	}
}

TekEnqMouse(c)
int c;
{
	register TScreen *screen = &term->screen;
	int mousex, mousey, rootx, rooty;
	unsigned int mask; /* XQueryPointer */
	Window root, subw;

	XQueryPointer(
	    screen->display, TWindow(screen), 
	    &root, &subw,
	    &rootx, &rooty,
	    &mousex, &mousey,
	    &mask);
	if((mousex = (mousex - screen->border) / TekScale(screen)) < 0)
		mousex = 0;
	else if(mousex >= TEKWIDTH)
		mousex = TEKWIDTH - 1;
	if((mousey = TEKHEIGHT + TEKTOPPAD - (mousey - screen->border) /
	     TekScale(screen)) < 0)
		mousey = 0;
	else if(mousey >= TEKHEIGHT)
		mousey = TEKHEIGHT - 1;
	TekEnq(c, mousex, mousey);
}

TekEnq (status, x, y)
int status;
register int x, y;
{
	register TScreen *screen = &term->screen;
	int pty = screen->respond;
	char cplot [5];

	/* Translate x and y to Tektronix code */
	cplot[1] = 040 | ((x >> SHIFTHI) & FIVEBITS);
	cplot[2] = 040 | ((x >> SHIFTLO) & FIVEBITS);
	cplot[3] = 040 | ((y >> SHIFTHI) & FIVEBITS);
	cplot[4] = 040 | ((y >> SHIFTLO) & FIVEBITS);
	if(cplot[0] = status)
		write (pty, cplot, 5);
	else
		write (pty, &cplot[1], 4);
}

TekRun()
{
	register TScreen *screen = &term->screen;
	register int i;
	
	if(!TWindow(screen) && !TekInit()) {
		if(VWindow(screen)) {
			screen->TekEmu = FALSE;
			return;
		}
		Exit(ERROR_TINIT);
	}
	if(!screen->Tshow) {
	    XtRealizeWidget (tekWidget->core.parent);
	    set_tek_visibility (TRUE);
	} 

	if(screen->select || screen->always_highlight)
		TekSelect();
	if (L_flag > 0) {
		XWarpPointer (screen->display, None, TWindow(screen),
			    0, 0, 0, 0,
			    TFullWidth(screen) >> 1, TFullHeight(screen) >> 1);
		L_flag = -1;
	}
	Tpushback = Tpushb;
	Tbptr = Tbuffer;
	for(i = Tbcnt = bcnt ; i > 0 ; i--)
		*Tbptr++ = *bptr++;
	Tbptr = Tbuffer;
	Ttoggled = TRUE;
	if(!setjmp(Tekend))
		Tekparse();
	if(!Ttoggled) {
		TCursorToggle(TOGGLE);
		Ttoggled = TRUE;
	}
	screen->TekEmu = FALSE;
	if (!screen->always_highlight)
	    TekUnselect ();
	reselectwindow (screen);
}

#define DOTTED_LENGTH 2
#define DOT_DASHED_LENGTH 4
#define SHORT_DASHED_LENGTH 2
#define LONG_DASHED_LENGTH 2

static int dash_length[TEKNUMLINES] = {
	DOTTED_LENGTH,
	DOT_DASHED_LENGTH,
	SHORT_DASHED_LENGTH,
	LONG_DASHED_LENGTH,
};

static unsigned char dotted[DOTTED_LENGTH] = {3, 1};
static unsigned char dot_dashed[DOT_DASHED_LENGTH] = {3, 4, 3, 1};
static unsigned char short_dashed[SHORT_DASHED_LENGTH] = {4, 4};
static unsigned char long_dashed[LONG_DASHED_LENGTH] = {4, 7};

static unsigned char *dashes[TEKNUMLINES] = {
	dotted,
	dot_dashed,
	short_dashed,
	long_dashed,
};



/*
 * The following is called the create the tekWidget
 */

static void TekInitialize(request, new)
    Widget request, new;
{
   /* look for focus related events on the shell, because we need
    * to care about the shell's border being part of our focus.
    */
    XtAddEventHandler(XtParent(new), EnterWindowMask, FALSE,
		      HandleEnterWindow, (caddr_t)NULL);
    XtAddEventHandler(XtParent(new), LeaveWindowMask, FALSE,
		      HandleLeaveWindow, (caddr_t)NULL);
    XtAddEventHandler(XtParent(new), FocusChangeMask, FALSE,
		      HandleFocusChange, (caddr_t)NULL);
    XtAddEventHandler(new, ButtonPressMask, FALSE,
		      TekButtonPressed, (caddr_t)NULL);
}


static void TekRealize (gw, valuemaskp, values)
    Widget gw;
    XtValueMask *valuemaskp;
    XSetWindowAttributes *values;
{
    TekWidget tw = (TekWidget) gw;
    register TScreen *screen = &term->screen;
    register int i;
    register TekLink *tek;
    register double d;
    register int border = 2 * screen->border;
    XColor cdef;
    Pixel pixels[2];
    int pr;
    XGCValues gcv;
    int winX, winY, width, height;
    XSizeHints sizehints;
    char Tdefault[32];
    extern char *malloc();

    tw->core.border_pixel = term->core.border_pixel;

    if (!(screen->Tfont[SMALLFONT] = XLoadQueryFont (screen->display,
						     SMALLFONTNAME))) {
	fprintf(stderr, "%s: Could not get font %s; using server default\n",
		xterm_name, SMALLFONTNAME);
	screen->Tfont[SMALLFONT] = XQueryFont (screen->display, DefaultGCID);
    }
    if((Tbuffer = (char *)malloc(BUF_SIZE)) == NULL ||
       (Tpushb = (char *)malloc(10)) == NULL ||
       (Tline = (XSegment *)malloc(MAX_VTX * sizeof(XSegment))) == NULL) {
	fprintf (stderr, "%s: Not enough core for Tek mode\n", xterm_name);
	goto mallocfailed;
    }

    screen->xorplane = 1;

    screen->Tbackground = term->core.background_pixel;
    screen->Tforeground = screen->foreground;
    screen->Tcursorcolor = screen->foreground;

    if (term->misc.T_geometry == NULL) {
	sprintf (Tdefault, "=%dx%d",
		TEKDEFWIDTH + border, TEKDEFHEIGHT + border);
	term->misc.T_geometry = Tdefault;
    }

    winX = 1;
    winY = 1;
    width = TEKDEFWIDTH + border;
    height = TEKDEFHEIGHT + border;

    pr = XParseGeometry(term->misc.T_geometry, &winX, &winY, &width, &height);
    if ((pr & XValue) && (pr & XNegative))
      winX += DisplayWidth(screen->display, DefaultScreen(screen->display))
                        - width - (term->core.parent->core.border_width * 2);
    if ((pr & YValue) && (pr & YNegative))
      winY += DisplayHeight(screen->display, DefaultScreen(screen->display))
	- height - (term->core.parent->core.border_width * 2);
  
    /* set up size hints */
    sizehints.min_width = TEKMINWIDTH + border;
    sizehints.min_height = TEKMINHEIGHT + border;
    sizehints.width_inc = 1;
    sizehints.height_inc = 1;
    sizehints.flags = PMinSize|PResizeInc;
    sizehints.x = winX;
    sizehints.y = winY;
    if ((XValue&pr) || (YValue&pr))
      sizehints.flags |= USSize|USPosition;
    else sizehints.flags |= PSize|PPosition;
    sizehints.width = width;
    sizehints.height = height;
    if ((WidthValue&pr) || (HeightValue&pr))
      sizehints.flags |= USSize;
    else sizehints.flags |= PSize;

    (void) XtMakeResizeRequest ((Widget) tw, width, height,
				&tw->core.width, &tw->core.height);

    /* XXX This is bogus.  We are parsing geometries too late.  This
     * is information that the shell widget ought to have before we get
     * realized, so that it can do the right thing.
     */
    if (sizehints.flags & USPosition)
      XMoveWindow (XtDisplay(tw), tw->core.parent->core.window,
		   sizehints.x, sizehints.y);
    XSetNormalHints (XtDisplay(tw), tw->core.parent->core.window,
		     &sizehints);

    values->win_gravity = NorthWestGravity;
    values->background_pixel = screen->Tbackground;

    if((tw->core.window = TWindow(screen) = 
	XCreateWindow (screen->display,
		       tw->core.parent->core.window,
		       tw->core.x, tw->core.y,
		       tw->core.width, tw->core.height, tw->core.border_width,
		       (int) tw->core.depth,
		       InputOutput, CopyFromParent,
		       ((*valuemaskp)|CWBackPixel|CWWinGravity),
		       values)) == NULL) {
	fprintf(stderr, "%s: Can't create Tek window\n", xterm_name);
	free((char *)Tline);
      mallocfailed:
	if(Tpushb) free((char *)Tpushb);
	if(Tbuffer) free((char *)Tbuffer);
	XFreeFont(screen->display, screen->Tfont[SMALLFONT]);
	Tfailed = TRUE;
	return;
    }

    screen->Tbox = T_box;

    TFullWidth(screen) = width;
    TFullHeight(screen) = height;
    TWidth(screen) = width - border;
    THeight(screen) = height - border;
    TekScale(screen) = (double)TWidth(screen) / TEKWIDTH;
    if((d = (double)THeight(screen) / (TEKHEIGHT + TEKTOPPAD +
				       TEKBOTTOMPAD)) < TekScale(screen))
      TekScale(screen) = d;
    
    screen->tobaseline[SMALLFONT] = screen->Tfont[SMALLFONT]->ascent;

    if (!(screen->Tfont[THREEFONT] = XLoadQueryFont(screen->display,
						    THREEFONTNAME)))
      screen->Tfont[THREEFONT] = screen->Tfont[SMALLFONT];
    screen->tobaseline[THREEFONT] = screen->Tfont[THREEFONT]->ascent;

    if (!(screen->Tfont[TWOFONT] = XLoadQueryFont(screen->display,
						  TWOFONTNAME)))
      screen->Tfont[TWOFONT] = screen->Tfont[THREEFONT];
    screen->tobaseline[TWOFONT] = screen->Tfont[TWOFONT]->ascent;

    if (!(screen->Tfont[LARGEFONT] = XLoadQueryFont(screen->display,
						    LARGEFONTNAME)))
      screen->Tfont[LARGEFONT] = screen->Tfont[TWOFONT];
    screen->tobaseline[LARGEFONT] = screen->Tfont[LARGEFONT]->ascent;

    screen->cur.fontsize = LARGEFONT;	/* set large font	*/

    gcv.graphics_exposures = TRUE;	/* default */
    gcv.font = screen->Tfont[screen->cur.fontsize]->fid;
    gcv.foreground = screen->Tforeground;
    gcv.background = screen->Tbackground;
    
    /* if font wasn't successfully opened, then gcv.font will contain
       the Default GC's ID, meaning that we must use the server default font. 
     */
    TEKgcFontMask = (gcv.font == DefaultGCID) ? 0 : GCFont;
    screen->TnormalGC = XCreateGC (screen->display, TWindow(screen), 
				   (TEKgcFontMask|GCGraphicsExposures|
				    GCForeground|GCBackground), &gcv);

    gcv.function = GXinvert;
    gcv.plane_mask = screen->xorplane = (screen->Tbackground ^
					 screen->Tcursorcolor);
    screen->TcursorGC = XCreateGC (screen->display, TWindow(screen), 
				   (GCFunction|GCPlaneMask), &gcv);

    gcv.foreground = screen->Tforeground;
    gcv.line_style = LineOnOffDash;
    for(i = 0 ; i < TEKNUMLINES ; i++) {
	screen->linepat[i] = XCreateGC (screen->display, TWindow(screen),
					(GCForeground|GCLineStyle), &gcv); 
	XSetDashes (screen->display, screen->linepat[i], 0,
		    dashes[i], dash_length[i]);
    }

    if (term->flags & REVERSE_VIDEO)
      TekReverseVideo(screen);
    else
      TekBackground(screen);

    screen->margin = MARGIN1;		/* Margin 1		*/
    screen->TekGIN = FALSE;			/* GIN off		*/


    XDefineCursor(screen->display, TShellWindow, screen->pointer_cursor);
    TekUnselect ();

    {	/* there's gotta be a better way... */
	static Arg args[] = {
	    {XtNtitle, NULL},
	    {XtNiconName, NULL},
	};
	char *icon_name, *title, *tek_icon_name, *tek_title;

	args[0].value = (XtArgVal)&icon_name;
	args[1].value = (XtArgVal)&title;
	XtGetValues (tw->core.parent, args, 2);
	tek_icon_name = XtMalloc(strlen(icon_name)+7);
	strcpy(tek_icon_name, icon_name);
	strcat(tek_icon_name, "(Tek)");
	tek_title = XtMalloc(strlen(title)+7);
	strcpy(tek_title, title);
	strcat(tek_title, "(Tek)");
	args[0].value = (XtArgVal)tek_icon_name;
	args[1].value = (XtArgVal)tek_title;
	XtSetValues (tw->core.parent, args, 2);
	XtFree( tek_icon_name );
	XtFree( tek_title );
    }

    tek = TekRecord = &Tek0;
    tek->next = (TekLink *)0;
    tek->count = 0;
    tek->ptr = tek->data;
    Tpushback = Tpushb;
    Tbptr = Tbuffer;
    screen->cur_X = 0;
    screen->cur_Y = TEKHOME;
    line_pt = Tline;
    Ttoggled = TRUE;
    return;
}

TekReverseVideo(screen)
register TScreen *screen;
{
	register int flag, i;
	XGCValues gcv;
	 

	i = screen->Tbackground;
	screen->Tbackground = screen->Tforeground;
	screen->Tforeground = i;
	
	XSetForeground(screen->display, screen->TnormalGC, 
	 screen->Tforeground);
	XSetBackground(screen->display, screen->TnormalGC, 
	 screen->Tbackground);

	if (tekWidget) {
	    if (tekWidget->core.border_pixel == screen->Tbackground) {
		tekWidget->core.border_pixel = screen->Tforeground;
		tekWidget->core.parent->core.border_pixel =
		  screen->Tforeground;
		if (tekWidget->core.parent->core.window)
		  XSetWindowBorder (screen->display,
				    tekWidget->core.parent->core.window,
				    tekWidget->core.border_pixel);
	    }
	}

	for(i = 0 ; i < TEKNUMLINES ; i++) {
		XSetForeground(screen->display, screen->linepat[i], 
		 screen->Tforeground);
	}

	if(screen->cellsused) {
		flag = (term->flags & REVERSE_VIDEO) != 0;
		screen->Tcursorcolor = screen->Tbackground | screen->xorplane;
		i = screen->select ? 2 : !flag;
		screen->colorcells[i].pixel = screen->Tcursorcolor;
		XStoreColor(screen->display, TekColormap, &screen->colorcells[i]);
		screen->colorcells[flag].pixel = screen->Tforeground |
		 screen->xorplane;
		XStoreColor(screen->display, TekColormap, &screen->colorcells[flag]);
	} else
		screen->Tcursorcolor = screen->Tforeground;

	gcv.plane_mask = screen->xorplane = (screen->Tbackground ^
					     screen->Tcursorcolor);
	XChangeGC (screen->display, screen->TcursorGC, GCPlaneMask, &gcv);
	TekBackground(screen);
}

TekBackground(screen)
register TScreen *screen;
{
	if(TWindow(screen))
		XSetWindowBackground(screen->display, TWindow(screen), 
		 screen->Tbackground);
}

/*
 * Toggles cursor on or off at cursor position in screen.
 */
TCursorToggle(toggle)
int toggle;
{
	register TScreen *screen = &term->screen;
	register int c, x, y;
	register T_fontsize *Tf;

	if (!screen->Tshow) return;

	c = screen->cur.fontsize;
	Tf = &Tfontsize[c];

	x = (screen->cur_X * TekScale(screen)) + screen->border;
	y = ((TEKHEIGHT + TEKTOPPAD - screen->cur_Y) * TekScale(screen)) +
	 screen->border - screen->tobaseline[c];
	if (toggle) {
	   if (screen->select || screen->always_highlight) 
		XFillRectangle(
		    screen->display, TWindow(screen), screen->TcursorGC,
		    x, y,
		    (unsigned) Tf->Twidth, (unsigned) Tf->Theight);
	  else { /* fix to use different GC! */
		  screen->Tbox[c]->x = x;
		  screen->Tbox[c]->y = y ;
		  XDrawLines(screen->display, TWindow(screen), 
		    screen->TcursorGC,
		    screen->Tbox[c], NBOX, CoordModePrevious);
	   }
	} else {
	   if (screen->select || screen->always_highlight) 
		XClearArea(screen->display, TWindow(screen), x, y,
		    (unsigned) Tf->Twidth, (unsigned) Tf->Theight, FALSE);
	   else { 
		  screen->Tbox[c]->x = x;
		  screen->Tbox[c]->y = y ;
		  XDrawLines(screen->display, TWindow(screen), 
		    screen->TcursorGC,
		    screen->Tbox[c], NBOX, CoordModePrevious);
	   }
	}
}

TekSelect()
{
	register TScreen *screen = &term->screen;

	if (tekWidget && TShellWindow)
	  XSetWindowBorder (screen->display, TShellWindow,
			    tekWidget->core.border_pixel);
}

TekUnselect()
{
	register TScreen *screen = &term->screen;

	if (tekWidget && TShellWindow && !screen->always_highlight)
	  XSetWindowBorderPixmap (screen->display, TShellWindow,
				  screen->graybordertile);
}

TekCopy()
{
	register TekLink *Tp;
	register int fd;
	register TScreen *screen = &term->screen;
	register struct tm *tp;
	long l;
	char buf[32];

	/* for login windows, check that a user has logged in */
	if(L_flag && !checklogin()) {
		Bell();
		return;
	}
	time(&l);
	tp = localtime(&l);
	sprintf(buf, "COPY%02d-%02d-%02d.%02d:%02d:%02d", tp->tm_year,
	 tp->tm_mon, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
	if(access(buf, F_OK) >= 0) {	/* file exists */
		if(access(buf, W_OK) < 0) {
			Bell();
			return;
		}
	} else if(access(".", W_OK) < 0) {	/* can't write in directory */
		Bell();
		return;
	}
	if((fd = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
		Bell();
		return;
	}
	chown(buf, screen->uid, screen->gid);
	sprintf(buf, "\033%c\033%c", screen->page.fontsize + '8',
	 screen->page.linetype + '`');
	write(fd, buf, 4);
	Tp = &Tek0; 
	do
		write(fd, (char *)Tp->data, Tp->count);
	while(Tp = Tp->next);
	close(fd);
}

#ifdef MODEMENU
/*
 * TMENU_LARGE through TMENU_SMALL must be first, as they must be the same
 * as the font size values LARGEFONT through SMALLFONT
 */
#define	TMENU_LARGE	0
#define	TMENU_NUM2	(TMENU_LARGE+1)
#define	TMENU_NUM3	(TMENU_NUM2+1)
#define	TMENU_SMALL	(TMENU_NUM3+1)
#define	TMENU_VTWIN	(TMENU_SMALL+1)
#define	TMENU_LINE	(TMENU_VTWIN+1)
#define	TMENU_PAGE	(TMENU_LINE+1)
#define	TMENU_RESET	(TMENU_PAGE+1)
#define	TMENU_COPY	(TMENU_RESET+1)
#define	TMENU_VTMODE	(TMENU_COPY+1)
#define	TMENU_HIDETEK	(TMENU_VTMODE+1)

static char *Ttext[] = {
	"Large Characters",
	"#2 Size Characters",
	"#3 Size Characters",
	"Small Characters",
	"VT Window Showing",
	"-",
	"PAGE",
	"RESET",
	"COPY",
	"Select VT Mode",
	"Hide Tek Window",
	0,
};

static Tmodes curmodes;
static int Tsize;
static Boolean vshow;

Menu *Tsetupmenu(menu)
register Menu **menu;
{
	register TScreen *screen = &term->screen;
	register char **cp;
	register int size = screen->cur.fontsize;

	curmodes = screen->cur;
	if (*menu == NULL) {
		if ((*menu = NewMenu("Tektronix")) == NULL)
			return(NULL);
		for(cp = Ttext ; *cp ; cp++)
			AddMenuItem(*menu, *cp);
		CheckItem(*menu, size);
		if(vshow = screen->Vshow)
			CheckItem(*menu, TMENU_VTWIN);
		else
			DisableItem(*menu, TMENU_HIDETEK);

		DisableItem(*menu, TMENU_LINE);
		Tsize = size;
		return(*menu);
	}
	if (Tsize != size) {
		UncheckItem(*menu, Tsize);
		CheckItem(*menu, Tsize = size);
	}
	if(vshow != screen->Vshow) {
		SetItemCheck(*menu, TMENU_VTWIN, (vshow = screen->Vshow));
		SetItemDisable(*menu, TMENU_HIDETEK, !vshow);
	}

	return(*menu);
}

static char *changesize[] = {
	"\0338",
	"\0339",
	"\033:",
	"\033;",
};

Tdomenufunc(item)
int item;
{
	register TScreen *screen = &term->screen;
	register char *tp;
	register char *fp;

	switch (item) {
	case TMENU_LARGE:
	case TMENU_NUM2:
	case TMENU_NUM3:
	case TMENU_SMALL:
		if(!Ttoggled) {
			TCursorToggle(TOGGLE);
			Ttoggled = TRUE;
		}
		if(Tbcnt < 0)
			Tbcnt = 0;
		for(fp = changesize[item], tp = &Tbptr[Tbcnt] ; *fp ; ) {
			*tp++ = *fp++;
			Tbcnt++;
		}
		break;

	case TMENU_RESET:
		bzero((char *)&curmodes, sizeof(Tmodes));
			/* drop through */
	case TMENU_PAGE:
		TekRefresh = (TekLink *)0;
		screen->cur = curmodes;
		TekPage();
		screen->cur_X = 0;
		screen->cur_Y = TEKHOME;
		break;

	case TMENU_COPY:
		TekCopy();
		break;

	case TMENU_HIDETEK:
		set_tek_visibility (FALSE);
		reselectwindow(screen);
		TekRefresh = (TekLink *)0;
			/* drop through */
	case TMENU_VTMODE:
		if(TekRefresh)
			dorefresh();
		end_tek_mode ();
		break;

	case TMENU_VTWIN:
		if (!screen->Vshow) {
		    set_vt_visibility (TRUE);
		} else {
		    set_vt_visibility (FALSE);
		    if (!screen->TekEmu && TekRefresh) dorefresh ();
		    end_vt_mode ();
		}

		reselectwindow(screen);
		break;
	}
	if(TekRefresh)
		dorefresh();
}
#endif	/* MODEMENU */


