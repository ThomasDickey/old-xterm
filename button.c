/*
 *	@Header: button.c,v 1.1 88/02/10 13:08:02 jim Exp @
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

/*
button.c	Handles button events in the terminal emulator.
		does cut/paste operations, change modes via menu,
		passes button events through to some applications.
				J. Gettys.
*/
#ifndef lint
static char rcs_id[] = "@Header: button.c,v 1.1 88/02/10 13:08:02 jim Exp @";
#endif	/* lint */
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include <X11/Intrinsic.h>
#include "ptyx.h"
#include "data.h"
#include "error.h"
#ifdef MODEMENU
#include "menu.h"
#endif	/* MODEMENU */

extern char *malloc();

#define KeyState(x) (((x) & (ShiftMask|ControlMask)) + (((x) & Mod1Mask) ? 2 : 0))
    /* adds together the bits:
        shift key -> 1
        meta key  -> 2
        control key -> 4 */
  
#define TEXTMODES 4
#define NBUTS 3
#define DIRS 2
#define UP 1
#define DOWN 0
#define SHIFTS 8		/* three keys, so eight combinations */
#define	Coordinate(r,c)		((r) * (term->screen.max_col+1) + (c))

char *SaveText();
extern UnSaltText();
extern StartCut();
extern StartExtend();
extern EditorButton();
extern TrackDown();

extern ModeMenu();
extern char *xterm_name;
extern Bogus(), Silence();
extern GINbutton();

/* due to LK201 limitations, not all of the below are actually possible */
static int (*textfunc[TEXTMODES][SHIFTS][DIRS][NBUTS])() = {
/*	left		middle		right	*/
	StartCut,	Silence,	StartExtend,	/* down |	  */
	Silence,	UnSaltText,	Silence,	/* up	|no shift */

	StartCut,	Silence,	StartExtend,	/* down |	  */
	Silence,	UnSaltText,	Silence,	/* up	|shift	  */

	Bogus,		Bogus,		Bogus,		/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|meta	  */

	Bogus,		Bogus,		Bogus,		/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|meta shift */

	ModeMenu,	ModeMenu,	ModeMenu,	/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|control  */

	ModeMenu,	ModeMenu,	ModeMenu,	/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|ctl shift */

	Bogus,		Bogus,		Bogus,		/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|ctl meta */

	Bogus,		Bogus,		Bogus,		/* down	| control  */
	Silence,	Silence,	Silence,	/* up	|meta shift*/

/* MIT mouse bogus sequence 			*/
/* 	button, shift keys, and direction 	*/
/*	left		middle		right	*/
	EditorButton,	EditorButton,	EditorButton,	/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|no shift */

	StartCut,	Silence,	StartExtend,	/* down |	  */
	Silence,	UnSaltText,	Silence,	/* up	|shift	  */

	Bogus,		Bogus,		Bogus,		/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|meta	  */

	Bogus,		Bogus,		Bogus,		/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|meta shift */

	ModeMenu,	ModeMenu,	ModeMenu,	/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|control  */

	ModeMenu,	ModeMenu,	ModeMenu,	/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|ctl shift */

	Bogus,		Bogus,		Bogus,		/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|ctl meta */

	Bogus,		Bogus,		Bogus,		/* down	| control  */
	Silence,	Silence,	Silence,	/* up	|meta shift*/

/* DEC mouse bogus sequence 			*/
/* 	button, shift keys, and direction 	*/
/*	left		middle		right	*/
	EditorButton,	EditorButton,	EditorButton,	/* down	|	  */
	EditorButton,	EditorButton,	EditorButton,	/* up	|no shift */

	StartCut,	Silence,	StartExtend,	/* down |	  */
	Silence,	UnSaltText,	Silence,	/* up	|shift	  */

	Bogus,		Bogus,		Bogus,		/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|meta	  */

	Bogus,		Bogus,		Bogus,		/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|meta shift */

	EditorButton,	EditorButton,	EditorButton,	/* down	|	  */
	EditorButton,	EditorButton,	EditorButton,	/* up	|control  */

	ModeMenu,	ModeMenu,	ModeMenu,	/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|ctl shift */

	Bogus,		Bogus,		Bogus,		/* down	|	  */
	Silence,	Silence,	Silence,	/* up	|ctl meta */

	Bogus,		Bogus,		Bogus,		/* down	| control  */
	Silence,	Silence,	Silence,	/* up	|meta shift*/

/* Hilite tracking DEC mouse bogus sequence 	*/
/* 	button, shift keys, and direction 	*/
/*	left		middle		right	*/
	TrackDown,	EditorButton,	EditorButton,	/* down	|	    */
	EditorButton,	EditorButton,	EditorButton,	/* up	|no shift   */

	StartCut,	Silence,	StartExtend,	/* down |	    */
	Silence,	UnSaltText,	Silence,	/* up	|shift	    */

	Bogus,		Bogus,		Bogus,		/* down	|	    */
	Silence,	Silence,	Silence,	/* up	|meta	    */

	Bogus,		Bogus,		Bogus,		/* down	|	    */
	Silence,	Silence,	Silence,	/* up	|meta shift */

	EditorButton,	EditorButton,	EditorButton,	/* down	|	    */
	EditorButton,	EditorButton,	EditorButton,	/* up	|control    */

	ModeMenu,	ModeMenu,	ModeMenu,	/* down	|	    */
	Silence,	Silence,	Silence,	/* up	|ctl shift  */

	Bogus,		Bogus,		Bogus,		/* down	|	    */
	Silence,	Silence,	Silence,	/* up	|ctl meta   */

	Bogus,		Bogus,		Bogus,		/* down	| control   */
	Silence,	Silence,	Silence		/* up	|meta shift */

};

	/* button and shift keys for Tek mode */
