/*
 * $XConsortium: charproc.c,v 1.64 89/01/04 13:37:50 jim Exp $
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


/* charproc.c */

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
#ifdef macII
#undef FIOCLEX					/* redefined from sgtty.h */
#undef FIONCLEX					/* redefined from sgtty.h */
#endif
#include <sys/ioctl.h>
#include "ptyx.h"
#include "VTparse.h"
#include "data.h"
#include <X11/Xutil.h>
#include "error.h"
#include "main.h"
#include <X11/StringDefs.h>
#ifdef MODEMENU
#include "menu.h"
#endif	/* MODEMENU */

extern void exit(), Bcopy();
static void VTallocbuf();

#define	DEFAULT		-1
#define	TEXT_BUF_SIZE	256
#define TRACKTIMESEC	4L
#define TRACKTIMEUSEC	0L

#define XtNalwaysHighlight	"alwaysHighlight"
#define	XtNboldFont		"boldFont"
#define	XtNc132			"c132"
#define XtNcharClass		"charClass"
#define	XtNcurses		"curses"
#define	XtNcursorColor		"cursorColor"
#define XtNcutNewline		"cutNewline"
#define XtNcutToBeginningOfLine	"cutToBeginningOfLine"
#define XtNgeometry		"geometry"
#define XtNtekGeometry		"tekGeometry"
#define	XtNinternalBorder	"internalBorder"
#define	XtNjumpScroll		"jumpScroll"
#define	XtNlogFile		"logFile"
#define	XtNlogging		"logging"
#define	XtNlogInhibit		"logInhibit"
#define	XtNloginShell		"loginShell"
#define	XtNmarginBell		"marginBell"
#define	XtNpointerColor		"pointerColor"
#define	XtNpointerShape		"pointerShape"
#define XtNmultiClickTime	"multiClickTime"
#define	XtNmultiScroll		"multiScroll"
#define	XtNnMarginBell		"nMarginBell"
#define	XtNreverseWrap		"reverseWrap"
#define	XtNsaveLines		"saveLines"
#define	XtNscrollBar		"scrollBar"
#define	XtNscrollInput		"scrollInput"
#define	XtNscrollKey		"scrollKey"
#define XtNscrollPos    	"scrollPos"
#define	XtNsignalInhibit	"signalInhibit"
#define	XtNtekInhibit		"tekInhibit"
#define	XtNtekStartup		"tekStartup"
#define XtNtiteInhibit		"titeInhibit"
#define	XtNvisualBell		"visualBell"
#define XtNallowSendEvents	"allowSendEvents"

#define XtCAlwaysHighlight	"AlwaysHighlight"
#define	XtCC132			"C132"
#define XtCCharClass		"CharClass"
#define	XtCCurses		"Curses"
#define XtCCutNewline		"CutNewline"
#define XtCCutToBeginningOfLine	"CutToBeginningOfLine"
#define XtCGeometry		"Geometry"
#define	XtCJumpScroll		"JumpScroll"
#define	XtCLogfile		"Logfile"
#define	XtCLogging		"Logging"
#define	XtCLogInhibit		"LogInhibit"
#define	XtCLoginShell		"LoginShell"
#define	XtCMarginBell		"MarginBell"
#define XtCMultiClickTime	"MultiClickTime"
#define	XtCMultiScroll		"MultiScroll"
#define	XtCColumn		"Column"
#define	XtCReverseVideo		"ReverseVideo"
#define	XtCReverseWrap		"ReverseWrap"
#define XtCSaveLines		"SaveLines"
#define	XtCScrollBar		"ScrollBar"
#define XtCScrollPos     	"ScrollPos"
#define	XtCScrollCond		"ScrollCond"
#define	XtCSignalInhibit	"SignalInhibit"
#define	XtCTekInhibit		"TekInhibit"
#define	XtCTekStartup		"TekStartup"
#define XtCTiteInhibit		"TiteInhibit"
#define	XtCVisualBell		"VisualBell"
#define XtCAllowSendEvents	"AllowSendEvents"

#define	doinput()		(bcnt-- > 0 ? *bptr++ : in_put())

#ifndef lint
static char rcs_id[] = "$XConsortium: charproc.c,v 1.64 89/01/04 13:37:50 jim Exp $";
#endif	/* lint */

static long arg;
static int nparam;
static ANSI reply;
static int param[NPARAM];

static unsigned long ctotal;
static unsigned long ntotal;
static jmp_buf vtjmpbuf;

extern int groundtable[];
extern int csitable[];
extern int dectable[];
extern int eigtable[];
extern int esctable[];
extern int iestable[];
extern int igntable[];
extern int scrtable[];
extern int scstable[];


/* event handlers */
extern void HandleKeyPressed();
extern void HandleStringEvent();
extern void HandleEnterWindow();
extern void HandleLeaveWindow();
extern void HandleFocusChange();
       void HandleKeymapChange();
extern void HandleModeMenu();
extern void HandleInsertSelection();
extern void HandleSelectStart();
extern void HandleSelectExtend();
extern void HandleSelectEnd();
extern void HandleStartExtend();
       void HandleBell();
       void HandleIgnore();
extern void HandleSecure();

/*
 * NOTE: VTInitialize zeros out the entire ".screen" component of the 
 * XtermWidget, so make sure to add an assignment statement in VTInitialize() 
 * for each new ".screen" field added to this resource list.
 */

/* Defaults */
static  Boolean	defaultFALSE	   = FALSE;
static  Boolean	defaultTRUE	   = TRUE;
static  int	defaultBorderWidth = DEFBORDERWIDTH;
static  int	defaultIntBorder   = DEFBORDER;
static  int	defaultSaveLines   = SAVELINES;
static  int	defaultNMarginBell = N_MARGINBELL;
static  int	defaultMultiClickTime = MULTICLICKTIME;

static char defaultTranslations[] =
"\
	    <KeyPress>:		insert()	\n\
 Ctrl ~Meta <Btn1Down>:		mode-menu()	\n\
      ~Meta <Btn1Down>:		select-start()	\n\
      ~Meta <Btn1Motion>:	select-extend() \n\
 Ctrl ~Meta <Btn2Down>:		mode-menu()	\n\
~Ctrl ~Meta <Btn2Down>:		ignore()	\n\
      ~Meta <Btn2Up>:		insert-selection(PRIMARY, CUT_BUFFER0) \n\
~Ctrl ~Meta <Btn3Down>:		start-extend()	\n\
      ~Meta <Btn3Motion>:	select-extend()	\n\
      ~Meta <BtnUp>:		select-end(PRIMARY, CUT_BUFFER0) \n\
	    <BtnDown>:		bell(0)		\
";

static XtActionsRec actionsList[] = { 
    { "bell",		  HandleBell },
    { "ignore",		  HandleIgnore },
    { "insert",		  HandleKeyPressed },
    { "insert-selection", HandleInsertSelection },
    { "keymap", 	  HandleKeymapChange },
    { "mode-menu",	  HandleModeMenu },
    { "secure",		  HandleSecure },
    { "select-start",	  HandleSelectStart },
    { "select-extend",	  HandleSelectExtend },
    { "select-end",	  HandleSelectEnd },
    { "start-extend",	  HandleStartExtend },
    { "string",		  HandleStringEvent },
};

static XtResource resources[] = {
{XtNfont, XtCFont, XtRString, sizeof(char *),
	XtOffset(XtermWidget, misc.f_n), XtRString,
	DEFFONT},
{XtNboldFont, XtCFont, XtRString, sizeof(char *),
	XtOffset(XtermWidget, misc.f_b), XtRString,
	DEFBOLDFONT},
{XtNc132, XtCC132, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.c132),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNcharClass, XtCCharClass, XtRString, sizeof(char *),
	XtOffset(XtermWidget, screen.charClass),
	XtRString, (caddr_t) NULL},
{XtNcurses, XtCCurses, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.curses),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNcutNewline, XtCCutNewline, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.cutNewline),
	XtRBoolean, (caddr_t) &defaultTRUE},
{XtNcutToBeginningOfLine, XtCCutToBeginningOfLine, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.cutToBeginningOfLine),
	XtRBoolean, (caddr_t) &defaultTRUE},
{XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
	XtOffset(XtermWidget, core.background_pixel),
	XtRString, "White"},
{XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	XtOffset(XtermWidget, screen.foreground),
	XtRString, "Black"},
{XtNcursorColor, XtCForeground, XtRPixel, sizeof(Pixel),
	XtOffset(XtermWidget, screen.cursorcolor),
	XtRString, "Black"},
{XtNgeometry,XtCGeometry, XtRString, sizeof(char *),
	XtOffset(XtermWidget, misc.geo_metry),
	XtRString, (caddr_t) NULL},
{XtNalwaysHighlight,XtCAlwaysHighlight,XtRBoolean,
        sizeof(Boolean),XtOffset(XtermWidget, screen.always_highlight),
        XtRBoolean, (caddr_t) &defaultFALSE},
{XtNtekGeometry,XtCGeometry, XtRString, sizeof(char *),
	XtOffset(XtermWidget, misc.T_geometry),
	XtRString, (caddr_t) NULL},
{XtNinternalBorder,XtCBorderWidth,XtRInt, sizeof(int),
	XtOffset(XtermWidget, screen.border),
	XtRInt, (caddr_t) &defaultIntBorder},
{XtNjumpScroll, XtCJumpScroll, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.jumpscroll),
	XtRBoolean, (caddr_t) &defaultTRUE},
{XtNlogFile, XtCLogfile, XtRString, sizeof(char *),
	XtOffset(XtermWidget, screen.logfile),
	XtRString, (caddr_t) NULL},
{XtNlogging, XtCLogging, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, misc.log_on),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNlogInhibit, XtCLogInhibit, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, misc.logInhibit),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNloginShell, XtCLoginShell, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, misc.login_shell),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNmarginBell, XtCMarginBell, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.marginbell),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNpointerColor, XtCForeground, XtRPixel, sizeof(Pixel),
	XtOffset(XtermWidget, screen.mousecolor),
	XtRString, "Black"},
{XtNpointerShape,XtCCursor, XtRCursor, sizeof(Cursor),
	XtOffset(XtermWidget, screen.pointer_cursor),
	XtRString, (caddr_t) "xterm"},
{XtNmultiClickTime,XtCMultiClickTime, XtRInt, sizeof(int),
	XtOffset(XtermWidget, screen.multiClickTime),
	XtRInt, (caddr_t) &defaultMultiClickTime},
{XtNmultiScroll,XtCMultiScroll, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.multiscroll),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNnMarginBell,XtCColumn, XtRInt, sizeof(int),
	XtOffset(XtermWidget, screen.nmarginbell),
	XtRInt, (caddr_t) &defaultNMarginBell},
{XtNreverseVideo,XtCReverseVideo,XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, misc.re_verse),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNreverseWrap,XtCReverseWrap, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, misc.reverseWrap),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNsaveLines, XtCSaveLines, XtRInt, sizeof(int),
	XtOffset(XtermWidget, screen.savelines),
	XtRInt, (caddr_t) &defaultSaveLines},
{XtNscrollBar, XtCScrollBar, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, misc.scrollbar),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNscrollInput,XtCScrollCond, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.scrollinput),
	XtRBoolean, (caddr_t) &defaultTRUE},
{XtNscrollKey, XtCScrollCond, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.scrollkey),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNsignalInhibit,XtCSignalInhibit,XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, misc.signalInhibit),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNtekInhibit, XtCTekInhibit, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, misc.tekInhibit),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNtekStartup, XtCTekStartup, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.TekEmu),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNtiteInhibit, XtCTiteInhibit, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, misc.titeInhibit),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNvisualBell, XtCVisualBell, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.visualbell),
	XtRBoolean, (caddr_t) &defaultFALSE},
{XtNallowSendEvents, XtCAllowSendEvents, XtRBoolean, sizeof(Boolean),
	XtOffset(XtermWidget, screen.allowSendEvents),
	XtRBoolean, (caddr_t) &defaultFALSE}
};


