/*
 *	$XConsortium: button.c,v 1.32 89/01/05 12:47:45 swick Exp $
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
static char rcs_id[] = "$XConsortium: button.c,v 1.22 88/10/17 20:10:47 swick Exp $";
#endif	/* lint */
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu.h>
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
extern EditorButton();

extern ModeMenu();
extern char *xterm_name;
extern Bogus();
extern GINbutton();

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


Boolean SendMousePosition(w, event)
Widget w;
XEvent* event;
{
    register TScreen *screen = &((XtermWidget)w)->screen;
    
    if (screen->send_mouse_pos == 0) return False;

    if (event->type != ButtonPress && event->type != ButtonRelease)
	return False;

#define KeyModifiers \
    (event->xbutton.state & (ShiftMask | LockMask | ControlMask | Mod1Mask | \
			     Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask ))

#define ButtonModifiers \
    (event->xbutton.state & (ShiftMask | LockMask | ControlMask | Mod1Mask | \
			     Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask ))

    switch (screen->send_mouse_pos) {
      case 1: /* X10 compatibility sequences */

	if (KeyModifiers == 0) {
	    if (event->type == ButtonPress)
		EditorButton(event);
	    return True;
	}
	return False;

      case 2: /* DEC vt200 compatible */

	if (KeyModifiers == 0 || KeyModifiers == ControlMask) {
	    EditorButton(event);
	    return True;
	}
	return False;

      case 3: /* DEC vt200 hilite tracking */
	if (  event->type == ButtonPress &&
	      KeyModifiers == 0 &&
	      event->xbutton.button == Button1 ) {
	    TrackDown(event);
	    return True;
	}
	if (KeyModifiers == 0 || KeyModifiers == ControlMask) {
	    EditorButton(event);
	    return True;
	}
	/* fall through */

      default:
	return False;
    }
#undef KeyModifiers
}


/*ARGSUSED*/
void HandleSelectExtend(w, event, params, num_params)
Widget w;
XEvent *event;			/* must be XMotionEvent */
String *params;			/* unused */
Cardinal *num_params;		/* unused */
{
	((XtermWidget)w)->screen.selection_time = event->xmotion.time;
	switch (eventMode) {
		case LEFTEXTENSION :
		case RIGHTEXTENSION :
			ExtendExtend(event->xmotion.x, event->xmotion.y);
			break;
		case NORMAL :
			/* will get here if send_mouse_pos != 0 */
		        break;
	}
}