static int (*Tbfunc[SHIFTS][NBUTS])() = {
/*	left		middle		right	*/
	GINbutton,	GINbutton,	GINbutton,	/* down	|no shift   */

	GINbutton,	GINbutton,	GINbutton,	/* down |shift	    */

	Bogus,		Bogus,		Bogus,		/* down	|meta	    */

	Bogus,		Bogus,		Bogus,		/* down	|meta shift */

	ModeMenu,	ModeMenu,	ModeMenu,	/* down	|control    */

	ModeMenu,	ModeMenu,	ModeMenu,	/* down	|ctl shift  */

	Bogus,		Bogus,		Bogus,		/* down	|ctl meta   */

	Bogus,		Bogus,		Bogus,		/* down	| all       */

};	/* button and shift keys */

extern XtermWidget term;

/* Selection/extension variables */

/* Raw char position where the selection started */
static int rawRow, rawCol;

/* Hilited area */
static int startHRow, startHCol, endHRow, endHCol, startHCoord, endHCoord = 0;

/* Selected area before CHAR, WORD, LINE selectUnit processing */
static int startRRow, startRCol, endRRow, endRCol = 0;

/* Selected area after CHAR, WORD, LINE selectUnit processing */
static int startSRow, startSCol, endSRow, endSCol = 0;

/* Valid rows for selection clipping */
static int firstValidRow, lastValidRow;

/* Start, end of extension */
static int startERow, startECol, endERow, endECol;

/* Saved values of raw selection for extend to restore to */
static int saveStartRRow, saveStartRCol, saveEndRRow, saveEndRCol;

/* Multi-click handling */
static int numberOfClicks = 0;
static long int lastButtonUpTime = 0;
typedef enum {SELECTCHAR, SELECTWORD, SELECTLINE} SelectUnit;
static SelectUnit selectUnit;

/* Send emacs escape code when done selecting or extending? */
static int replyToEmacs;


/*ARGSUSED*/
void VTButtonPressed(w, eventdata, event)
Widget w;
caddr_t eventdata;
register XButtonEvent *event;
{
	register TScreen *screen = &term->screen;
	/* so table above will be nice, we index from 0 */
	int button = event->button - 1; 
	int shift = KeyState(event->state);

	if (eventMode != NORMAL)
		return;
	if (screen->incopy)
		CopyWait (screen);
	textfunc[screen->send_mouse_pos][shift][0][button](event);
}

/*ARGSUSED*/
void VTMouseMoved(w, eventdata, event)
Widget w;
caddr_t eventdata;
register XMotionEvent *event;
{
	switch (eventMode) {
		case LEFTEXTENSION :
		case RIGHTEXTENSION :
			ExtendExtend(event->x, event->y);
			break;
		default :
			/* Should get here rarely when everything
			   fixed with windows and the event mgr */
/*			fprintf(stderr, "Race mouse motion\n");
*/			break;
	}
}


/*ARGSUSED*/
void VTButtonReleased(w, eventdata, event)
Widget w;
caddr_t eventdata;
register XButtonEvent *event;
{
	register TScreen *screen = &term->screen;
	/* so table above will be nice, we index from 0 */
	int button = event->button - 1; 
	int shift = KeyState(event->state);

	switch (eventMode) {
		case NORMAL :
			textfunc[screen->send_mouse_pos][shift][1][button]
			 (event);
			break;
		case LEFTEXTENSION :
		case RIGHTEXTENSION :
			if AllButtonsUp(event->state, event->button)
				EndExtend(event);
			break;
	}
}

/*ARGSUSED*/
void TekButtonPressed(w, eventdata, event)
Widget w;
caddr_t eventdata;
register XButtonEvent *event;
{
	register TScreen *screen = &term->screen;
	/* so table above will be nice, we index from 0 */
	int button = event->button - 1; 
	int shift = KeyState(event->state);

	if (screen->incopy)
		CopyWait (screen);
	Tbfunc[shift][button](event);
}