static void VTInitialize(), VTRealize(), VTExpose(), VTConfigure();
static void VTDestroy();

WidgetClassRec xtermClassRec = {
  {
/* core_class fields */	
    /* superclass	  */	(WidgetClass) &widgetClassRec,
    /* class_name	  */	"VT100",
    /* widget_size	  */	sizeof(XtermWidgetRec),
    /* class_initialize   */    NULL,
    /* class_part_initialize */ NULL,
    /* class_inited       */	FALSE,
    /* initialize	  */	VTInitialize,
    /* initialize_hook    */    NULL,				
    /* realize		  */	VTRealize,
    /* actions		  */	actionsList,
    /* num_actions	  */	XtNumber(actionsList),
    /* resources	  */	resources,
    /* num_resources	  */	XtNumber(resources),
    /* xrm_class	  */	NULLQUARK,
    /* compress_motion	  */	TRUE,
    /* compress_exposure  */	FALSE,
    /* compress_enterleave */   TRUE,
    /* visible_interest	  */	FALSE,
    /* destroy		  */	VTDestroy,
    /* resize		  */	VTConfigure,
    /* expose		  */	VTExpose,
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

WidgetClass xtermWidgetClass = (WidgetClass)&xtermClassRec;

VTparse()
{
	register TScreen *screen = &term->screen;
	register int *parsestate = groundtable;
	register int c;
	register char *cp;
	register int row, col, top, bot, scstype;
	extern int bitset(), bitclr(), finput(), TrackMouse();

	if(setjmp(vtjmpbuf))
		parsestate = groundtable;
	for( ; ; )
		switch(parsestate[c = doinput()]) {
		 case CASE_PRINT:
			/* printable characters */
			top = bcnt > TEXT_BUF_SIZE ? TEXT_BUF_SIZE : bcnt;
			cp = bptr;
			*--bptr = c;
			while(top > 0 && isprint(*cp)) {
				top--;
				bcnt--;
				cp++;
			}
			if(screen->curss) {
				dotext(screen, term->flags,
				 screen->gsets[screen->curss], bptr, bptr + 1);
				screen->curss = 0;
				bptr++;
			}
			if(bptr < cp)
				dotext(screen, term->flags,
				 screen->gsets[screen->curgl], bptr, cp);
			bptr = cp;
			break;

		 case CASE_GROUND_STATE:
			/* exit ignore mode */
			parsestate = groundtable;
			break;

		 case CASE_IGNORE_STATE:
			/* Ies: ignore anything else */
			parsestate = igntable;
			break;

		 case CASE_IGNORE_ESC:
			/* Ign: escape */
			parsestate = iestable;
			break;

		 case CASE_IGNORE:
			/* Ignore character */
			break;

		 case CASE_BELL:
			/* bell */
			Bell();
			break;

		 case CASE_BS:
			/* backspace */
			CursorBack(screen, 1);
			break;

		 case CASE_CR:
			/* carriage return */
			CarriageReturn(screen);
			break;

		 case CASE_ESC:
			/* escape */
			parsestate = esctable;
			break;

		 case CASE_VMOT:
			/*
			 * form feed, line feed, vertical tab
			 */
			Index(screen, 1);
			if (term->flags & LINEFEED)
				CarriageReturn(screen);
			if(screen->display->qlen > 0 ||
			 (ioctl(screen->display->fd, FIONREAD, (char *) &arg), arg) > 0)
				xevents();
			break;

		 case CASE_TAB:
			/* tab */
			screen->cur_col = TabNext(term->tabs, screen->cur_col);
			if (screen->cur_col > screen->max_col)
				screen->cur_col = screen->max_col;
			break;

		 case CASE_SI:
			screen->curgl = 0;
			break;

		 case CASE_SO:
			screen->curgl = 1;
			break;

		 case CASE_SCR_STATE:
			/* enter scr state */
			parsestate = scrtable;
			break;

		 case CASE_SCS0_STATE:
			/* enter scs state 0 */
			scstype = 0;
			parsestate = scstable;
			break;

		 case CASE_SCS1_STATE:
			/* enter scs state 1 */
			scstype = 1;
			parsestate = scstable;
			break;

		 case CASE_SCS2_STATE:
			/* enter scs state 2 */
			scstype = 2;
			parsestate = scstable;
			break;

		 case CASE_SCS3_STATE:
			/* enter scs state 3 */
			scstype = 3;
			parsestate = scstable;
			break;

		 case CASE_ESC_IGNORE:
			/* unknown escape sequence */
			parsestate = eigtable;
			break;

		 case CASE_ESC_DIGIT:
			/* digit in csi or dec mode */
			if((row = param[nparam - 1]) == DEFAULT)
				row = 0;
			param[nparam - 1] = 10 * row + (c - '0');
			break;

		 case CASE_ESC_SEMI:
			/* semicolon in csi or dec mode */
			param[nparam++] = DEFAULT;
			break;

		 case CASE_DEC_STATE:
			/* enter dec mode */
			parsestate = dectable;
			break;

		 case CASE_ICH:
			/* ICH */
			if((c = param[0]) < 1)
				c = 1;
			InsertChar(screen, c);
			parsestate = groundtable;
			break;

		 case CASE_CUU:
			/* CUU */
			if((c = param[0]) < 1)
				c = 1;
			CursorUp(screen, c);
			parsestate = groundtable;
			break;

		 case CASE_CUD:
			/* CUD */
			if((c = param[0]) < 1)
				c = 1;
			CursorDown(screen, c);
			parsestate = groundtable;
			break;

		 case CASE_CUF:
			/* CUF */
			if((c = param[0]) < 1)
				c = 1;
			CursorForward(screen, c);
			parsestate = groundtable;
			break;

		 case CASE_CUB:
			/* CUB */
			if((c = param[0]) < 1)
				c = 1;
			CursorBack(screen, c);
			parsestate = groundtable;
			break;

		 case CASE_CUP:
			/* CUP | HVP */
			if((row = param[0]) < 1)
				row = 1;
			if(nparam < 2 || (col = param[1]) < 1)
				col = 1;
			CursorSet(screen, row-1, col-1, term->flags);
			parsestate = groundtable;
			break;

		 case CASE_ED:
			/* ED */
			switch (param[0]) {
			 case DEFAULT:
			 case 0:
				ClearBelow(screen);
				break;

			 case 1:
				ClearAbove(screen);
				break;

			 case 2:
				ClearScreen(screen);
				break;
			}
			parsestate = groundtable;
			break;

		 case CASE_EL:
			/* EL */
			switch (param[0]) {
			 case DEFAULT:
			 case 0:
				ClearRight(screen);
				break;
			 case 1:
				ClearLeft(screen);
				break;
			 case 2:
				ClearLine(screen);
				break;
			}
			parsestate = groundtable;
			break;

		 case CASE_IL:
			/* IL */
			if((c = param[0]) < 1)
				c = 1;
			InsertLine(screen, c);
			parsestate = groundtable;
			break;

		 case CASE_DL:
			/* DL */
			if((c = param[0]) < 1)
				c = 1;
			DeleteLine(screen, c);
			parsestate = groundtable;
			break;

		 case CASE_DCH:
			/* DCH */
			if((c = param[0]) < 1)
				c = 1;
			DeleteChar(screen, c);
			parsestate = groundtable;
			break;

		 case CASE_TRACK_MOUSE:
		 	/* Track mouse as long as in window and between
			   specified rows */
			TrackMouse(param[0], param[2]-1, param[1]-1,
			 param[3]-1, param[4]-2);
			break;

		 case CASE_DECID:
			param[0] = -1;		/* Default ID parameter */
			/* Fall through into ... */
		 case CASE_DA1:
			/* DA1 */
			if (param[0] <= 0) {	/* less than means DEFAULT */
				reply.a_type   = CSI;
				reply.a_pintro = '?';
				reply.a_nparam = 2;
				reply.a_param[0] = 1;		/* VT102 */
				reply.a_param[1] = 2;		/* VT102 */
				reply.a_inters = 0;
				reply.a_final  = 'c';
				unparseseq(&reply, screen->respond);
			}
			parsestate = groundtable;
			break;

		 case CASE_TBC:
			/* TBC */
			if ((c = param[0]) <= 0) /* less than means default */
				TabClear(term->tabs, screen->cur_col);
			else if (c == 3)
				TabZonk(term->tabs);
			parsestate = groundtable;
			break;

		 case CASE_SET:
			/* SET */
			modes(term, bitset);
			parsestate = groundtable;
			break;

		 case CASE_RST:
			/* RST */
			modes(term, bitclr);
			parsestate = groundtable;
			break;

		 case CASE_SGR:
			/* SGR */
			for (c=0; c<nparam; ++c) {
				switch (param[c]) {
				 case DEFAULT:
				 case 0:
					term->flags &= ~(INVERSE|BOLD|UNDERLINE);
					break;
				 case 1:
				 case 5:	/* Blink, really.	*/
					term->flags |= BOLD;
					break;
				 case 4:	/* Underscore		*/
					term->flags |= UNDERLINE;
					break;
				 case 7:
					term->flags |= INVERSE;
				}
			}
			parsestate = groundtable;
			break;

		 case CASE_CPR:
			/* CPR */
			if ((c = param[0]) == 5) {
				reply.a_type = CSI;
				reply.a_pintro = 0;
				reply.a_nparam = 1;
				reply.a_param[0] = 0;
				reply.a_inters = 0;
				reply.a_final  = 'n';
				unparseseq(&reply, screen->respond);
			} else if (c == 6) {
				reply.a_type = CSI;
				reply.a_pintro = 0;
				reply.a_nparam = 2;
				reply.a_param[0] = screen->cur_row+1;
				reply.a_param[1] = screen->cur_col+1;
				reply.a_inters = 0;
				reply.a_final  = 'R';
				unparseseq(&reply, screen->respond);
			}
			parsestate = groundtable;
			break;

		 case CASE_DECSTBM:
			/* DECSTBM */
			if((top = param[0]) < 1)
				top = 1;
			if(nparam < 2 || (bot = param[1]) == DEFAULT
			   || bot > screen->max_row + 1
			   || bot == 0)
				bot = screen->max_row+1;
			if (bot > top) {
				if(screen->scroll_amt)
					FlushScroll(screen);
				screen->top_marg = top-1;
				screen->bot_marg = bot-1;
				CursorSet(screen, 0, 0, term->flags);
			}
			parsestate = groundtable;
			break;

		 case CASE_DECREQTPARM:
			/* DECREQTPARM */
			if ((c = param[0]) == DEFAULT)
				c = 0;
			if (c == 0 || c == 1) {
				reply.a_type = CSI;
				reply.a_pintro = 0;
				reply.a_nparam = 7;
				reply.a_param[0] = c + 2;
				reply.a_param[1] = 1;	/* no parity */
				reply.a_param[2] = 1;	/* eight bits */
				reply.a_param[3] = 112;	/* transmit 9600 baud */
				reply.a_param[4] = 112;	/* receive 9600 baud */
				reply.a_param[5] = 1;	/* clock multiplier ? */
				reply.a_param[6] = 0;	/* STP flags ? */
				reply.a_inters = 0;
				reply.a_final  = 'x';
				unparseseq(&reply, screen->respond);
			}
			parsestate = groundtable;
			break;

		 case CASE_DECSET:
			/* DECSET */
			dpmodes(term, bitset);
			parsestate = groundtable;
			if(screen->TekEmu)
				return;
			break;

		 case CASE_DECRST:
			/* DECRST */
			dpmodes(term, bitclr);
			parsestate = groundtable;
			break;

		 case CASE_DECALN:
			/* DECALN */
			if(screen->cursor_state)
				HideCursor();
			for(row = screen->max_row ; row >= 0 ; row--) {
				bzero(screen->buf[2 * row + 1],
				 col = screen->max_col + 1);
				for(cp = screen->buf[2 * row] ; col > 0 ; col--)
					*cp++ = 'E';
			}
			ScrnRefresh(screen, 0, 0, screen->max_row + 1,
			 screen->max_col + 1, False);
			parsestate = groundtable;
			break;

		 case CASE_GSETS:
			screen->gsets[scstype] = c;
			parsestate = groundtable;
			break;

		 case CASE_DECSC:
			/* DECSC */
			CursorSave(term, &screen->sc);
			parsestate = groundtable;
			break;

		 case CASE_DECRC:
			/* DECRC */
			CursorRestore(term, &screen->sc);
			parsestate = groundtable;
			break;

		 case CASE_DECKPAM:
			/* DECKPAM */
			term->keyboard.flags |= KYPD_APL;
			parsestate = groundtable;
			break;

		 case CASE_DECKPNM:
			/* DECKPNM */
			term->keyboard.flags &= ~KYPD_APL;
			parsestate = groundtable;
			break;

		 case CASE_IND:
			/* IND */
			Index(screen, 1);
			if(screen->display->qlen > 0 ||
			 (ioctl(screen->display->fd, FIONREAD, (char *)&arg), arg) > 0)
				xevents();
			parsestate = groundtable;
			break;

		 case CASE_NEL:
			/* NEL */
			Index(screen, 1);
			CarriageReturn(screen);
			if(screen->display->qlen > 0 ||
			 (ioctl(screen->display->fd, FIONREAD, (char *)&arg), arg) > 0)
				xevents();
			parsestate = groundtable;
			break;

		 case CASE_HTS:
			/* HTS */
			TabSet(term->tabs, screen->cur_col);
			parsestate = groundtable;
			break;

		 case CASE_RI:
			/* RI */
			RevIndex(screen, 1);
			parsestate = groundtable;
			break;

		 case CASE_SS2:
			/* SS2 */
			screen->curss = 2;
			parsestate = groundtable;
			break;

		 case CASE_SS3:
			/* SS3 */
			screen->curss = 3;
			parsestate = groundtable;
			break;

		 case CASE_CSI_STATE:
			/* enter csi state */
			nparam = 1;
			param[0] = DEFAULT;
			parsestate = csitable;
			break;

		 case CASE_OSC:
			/* do osc escapes */
			do_osc(finput);
			parsestate = groundtable;
			break;

		 case CASE_RIS:
			/* RIS */
			VTReset(TRUE);
			parsestate = groundtable;
			break;

		 case CASE_LS2:
			/* LS2 */
			screen->curgl = 2;
			parsestate = groundtable;
			break;

		 case CASE_LS3:
			/* LS3 */
			screen->curgl = 3;
			parsestate = groundtable;
			break;

		 case CASE_LS3R:
			/* LS3R */
			screen->curgr = 3;
			parsestate = groundtable;
			break;

		 case CASE_LS2R:
			/* LS2R */
			screen->curgr = 2;
			parsestate = groundtable;
			break;

		 case CASE_LS1R:
			/* LS1R */
			screen->curgr = 1;
			parsestate = groundtable;
			break;

		 case CASE_XTERM_SAVE:
			savemodes(term);
			parsestate = groundtable;
			break;

		 case CASE_XTERM_RESTORE:
			restoremodes(term);
			parsestate = groundtable;
			break;
		}
}

finput()
{
	return(doinput());
}

static int select_mask;
static int write_mask;

static char v_buffer[4096];
static char *v_bufstr;
static char *v_bufptr;
static char *v_bufend;
#define	ptymask()	(v_bufptr > v_bufstr ? pty_mask : 0)

v_write(f, d, l)
int f;
char *d;
int l;
{
	int r;
	int c = l;

	if (!v_bufstr) {
		v_bufstr = v_buffer;
		v_bufptr = v_buffer;
		v_bufend = &v_buffer[4096];
	}


	if ((1 << f) != pty_mask)
		return(write(f, d, l));

	if (v_bufptr > v_bufstr) {
		if (l) {
			if (v_bufend > v_bufptr + l) {
				Bcopy(d, v_bufptr, l);
				v_bufptr += l;
			} else {
				if (v_bufstr != v_buffer) {
					Bcopy(v_bufstr, v_buffer,
					      v_bufptr - v_bufstr);
					v_bufptr -= v_bufstr - v_buffer;
					v_bufstr = v_buffer;
				}
				if (v_bufend > v_bufptr + l) {
					Bcopy(d, v_bufptr, l);
					v_bufptr += l;
				} else if (v_bufptr < v_bufend) {
					fprintf(stderr, "Out of buffer space\n");
					c = v_bufend - v_bufptr;
					Bcopy(d, v_bufptr, c);
					v_bufptr = v_bufend;
				} else {
					fprintf(stderr, "Out of buffer space\n");
					c = 0;
				}
			}
		}
		if (v_bufptr > v_bufstr) {
			if ((r = write(f, v_bufstr, v_bufptr - v_bufstr)) <= 0)
				return(r);
			if ((v_bufstr += r) >= v_bufptr)
				v_bufstr = v_bufptr = v_buffer;
		}
	} else if (l) {
		if ((r = write(f, d, l)) < 0) {
			if (errno == EWOULDBLOCK)
				r = 0;
			else if (errno == EINTR)
				r = 0;
			else
				return(r);
		}
		if (l - r) {
			if (l - r > v_bufend - v_buffer) {
				fprintf(stderr, "Truncating to %d\n",
						v_bufend - v_buffer);
				l = (v_bufend - v_buffer) + r;
			}
			Bcopy(d + r, v_buffer, l - r);
			v_bufstr = v_buffer;
			v_bufptr = v_buffer + (l - r);
		}
	}
	return(c);
}

in_put()
{
	register TScreen *screen = &term->screen;
	register char *cp;
	register int i;
	static struct timeval trackTimeOut;

	select_mask = pty_mask;	/* force initial read */
	for( ; ; ) {
		if((select_mask & pty_mask) && (eventMode == NORMAL)) {
			if(screen->logging)
				FlushLog(screen);
			if((bcnt = read(screen->respond, bptr = buffer,
			 BUF_SIZE)) < 0) {
				if(errno == EIO)
					Cleanup (0);
				else if(errno != EWOULDBLOCK)
					Panic(
				 "input: read returned unexpected error (%d)\n",
					 errno);
			} else if(bcnt == 0)
				Panic("input: read returned zero\n", 0);
			else {
				/* strip parity bit */
				for(i = bcnt, cp = bptr ; i > 0 ; i--)
					*cp++ &= CHAR;
				if(screen->scrollWidget && screen->scrollinput &&
				 screen->topline < 0)
					/* Scroll to bottom */
					WindowScroll(screen, 0);
				break;
			}
		}
		if(screen->scroll_amt)
			FlushScroll(screen);
		if(screen->cursor_set && (screen->cursor_col != screen->cur_col
		 || screen->cursor_row != screen->cur_row)) {
			if(screen->cursor_state)
				HideCursor();
			ShowCursor();
		} else if(screen->cursor_set != screen->cursor_state) {
			if(screen->cursor_set)
				ShowCursor();
			else
				HideCursor();
		}
		
	if (waitingForTrackInfo) {
			trackTimeOut.tv_sec = TRACKTIMESEC;
			trackTimeOut.tv_usec = TRACKTIMEUSEC;
			select_mask = pty_mask;
			if ((i = select(max_plus1, &select_mask, (int *)NULL, (int *)NULL,
			 &trackTimeOut)) < 0) {
			 	if (errno != EINTR)
					SysError(ERROR_SELECT);
				continue;
			} else if (i == 0) {
				/* emacs just isn't replying, go on */
				waitingForTrackInfo = 0;
				Bell();
				select_mask = Select_mask;
			}
		} else if (QLength(screen->display))
			select_mask = X_mask;
		else {
			write_mask = ptymask();
			XFlush(screen->display);
			select_mask = Select_mask;
			if (eventMode != NORMAL)
				select_mask = X_mask;
			if(select(max_plus1, &select_mask, &write_mask, 
				(int *)NULL, (struct timeval *) NULL) < 0){
				if (errno != EINTR)
					SysError(ERROR_SELECT);
				continue;
			} 
		}
		if (write_mask & ptymask())
			v_write(screen->respond, 0, 0);	/* flush buffer */
		if(select_mask & X_mask) {
			if (bcnt <= 0) {
				bcnt = 0;
				bptr = buffer;
			}
			xevents();
			if (bcnt > 0)
				break;
		}
	}
	bcnt--;
	return(*bptr++);
}

/*
 * process a string of characters according to the character set indicated
 * by charset.  worry about end of line conditions (wraparound if selected).
 */
dotext(screen, flags, charset, buf, ptr)
register TScreen	*screen;
unsigned	flags;
char		charset;
char	*buf;
char	*ptr;
{
	register char	*s;
	register int	len;
	register int	n;
	register int	next_col;

	switch (charset) {
	case 'A':	/* United Kingdom set			*/
		for (s=buf; s<ptr; ++s)
			if (*s == '#')
				*s = '\036';	/* UK pound sign*/
		break;

	case 'B':	/* ASCII set				*/
		break;

	case '0':	/* special graphics (line drawing)	*/
		for (s=buf; s<ptr; ++s)
			if (*s>=0x5f && *s<=0x7e)
				*s = *s == 0x5f ? 0x7f : *s - 0x5f;
		break;

	default:	/* any character sets we don't recognize*/
		return;
	}

	len = ptr - buf; 
	ptr = buf;
	while (len > 0) {
		n = screen->max_col-screen->cur_col+1;
		if (n <= 1) {
			if (screen->do_wrap && (flags&WRAPAROUND)) {
				Index(screen, 1);
				screen->cur_col = 0;
				screen->do_wrap = 0;
				n = screen->max_col+1;
			} else
				n = 1;
		}
		if (len < n)
			n = len;
		next_col = screen->cur_col + n;
		WriteText(screen, ptr, n, flags);
		/*
		 * the call to WriteText updates screen->cur_col.
		 * If screen->cur_col != next_col, we must have
		 * hit the right margin, so set the do_wrap flag.
		 */
		screen->do_wrap = (screen->cur_col < next_col);
		len -= n;
		ptr += n;
	}
}
 
/*
 * write a string str of length len onto the screen at
 * the current cursor position.  update cursor position.
 */
WriteText(screen, str, len, flags)
register TScreen	*screen;
register char	*str;
register int	len;
unsigned	flags;
{
	register int cx, cy;
	register unsigned fgs = flags;
	GC	currentGC;
 
   if(screen->cur_row - screen->topline <= screen->max_row) {
	/*
	if(screen->cur_row == screen->cursor_row && screen->cur_col <=
	 screen->cursor_col && screen->cursor_col <= screen->cur_col + len - 1)
		screen->cursor_state = OFF;
	 */
	if(screen->cursor_state)
		HideCursor();

	/*
	 *	make sure that the correct GC is current
	 */

	if (fgs & BOLD)
		if (fgs & INVERSE)
			currentGC = screen->reverseboldGC;
		else	currentGC = screen->normalboldGC;
	else  /* not bold */
		if (fgs & INVERSE)
			currentGC = screen->reverseGC;
		else	currentGC = screen->normalGC;

	if (fgs & INSERT)
		InsertChar(screen, len);
      if (!(AddToRefresh(screen))) {
		if(screen->scroll_amt)
			FlushScroll(screen);
	cx = CursorX(screen, screen->cur_col);
	cy = CursorY(screen, screen->cur_row)+screen->fnt_norm->ascent;
 	XDrawImageString(screen->display, TextWindow(screen), currentGC,
			cx, cy, str, len);

	if((fgs & BOLD) && screen->enbolden) 
		if (currentGC == screen->normalGC || screen->reverseGC)
			XDrawString(screen->display, TextWindow(screen),
			      	currentGC,cx + 1, cy, str, len);

	if(fgs & UNDERLINE) 
		XDrawLine(screen->display, TextWindow(screen), currentGC,
			cx, cy+1,
			cx + len * FontWidth(screen), cy+1);
	/*
	 * the following statements compile data to compute the average 
	 * number of characters written on each call to XText.  The data
	 * may be examined via the use of a "hidden" escape sequence.
	 */
	ctotal += len;
	++ntotal;
      }
    }
	ScreenWrite(screen, str, flags, len);
	CursorForward(screen, len);
}
 
/*
 * process ANSI modes set, reset
 */
modes(term, func)
XtermWidget	term;
int		(*func)();
{
	register int	i;

	for (i=0; i<nparam; ++i) {
		switch (param[i]) {
		case 4:			/* IRM				*/
			(*func)(&term->flags, INSERT);
			break;

		case 20:		/* LNM				*/
			(*func)(&term->flags, LINEFEED);
			break;
		}
	}
}

/*
 * process DEC private modes set, reset
 */
dpmodes(term, func)
XtermWidget	term;
int		(*func)();
{
	register TScreen	*screen	= &term->screen;
	register int	i, j;
	extern int bitset();

	for (i=0; i<nparam; ++i) {
		switch (param[i]) {
		case 1:			/* DECCKM			*/
			(*func)(&term->keyboard.flags, CURSOR_APL);
			break;
		case 2:			/* ANSI/VT52 mode		*/
			if (func == bitset) {
				screen->gsets[0] =
					screen->gsets[1] =
					screen->gsets[2] =
					screen->gsets[3] = 'B';
				screen->curgl = 0;
				screen->curgr = 2;
			}
			break;
		case 3:			/* DECCOLM			*/
			if(screen->c132) {
				ClearScreen(screen);
				CursorSet(screen, 0, 0, term->flags);
				if((j = func == bitset ? 132 : 80) !=
				 ((term->flags & IN132COLUMNS) ? 132 : 80) ||
				 j != screen->max_col + 1) {
				        Dimension replyWidth, replyHeight;
					XtGeometryResult status;

					status = XtMakeResizeRequest (
					    (Widget) term, 
					    (Dimension) FontWidth(screen) * j
					        + 2*screen->border
						+ screen->scrollbar,
					    (Dimension) FontHeight(screen)
						* (screen->max_row + 1)
						+ 2 * screen->border,
					    &replyWidth, &replyHeight);

					if (status == XtGeometryYes ||
					    status == XtGeometryDone) {
					    ScreenResize (&term->screen,
							  replyWidth,
							  replyHeight,
							  &term->flags);
					}
				}
				(*func)(&term->flags, IN132COLUMNS);
			}
			break;
		case 4:			/* DECSCLM (slow scroll)	*/
			if (func == bitset) {
				screen->jumpscroll = 0;
				if (screen->scroll_amt)
					FlushScroll(screen);
			} else
				screen->jumpscroll = 1;
			(*func)(&term->flags, SMOOTHSCROLL);
			break;
		case 5:			/* DECSCNM			*/
			j = term->flags;
			(*func)(&term->flags, REVERSE_VIDEO);
			if ((term->flags ^ j) & REVERSE_VIDEO)
				ReverseVideo(term);
			break;

		case 6:			/* DECOM			*/
			(*func)(&term->flags, ORIGIN);
			CursorSet(screen, 0, 0, term->flags);
			break;

		case 7:			/* DECAWM			*/
			(*func)(&term->flags, WRAPAROUND);
			break;
		case 8:			/* DECARM			*/
#ifdef DO_AUTOREPEAT
			j = term->flags;
			(*func)(&term->flags, AUTOREPEAT);
			if ((term->flags ^ j) & AUTOREPEAT)
				if(term->flags & AUTOREPEAT)
					XAutoRepeatOn(screen->display);
				else
					XAutoRepeatOff(screen->display);
#endif /* DO_AUTOREPEAT */
			break;
		case 9:			/* MIT bogus sequence		*/
			if(func == bitset)
				screen->send_mouse_pos = 1;
			else
				screen->send_mouse_pos = 0;
			break;
		case 38:		/* DECTEK			*/
			if(func == bitset & !(screen->inhibit & I_TEK)) {
				if(screen->logging) {
					FlushLog(screen);
					screen->logstart = Tbuffer;
				}
				screen->TekEmu = TRUE;
			}
			break;
		case 40:		/* 132 column mode		*/
			(*func)(&screen->c132, 1);
			break;
		case 41:		/* curses hack			*/
			(*func)(&screen->curses, 1);
			break;
		case 44:		/* margin bell			*/
			(*func)(&screen->marginbell, 1);
			if(!screen->marginbell)
				screen->bellarmed = -1;
			break;
		case 45:		/* reverse wraparound	*/
			(*func)(&term->flags, REVERSEWRAP);
			break;
		case 46:		/* logging		*/
			if(func == bitset)
				StartLog(screen);
			else
				CloseLog(screen);
			break;
		case 47:		/* alternate buffer		*/
			if(func == bitset)
				ToAlternate(screen);
			else
				FromAlternate(screen);
			break;
		case 1000:		/* xtem bogus sequence		*/
			if(func == bitset)
				screen->send_mouse_pos = 2;
			else
				screen->send_mouse_pos = 0;
			break;
		case 1001:		/* xtem sequence w/hilite tracking */
			if(func == bitset)
				screen->send_mouse_pos = 3;
			else
				screen->send_mouse_pos = 0;
			break;
		}
	}
}

/*
 * process xterm private modes save
 */
savemodes(term)
XtermWidget term;
{
	register TScreen	*screen	= &term->screen;
	register int i;

	for (i = 0; i < nparam; i++) {
		switch (param[i]) {
		case 1:			/* DECCKM			*/
			screen->save_modes[0] = term->keyboard.flags &
			 CURSOR_APL;
			break;
		case 3:			/* DECCOLM			*/
			if(screen->c132)
				screen->save_modes[1] = term->flags &
				 IN132COLUMNS;
			break;
		case 4:			/* DECSCLM (slow scroll)	*/
			screen->save_modes[2] = term->flags & SMOOTHSCROLL;
			break;
		case 5:			/* DECSCNM			*/
			screen->save_modes[3] = term->flags & REVERSE_VIDEO;
			break;
		case 6:			/* DECOM			*/
			screen->save_modes[4] = term->flags & ORIGIN;
			break;

		case 7:			/* DECAWM			*/
			screen->save_modes[5] = term->flags & WRAPAROUND;
			break;
		case 8:			/* DECARM			*/
#ifdef DO_AUTOREPEAT
			screen->save_modes[6] = term->flags & AUTOREPEAT;
#endif /* DO_AUTOREPEAT */
			break;
		case 9:			/* mouse bogus sequence */
			screen->save_modes[7] = screen->send_mouse_pos;
			break;
		case 40:		/* 132 column mode		*/
			screen->save_modes[8] = screen->c132;
			break;
		case 41:		/* curses hack			*/
			screen->save_modes[9] = screen->curses;
			break;
		case 44:		/* margin bell			*/
			screen->save_modes[12] = screen->marginbell;
			break;
		case 45:		/* reverse wraparound	*/
			screen->save_modes[13] = term->flags & REVERSEWRAP;
			break;
		case 46:		/* logging		*/
			screen->save_modes[14] = screen->logging;
			break;
		case 47:		/* alternate buffer		*/
			screen->save_modes[15] = screen->alternate;
			break;
		case 1000:		/* mouse bogus sequence		*/
		case 1001:
			screen->save_modes[7] = screen->send_mouse_pos;
			break;
		}
	}
}

/*
 * process xterm private modes restore
 */
restoremodes(term)
XtermWidget term;
{
	register TScreen	*screen	= &term->screen;
	register int i, j;

	for (i = 0; i < nparam; i++) {
		switch (param[i]) {
		case 1:			/* DECCKM			*/
			term->keyboard.flags &= ~CURSOR_APL;
			term->keyboard.flags |= screen->save_modes[0] &
			 CURSOR_APL;
			break;
		case 3:			/* DECCOLM			*/
			if(screen->c132) {
				ClearScreen(screen);
				CursorSet(screen, 0, 0, term->flags);
				if((j = (screen->save_modes[1] & IN132COLUMNS)
				 ? 132 : 80) != ((term->flags & IN132COLUMNS)
				 ? 132 : 80) || j != screen->max_col + 1) {
				        Dimension replyWidth, replyHeight;
					XtGeometryResult status;
					status = XtMakeResizeRequest (
					    (Widget) term,
					    (Dimension) FontWidth(screen) * j 
						+ 2*screen->border
						+ screen->scrollbar,
					    (Dimension) FontHeight(screen)
						* (screen->max_row + 1)
						+ 2*screen->border,
					    &replyWidth, &replyHeight);

					if (status == XtGeometryYes ||
					    status == XtGeometryDone) {
					    ScreenResize (&term->screen,
							  replyWidth,
							  replyHeight,
							  &term->flags);
					}
				}
				term->flags &= ~IN132COLUMNS;
				term->flags |= screen->save_modes[1] &
				 IN132COLUMNS;
			}
			break;
		case 4:			/* DECSCLM (slow scroll)	*/
			if (screen->save_modes[2] & SMOOTHSCROLL) {
				screen->jumpscroll = 0;
				if (screen->scroll_amt)
					FlushScroll(screen);
			} else
				screen->jumpscroll = 1;
			term->flags &= ~SMOOTHSCROLL;
			term->flags |= screen->save_modes[2] & SMOOTHSCROLL;
			break;
		case 5:			/* DECSCNM			*/
			if((screen->save_modes[3] ^ term->flags) &
			 REVERSE_VIDEO) {
				term->flags &= ~REVERSE_VIDEO;
				term->flags |= screen->save_modes[3] &
				 REVERSE_VIDEO;
				ReverseVideo(term);
			}
			break;
		case 6:			/* DECOM			*/
			term->flags &= ~ORIGIN;
			term->flags |= screen->save_modes[4] & ORIGIN;
			CursorSet(screen, 0, 0, term->flags);
			break;

		case 7:			/* DECAWM			*/
			term->flags &= ~WRAPAROUND;
			term->flags |= screen->save_modes[5] & WRAPAROUND;
			break;
		case 8:			/* DECARM			*/
#ifdef DO_AUTOREPEAT
			if((screen->save_modes[6] ^ term->flags) & AUTOREPEAT) {
				term->flags &= ~REVERSE_VIDEO;
				term->flags |= screen->save_modes[6] &
				 REVERSE_VIDEO;
				if(term->flags & AUTOREPEAT)
					XAutoRepeatOn(screen->display);
				else
					XAutoRepeatOff(screen->display);
			}
#endif /* DO_AUTOREPEAT */
			break;
		case 9:			/* MIT bogus sequence		*/
			screen->send_mouse_pos = screen->save_modes[7];
			break;
		case 40:		/* 132 column mode		*/
			screen->c132 = screen->save_modes[8];
			break;
		case 41:		/* curses hack			*/
			screen->curses = screen->save_modes[9];
			break;
		case 44:		/* margin bell			*/
			if(!(screen->marginbell = screen->save_modes[12]))
				screen->bellarmed = -1;
			break;
		case 45:		/* reverse wraparound	*/
			term->flags &= ~REVERSEWRAP;
			term->flags |= screen->save_modes[13] & REVERSEWRAP;
			break;
		case 46:		/* logging		*/
			if(screen->save_modes[14])
				StartLog(screen);
			else
				CloseLog(screen);
			break;
		case 47:		/* alternate buffer		*/
			if(screen->save_modes[15])
				ToAlternate(screen);
			else
				FromAlternate(screen);
			break;
		case 1000:		/* mouse bogus sequence		*/
		case 1001:
			screen->send_mouse_pos = screen->save_modes[7];
			break;
		}
	}
}

/*
 * set a bit in a word given a pointer to the word and a mask.
 */
bitset(p, mask)
int	*p;
{
	*p |= mask;
}

/*
 * clear a bit in a word given a pointer to the word and a mask.
 */
bitclr(p, mask)
int	*p;
{
	*p &= ~mask;
}

unparseseq(ap, fd)
register ANSI	*ap;
{
	register int	c;
	register int	i;
	register int	inters;

	c = ap->a_type;
	if (c>=0x80 && c<=0x9F) {
		unparseputc(ESC, fd);
		c -= 0x40;
	}
	unparseputc(c, fd);
	c = ap->a_type;
	if (c==ESC || c==DCS || c==CSI || c==OSC || c==PM || c==APC) {
		if (ap->a_pintro != 0)
			unparseputc((char) ap->a_pintro, fd);
		for (i=0; i<ap->a_nparam; ++i) {
			if (i != 0)
				unparseputc(';', fd);
			unparseputn((unsigned int) ap->a_param[i], fd);
		}
		inters = ap->a_inters;
		for (i=3; i>=0; --i) {
			c = (inters >> (8*i)) & 0xff;
			if (c != 0)
				unparseputc(c, fd);
		}
		unparseputc((char) ap->a_final, fd);
	}
}

unparseputn(n, fd)
unsigned int	n;
int fd;
{
	unsigned int	q;

	q = n/10;
	if (q != 0)
		unparseputn(q, fd);
	unparseputc((char) ('0' + (n%10)), fd);
}

unparseputc(c, fd)
char c;
int fd;
{
	char	buf[2];
	register i = 1;
	extern XtermWidget term;

	if((buf[0] = c) == '\r' && (term->flags & LINEFEED)) {
		buf[1] = '\n';
		i++;
	}
	if (write(fd, buf, i) != i)
		Panic("unparseputc: error writing character\n", 0);
}

ToAlternate(screen)
register TScreen *screen;
{
	extern ScrnBuf Allocate();

	if(screen->alternate)
		return;
	if(!screen->altbuf)
		screen->altbuf = Allocate(screen->max_row + 1, screen->max_col
		 + 1);
	SwitchBufs(screen);
	screen->alternate = TRUE;
}

FromAlternate(screen)
register TScreen *screen;
{
	if(!screen->alternate)
		return;
	screen->alternate = FALSE;
	SwitchBufs(screen);
}

SwitchBufs(screen)
register TScreen *screen;
{
	register int rows, top;
	char *save [2 * MAX_ROWS];

	if(screen->cursor_state)
		HideCursor();
	rows = screen->max_row + 1;
	Bcopy((char *)screen->buf, (char *)save, 2 * sizeof(char *) * rows);
	Bcopy((char *)screen->altbuf, (char *)screen->buf, 2 * sizeof(char *) *
	 rows);
	Bcopy((char *)save, (char *)screen->altbuf, 2 * sizeof(char *) * rows);

	if((top = -screen->topline) <= screen->max_row) {
		if(screen->scroll_amt)
			FlushScroll(screen);
		if(top == 0)
			XClearWindow(screen->display, TextWindow(screen));
		else
			XClearArea(
			    screen->display,
			    TextWindow(screen),
			    (int) screen->border + screen->scrollbar,
			    (int) top * FontHeight(screen) + screen->border,
			    (unsigned) Width(screen),
			    (unsigned) (screen->max_row - top + 1)
				* FontHeight(screen),
			    FALSE);
	}
	ScrnRefresh(screen, 0, 0, rows, screen->max_col + 1, False);
}

VTRun()
{
	register TScreen *screen = &term->screen;
	register int i;
	
	if (!screen->Vshow) {
	    XtRealizeWidget (term->core.parent);
	    set_vt_visibility (TRUE);
	} 

	if (screen->allbuf == NULL) VTallocbuf ();

	screen->cursor_state = OFF;
	screen->cursor_set = ON;
	if(screen->select || screen->always_highlight)
		VTSelect();
	if (L_flag > 0) {
		XWarpPointer (screen->display, None, VWindow(screen),
			    0, 0, 0, 0,
			    FullWidth(screen) >> 1, FullHeight(screen) >>1);
		L_flag = -1;
	}
	bcnt = 0;
	bptr = buffer;
	while(Tpushb > Tpushback) {
		*bptr++ = *--Tpushb;
		bcnt++;
	}
	bcnt += (i = Tbcnt);
	for( ; i > 0 ; i--)
		*bptr++ = *Tbptr++;
	bptr = buffer;
	if(!setjmp(VTend))
		VTparse();
	HideCursor();
	screen->cursor_set = OFF;
	if (!screen->always_highlight)
	    VTUnselect ();
	reselectwindow (screen);
}

/*ARGSUSED*/
static void VTExpose(w, event, region)
Widget w;
XEvent *event;
Region region;
{
	register TScreen *screen = &term->screen;

#ifdef DEBUG
	if(debug)
		fputs("Expose\n", stderr);
#endif	/* DEBUG */
	if (event->type == Expose)
		HandleExposure (screen, (XExposeEvent *)event);
}

static void VTGraphicsOrNoExpose (event)
XEvent *event;
    {
	register TScreen *screen = &term->screen;
	if (screen->incopy <= 0) {
		screen->incopy = 1;
		if (screen->scrolls > 0)
			screen->scrolls--;
	}
	if (event->type == GraphicsExpose)
	  if (HandleExposure (screen, (XExposeEvent *)event))
		screen->cursor_state = OFF;
	if ((event->type == NoExpose) || ((XGraphicsExposeEvent *)event)->count == 0) {
		if (screen->incopy <= 0 && screen->scrolls > 0)
			screen->scrolls--;
		if (screen->scrolls)
			screen->incopy = -1;
		else
			screen->incopy = 0;
	}
}

/*ARGSUSED*/
static void VTNonMaskableEvent (w, closure, event)
Widget w;
caddr_t closure;
XEvent *event;
    {
    switch (event->type) {
       case MappingNotify:
	  XRefreshKeyboardMapping (event);
	  break;
       case GraphicsExpose:
       case NoExpose:
	  VTGraphicsOrNoExpose (event);
	  break;
	  }
    }




static void VTConfigure(w)
Widget w;
{
       if (XtIsRealized(w))
          ScreenResize (&term->screen, term->core.width, term->core.height, &term->flags);
}

static Boolean failed = FALSE;

int VTInit ()
{
    register TScreen *screen = &term->screen;

    if (failed) return (0);
    XtRealizeWidget (term->core.parent);
    if (screen->allbuf == NULL) VTallocbuf ();
    return (1);
}

static void VTallocbuf ()
{
    register TScreen *screen = &term->screen;
    int nrows = screen->max_row + 1;

    /* allocate screen buffer now, if necessary. */
    if (screen->scrollWidget)
      nrows += screen->savelines;
    screen->allbuf = (ScrnBuf) Allocate (nrows, screen->max_col + 1);
    if (screen->scrollWidget)
      screen->buf = &screen->allbuf[2 * screen->savelines];
    else
      screen->buf = screen->allbuf;
    return;
}

static void VTInitialize (request, new)
   XtermWidget request, new;
{
   /* Zero out the entire "screen" component of "new" widget,
      then do field-by-field assigment of "screen" fields
      that are named in the resource list. */

   bzero ((char *) &new->screen, sizeof(new->screen));
   new->screen.c132 = request->screen.c132;
   new->screen.curses = request->screen.curses;
   new->screen.foreground = request->screen.foreground;
   new->screen.cursorcolor = request->screen.cursorcolor;
   new->screen.border = request->screen.border;
   new->screen.jumpscroll = request->screen.jumpscroll;
   new->screen.logfile = request->screen.logfile;
   new->screen.marginbell = request->screen.marginbell;
   new->screen.mousecolor = request->screen.mousecolor;
   new->screen.multiscroll = request->screen.multiscroll;
   new->screen.nmarginbell = request->screen.nmarginbell;
   new->screen.savelines = request->screen.savelines;
   new->screen.scrollinput = request->screen.scrollinput;
   new->screen.scrollkey = request->screen.scrollkey;
   new->screen.visualbell = request->screen.visualbell;
   new->screen.TekEmu = request->screen.TekEmu;
   new->misc.re_verse = request->misc.re_verse;
   new->screen.multiClickTime = request->screen.multiClickTime;
   new->screen.charClass = request->screen.charClass;
   new->screen.cutNewline = request->screen.cutNewline;
   new->screen.cutToBeginningOfLine = request->screen.cutToBeginningOfLine;
   new->screen.always_highlight = request->screen.always_highlight;
   new->screen.pointer_cursor = request->screen.pointer_cursor;
   new->misc.titeInhibit = request->misc.titeInhibit;

    /*
     * set the colors if reverse video; this is somewhat tricky since
     * there are 5 colors:
     *
     *     background - paper		white
     *     foreground - text		black
     *     border - border			black (foreground)
     *     textcursor - block		black (foreground)
     *     mousecursor - mouse		black (foreground)
     *
     */
    if (new->misc.re_verse) {
	unsigned long fg = new->screen.foreground;
	unsigned long bg = new->core.background_pixel;

	if (new->screen.mousecolor == fg) new->screen.mousecolor = bg;
	if (new->screen.cursorcolor == fg) new->screen.cursorcolor = bg;
	if (new->core.border_pixel == fg) new->core.border_pixel = bg;
	new->screen.foreground = bg;
	new->core.background_pixel = fg;
    }	

   new->keyboard.flags = 0;
   new->screen.display = new->core.screen->display;
   new->core.height = new->core.width = 1;
      /* dummy values so that we don't try to Realize the parent shell 
	 with height or width of 0, which is illegal in X.  The real
	 size is computed in the xtermWidget's Realize proc,
	 but the shell's Realize proc is called first, and must see
	 a valid size. */

   /* look for focus related events on the shell, because we need
    * to care about the shell's border being part of our focus.
    */
   XtAddEventHandler(XtParent(new), EnterWindowMask, FALSE,
		HandleEnterWindow, (Opaque)NULL);
   XtAddEventHandler(XtParent(new), LeaveWindowMask, FALSE,
		HandleLeaveWindow, (Opaque)NULL);
   XtAddEventHandler(XtParent(new), FocusChangeMask, FALSE,
		HandleFocusChange, (Opaque)NULL);
   XtAddEventHandler(new, 0L, TRUE,
		VTNonMaskableEvent, (Opaque)NULL);

   set_character_class (new->screen.charClass);

   /* create it, but don't realize it */
   ScrollBarOn (new, TRUE, FALSE);

   return;
}


static void VTDestroy (w)
Widget w;
{
    XtFree(((XtermWidget)w)->screen.selection);
}

/*ARGSUSED*/
static void VTRealize (w, valuemask, values)
Widget w;
XtValueMask *valuemask;
XSetWindowAttributes *values;
{
	unsigned int width, height;
	register TScreen *screen = &term->screen;
	register int i, j;
	XPoint	*vp;
	static short failed;
	int xpos, ypos, pr;
	extern char *malloc();
	XGCValues		xgcv;
	XtGCMask			mask;
	XSizeHints		sizehints;
	XWMHints		wmhints;
	extern int		VTgcFontMask;
	int scrollbar_width;

	if(failed)
		return;
	
	TabReset (term->tabs);

	screen->fnt_norm = screen->fnt_bold = NULL;
	
	/* do the XFont calls */

	if ((screen->fnt_norm = XLoadQueryFont(screen->display, term->misc.f_n)) == NULL) {
	    fprintf(stderr, "%s: Could not open font %s; using server default\n", 
		    xterm_name, term->misc.f_n);
	    screen->fnt_norm =
		  XQueryFont(screen->display,
			     DefaultGC(screen->display,
				       DefaultScreen(screen->display)
				       )->gid
			     );
	    VTgcFontMask = 0;
	}

	if (!term->misc.f_b || !VTgcFontMask
	    || !(screen->fnt_bold = XLoadQueryFont(screen->display, term->misc.f_b))) {
		screen->fnt_bold = screen->fnt_norm;
		screen->enbolden = TRUE;  /*no bold font */
	}

	/* find the max width and higth of the font */

	screen->fullVwin.f_width = screen->fnt_norm->max_bounds.width;
	screen->fullVwin.f_height = screen->fnt_norm->ascent +
				    screen->fnt_norm->descent;

	/* making cursor */
	{
	    unsigned long fg, bg;

	    bg = term->core.background_pixel;
	    if (screen->mousecolor == term->core.background_pixel) {
		fg = screen->foreground;
	    } else {
		fg = screen->mousecolor;
	    }

	    if (!screen->pointer_cursor) 
	    	screen->pointer_cursor = make_xterm (fg, bg);
	    else 
	        recolor_cursor (screen->pointer_cursor, fg, bg);
	}

	scrollbar_width = (term->misc.scrollbar ? 
			   screen->scrollWidget->core.width : 0);
	i = 2 * screen->border + scrollbar_width;
	j = 2 * screen->border;


	/* set defaults */
	xpos = 1; ypos = 1; width = 80; height = 24;

	pr = XParseGeometry(term->misc.geo_metry, &xpos, &ypos, &width, &height);

	screen->max_col = width;
	screen->max_row = height;
	width = width * screen->fullVwin.f_width + i;
	height = height * screen->fullVwin.f_height + j;

	if ((pr & XValue) && (XNegative&pr)) 
	  xpos += DisplayWidth(screen->display, DefaultScreen(screen->display)) 
			- width - (term->core.parent->core.border_width * 2);
	if ((pr & YValue) && (YNegative&pr))
	  ypos += DisplayHeight(screen->display, DefaultScreen(screen->display)) 
			- height - (term->core.parent->core.border_width * 2);

	/* set up size hints for window manager */
	sizehints.min_width = 2 * screen->border + scrollbar_width;
	sizehints.min_height = 2 * screen->border;
	sizehints.width_inc = FontWidth(screen);
	sizehints.height_inc = FontHeight(screen);
	sizehints.flags = PMinSize|PResizeInc;
	sizehints.x = xpos;
	sizehints.y = ypos;
	if ((XValue&pr) || (YValue&pr)) 
	  sizehints.flags |= USSize|USPosition;
	else sizehints.flags |= PSize|PPosition;
	sizehints.width = width;
	sizehints.height = height;
	if ((WidthValue&pr) || (HeightValue&pr)) 
	  sizehints.flags |= USSize;
	else sizehints.flags |= PSize;

	(void) XtMakeResizeRequest((Widget) term,
				   (Dimension)width, (Dimension)height,
				   &term->core.width, &term->core.height);

	/* XXX This is bogus.  We are parsing geometries too late.  This
	 * is information that the shell widget ought to have before we get
	 * realized, so that it can do the right thing.
	 */
        if (sizehints.flags & USPosition)
	    XMoveWindow (XtDisplay(term), term->core.parent->core.window,
			 sizehints.x, sizehints.y);

	XSetNormalHints(XtDisplay(term), term->core.parent->core.window,
		&sizehints);

        screen->fullVwin.fullwidth = width;
        screen->fullVwin.fullheight = height;
        screen->fullVwin.width = width - i;
        screen->fullVwin.height = height - j;

	if (term->misc.re_verse && (term->core.border_pixel == term->core.background_pixel))
		values->border_pixel = term->core.border_pixel = term->screen.foreground;
	
	values->bit_gravity = NorthWestGravity;
	term->screen.fullVwin.window = term->core.window =
	  XCreateWindow(XtDisplay(term), XtWindow(term->core.parent),
		term->core.x, term->core.y,
		term->core.width, term->core.height, term->core.border_width,
		(int) term->core.depth,
		InputOutput, CopyFromParent,	
		*valuemask|CWBitGravity, values);

	/* do the GC stuff */

	mask = VTgcFontMask | GCForeground | GCBackground 
	  	| GCGraphicsExposures | GCFunction;

	xgcv.graphics_exposures = TRUE;	/* default */
	xgcv.function = GXcopy;
	xgcv.font = screen->fnt_norm->fid;
	xgcv.foreground = screen->foreground;
	xgcv.background = term->core.background_pixel;

	screen->normalGC = XtGetGC((Widget)term, mask, &xgcv);

	if (screen->enbolden) { /* there is no bold font */
		xgcv.font = screen->fnt_norm->fid;
		screen->normalboldGC = screen->normalGC;
	} else {
		xgcv.font = screen->fnt_bold->fid;
		screen->normalboldGC = XtGetGC((Widget)term, mask, &xgcv);
	}

	xgcv.font = screen->fnt_norm->fid;
	xgcv.foreground = term->core.background_pixel;
	xgcv.background = screen->foreground;

	screen->reverseGC = XtGetGC((Widget)term, mask, &xgcv);

	if (screen->enbolden) /* there is no bold font */
		xgcv.font = screen->fnt_norm->fid;
	else
		xgcv.font = screen->fnt_bold->fid;

	screen->reverseboldGC = XtGetGC((Widget)term, mask, &xgcv);

	/* we also need a set of caret (called a cursor here) gc's */

	xgcv.font = screen->fnt_norm->fid;

	/*
	 * Let's see, there are three things that have "color":
	 *
	 *     background
	 *     text
	 *     cursorblock
	 *
	 * And, there are four situation when drawing a cursor, if we decide
	 * that we like have a solid block of cursor color with the letter
	 * that it is highlighting shown in the background color to make it
	 * stand out:
	 *
	 *     selected window, normal video - background on cursor
	 *     selected window, reverse video - foreground on cursor
	 *     unselected window, normal video - foreground on background
	 *     unselected window, reverse video - background on foreground
	 *
	 * Since the last two are really just normalGC and reverseGC, we only
	 * need two new GC's.  Under monochrome, we get the same effect as
	 * above by setting cursor color to foreground.
	 */

	{
	    unsigned long cc = screen->cursorcolor;
	    unsigned long fg = screen->foreground;
	    unsigned long bg = term->core.background_pixel;

	    if (cc != fg && cc != bg) {
		/* we have a colored cursor */
		xgcv.foreground = fg;
		xgcv.background = cc;
		screen->cursorGC = XtGetGC ((Widget) term, mask, &xgcv);

		if (screen->always_highlight) {
		    screen->reversecursorGC = (GC) 0;
		    screen->cursoroutlineGC = (GC) 0;
		} else {
		    xgcv.foreground = bg;
		    xgcv.background = cc;
		    screen->reversecursorGC = XtGetGC ((Widget) term, mask, &xgcv);
		    xgcv.foreground = cc;
		    xgcv.background = bg;
		    screen->cursoroutlineGC = XtGetGC ((Widget) term, mask, &xgcv);
		}
	    } else {
		screen->cursorGC = (GC) 0;
		screen->reversecursorGC = (GC) 0;
		screen->cursoroutlineGC = (GC) 0;
	    }
	}

	/* Reset variables used by ANSI emulation. */

	screen->gsets[0] = 'B';			/* ASCII_G		*/
	screen->gsets[1] = 'B';
	screen->gsets[2] = 'B';			/* DEC supplemental.	*/
	screen->gsets[3] = 'B';
	screen->curgl = 0;			/* G0 => GL.		*/
	screen->curgr = 2;			/* G2 => GR.		*/
	screen->curss = 0;			/* No single shift.	*/

	XDefineCursor(screen->display, VShellWindow, screen->pointer_cursor);

        screen->cur_col = screen->cur_row = 0;
	screen->max_col = Width(screen)  / screen->fullVwin.f_width - 1;
	screen->top_marg = 0;
	screen->bot_marg = screen->max_row = Height(screen) /
				screen->fullVwin.f_height - 1;

	screen->sc.row = screen->sc.col = screen->sc.flags = NULL;

	/* Mark screen buffer as unallocated.  We wait until the run loop so
	   that the child process does not fork and exec with all the dynamic
	   memory it will never use.  If we were to do it here, the
	   swap space for new process would be huge for huge savelines. */
	if (!tekWidget)			/* if not called after fork */
	  screen->buf = screen->allbuf = NULL;

	screen->do_wrap = NULL;
	screen->scrolls = screen->incopy = 0;
/*	free((char *)fInfo);	*/
	vp = &VTbox[1];
	(vp++)->x = FontWidth(screen) - 1;
	(vp++)->y = FontHeight(screen) - 1;
	(vp++)->x = -(FontWidth(screen) - 1);
	vp->y = -(FontHeight(screen) - 1);
	screen->box = VTbox;

	screen->savedlines = 0;

	if (term->misc.scrollbar) {
		screen->scrollbar = 0;
		ScrollBarOn (term, FALSE, TRUE);
	}
	CursorSave (term, &screen->sc);
	VTUnselect();
	return;
}

/*
 * Shows cursor at new cursor position in screen.
 */
ShowCursor()
{
	register TScreen *screen = &term->screen;
	register int x, y, flags;
	char c;
	GC	currentGC;
	Boolean	in_selection;

	if (eventMode != NORMAL) return;

	if (screen->cur_row - screen->topline > screen->max_row)
		return;
	c = screen->buf[y = 2 * (screen->cursor_row = screen->cur_row)]
	 [x = screen->cursor_col = screen->cur_col];
	flags = screen->buf[y + 1][x];
	if (c == 0)
		c = ' ';

	if (screen->cur_row > screen->endHRow ||
	    (screen->cur_row == screen->endHRow &&
	     screen->cur_col >= screen->endHCol) ||
	    screen->cur_row < screen->startHRow ||
	    (screen->cur_row == screen->startHRow &&
	     screen->cur_col < screen->startHCol))
	    in_selection = False;
	else
	    in_selection = True;

	if(screen->select || screen->always_highlight) {
		if (( (flags & INVERSE) && !in_selection) ||
		    (!(flags & INVERSE) &&  in_selection)){
		    /* text is reverse video */
		    if (screen->cursorGC) {
			currentGC = screen->cursorGC;
		    } else {
			if (flags & BOLD) {
				currentGC = screen->normalboldGC;
			} else {
				currentGC = screen->normalGC;
			}
		    }
		} else { /* normal video */
		    if (screen->reversecursorGC) {
			currentGC = screen->reversecursorGC;
		    } else {
			if (flags & BOLD) {
				currentGC = screen->reverseboldGC;
			} else {
				currentGC = screen->reverseGC;
			}
		    }
		}
	} else { /* not selected */
		if (( (flags & INVERSE) && !in_selection) ||
		    (!(flags & INVERSE) &&  in_selection)) {
		    /* text is reverse video */
			currentGC = screen->reverseGC;
		} else { /* normal video */
			currentGC = screen->normalGC;
		}
	    
	}

	x = CursorX (screen, screen->cur_col);
	y = CursorY(screen, screen->cur_row) + 
	  screen->fnt_norm->ascent;
	XDrawImageString(screen->display, TextWindow(screen), currentGC,
		x, y, &c, 1);

	if((flags & BOLD) && screen->enbolden) /* no bold font */
		XDrawString(screen->display, TextWindow(screen), currentGC,
			x + 1, y, &c, 1);
	if(flags & UNDERLINE) 
		XDrawLine(screen->display, TextWindow(screen), currentGC,
			x, y+1, x + FontWidth(screen), y+1);
	if (!screen->select && !screen->always_highlight) {
		screen->box->x = x;
		screen->box->y = y - screen->fnt_norm->ascent;
		XDrawLines (screen->display, TextWindow(screen), 
			    screen->cursoroutlineGC ? screen->cursoroutlineGC 
			    			    : currentGC,
			    screen->box, NBOX, CoordModePrevious);
	}
	screen->cursor_state = ON;
}

/*
 * hide cursor at previous cursor position in screen.
 */
HideCursor()
{
	register TScreen *screen = &term->screen;
	GC	currentGC;
	register int x, y, flags;
	char c;
	Boolean	in_selection;

	if(screen->cursor_row - screen->topline > screen->max_row)
		return;
	c = screen->buf[y = 2 * screen->cursor_row][x = screen->cursor_col];
	flags = screen->buf[y + 1][x];

	if (screen->cursor_row > screen->endHRow ||
	    (screen->cursor_row == screen->endHRow &&
	     screen->cursor_col >= screen->endHCol) ||
	    screen->cursor_row < screen->startHRow ||
	    (screen->cursor_row == screen->startHRow &&
	     screen->cursor_col < screen->startHCol))
	    in_selection = False;
	else
	    in_selection = True;

	if (( (flags & INVERSE) && !in_selection) ||
	    (!(flags & INVERSE) &&  in_selection)) {
		if(flags & BOLD) {
			currentGC = screen->reverseboldGC;
		} else {
			currentGC = screen->reverseGC;
		}
	} else {
		if(flags & BOLD) {
			currentGC = screen->normalboldGC;
		} else {
			currentGC = screen->normalGC;
		}
	}

	if (c == 0)
		c = ' ';
	x = CursorX (screen, screen->cursor_col);
	y = (((screen->cursor_row - screen->topline) * FontHeight(screen))) +
	 screen->border;
	y = y+screen->fnt_norm->ascent;
	XDrawImageString(screen->display, TextWindow(screen), currentGC,
		x, y, &c, 1);
	if((flags & BOLD) && screen->enbolden)
		XDrawString(screen->display, TextWindow(screen), currentGC,
			x + 1, y, &c, 1);
	if(flags & UNDERLINE) 
		XDrawLine(screen->display, TextWindow(screen), currentGC,
			x, y+1, x + FontWidth(screen), y+1);
	screen->cursor_state = OFF;
}

VTSelect()
{
	register TScreen *screen = &term->screen;

	if (VShellWindow)
	  XSetWindowBorder (screen->display, VShellWindow,
			    term->core.border_pixel);
}

VTUnselect()
{
	register TScreen *screen = &term->screen;

	if (VShellWindow)
	  XSetWindowBorderPixmap (screen->display, VShellWindow,
				  screen->graybordertile);
}

VTReset(full)
int full;
{
	register TScreen *screen = &term->screen;

	/* reset scrolling region */
	screen->top_marg = 0;
	screen->bot_marg = screen->max_row;
	term->flags &= ~ORIGIN;
	if(full) {
		TabReset (term->tabs);
		term->keyboard.flags = NULL;
		screen->gsets[0] = 'B';
		screen->gsets[1] = 'B';
		screen->gsets[2] = 'B';
		screen->gsets[3] = 'B';
		screen->curgl = 0;
		screen->curgr = 2;
		screen->curss = 0;
		ClearScreen(screen);
		screen->cursor_state = OFF;
		if (term->flags & REVERSE_VIDEO)
			ReverseVideo(term);

		term->flags = term->initflags;
		if(screen->c132 && (term->flags & IN132COLUMNS)) {
		        Dimension junk;
			XtMakeResizeRequest(
			    (Widget) term,
			    (Dimension) 80*FontWidth(screen)
				+ 2 * screen->border + screen->scrollbar,
			    (Dimension) FontHeight(screen)
			        * (screen->max_row + 1) + 2 * screen->border,
			    &junk, &junk);
			XSync(screen->display, FALSE);	/* synchronize */
			if(QLength(screen->display) > 0)
				xevents();
		}
		CursorSet(screen, 0, 0, term->flags);
	}
	longjmp(vtjmpbuf, 1);	/* force ground state in parser */
}


#ifdef MODEMENU
#define	MMENU_SCROLLBAR		0
#define	MMENU_SCROLL		(MMENU_SCROLLBAR+1)
#define	MMENU_VIDEO		(MMENU_SCROLL+1)
#define	MMENU_WRAP		(MMENU_VIDEO+1)
#define	MMENU_REVERSEWRAP	(MMENU_WRAP+1)
#define	MMENU_NLM		(MMENU_REVERSEWRAP+1)
#define	MMENU_CURSOR		(MMENU_NLM+1)
#define	MMENU_PAD		(MMENU_CURSOR+1)
#ifdef DO_AUTOREPEAT
#define	MMENU_REPEAT		(MMENU_PAD+1)
#define	MMENU_SCROLLKEY		(MMENU_REPEAT+1)
#else /* else not DO_AUTOREPEAT */
#define MMENU_SCROLLKEY		(MMENU_PAD+1)
#endif /* DO_AUTOREPEAT */
#define	MMENU_SCROLLINPUT	(MMENU_SCROLLKEY+1)
#define	MMENU_C132		(MMENU_SCROLLINPUT+1)
#define	MMENU_CURSES		(MMENU_C132+1)
#define	MMENU_MARGBELL		(MMENU_CURSES+1)
#define	MMENU_TEKWIN		(MMENU_MARGBELL+1)
#define	MMENU_ALTERN		(MMENU_TEKWIN+1)
#define	MMENU_LINE		(MMENU_ALTERN+1)
#define	MMENU_RESET		(MMENU_LINE+1)
#define	MMENU_FULLRESET		(MMENU_RESET+1)
#define	MMENU_TEKMODE		(MMENU_FULLRESET+1)
#define	MMENU_HIDEVT		(MMENU_TEKMODE+1)

static char *vtext[] = {
	"Scrollbar",
	"Jump Scroll",
	"Reverse Video",
	"Auto Wraparound",
	"Reverse Wraparound",
	"Auto Linefeed",
	"Application Cursor Mode",
	"Application Keypad Mode",
#ifdef DO_AUTOREPEAT
	"Auto Repeat",
#endif /* DO_AUTOREPEAT */
	"Scroll to bottom on key press",
	"Scroll to bottom on tty output",
	"Allow 80/132 switching",
	"Curses Emulation",
	"Margin Bell",
	"Tek Window Showing",
	"Alternate Screen",
	"-",
	"Soft Reset",
	"Full Reset",
	"Select Tek Mode",
	"Hide VT Window",
	0,
};


static int menutermflags;
static int menukbdflags;
static int t132;
static int taltern;
static int tcurses;
static int tmarginbell;
static int tscrollbar;
static int tscrollkey;
static int tscrollinput;
static Boolean tshow;

Menu *setupmenu(menu)
register Menu **menu;
{
	register TScreen *screen = &term->screen;
	register char **cp;
	register int flags = term->flags;
	register int kflags = term->keyboard.flags;

	if (*menu == NULL) {
		if ((*menu = NewMenu("Modes")) == NULL)
			return(NULL);
		for(cp = vtext ; *cp ; cp++)
			AddMenuItem(*menu, *cp);
		if(!(flags & SMOOTHSCROLL))
			CheckItem(*menu, MMENU_SCROLL);
		if(flags & REVERSE_VIDEO)
			CheckItem(*menu, MMENU_VIDEO);
		if(flags & WRAPAROUND)
			CheckItem(*menu, MMENU_WRAP);
		if(flags & REVERSEWRAP)
			CheckItem(*menu, MMENU_REVERSEWRAP);
		if(flags & LINEFEED)
			CheckItem(*menu, MMENU_NLM);
		if(kflags & CURSOR_APL)
			CheckItem(*menu, MMENU_CURSOR);
		if(kflags & KYPD_APL)
			CheckItem(*menu, MMENU_PAD);
#ifdef DO_AUTOREPEAT
		if(flags & AUTOREPEAT)
			CheckItem(*menu, MMENU_REPEAT);
#endif /* DO_AUTOREPEAT */
		if(tscrollbar = (screen->scrollbar > 0)) {
			CheckItem(*menu, MMENU_SCROLLBAR);
			if(tscrollkey = screen->scrollkey)
				CheckItem(*menu, MMENU_SCROLLKEY);
			if(tscrollinput = screen->scrollinput)
				CheckItem(*menu, MMENU_SCROLLINPUT);
		} else {
			tscrollkey = FALSE;
			DisableItem(*menu, MMENU_SCROLLKEY);
			tscrollinput = FALSE;
			DisableItem(*menu, MMENU_SCROLLINPUT);
		}
		if(t132 = screen->c132)
			CheckItem(*menu, MMENU_C132);
		if(tcurses = screen->curses)
			CheckItem(*menu, MMENU_CURSES);
		if(tmarginbell = screen->marginbell)
			CheckItem(*menu, MMENU_MARGBELL);
		if(tshow = screen->Tshow)
			CheckItem(*menu, MMENU_TEKWIN);
		else
			DisableItem(*menu, MMENU_HIDEVT);
		DisableItem(*menu, MMENU_ALTERN);
		if(taltern = screen->alternate) {
			CheckItem(*menu, MMENU_ALTERN);
		}
		DisableItem(*menu, MMENU_LINE);
		if(screen->inhibit & I_TEK) {
			DisableItem(*menu, MMENU_TEKWIN);
			DisableItem(*menu, MMENU_TEKMODE);
		}
		menutermflags = flags;
		menukbdflags = kflags;
		return(*menu);
	}
	menutermflags ^= flags;
	menukbdflags ^= kflags;
	if (menutermflags & SMOOTHSCROLL)
		SetItemCheck(*menu, MMENU_SCROLL, !(flags & SMOOTHSCROLL));
	if (menutermflags & REVERSE_VIDEO)
		SetItemCheck(*menu, MMENU_VIDEO, flags & REVERSE_VIDEO);
	if (menutermflags & WRAPAROUND)
		SetItemCheck(*menu, MMENU_WRAP, flags & WRAPAROUND);
	if (menutermflags & REVERSEWRAP)
		SetItemCheck(*menu, MMENU_REVERSEWRAP, flags & REVERSEWRAP);
	if (menutermflags & LINEFEED)
		SetItemCheck(*menu, MMENU_NLM, flags & LINEFEED);
	if (menukbdflags & CURSOR_APL)
		SetItemCheck(*menu, MMENU_CURSOR, kflags & CURSOR_APL);
	if (menukbdflags & KYPD_APL)
		SetItemCheck(*menu, MMENU_PAD, kflags & KYPD_APL);
#ifdef DO_AUTOREPEAT
        if (menutermflags & AUTOREPEAT)
                SetItemCheck(*menu, MMENU_REPEAT, flags & AUTOREPEAT);
#endif /* DO_AUTOREPEAT */
	if(tscrollbar != (screen->scrollbar > 0)) {
		SetItemCheck(*menu, MMENU_SCROLLBAR, (tscrollbar =
		 (screen->scrollbar > 0)));
		SetItemDisable(*menu, MMENU_SCROLLKEY, !tscrollbar);
		SetItemCheck(*menu, MMENU_SCROLLKEY,
		 tscrollkey = (tscrollbar && screen->scrollkey));
		SetItemDisable(*menu, MMENU_SCROLLINPUT, !tscrollbar);
		SetItemCheck(*menu, MMENU_SCROLLINPUT,
		 tscrollinput = (tscrollbar && screen->scrollinput));
	} else if (tscrollbar) {
		if (tscrollkey != screen->scrollkey)
			SetItemCheck(*menu, MMENU_SCROLLKEY,
			 tscrollkey = screen->scrollkey);
		if (tscrollinput != screen->scrollinput)
			SetItemCheck(*menu, MMENU_SCROLLINPUT,
			 tscrollinput = screen->scrollinput);
	}
	if(t132 != screen->c132)
		SetItemCheck(*menu, MMENU_C132, (t132 = screen->c132));
	if(tcurses != screen->curses)
		SetItemCheck(*menu, MMENU_CURSES, (tcurses = screen->curses));
	if(tmarginbell != screen->marginbell)
		SetItemCheck(*menu, MMENU_MARGBELL, (tmarginbell =
		screen->marginbell));
	if(tshow != screen->Tshow) {
		SetItemCheck(*menu, MMENU_TEKWIN, (tshow = screen->Tshow));
		SetItemDisable(*menu, MMENU_HIDEVT, !tshow);
	}
	if(taltern != screen->alternate) {
		SetItemCheck(*menu, MMENU_ALTERN, (taltern =
		 screen->alternate));
	}
	menutermflags = flags;
	menukbdflags = kflags;
	return(*menu);
}

domenufunc(item)
int item;
{
	register TScreen *screen = &term->screen;

	switch (item) {
	case MMENU_SCROLL:
		term->flags ^= SMOOTHSCROLL;
		if (term->flags & SMOOTHSCROLL) {
			screen->jumpscroll = FALSE;
			if (screen->scroll_amt)
				FlushScroll(screen);
		} else
			screen->jumpscroll = TRUE;
		break;

	case MMENU_VIDEO:
		ReverseVideo(term);
		break;

	case MMENU_WRAP:
		term->flags ^= WRAPAROUND;
		break;

	case MMENU_REVERSEWRAP:
		term->flags ^= REVERSEWRAP;
		break;

	case MMENU_NLM:
		term->flags ^= LINEFEED;
		break;

	case MMENU_CURSOR:
		term->keyboard.flags ^= CURSOR_APL;
		break;

	case MMENU_PAD:
		term->keyboard.flags ^= KYPD_APL;
		break;

#ifdef DO_AUTOREPEAT
	case MMENU_REPEAT:
		term->flags ^= AUTOREPEAT;
		if (term->flags & AUTOREPEAT)
			XAutoRepeatOn(screen->display);
		else
			XAutoRepeatOff(screen->display);
		break;
#endif /* DO_AUTOREPEAT */

	case MMENU_SCROLLBAR:
		if(screen->scrollbar)
			ScrollBarOff(screen);
		else
			ScrollBarOn (term, FALSE, FALSE);
		break;

	case MMENU_SCROLLKEY:
		screen->scrollkey = !screen->scrollkey;
		break;

	case MMENU_SCROLLINPUT:
		screen->scrollinput = !screen->scrollinput;
		break;

		
	case MMENU_C132:
		screen->c132 = !screen->c132;
		break;

	case MMENU_MARGBELL:
		if(!(screen->marginbell = !screen->marginbell))
			screen->bellarmed = -1;
		break;

	case MMENU_CURSES:
		screen->curses = !screen->curses;
		break;

	case MMENU_FULLRESET:
		VTReset(TRUE);
		break;

	case MMENU_RESET:
		VTReset(FALSE);
		break;

	case MMENU_HIDEVT:
		set_vt_visibility (FALSE);
		reselectwindow(screen);
		/* drop through */

	case MMENU_TEKMODE:
		end_vt_mode ();
		break;

	case MMENU_TEKWIN:
		if (!screen->Tshow) {
		    set_tek_visibility (TRUE);
		} else {
		    set_tek_visibility (FALSE);
		    end_tek_mode ();
		}
		reselectwindow(screen);
		break;
	}
}
#endif	/* MODEMENU */

/*
 * set_character_class - takes a string of the form
 * 
 *                 low[-high]:val[,low[-high]:val[...]]
 * 
 * and sets the indicated ranges to the indicated values.
 */

int set_character_class (s)
    register char *s;
{
    register int i;			/* iterator, index into s */
    int len;				/* length of s */
    int acc;				/* accumulator */
    int low, high;			/* bounds of range [0..127] */
    int base;				/* 8, 10, 16 (octal, decimal, hex) */
    int numbers;			/* count of numbers per range */
    int digits;				/* count of digits in a number */
    static char *errfmt = "%s:  %s in range string \"%s\" (position %d)\n";
    extern char *ProgramName;

    if (!s || !s[0]) return;

    base = 10;				/* in case we ever add octal, hex */
    low = high = -1;			/* out of range */

    for (i = 0, len = strlen (s), acc = 0, numbers = digits = 0;
	 i < len; i++) {
	char c = s[i];

	if (isspace(c)) {
	    continue;
	} else if (isdigit(c)) {
	    acc = acc * base + (c - '0');
	    digits++;
	    continue;
	} else if (c == '-') {
	    low = acc;
	    acc = 0;
	    if (digits == 0) {
		fprintf (stderr, errfmt, ProgramName, "missing number", s, i);
		return (-1);
	    }
	    digits = 0;
	    numbers++;
	    continue;
	} else if (c == ':') {
	    if (numbers == 0)
	      low = acc;
	    else if (numbers == 1)
	      high = acc;
	    else {
		fprintf (stderr, errfmt, ProgramName, "too many numbers",
			 s, i);
		return (-1);
	    }
	    digits = 0;
	    numbers++;
	    acc = 0;
	    continue;
	} else if (c == ',') {
	    /*
	     * now, process it
	     */

	    if (high < 0) {
		high = low;
		numbers++;
	    }
	    if (numbers != 2) {
		fprintf (stderr, errfmt, ProgramName, "bad value number", 
			 s, i);
	    } else if (SetCharacterClassRange (low, high, acc) != 0) {
		fprintf (stderr, errfmt, ProgramName, "bad range", s, i);
	    }

	    low = high = -1;
	    acc = 0;
	    digits = 0;
	    numbers = 0;
	    continue;
	} else {
	    fprintf (stderr, errfmt, ProgramName, "bad character", s, i);
	    return (-1);
	}				/* end if else if ... else */

    }

    if (low < 0 && high < 0) return (0);

    /*
     * now, process it
     */

    if (high < 0) high = low;
    if (numbers < 1 || numbers > 2) {
	fprintf (stderr, errfmt, ProgramName, "bad value number", s, i);
    } else if (SetCharacterClassRange (low, high, acc) != 0) {
	fprintf (stderr, errfmt, ProgramName, "bad range", s, i);
    }

    return (0);
}

/* ARGSUSED */
static void HandleKeymapChange(w, event, params, param_count)
    Widget w;
    XEvent *event;
    String *params;
    Cardinal *param_count;
{
    static XtTranslations keymap, original;
    static XtResource resources[] = {
	{ XtNtranslations, XtCTranslations, XtRTranslationTable,
	      sizeof(XtTranslations), 0, XtRTranslationTable, (caddr_t)NULL}
    };
    char mapName[1000];
    char mapClass[1000];

    if (*param_count != 1) return;

    if (original == NULL) original = w->core.tm.translations;

    if (strcmp(params[0], "None") == 0) {
	XtOverrideTranslations(w, original);
	return;
    }
    (void) sprintf( mapName, "%sKeymap", params[0] );
    (void) strcpy( mapClass, mapName );
    if (islower(mapClass[0])) mapClass[0] = toupper(mapClass[0]);
    XtGetSubresources( w, &keymap, mapName, mapClass,
		       resources, (Cardinal)1, NULL, (Cardinal)0 );
    if (keymap != NULL)
	XtOverrideTranslations(w, keymap);
}


/* ARGSUSED */
static void HandleBell(w, event, params, param_count)
    Widget w;
    XEvent *event;		/* unused */
    String *params;		/* [0] = volume */
    Cardinal *param_count;	/* 0 or 1 */
{
    int percent = (*param_count) ? atoi(params[0]) : 0;

    XBell( XtDisplay(w), percent );
}


/* ARGSUSED */
static void HandleIgnore(w, event, params, param_count)
    Widget w;
    XEvent *event;		/* unused */
    String *params;		/* unused */
    Cardinal *param_count;	/* unused */
{
    /* do nothing, but check for funny escape sequences */
    (void) SendMousePosition(w, event);
}