/*ARGSUSED*/
void HandleSelectEnd(w, event, params, num_params)
Widget w;
XEvent *event;			/* must be XButtonEvent */
String *params;			/* selections */
Cardinal *num_params;
{
	register TScreen *screen = &((XtermWidget)w)->screen;

	((XtermWidget)w)->screen.selection_time = event->xbutton.time;
	switch (eventMode) {
		case NORMAL :
		        (void) SendMousePosition(w, event);
			break;
		case LEFTEXTENSION :
		case RIGHTEXTENSION :
			EndExtend(event, params, *num_params);
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
	(*(Tbfunc[shift][button]))(event);
}

struct _SelectionList {
    String *params;
    Cardinal count;
    Time time;
};


static void _GetSelection(w, time, params, num_params)
Widget w;
Time time;
String *params;			/* selections in precedence order */
Cardinal num_params;
{
    void SelectionReceived();
    Atom selection;
    int buffer;

    XmuInternStrings(XtDisplay(w), params, (Cardinal)1, &selection);
    switch (selection) {
      case XA_CUT_BUFFER0: buffer = 0; break;
      case XA_CUT_BUFFER1: buffer = 1; break;
      case XA_CUT_BUFFER2: buffer = 2; break;
      case XA_CUT_BUFFER3: buffer = 3; break;
      case XA_CUT_BUFFER4: buffer = 4; break;
      case XA_CUT_BUFFER5: buffer = 5; break;
      case XA_CUT_BUFFER6: buffer = 6; break;
      case XA_CUT_BUFFER7: buffer = 7; break;
      default:	       buffer = -1;
    }
    if (buffer >= 0) {
	register TScreen *screen = &((XtermWidget)w)->screen;
	unsigned long nbytes;
	int fmt8 = 8;
	Atom type = XA_STRING;
	char *line = XFetchBuffer(screen->display, &nbytes, buffer);
	if (nbytes > 0)
	    SelectionReceived(w, NULL, &selection, &type, (caddr_t)line,
			      &nbytes, &fmt8);
	else if (num_params > 1)
	    _GetSelection(w, time, params+1, num_params-1);
    } else {
	struct _SelectionList* list;
	if (--num_params) {
	    list = XtNew(struct _SelectionList);
	    list->params = params + 1;
	    list->count = num_params;
	    list->time = time;
	} else list = NULL;
	XtGetSelectionValue(w, selection, XA_STRING, SelectionReceived,
			    (caddr_t)list, time);
    }
}


/* ARGSUSED */
static void SelectionReceived(w, client_data, selection, type,
			      value, length, format)
Widget w;
caddr_t client_data;
Atom *selection, *type;
caddr_t value;
unsigned long *length;
int *format;
{
    int pty = ((XtermWidget)w)->screen.respond;	/* file descriptor of pty */
    register char *lag, *cp, *end;
    char *line = (char*)value;
				  
    if (*type == 0 /*XT_CONVERT_FAIL*/ || *length == 0) {
	struct _SelectionList* list = (struct _SelectionList*)client_data;
	if (list != NULL) {
	    _GetSelection(w, list->time, list->params, list->count);
	    XtFree(client_data);
	}
	return;
    }

    end = &line[*length];
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

    XtFree(client_data);
    XtFree(value);
}


/* ARGSUSED */
HandleInsertSelection(w, event, params, num_params)
Widget w;
XEvent *event;			/* assumed to be XButtonEvent* */
String *params;			/* selections in precedence order */
Cardinal *num_params;
{
    if (SendMousePosition(w, event)) return;
    _GetSelection(w, event->xbutton.time, params, *num_params);
}


SetSelectUnit(buttonDownTime, defaultUnit)
unsigned long buttonDownTime;
SelectUnit defaultUnit;
{
/* Do arithmetic as integers, but compare as unsigned solves clock wraparound */
	if ((long unsigned)((long int)buttonDownTime - lastButtonUpTime)
	 > term->screen.multiClickTime) {
		numberOfClicks = 1;
		selectUnit = defaultUnit;
	} else {
		++numberOfClicks;
		/* Don't bitch.  This is only temporary. */
		selectUnit = (SelectUnit) (((int) selectUnit + 1) % 3);
	}
}

/* ARGSUSED */
HandleSelectStart(w, event, params, num_params)
Widget w;
XEvent *event;			/* must be XButtonEvent* */
String *params;			/* unused */
Cardinal *num_params;		/* unused */
{
	register TScreen *screen = &((XtermWidget)w)->screen;
	int startrow, startcol;

	if (SendMousePosition(w, event)) return;
	firstValidRow = 0;
	lastValidRow  = screen->max_row;
	SetSelectUnit(event->xbutton.time, SELECTCHAR);
	PointToRowCol(event->xbutton.y, event->xbutton.x, &startrow, &startcol);
	replyToEmacs = FALSE;
	StartSelect(startrow, startcol);
}


static TrackDown(event)
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

EndExtend(event, params, num_params)
XEvent *event;			/* must be XButtonEvent */
String *params;			/* selections */
Cardinal num_params;
{
	int	row, col;
	TScreen *screen = &term->screen;
	char line[9];

	ExtendExtend(event->xbutton.x, event->xbutton.y);

	lastButtonUpTime = event->xbutton.time;
	PointToRowCol(event->xbutton.y, event->xbutton.x, &row, &col);
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
			TrackText(0, 0, 0, 0);
		}
		SaltTextAway(startSRow, startSCol, endSRow, endSCol,
			     params, num_params);
	} else DisownSelection(term);

	/* TrackText(0, 0, 0, 0); */
	eventMode = NORMAL;
}

#define Abs(x)		((x) < 0 ? -(x) : (x))