/*ARGSUSED*/
UnSaltText(event)
XEvent *event;
{
	int pty = term->screen.respond;	/* file descriptor of pty */
	char *line;
	int nbytes;
	register char *lag, *cp, *end;
	register TScreen *screen = &term->screen;

	line = XFetchBytes(screen->display,&nbytes);
	if (!nbytes) return;
	end = &line[nbytes];
	lag = line;
	for (cp = line; cp != end; cp++)
	{
		if (*cp != '\n') continue;
		*cp = '\r';
		v_write(pty, lag, cp - lag + 1);
		lag = cp + 1;
	}
	if (lag != end)
		v_write(pty, lag, end - lag);
	free (line);	/* free text from fetch */
}

	
#define MULTICLICKTIME 250

SetSelectUnit(buttonDownTime, defaultUnit)
unsigned long buttonDownTime;
SelectUnit defaultUnit;
{
/* Do arithmetic as integers, but compare as unsigned solves clock wraparound */
	if ((long unsigned)((long int)buttonDownTime - lastButtonUpTime)
	 > MULTICLICKTIME) {
		numberOfClicks = 1;
		selectUnit = defaultUnit;
	} else {
		++numberOfClicks;
		/* Don't bitch.  This is only temporary. */
		selectUnit = (SelectUnit) (((int) selectUnit + 1) % 3);
	}
}

StartCut(event)
register XButtonEvent *event;
{
	register TScreen *screen = &term->screen;
	int startrow, startcol;

	firstValidRow = 0;
	lastValidRow  = screen->max_row;
	SetSelectUnit(event->time, SELECTCHAR);
	PointToRowCol(event->y, event->x, &startrow, &startcol);
	replyToEmacs = FALSE;
	StartSelect(startrow, startcol);
}


TrackDown(event)
register XButtonEvent *event;
{
	int startrow, startcol;

	SetSelectUnit(event->time, SELECTCHAR);
	if (numberOfClicks > 1 ) {
		PointToRowCol(event->y, event->x, &startrow, &startcol);
		replyToEmacs = TRUE;
		StartSelect(startrow, startcol);
	} else {
		waitingForTrackInfo = 1;
		EditorButton(event);
	}
}


TrackMouse(func, startrow, startcol, firstrow, lastrow)
int func, startrow, startcol, firstrow, lastrow;
{
	if (!waitingForTrackInfo) {	/* Timed out, so ignore */
		return;
	}
	waitingForTrackInfo = 0;
	if (func == 0) return;

	firstValidRow = firstrow;
	lastValidRow  = lastrow;
	replyToEmacs = TRUE;
	StartSelect(startrow, startcol);
}

StartSelect(startrow, startcol)
int startrow, startcol;
{
	TScreen *screen = &term->screen;

	if (screen->cursor_state)
	    HideCursor ();
	if (numberOfClicks == 1) {
		/* set start of selection */
		rawRow = startrow;
		rawCol = startcol;
		
	} /* else use old values in rawRow, Col */

	saveStartRRow = startERow = rawRow;
	saveStartRCol = startECol = rawCol;
	saveEndRRow   = endERow   = rawRow;
	saveEndRCol   = endECol   = rawCol;
	if (Coordinate(startrow, startcol) < Coordinate(rawRow, rawCol)) {
		eventMode = LEFTEXTENSION;
		startERow = startrow;
		startECol = startcol;
	} else {
		eventMode = RIGHTEXTENSION;
		endERow = startrow;
		endECol = startcol;
	}
	ComputeSelect(startERow, startECol, endERow, endECol);

}

EndExtend(event)
XButtonEvent *event;
{
	int	row, col;
	TScreen *screen = &term->screen;
	char line[9];

	ExtendExtend(event->x, event->y);

	lastButtonUpTime = event->time;
	PointToRowCol(event->y, event->x, &row, &col);
	/* Only do select stuff if non-null select */
	if (startSRow != endSRow || startSCol != endSCol) {
		if (replyToEmacs) {
			if (rawRow == startSRow && rawCol == startSCol 
			 && row == endSRow && col == endSCol) {
			 	/* Use short-form emacs select */
				strcpy(line, "\033[t");
				line[3] = ' ' + endSCol + 1;
				line[4] = ' ' + endSRow + 1;
				v_write(screen->respond, line, 5);
			} else {
				/* long-form, specify everything */
				strcpy(line, "\033[T");
				line[3] = ' ' + startSCol + 1;
				line[4] = ' ' + startSRow + 1;
				line[5] = ' ' + endSCol + 1;
				line[6] = ' ' + endSRow + 1;
				line[7] = ' ' + col + 1;
				line[8] = ' ' + row + 1;
				v_write(screen->respond, line, 9);
			}
		}
		SaltTextAway(startSRow, startSCol, endSRow, endSCol);
	}
	TrackText(0, 0, 0, 0);
	eventMode = NORMAL;
}

#define Abs(x)		((x) < 0 ? -(x) : (x))

StartExtend(event)
XButtonEvent *event;
{
	TScreen *screen = &term->screen;
	int row, col, coord;

	firstValidRow = 0;
	lastValidRow  = screen->max_row;
	SetSelectUnit(event->time, selectUnit);
	replyToEmacs = FALSE;

	if (numberOfClicks == 1) {
		/* Save existing selection so we can reestablish it if the guy
		   extends past the other end of the selection */
		saveStartRRow = startERow = startRRow;
		saveStartRCol = startECol = startRCol;
		saveEndRRow   = endERow   = endRRow;
		saveEndRCol   = endECol   = endRCol;
	} else {
		/* He just needed the selection mode changed, use old values. */
		startERow = startRRow = saveStartRRow;
		startECol = startRCol = saveStartRCol;
		endERow   = endRRow   = saveEndRRow;
		endECol   = endRCol   = saveEndRCol;

	}
	PointToRowCol(event->y, event->x, &row, &col);
	coord = Coordinate(row, col);

	if (Abs(coord - Coordinate(startSRow, startSCol))
	     < Abs(coord - Coordinate(endSRow, endSCol))
	    || coord < Coordinate(startSRow, startSCol)) {
	 	/* point is close to left side of selection */
		eventMode = LEFTEXTENSION;
		startERow = row;
		startECol = col;
	} else {
	 	/* point is close to left side of selection */
		eventMode = RIGHTEXTENSION;
		endERow = row;
		endECol = col;
	}
	ComputeSelect(startERow, startECol, endERow, endECol);
}

ExtendExtend(x, y)
int x, y;
{
	int row, col, coord;

	PointToRowCol(y, x, &row, &col);
	coord = Coordinate(row, col);
	
	if (eventMode == LEFTEXTENSION 
	 && (coord + (selectUnit!=SELECTCHAR)) > Coordinate(endSRow, endSCol)) {
		/* Whoops, he's changed his mind.  Do RIGHTEXTENSION */
		eventMode = RIGHTEXTENSION;
		startERow = saveStartRRow;
		startECol = saveStartRCol;
	} else if (eventMode == RIGHTEXTENSION
	 && coord < Coordinate(startSRow, startSCol)) {
	 	/* Whoops, he's changed his mind.  Do LEFTEXTENSION */
		eventMode = LEFTEXTENSION;
		endERow   = saveEndRRow;
		endECol   = saveEndRCol;
	}
	if (eventMode == LEFTEXTENSION) {
		startERow = row;
		startECol = col;
	} else {
		endERow = row;
		endECol = col;
	}
	ComputeSelect(startERow, startECol, endERow, endECol);
}


ScrollSelection(amount)
int amount;
{
	/* Sent by scrollbar stuff, so amount never takes selection out of
	   saved text */
startRRow += amount; endRRow += amount; startSRow += amount; endSRow += amount;
rawRow += amount;
}


PointToRowCol(y, x, r, c)
register int y, x;
int *r, *c;
/* Convert pixel coordinates to character coordinates.
   Rows are clipped between firstValidRow and lastValidRow.
   Columns are clipped between to be 0 or greater, but are not clipped to some
       maximum value. */
{
	register TScreen *screen = &term->screen;
	register row, col;

	row = (y - screen->border) / FontHeight(screen);
	if(row < firstValidRow)
		row = firstValidRow;
	else if(row > lastValidRow)
		row = lastValidRow;
	col = (x - screen->border - screen->scrollbar) / FontWidth(screen);
	if(col < 0)
		col = 0;
	else if(col > screen->max_col+1) {
		col = screen->max_col+1;
	}
	*r = row;
	*c = col;
}

int LastTextCol(row)
register int row;
{
	register TScreen *screen =  &term->screen;
	register int i;
	register char *ch;

	for(i = screen->max_col,
	 ch = screen->buf[2 * (row + screen->topline)] + i ;
	 i > 0 && *ch == 0 ; ch--, i--);
	return(i);
}	

static int charClass[128] = {
/* NUL  SOH  STX  ETX  EOT  ENQ  ACK  BEL */
    32,   1,   1,   1,   1,   1,   1,   1,
/*  BS   HT   NL   VT   NP   CR   SO   SI */
     1,  32,   1,   1,   1,   1,   1,   1,
/* DLE  DC1  DC2  DC3  DC4  NAK  SYN  ETB */
     1,   1,   1,   1,   1,   1,   1,   1,
/* CAN   EM  SUB  ESC   FS   GS   RS   US */
     1,   1,   1,   1,   1,   1,   1,   1,
/*  SP    !    "    #    $    %    &    ' */
    32,  33,  34,  35,  36,  37,  38,  39,
/*   (    )    *    +    ,    -    .    / */
    40,  41,  42,  43,  44,  45,  46,  47,
/*   0    1    2    3    4    5    6    7 */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   8    9    :    ;    <    =    >    ? */
    48,  48,  58,  59,  60,  61,  62,  63,
/*   @    A    B    C    D    E    F    G */
    64,  48,  48,  48,  48,  48,  48,  48,
/*   H    I    J    K    L    M    N    O */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   P    Q    R    S    T    U    V    W */ 
    48,  48,  48,  48,  48,  48,  48,  48,
/*   X    Y    Z    [    \    ]    ^    _ */
    48,  48,  48,  91,  92,  93,  94,  48,
/*   `    a    b    c    d    e    f    g */
    96,  48,  48,  48,  48,  48,  48,  48,
/*   h    i    j    k    l    m    n    o */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   p    q    r    s    t    u    v    w */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   x    y    z    {    |    }    ~  DEL */
    48,  48,  48, 123, 124, 125, 126,   1};