/* ARGSUSED */
HandleStartExtend(w, event, params, num_params)
Widget w;
XEvent *event;			/* must be XButtonEvent* */
String *params;			/* unused */
Cardinal *num_params;		/* unused */
{
	TScreen *screen = &((XtermWidget)w)->screen;
	int row, col, coord;

	if (SendMousePosition(w, event)) return;
	firstValidRow = 0;
	lastValidRow  = screen->max_row;
	SetSelectUnit(event->xbutton.time, selectUnit);
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
	PointToRowCol(event->xbutton.y, event->xbutton.x, &row, &col);
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


ScrollSelection(screen, amount)
register TScreen* screen;
register int amount;
{
    register int minrow = -screen->savedlines;

    /* Sent by scrollbar stuff, so amount never takes selection out of
       saved text */

    /* XXX - the preceeding is false; cat /etc/termcap (or anything
       larger than the number of saved lines plus the screen height) and then
       hit extend select */

    startRRow += amount; endRRow += amount;
    startSRow += amount; endSRow += amount;
    rawRow += amount;
    screen->startHRow += amount;
    screen->endHRow += amount;

    if (startRRow < minrow) {
	startRRow = minrow;
	startRCol = 0;
    }
    if (endRRow < minrow) {
	endRRow = minrow;
        endRCol = 0;
    }
    if (startSRow < minrow) {
	startSRow = minrow;
	startSCol = 0;
    }
    if (endSRow < minrow) {
	endSRow = minrow;
	endSCol = 0;
    }
    if (rawRow < minrow) {
	rawRow = minrow;
	rawCol = 0;
    }
    if (screen->startHRow < minrow) {
	screen->startHRow = minrow;
	screen->startHCol = 0;
    }
    if (screen->endHRow < minrow) {
	screen->endHRow = minrow;
	screen->endHCol = 0;
    }
    screen->startHCoord = Coordinate (screen->startHRow, screen->startHCol);
    screen->endHCoord = Coordinate (screen->endHRow, screen->endHCol);
}


ResizeSelection (screen, rows, cols)
    TScreen *screen;
    int rows, cols;
{
    rows--;				/* decr to get 0-max */
    cols--;

    if (startRRow > rows) startRRow = rows;
    if (startSRow > rows) startSRow = rows;
    if (endRRow > rows) endRRow = rows;
    if (endSRow > rows) endSRow = rows;
    if (rawRow > rows) rawRow = rows;

    if (startRCol > cols) startRCol = cols;
    if (startSCol > cols) startSCol = cols;
    if (endRCol > cols) endRCol = cols;
    if (endSCol > cols) endSCol = cols;
    if (rawCol > cols) rawCol = cols;
}

static PointToRowCol(y, x, r, c)
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
	 i > 0 && (*ch == ' ' || *ch == 0); ch--, i--);
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


int SetCharacterClassRange (low, high, value)
    register int low, high;		/* in range of [0..127] */
    register int value;			/* arbitrary */
{

    if (low < 0 || high > 127 || high < low) return (-1);

    for (; low <= high; low++) charClass[low] = value;

    return (0);
}


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
			if (term->screen.cutToBeginningOfLine) startSCol = 0;
			if (term->screen.cutNewline) {
			    endSCol = 0;
			    ++endSRow;
			} else {
			    endSCol = LastTextCol(endSRow) + 1;
			}
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
	int old_startrow, old_startcol, old_endrow, old_endcol;

	/* (frow, fcol) may have been scrolled off top of display */
	if (frow < 0)
		frow = fcol = 0;
	/* (trow, tcol) may have been scrolled off bottom of display */
	if (trow > screen->max_row+1) {
		trow = screen->max_row+1;
		tcol = 0;
	}
	old_startrow = screen->startHRow;
	old_startcol = screen->startHCol;
	old_endrow = screen->endHRow;
	old_endcol = screen->endHCol;
	if (frow == old_startrow && fcol == old_startcol &&
	    trow == old_endrow   && tcol == old_endcol) return;
	screen->startHRow = frow;
	screen->startHCol = fcol;
	screen->endHRow   = trow;
	screen->endHCol   = tcol;
	from = Coordinate(frow, fcol);
	to = Coordinate(trow, tcol);
	if (to <= screen->startHCoord || from > screen->endHCoord) {
	    /* No overlap whatsoever between old and new hilite */
	    ReHiliteText(old_startrow, old_startcol, old_endrow, old_endcol);
	    ReHiliteText(frow, fcol, trow, tcol);
	} else {
	    if (from < screen->startHCoord) {
		    /* Extend left end */
		    ReHiliteText(frow, fcol, old_startrow, old_startcol);
	    } else if (from > screen->startHCoord) {
		    /* Shorten left end */
		    ReHiliteText(old_startrow, old_startcol, frow, fcol);
	    }
	    if (to > screen->endHCoord) {
		    /* Extend right end */
		    ReHiliteText(old_endrow, old_endcol, trow, tcol);
	    } else if (to < screen->endHCoord) {
		    /* Shorten right end */
		    ReHiliteText(trow, tcol, old_endrow, old_endcol);
	    }
	}
	screen->startHCoord = from;
	screen->endHCoord = to;
}