ComputeSelect(startRow, startCol, endRow, endCol)
int startRow, startCol, endRow, endCol;
{
	register TScreen *screen = &term->screen;
	register char *ptr;
	register int length;
	register int class;

	if (Coordinate(startRow, startCol) <= Coordinate(endRow, endCol)) {
		startSRow = startRRow = startRow;
		startSCol = startRCol = startCol;
		endSRow   = endRRow   = endRow;
		endSCol   = endRCol   = endCol;
	} else {	/* Swap them */
		startSRow = startRRow = endRow;
		startSCol = startRCol = endCol;
		endSRow   = endRRow   = startRow;
		endSCol   = endRCol   = startCol;
	}	

	switch (selectUnit) {
		case SELECTCHAR :
			if (startSCol > (LastTextCol(startSRow) + 1)) {
				startSCol = 0;
				startSRow++;
			}
			if (endSCol > (LastTextCol(endSRow) + 1)) {
				endSCol = 0;
				endSRow++;
			}
			break;
		case SELECTWORD :
			if (startSCol > (LastTextCol(startSRow) + 1)) {
				startSCol = 0;
				startSRow++;
			} else {
				ptr = screen->buf[2*(startSRow+screen->topline)]
				 + startSCol;
				class = charClass[*ptr];
				do {
					--startSCol;
					--ptr;
				} while (startSCol >= 0
				 && charClass[*ptr] == class);
				++startSCol;
			}
			if (endSCol > (LastTextCol(endSRow) + 1)) {
				endSCol = 0;
				endSRow++;
			} else {
				length = LastTextCol(endSRow);
				ptr = screen->buf[2*(endSRow+screen->topline)]
				 + endSCol;
				class = charClass[*ptr];
				do {
					++endSCol;
					++ptr;
				} while (endSCol <= length
				 && charClass[*ptr] == class);
				/* Word select selects if pointing to any char
				   in "word", especially in that it includes
				   the last character in a word.  So no --endSCol
				   and do special eol handling */
				if (endSCol > length+1) {
					endSCol = 0;
					++endSRow;
				}
			}
			break;
		case SELECTLINE :
			startSCol = 0;
			endSCol = 0;
			++endSRow;
			break;
	}
	TrackText(startSRow, startSCol, endSRow, endSCol);
	return;
}


TrackText(frow, fcol, trow, tcol)
register int frow, fcol, trow, tcol;
/* Guaranteed (frow, fcol) <= (trow, tcol) */
{
	register int from, to;
	register TScreen *screen = &term->screen;

	/* (frow, fcol) may have been scrolled off top of display */
	if (frow < 0)
		frow = fcol = 0;
	/* (trow, tcol) may have been scrolled off bottom of display */
	if (trow > screen->max_row+1) {
		trow = screen->max_row+1;
		tcol = 0;
	}
	from = Coordinate(frow, fcol);
	to = Coordinate(trow, tcol);
	if (to <= startHCoord || from > endHCoord) {
		/* No overlap whatsoever between old and new hilite */
		HiliteText(startHRow, startHCol, endHRow, endHCol, FALSE);
		HiliteText(frow, fcol, trow, tcol, TRUE);
	} else {
		if (from < startHCoord) {
			/* Extend left end */
			HiliteText(frow, fcol, startHRow, startHCol, TRUE); 
		} else if (from > startHCoord) {
			/* Shorten left end */
			HiliteText(startHRow, startHCol, frow, fcol, FALSE);
		}
		if (to > endHCoord) {
			/* Extend right end */
			HiliteText(endHRow, endHCol, trow, tcol, TRUE); 
		} else if (to < endHCoord) {
			/* Shorten right end */
			HiliteText(trow, tcol, endHRow, endHCol, FALSE);
		}
	}
	startHRow = frow;
	startHCol = fcol;
	endHRow   = trow;
	endHCol   = tcol;
	startHCoord = from;
	endHCoord = to;
}

HiliteText(frow, fcol, trow, tcol, hilite)
register int frow, fcol, trow, tcol;
int hilite;
/* Guaranteed that (frow, fcol) <= (trow, tcol) */
{
	register TScreen *screen = &term->screen;
	register int i, j;
	GC tempgc;

	if (frow == trow && fcol == tcol)
		return;
	if(hilite) {
		tempgc = screen->normalGC;
		screen->normalGC = screen->reverseGC;
		screen->reverseGC = tempgc;
		tempgc = screen->normalboldGC;
		screen->normalboldGC = screen->reverseboldGC;
		screen->reverseboldGC = tempgc;


		i = screen->foreground;
		screen->foreground = term->core.background_pixel;
		term->core.background_pixel = i;
		XSetWindowBackground(screen->display,VWindow(screen),term->core.background_pixel);

	}
	if(frow != trow) {	/* do multiple rows */
		if((i = screen->max_col - fcol + 1) > 0) {	/* first row */
			XClearArea(
			    screen->display,
			    VWindow(screen),
			    (int) CursorX(screen, fcol),
			    (int) frow * FontHeight(screen) + screen->border,
			    (unsigned) i*FontWidth(screen),
			    (unsigned) FontHeight(screen),
			    FALSE);
			ScrnRefresh(screen, frow, fcol, 1, i);
		}
		if((i = trow - frow - 1) > 0) {			/* middle rows*/
			j = screen->max_col + 1;
			XClearArea(
			    screen->display,
			    VWindow(screen),
			    (int) screen->border + screen->scrollbar,
			    (int) (frow+1)*FontHeight(screen) + screen->border,
			    (unsigned) j * FontWidth(screen),
			    (unsigned) i * FontHeight(screen),
			    FALSE);
			ScrnRefresh(screen, frow + 1, 0, i, j);
		}
		if(tcol > 0 && trow <= screen->max_row) {	/* last row */
			XClearArea(
			    screen->display,
			    VWindow(screen),
			    (int) screen->border + screen->scrollbar,
			    (int) trow * FontHeight(screen) + screen->border,
			    (unsigned) tcol * FontWidth(screen),
			    (unsigned) FontHeight(screen),
			    FALSE);
			ScrnRefresh(screen, trow, 0, 1, tcol);
		}
	} else {		/* do single row */
		i = tcol - fcol;
		XClearArea(
		    screen->display,
		    VWindow(screen), 
		    (int) CursorX(screen, fcol),
		    (int) frow * FontHeight(screen) + screen->border,
		    (unsigned) i * FontWidth(screen),
		    (unsigned) FontHeight(screen),
		    FALSE);
		ScrnRefresh(screen, frow, fcol, 1, tcol - fcol);
	}
	if(hilite) {
		tempgc = screen->normalGC;
		screen->normalGC = screen->reverseGC;
		screen->reverseGC = tempgc;
		tempgc = screen->normalboldGC;
		screen->normalboldGC = screen->reverseboldGC;
		screen->reverseboldGC = tempgc;

		i = screen->foreground;
		screen->foreground = term->core.background_pixel;
		term->core.background_pixel = i;
		XSetWindowBackground(screen->display,VWindow(screen),term->core.background_pixel);

	}
}

SaltTextAway(crow, ccol, row, col)
register crow, ccol, row, col;
/* Guaranteed that (crow, ccol) <= (row, col), and that both points are valid
   (may have row = screen->max_row+1, col = 0) */
{
	register TScreen *screen = &term->screen;
	register int i, j = 0;
	char *line, *lp;

	--col;
	/* first we need to know how long the string is before we can save it*/

	if ( row == crow ) j = Length(screen, crow, ccol, col);
	else {	/* two cases, cut is on same line, cut spans multiple lines */
		j += Length(screen, crow, ccol, screen->max_col) + 1;
		for(i = crow + 1; i < row; i++) 
			j += Length(screen, i, 0, screen->max_col) + 1;
		if (col >= 0)
			j += Length(screen, row, 0, col);
	}
	
	/* now get some memory to save it in */

	if((line = malloc((unsigned) j + 1)) == (char *)NULL)
		SysError(ERROR_BMALLOC2);
	line[j] = '\0';		/* make sure it is null terminated */
	lp = line;		/* lp points to where to save the text */
	if ( row == crow ) lp = SaveText(screen, row, ccol, col, lp);
	else {
		lp = SaveText(screen, crow, ccol, screen->max_col, lp);
		*lp ++ = '\n';	/* put in newline at end of line */
		for(i = crow +1; i < row; i++) {
			lp = SaveText(screen, i, 0, screen->max_col, lp);
			*lp ++ = '\n';
			}
		if (col >= 0)
			lp = SaveText(screen, row, 0, col, lp);
	}
	*lp = '\0';		/* make sure we have end marked */
	
	XStoreBytes(screen->display,line, j);
	free(line);
}