ReHiliteText(frow, fcol, trow, tcol)
register int frow, fcol, trow, tcol;
/* Guaranteed that (frow, fcol) <= (trow, tcol) */
{
	register TScreen *screen = &term->screen;
	register int i, j;
	GC tempgc;

	if (frow < 0)
	    frow = fcol = 0;
	else if (frow > screen->max_row)
	    return;		/* nothing to do, since trow >= frow */

	if (trow < 0)
	    return;		/* nothing to do, since frow <= trow */
	else if (trow > screen->max_row) {
	    trow = screen->max_row;
	    tcol = screen->max_col+1;
	}
	if (frow == trow && fcol == tcol)
		return;

	if(frow != trow) {	/* do multiple rows */
		if((i = screen->max_col - fcol + 1) > 0) {     /* first row */
		    ScrnRefresh(screen, frow, fcol, 1, i, True);
		}
		if((i = trow - frow - 1) > 0) {		       /* middle rows*/
		    ScrnRefresh(screen, frow+1, 0,i, screen->max_col+1, True);
		}
		if(tcol > 0 && trow <= screen->max_row) {      /* last row */
		    ScrnRefresh(screen, trow, 0, 1, tcol, True);
		}
	} else {		/* do single row */
		ScrnRefresh(screen, frow, fcol, 1, tcol - fcol, True);
	}
}

SaltTextAway(crow, ccol, row, col, params, num_params)
register crow, ccol, row, col;
String *params;			/* selections */
Cardinal num_params;
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

	if (screen->selection_size <= j) {
	    if((line = malloc((unsigned) j + 1)) == (char *)NULL)
		SysError(ERROR_BMALLOC2);
	    XtFree(screen->selection);
	    screen->selection = line;
	    screen->selection_size = j + 1;
	} else line = screen->selection;
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
	
	screen->selection_length = j;
	_OwnSelection(term, params, num_params);
}

static Boolean ConvertSelection(w, selection, target,
				type, value, length, format)
Widget w;
Atom *selection, *target, *type;
caddr_t *value;
unsigned long *length;
int *format;
{
    Display* d = XtDisplay(w);
    XtermWidget xterm = (XtermWidget)w;

    if (xterm->screen.selection == NULL) return False; /* can this happen? */

    if (*target == XA_TARGETS(d)) {
	Atom* targetP;
	Atom* std_targets;
	unsigned long std_length;
	XmuConvertStandardSelection(
		    w, xterm->screen.selection_time, selection,
		    target, type, (caddr_t*)&std_targets, &std_length, format
		   );
	*length = std_length + 4;
	*value = (caddr_t)XtMalloc(sizeof(Atom)*(*length));
	targetP = *(Atom**)value;
	*targetP++ = XA_STRING;
	*targetP++ = XA_TEXT(d);
	*targetP++ = XA_LENGTH(d);
	*targetP++ = XA_LIST_LENGTH(d);
	bcopy((char*)std_targets, (char*)targetP, sizeof(Atom)*std_length);
	XtFree((char*)std_targets);
	*type = XA_ATOM;
	*format = 32;
	return True;
    }

    if (*target == XA_STRING || *target == XA_TEXT(d)) {
	*type = XA_STRING;
	*value = xterm->screen.selection;
	*length = xterm->screen.selection_length;
	*format = 8;
	return True;
    }
    if (*target == XA_LIST_LENGTH(d)) {
	*value = XtMalloc(4);
	if (sizeof(long) == 4)
	    *(long*)*value = 1;
	else {
	    long temp = 1;
	    bcopy( ((char*)&temp)+sizeof(long)-4, (char*)*value, 4);
	}
	*type = XA_INTEGER;
	*length = 1;
	*format = 32;
	return True;
    }
    if (*target == XA_LENGTH(d)) {
	*value = XtMalloc(4);
	if (sizeof(long) == 4)
	    *(long*)*value = xterm->screen.selection_length;
	else {
	    long temp = xterm->screen.selection_length;
	    bcopy( ((char*)&temp)+sizeof(long)-4, (char*)*value, 4);
	}
	*type = XA_INTEGER;
	*length = 1;
	*format = 32;
	return True;
    }
    if (XmuConvertStandardSelection(w, xterm->screen.selection_time, selection,
				    target, type, value, length, format))
	return True;

    /* else */
    return False;

}


/* ARGSUSED */
static void LoseSelection(w, selection)
  Widget w;
  Atom *selection;
{
    register TScreen* screen = &((XtermWidget)w)->screen;
    register Atom* atomP;
    int i, empty;
    for (i = 0, atomP = screen->selection_atoms;
	 i < screen->selection_count; i++, atomP++)
    {
	if (*selection == *atomP) *atomP = (Atom)0;
	switch (*atomP) {
	  case XA_CUT_BUFFER0:
	  case XA_CUT_BUFFER1:
	  case XA_CUT_BUFFER2:
	  case XA_CUT_BUFFER3:
	  case XA_CUT_BUFFER4:
	  case XA_CUT_BUFFER5:
	  case XA_CUT_BUFFER6:
	  case XA_CUT_BUFFER7:	*atomP = (Atom)0;
	}
    }

    for (i = screen->selection_count; i; i--) {
	if (screen->selection_atoms[i-1] != 0) break;
    }
    screen->selection_count = i;

    for (i = 0, atomP = screen->selection_atoms;
	 i < screen->selection_count; i++, atomP++)
    {
	if (*atomP == (Atom)0) {
	    *atomP = screen->selection_atoms[--screen->selection_count];
	}
    }

    if (screen->selection_count == 0)
	TrackText(0, 0, 0, 0);
}


/* ARGSUSED */
static void SelectionDone(w, selection, target)
Widget w;
Atom *selection, *target;
{
    /* empty proc so Intrinsics know we want to keep storage */
}


static /* void */ _OwnSelection(term, selections, count)
register XtermWidget term;
String *selections;
Cardinal count;
{
    Atom* atoms = term->screen.selection_atoms;
    int i;
    Boolean have_selection = False;

    if (count > term->screen.sel_atoms_size) {
	XtFree((char*)atoms);
	atoms = (Atom*)XtMalloc(count*sizeof(Atom));
	term->screen.selection_atoms = atoms;
	term->screen.sel_atoms_size = count;
    }
    XmuInternStrings( XtDisplay((Widget)term), selections, count, atoms );
    for (i = 0; i < count; i++) {
	int buffer;
	switch (atoms[i]) {
	  case XA_CUT_BUFFER0: buffer = 0; break;
	  case XA_CUT_BUFFER1: buffer = 1; break;
	  case XA_CUT_BUFFER2: buffer = 2; break;
	  case XA_CUT_BUFFER3: buffer = 3; break;
	  case XA_CUT_BUFFER4: buffer = 4; break;
	  case XA_CUT_BUFFER5: buffer = 5; break;
	  case XA_CUT_BUFFER6: buffer = 6; break;
	  case XA_CUT_BUFFER7: buffer = 7; break;
	  default:	       buffer = -1;
	}
	if (buffer >= 0)
	    XStoreBytes( XtDisplay((Widget)term), term->screen.selection,
			 term->screen.selection_length, buffer );
	else if (!replyToEmacs) {
	    have_selection |=
		XtOwnSelection( (Widget)term, atoms[i],
			    term->screen.selection_time,
			    ConvertSelection, LoseSelection, SelectionDone );
	}
    }
    if (!replyToEmacs)
	term->screen.selection_count = count;
    if (!have_selection)
	TrackText(0, 0, 0, 0);
}