/* returns number of chars in line from scol to ecol out */
int Length(screen, row, scol, ecol)
register int row, scol, ecol;
register TScreen *screen;
{
	register char *ch;

	ch = screen->buf[2 * (row + screen->topline)];
	while (ecol >= scol && ch[ecol] == 0)
	    ecol--;
	return (ecol - scol + 1);
}

/* copies text into line, preallocated */
char *SaveText(screen, row, scol, ecol, lp)
int row;
int scol, ecol;
TScreen *screen;
register char *lp;		/* pointer to where to put the text */
{
	register int i = 0;
	register char *ch = screen->buf[2 * (row + screen->topline)];
	register int c;

	if ((i = Length(screen, row, scol, ecol)) == 0) return(lp);
	ecol = scol + i;
	for (i = scol; i < ecol; i++) {
		if ((c = ch[i]) == 0)
			c = ' ';
		else if(c < ' ') {
			if(c == '\036')
				c = '#';
			else
				c += 0x5f;
		} else if(c == 0x7f)
			c = 0x5f;
		*lp++ = c;
	}
	return(lp);
}

EditorButton(event)
register XButtonEvent *event;
{
	register TScreen *screen = &term->screen;
	int pty = screen->respond;
	char line[6];
	register unsigned row, col;
	int button; 

	button = event->button - 1; 

	row = (event->y - screen->border) 
	 / FontHeight(screen);
	col = (event->x - screen->border - screen->scrollbar)
	 / FontWidth(screen);
	(void) strcpy(line, "\033[M");
	if (screen->send_mouse_pos == 1) {
		line[3] = ' ' + button;
	} else {
		line[3] = ' ' + (KeyState(event->state) << 2) + 
			((event->type == ButtonPress)? button:3);
	}
	line[4] = ' ' + col + 1;
	line[5] = ' ' + row + 1;
	v_write(pty, line, 6);
}

#ifdef MODEMENU
#define	XTERMMENU	0
#define	VTMENU		1
#define	TEKMENU		2
#define	NMENUS		3

static Menu *menus[NMENUS];
static int type;
extern TekLink *TekRefresh;

ModeMenu(event)
register XButtonEvent *event;
{
	register TScreen *screen = &term->screen;
	register Menu *menu;
	register int item;
	static int inited;
	extern Menu *setupmenu(), *Tsetupmenu(), *xsetupmenu();


	if(!inited) {
		extern Pixmap Gray_Tile;
		extern Cursor Menu_DefaultCursor;
		extern char *Menu_DefaultFont;
		extern XFontStruct *Menu_DefaultFontInfo;

		inited++;
		Gray_Tile = make_gray(BlackPixel(screen->display,
					         DefaultScreen(screen->display)), 
		 WhitePixel(screen->display, DefaultScreen(screen->display)), 1);
		InitMenu(xterm_name);
		Menu_DefaultCursor = screen->arrow;
/*		if(XStrCmp(Menu_DefaultFont, f_t) == 0)
			Menu_DefaultFontInfo = screen->fnt_norm;
 */
	}
	if((event->button) == Button1)
		type = XTERMMENU;
	else if((event->button) == Button3)
		{
		    Bell();
		    return;
		}
	else if(event->window == VWindow(screen))
		type = VTMENU;
	else if(event->window == TWindow(screen))
		type = TEKMENU;
	else
		SysError(ERROR_BADMENU);
	switch(type) {
	 case XTERMMENU:
		if((menu = xsetupmenu(&menus[XTERMMENU])) == NULL)
			return;
		break;
	 case VTMENU:
		if((menu = setupmenu(&menus[VTMENU])) == NULL)
			return;
		break;
	 case TEKMENU:
		if((menu = Tsetupmenu(&menus[TEKMENU])) == NULL)
			return;
		screen->waitrefresh = TRUE;
		break;
	}
	/*
	 * Set the select mode manually.
	 */
	TrackMenu(menu, event); /* MenuButtonReleased calls FinishModeMenu */
}

FinishModeMenu(item)
register int item;
{
	TScreen *screen = &term->screen;

	menusync();
	screen->waitrefresh = FALSE;
	reselectwindow(screen);

	if (item < 0) {
		if(type == TEKMENU && TekRefresh)
			dorefresh();
		return;
	}
	switch(type) {
	 case XTERMMENU:
		xdomenufunc(item);
		break;
	 case VTMENU:
		domenufunc(item);
		break;
	 case TEKMENU:
		Tdomenufunc(item);
		break;
	}
}

menusync()
{
	TScreen *screen = &term->screen;
	XSync(screen->display, 0);
	if (QLength(screen->display) > 0)
		xevents();
}

#define	XMENU_VISUALBELL 0
#define	XMENU_LOG	(XMENU_VISUALBELL+1)
#define	XMENU_LINE	(XMENU_LOG+1)
#define	XMENU_REDRAW	(XMENU_LINE+1)
#define	XMENU_RESUME	(XMENU_REDRAW+1)
#define	XMENU_SUSPEND	(XMENU_RESUME+1)
#define	XMENU_INTR	(XMENU_SUSPEND+1)
#define	XMENU_HANGUP	(XMENU_INTR+1)
#define	XMENU_TERM	(XMENU_HANGUP+1)
#define	XMENU_KILL	(XMENU_TERM+1)