/* void */ DisownSelection(term)
register XtermWidget term;
{
    Atom* atoms = term->screen.selection_atoms;
    Cardinal count = term->screen.selection_count;
    int i;

    for (i = 0; i < count; i++) {
	int buffer;
	switch (atoms[i]) {
	  case XA_CUT_BUFFER0: buffer = 0; break;
	  case XA_CUT_BUFFER1: buffer = 1; break;
	  case XA_CUT_BUFFER2: buffer = 2; break;
	  case XA_CUT_BUFFER3: buffer = 3; break;
	  case XA_CUT_BUFFER4: buffer = 4; break;
	  case XA_CUT_BUFFER5: buffer = 5; break;
	  case XA_CUT_BUFFER6: buffer = 6; break;
	  case XA_CUT_BUFFER7: buffer = 7; break;
	  default:	       buffer = -1;
	}
	if (buffer < 0)
	    XtDisownSelection( (Widget)term, atoms[i],
			       term->screen.selection_time );
    }
    term->screen.selection_count = 0;
    term->screen.startHRow = term->screen.startHCol = 0;
    term->screen.endHRow = term->screen.endHCol = 0;
}


/* returns number of chars in line from scol to ecol out */
int Length(screen, row, scol, ecol)
register int row, scol, ecol;
register TScreen *screen;
{
	register char *ch;

	ch = screen->buf[2 * (row + screen->topline)];
	while (ecol >= scol && (ch[ecol] == ' ' || ch[ecol] == 0))
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
#define FIRSTMENU	0
#define	XTERMMENU	0
#define	VTMENU		1
#define	TEKMENU		2
#define	NMENUS		3

static Menu *menus[NMENUS];
static int type;
extern TekLink *TekRefresh;

/* ARGSUSED */
void HandleModeMenu(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;			/* unused */
Cardinal *num_params;		/* unused */
{
    ModeMenu(event);		/* %%% hack 'till TekWidget uses TM */
}

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

FinishModeMenu(item, time)
register int item;
Time time;
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
		xdomenufunc(item, time);
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

#define XMENU_GRABKBD	0
#define	XMENU_VISUALBELL (XMENU_GRABKBD+1)
#define	XMENU_LOG	(XMENU_VISUALBELL+1)
#define	XMENU_REDRAW	(XMENU_LOG+1)
#define	XMENU_LINE	(XMENU_REDRAW+1)
#define	XMENU_SUSPEND	(XMENU_LINE+1)
#define	XMENU_RESUME	(XMENU_SUSPEND+1)
#define	XMENU_INTR	(XMENU_RESUME+1)
#define	XMENU_HANGUP	(XMENU_INTR+1)
#define	XMENU_TERM	(XMENU_HANGUP+1)
#define	XMENU_KILL	(XMENU_TERM+1)
#define XMENU_LINE2	(XMENU_KILL+1)
#define XMENU_EXIT	(XMENU_LINE2+1)

static char *xtext[] = {
	"Secure Keyboard",
	"Visual Bell",
	"Logging",
	"Redraw",
	"-",
	"Suspend program",
	"Continue program",
	"Interrupt program",
	"Hangup program",
	"Terminate program",
	"Kill program",
	"-",
	"Quit",
	0,
};

static int xbell;
static int xlog;
static int xkgrab;

Menu *xsetupmenu(menu)
register Menu **menu;
{
	register TScreen *screen = &term->screen;
	register char **cp;
	register int i;

	if (*menu == NULL) {
		if ((*menu = NewMenu("xterm X11")) == NULL)
			return(NULL);
		for(cp = xtext ; *cp ; cp++)
			AddMenuItem(*menu, *cp);
		if(xkgrab = screen->grabbedKbd)
			CheckItem(*menu, XMENU_GRABKBD);
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
#if defined(SYSV) && !defined(JOBCONTROL)
		DisableItem(*menu, XMENU_RESUME);
		DisableItem(*menu, XMENU_SUSPEND);
#endif	/* defined(SYSV) && !defined(JOBCONTROL) */
		return(*menu);
	}
	/* if login window, check for completed login */
	if (!(L_flag && !checklogin()) && !(screen->inhibit & I_LOG))
		EnableItem(*menu, XMENU_LOG);
	if (xkgrab != screen->grabbedKbd)
		SetItemCheck(*menu, XMENU_GRABKBD, (xkgrab =
		 screen->grabbedKbd));
	if (xbell != screen->visualbell)
		SetItemCheck(*menu, XMENU_VISUALBELL, (xbell =
		 screen->visualbell));
	if (xlog != screen->logging)
		SetItemCheck(*menu, XMENU_LOG, (xlog = screen->logging));
	return(*menu);
}

xdomenufunc(item, time)
int item;
Time time;
{
	register TScreen *screen = &term->screen;

	switch (item) {
	case XMENU_GRABKBD:
		if (screen->grabbedKbd) {
		    XUngrabKeyboard(screen->display, time);
		    ReverseVideo(term);
		    screen->grabbedKbd = FALSE;
		} else {
		    if (XGrabKeyboard(screen->display,
				      term->core.parent->core.window,
				      True, GrabModeAsync, GrabModeAsync, time)
			  != GrabSuccess) {
		        XBell(screen->display, 100);
		    } else {
			ReverseVideo(term);
			screen->grabbedKbd = TRUE;
		    }
		}
		break;

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

/*
 * The following cases use the pid instead of the process group so that we
 * don't get hosed by programs that change their process group
 */

	case XMENU_RESUME:
#if !defined(SYSV) || defined(JOBCONTROL)
		if(screen->pid > 1)
			killpg (screen->pid, SIGCONT);
#endif	/* !defined(SYSV) || defined(JOBCONTROL) */
		break;

	case XMENU_SUSPEND:
#if !defined(SYSV) || defined(JOBCONTROL)
		if(screen->pid > 1)
			killpg (screen->pid, SIGTSTP);
#endif	/* !defined(SYSV) || defined(JOBCONTROL) */
		break;

	case XMENU_INTR:
		if(screen->pid > 1)
			killpg (screen->pid, SIGINT);
		break;

	case XMENU_HANGUP:
		if(screen->pid > 1)
			killpg (screen->pid, SIGHUP);
		break;

	case XMENU_TERM:
		if(screen->pid > 1)
			killpg (screen->pid, SIGTERM);
		break;

	case XMENU_KILL:
		if(screen->pid > 1)
			killpg (screen->pid, SIGKILL);
		break;

	case XMENU_EXIT:
		Cleanup (0);
		/* NOTREACHED */
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

ReverseVideoAllMenus ()
{
    int i;
    XtermWidget xw = term;
    Display *dpy = XtDisplay (xw);
    Pixel fg, bg;

    MenuResetGCs (&bg, &fg);

    for (i = FIRSTMENU; i < NMENUS; i++) {
	Menu *menu = menus[i];

	if (menu) {
	    menu->menuBgColor = bg;
	    menu->menuFgColor = fg;
	    XSetWindowBackground (dpy, menu->menuWindow, menu->menuBgColor);
	    menu->menuFlags |= menuChanged;
	}
    }
    return;
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

/* ARGSUSED */
void HandleSecure(w, event, params, param_count)
    Widget w;
    XEvent *event;		/* unused */
    String *params;		/* [0] = volume */
    Cardinal *param_count;	/* 0 or 1 */
{
    Time time = CurrentTime;

    if ((event->xany.type == KeyPress) ||
	(event->xany.type == KeyRelease))
	time = event->xkey.time;
    else if ((event->xany.type == ButtonPress) ||
	     (event->xany.type == ButtonRelease))
      time = event->xbutton.time;
    xdomenufunc(XMENU_GRABKBD, time);
}