static char *xtext[] = {
	"Visual Bell",
	"Logging",
	"-",
	"Redraw",
	"Continue",
	"Suspend",
	"Interrupt",
	"Hangup",
	"Terminate",
	"Kill",
	0,
};

static int xbell;
static int xlog;

Menu *xsetupmenu(menu)
register Menu **menu;
{
	register TScreen *screen = &term->screen;
	register char **cp;
	register int i;

	if (*menu == NULL) {
		if ((*menu = NewMenu("xterm X11", term->misc.re_verse)) == NULL)
			return(NULL);
		for(cp = xtext ; *cp ; cp++)
			AddMenuItem(*menu, *cp);
		if(xbell = screen->visualbell)
			CheckItem(*menu, XMENU_VISUALBELL);
		if(xlog = screen->logging)
			CheckItem(*menu, XMENU_LOG);
		DisableItem(*menu, XMENU_LINE);
		if((screen->inhibit & I_LOG) ||
		   /* if login window, check for completed login */
		   (L_flag && !checklogin()))
			DisableItem(*menu, XMENU_LOG);
		if(screen->inhibit & I_SIGNAL)
			for(i = XMENU_SUSPEND ; i <= XMENU_KILL ; i++)
				DisableItem(*menu, i);
		return(*menu);
	}
	/* if login window, check for completed login */
	if (!(L_flag && !checklogin()) && !(screen->inhibit & I_LOG))
		EnableItem(*menu, XMENU_LOG);
	if (xbell != screen->visualbell)
		SetItemCheck(*menu, XMENU_VISUALBELL, (xbell =
		 screen->visualbell));
	if (xlog != screen->logging)
		SetItemCheck(*menu, XMENU_LOG, (xlog = screen->logging));
	return(*menu);
}

xdomenufunc(item)
int item;
{
	register TScreen *screen = &term->screen;

	switch (item) {
	case XMENU_VISUALBELL:
		screen->visualbell = !screen->visualbell;
		break;

	case XMENU_LOG:
		if(screen->logging)
			CloseLog(screen);
		else
			StartLog(screen);
		break;

	case XMENU_REDRAW:
		Redraw();
		break;

	case XMENU_RESUME:
#if !defined(SYSV) || defined(JOBCONTROL)
		if(screen->pid > 1)
			killpg(getpgrp(screen->pid), SIGCONT);
#endif	/* !defined(SYSV) || defined(JOBCONTROL) */
		break;

	case XMENU_SUSPEND:
#if !defined(SYSV) || defined(JOBCONTROL)
		if(screen->pid > 1)
			killpg(getpgrp(screen->pid), SIGTSTP);
#endif	/* !defined(SYSV) || defined(JOBCONTROL) */
		break;

	case XMENU_INTR:
		if(screen->pid > 1)
			killpg(getpgrp(screen->pid), SIGINT);
		break;

	case XMENU_HANGUP:
		if(screen->pid > 1)
			killpg(getpgrp(screen->pid), SIGHUP);
		break;

	case XMENU_TERM:
		if(screen->pid > 1)
			killpg(getpgrp(screen->pid), SIGTERM);
		break;

	case XMENU_KILL:
		if(screen->pid > 1)
			killpg(getpgrp(screen->pid), SIGKILL);
		break;
	}
}


MenuNewCursor(cur)
register Cursor cur;
{
	register Menu **menu;
	register int i;
	register TScreen *screen = &term->screen;
	extern Cursor Menu_DefaultCursor;

	Menu_DefaultCursor = cur;
	for(i = XTERMMENU, menu = menus ; i <= TEKMENU ; menu++, i++) {
		if(!*menu)
			continue;
		(*menu)->menuCursor = cur;
		if((*menu)->menuWindow)
			XDefineCursor(screen->display, (*menu)->menuWindow, 
			 cur);
	}
}
#else	/* MODEMENU */

/*ARGSUSED*/
ModeMenu(event) register XButtonEvent *event; { Bell(); }
#endif	/* MODEMENU */

GINbutton(event)
XButtonEvent *event;
{
	register TScreen *screen = &term->screen;
	register int i;

	if(screen->TekGIN) {
		i = "rml"[event->button - 1];
		if(event->state & ShiftMask)
			i = toupper(i);
		TekEnqMouse(i | 0x80);	/* set high bit */
		TekGINoff();
	} else
		Bell();
}

/*ARGSUSED*/
Bogus(event)
XButtonEvent *event;
{
	Bell();
}

/*ARGSUSED*/
Silence(event)
XButtonEvent *event;
{
}
