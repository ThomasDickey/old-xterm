/*
 *	$XConsortium: screen.c,v 1.16 89/12/10 20:44:52 jim Exp $
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

/* screen.c */

#ifndef lint
static char rcs_id[] = "$XConsortium: screen.c,v 1.16 89/12/10 20:44:52 jim Exp $";
#endif	/* lint */

#include <X11/Xlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include "ptyx.h"
#include "error.h"

extern Char *calloc(), *malloc(), *realloc();
extern void bcopy();
extern void free();

ScrnBuf Allocate (nrow, ncol, addr)
/*
   allocates memory for a 2-dimensional array of chars and returns a pointer
   thereto
   each line is formed from a pair of char arrays.  The first (even) one is
   the actual character array and the second (odd) one is the attributes.
 */
register int nrow, ncol;
Char **addr;
{
	register ScrnBuf base;
	register Char *tmp;
	register int i;

	if ((base = (ScrnBuf) calloc ((unsigned)(nrow *= 2), sizeof (char *))) == 0)
		SysError (ERROR_SCALLOC);

	if ((tmp = calloc ((unsigned) (nrow * ncol), sizeof(char))) == 0)
		SysError (ERROR_SCALLOC2);

	*addr = tmp;
	for (i = 0; i < nrow; i++, tmp += ncol)
		base[i] = tmp;

	return (base);
}

/*
 *  This is called when the screen is resized. Not complex if you do
 *  things in the right order...
 */
static void
Reallocate(sbuf, sbufaddr, nrow, ncol, oldrow, oldcol)
ScrnBuf *sbuf;
Char **sbufaddr;
int nrow, ncol, oldrow, oldcol;
{
	register ScrnBuf base;
	register Char *tmp;
	register int i, minrows, mincols;
	Char *oldbuf;
	
	if (sbuf == NULL || *sbuf == NULL)
		return;

	oldrow *= 2;
	oldbuf = *sbufaddr;

	/*
	 * Special case if oldcol == ncol - straight forward realloc and
	 * update of the additional lines in sbuf
	 */

	/* 
	 *  realloc sbuf; we don't care about losing the lower lines if the
	 *  screen shrinks. It might be cleaner to readjust the screen so
	 *  that the UPPER lines vanish when the screen shrinks but that's
	 *  more work...
	 */
	nrow *= 2;
	*sbuf = (ScrnBuf) realloc((char *) (*sbuf),
	 (unsigned) (nrow * sizeof(char *)));
	if (*sbuf == 0)
		SysError(ERROR_RESIZE);
	base = *sbuf;

	/* 
	 *  create the new buffer space and copy old buffer contents there
	 *  line by line, updating the pointers in sbuf as we go; then free
	 *  the old buffer
	 */
	if ((tmp = calloc((unsigned) (nrow * ncol), sizeof(char))) == 0)
		SysError(ERROR_SREALLOC);
	*sbufaddr = tmp;
	minrows = (oldrow < nrow) ? oldrow : nrow;
	mincols = (oldcol < ncol) ? oldcol : ncol;
	for(i = 0; i < minrows; i++, tmp += ncol) {
		bcopy(base[i], tmp, mincols);
		base[i] = tmp;
	}
	if (oldrow < nrow) {
		for (i = minrows; i < nrow; i++, tmp += ncol)
			base[i] = tmp;
	}
	/* Now free the old buffer - simple, see... */
	free(oldbuf);
}

ScreenWrite (screen, str, flags, length)
/*
   Writes str into buf at row row and column col.  Characters are set to match
   flags.
 */
TScreen *screen;
char *str;
register unsigned flags;
register int length;		/* length of string */
{
	register Char *attrs;
	register int avail  = screen->max_col - screen->cur_col + 1;
	register Char *col;

	if (length > avail)
	    length = avail;
	if (length <= 0)
		return;

	col = screen->buf[avail = 2 * screen->cur_row] + screen->cur_col;
	attrs = screen->buf[avail + 1] + screen->cur_col;
	flags &= ATTRIBUTES;
	bcopy(str, col, length);
	while(length-- > 0)
		*attrs++ = flags;
}

ScrnInsertLine (sb, last, where, n, size)
/*
   Inserts n blank lines at sb + where, treating last as a bottom margin.
   Size is the size of each entry in sb.
   Requires: 0 <= where < where + n <= last
   	     n <= MAX_ROWS
 */
register ScrnBuf sb;
int last;
register int where, n, size;
{
	register int i;
	char *save [2 * MAX_ROWS];


	/* save n lines at bottom */
	bcopy ((char *) &sb [2 * (last -= n - 1)], (char *) save,
		2 * sizeof (char *) * n);
	
	/* clear contents of old rows */
	for (i = 2 * n - 1; i >= 0; i--)
		bzero ((char *) save [i], size);

	/*
	 * WARNING, overlapping copy operation.  Move down lines (pointers).
	 *
	 *   +----|---------|--------+
	 *
	 * is copied in the array to:
	 *
	 *   +--------|---------|----+
	 */
	bcopy ((char *) &sb [2 * where], (char *) &sb [2 * (where + n)],
		2 * sizeof (char *) * (last - where));

	/* reuse storage for new lines at where */
	bcopy ((char *)save, (char *) &sb[2 * where], 2 * sizeof(char *) * n);
}


ScrnDeleteLine (sb, last, where, n, size)
/*
   Deletes n lines at sb + where, treating last as a bottom margin.
   Size is the size of each entry in sb.
   Requires 0 <= where < where + n < = last
   	    n <= MAX_ROWS
 */
register ScrnBuf sb;
register int n, last, size;
int where;
{
	register int i;
	char *save [2 * MAX_ROWS];

	/* save n lines at where */
	bcopy ((char *) &sb[2 * where], (char *)save, 2 * sizeof(char *) * n);

	/* clear contents of old rows */
	for (i = 2 * n - 1 ; i >= 0 ; i--)
		bzero ((char *) save [i], size);

	/* move up lines */
	bcopy ((char *) &sb[2 * (where + n)], (char *) &sb[2 * where],
		2 * sizeof (char *) * ((last -= n - 1) - where));

	/* reuse storage for new bottom lines */
	bcopy ((char *)save, (char *) &sb[2 * last],
		2 * sizeof(char *) * n);
}


ScrnInsertChar (sb, row, col, n, size)
/*
   Inserts n blanks in sb at row, col.  Size is the size of each row.
 */
ScrnBuf sb;
int row, size;
register int col, n;
{
	register int i, j;
	register Char *ptr = sb [2 * row];
	register Char *attrs = sb [2 * row + 1];

	for (i = size - 1; i >= col + n; i--) {
		ptr[i] = ptr[j = i - n];
		attrs[i] = attrs[j];
	}

	bzero (ptr + col, n);
	bzero (attrs + col, n);
}


ScrnDeleteChar (sb, row, col, n, size)
/*
   Deletes n characters in sb at row, col. Size is the size of each row.
 */
ScrnBuf sb;
register int row, size;
register int n, col;
{
	register Char *ptr = sb[2 * row];
	register Char *attrs = sb[2 * row + 1];
	register nbytes = (size - n - col);

	bcopy (ptr + col + n, ptr + col, nbytes);
	bcopy (attrs + col + n, attrs + col, nbytes);
	bzero (ptr + size - n, n);
	bzero (attrs + size - n, n);
}


ScrnRefresh (screen, toprow, leftcol, nrows, ncols, force)
/*
   Repaints the area enclosed by the parameters.
   Requires: (toprow, leftcol), (toprow + nrows, leftcol + ncols) are
   	     coordinates of characters in screen;
	     nrows and ncols positive.
 */
register TScreen *screen;
int toprow, leftcol, nrows, ncols;
Boolean force;			/* ... leading/trailing spaces */
{
	int y = toprow * FontHeight(screen) + screen->border +
		screen->fnt_norm->ascent;
	register int row;
	register int topline = screen->topline;
	int maxrow = toprow + nrows - 1;
	int scrollamt = screen->scroll_amt;
	int max = screen->max_row;

	if(screen->cursor_col >= leftcol && screen->cursor_col <=
	 (leftcol + ncols - 1) && screen->cursor_row >= toprow + topline &&
	 screen->cursor_row <= maxrow + topline)
		screen->cursor_state = OFF;
	for (row = toprow; row <= maxrow; y += FontHeight(screen), row++) {
	   register Char *chars;
	   register Char *attrs;
	   register int col = leftcol;
	   int maxcol = leftcol + ncols - 1;
	   int lastind;
	   int flags;
	   int x, n;
	   GC gc;
	   Boolean hilite;	

	   if (row < screen->top_marg || row > screen->bot_marg)
		lastind = row;
	   else
		lastind = row - scrollamt;

	   if (lastind < 0 || lastind > max)
	   	continue;

	   chars = screen->buf [2 * (lastind + topline)];
	   attrs = screen->buf [2 * (lastind + topline) + 1];

	   if (row < screen->startHRow || row > screen->endHRow ||
	       (row == screen->startHRow && maxcol < screen->startHCol) ||
	       (row == screen->endHRow && col >= screen->endHCol))
	       {
	       /* row does not intersect selection; don't hilite */
	       if (!force) {
		   while (col <= maxcol && (attrs[col] & ~BOLD) == 0 &&
			  (chars[col] & ~040) == 0)
		       col++;

		   while (col <= maxcol && (attrs[maxcol] & ~BOLD) == 0 &&
			  (chars[maxcol] & ~040) == 0)
		       maxcol--;
	       }
	       hilite = False;
	   }
	   else {
	       /* row intersects selection; split into pieces of single type */
	       if (row == screen->startHRow && col < screen->startHCol) {
		   ScrnRefresh(screen, row, col, 1, screen->startHCol - col,
			       force);
		   col = screen->startHCol;
	       }
	       if (row == screen->endHRow && maxcol >= screen->endHCol) {
		   ScrnRefresh(screen, row, screen->endHCol, 1,
			       maxcol - screen->endHCol + 1, force);
		   maxcol = screen->endHCol - 1;
	       }
	       /* remaining piece should be hilited */
	       hilite = True;
	   }

	   if (col > maxcol) continue;

	   flags = attrs[col];

	   if ( (!hilite && (flags & INVERSE) != 0) ||
	        (hilite && (flags & INVERSE) == 0) )
	       if (flags & BOLD) gc = screen->reverseboldGC;
	       else gc = screen->reverseGC;
	   else 
	       if (flags & BOLD) gc = screen->normalboldGC;
	       else gc = screen->normalGC;

	   x = CursorX(screen, col);
	   lastind = col;

	   for (; col <= maxcol; col++) {
		if (attrs[col] != flags) {
		   XDrawImageString(screen->display, TextWindow(screen), 
		        	gc, x, y, (char *) &chars[lastind], n = col - lastind);
		   if((flags & BOLD) && screen->enbolden)
		 	XDrawString(screen->display, TextWindow(screen), 
			 gc, x + 1, y, (char *) &chars[lastind], n);
		   if(flags & UNDERLINE) 
			XDrawLine(screen->display, TextWindow(screen), 
			 gc, x, y+1, x+n*FontWidth(screen), y+1);

		   x += (col - lastind) * FontWidth(screen);

		   lastind = col;

		   flags = attrs[col];

	   	   if ((!hilite && (flags & INVERSE) != 0) ||
		       (hilite && (flags & INVERSE) == 0) )
	       		if (flags & BOLD) gc = screen->reverseboldGC;
	       		else gc = screen->reverseGC;
	  	    else 
	      		 if (flags & BOLD) gc = screen->normalboldGC;
	      		 else gc = screen->normalGC;
		}

		if(chars[col] == 0)
			chars[col] = ' ';
	   }


	   if ( (!hilite && (flags & INVERSE) != 0) ||
	        (hilite && (flags & INVERSE) == 0) )
	       if (flags & BOLD) gc = screen->reverseboldGC;
	       else gc = screen->reverseGC;
	   else 
	       if (flags & BOLD) gc = screen->normalboldGC;
	       else gc = screen->normalGC;
	   XDrawImageString(screen->display, TextWindow(screen), gc, 
	         x, y, (char *) &chars[lastind], n = col - lastind);
	   if((flags & BOLD) && screen->enbolden)
		XDrawString(screen->display, TextWindow(screen), gc,
		x + 1, y, (char *) &chars[lastind], n);
	   if(flags & UNDERLINE) 
		XDrawLine(screen->display, TextWindow(screen), gc, 
		 x, y+1, x + n * FontWidth(screen), y+1);
	}
}

ClearBufRows (screen, first, last)
/*
   Sets the rows first though last of the buffer of screen to spaces.
   Requires first <= last; first, last are rows of screen->buf.
 */
register TScreen *screen;
register int first, last;
{
	first *= 2;
	last = 2 * last + 1;
	while (first <= last)
		bzero (screen->buf [first++], (screen->max_col + 1));
}

ScreenResize (screen, width, height, flags)
/*
   Resizes screen:
   1. If new window would have fractional characters, sets window size so as to
      discard fractional characters and returns -1.
      Minimum screen size is 1 X 1.
      Note that this causes another ExposeWindow event.
   2. Enlarges screen->buf if necessary.  New space is appended to the bottom
      and to the right
   3. Reduces  screen->buf if necessary.  Old space is removed from the bottom
      and from the right
   4. Cursor is positioned as closely to its former position as possible
   5. Sets screen->max_row and screen->max_col to reflect new size
   6. Maintains the inner border (and clears the border on the screen).
   7. Clears origin mode and sets scrolling region to be entire screen.
   8. Returns 0
 */
register TScreen *screen;
int width, height;
unsigned *flags;
{
	int rows, cols;
	int border = 2 * screen->border;
#ifdef sun
#ifdef TIOCSSIZE
	struct ttysize ts;
#endif	/* TIOCSSIZE */
#else	/* sun */
#ifdef TIOCSWINSZ
	struct winsize ws;
#endif	/* TIOCSWINSZ */
#endif	/* sun */
	Window tw = TextWindow (screen);

	/* clear the right and bottom internal border because of NorthWest
	   gravity might have left junk on the right and bottom edges */
	XClearArea (screen->display, tw,
		    width - screen->border, 0,                /* right edge */
		    screen->border, height,           /* from top to bottom */
		    False);
	XClearArea (screen->display, tw, 
		    0, height - screen->border,	                  /* bottom */
		    width, screen->border,         /* all across the bottom */
		    False);

	/* round so that it is unlikely the screen will change size on  */
	/* small mouse movements.					*/
	rows = (height + FontHeight(screen) / 2 - border) /
	 FontHeight(screen);
	cols = (width + FontWidth(screen) / 2 - border - screen->scrollbar) /
	 FontWidth(screen);
	if (rows < 1) rows = 1;
	if (cols < 1) cols = 1;

	/* change buffers if the screen has changed size */
	if (screen->max_row != rows - 1 || screen->max_col != cols - 1) {
		register int savelines = screen->scrollWidget ?
		 screen->savelines : 0;
		
		if(screen->cursor_state)
			HideCursor();
		if (screen->altbuf) 
			Reallocate(&screen->altbuf, (Char **)&screen->abuf_address,
			 rows, cols, screen->max_row + 1, screen->max_col + 1);
		Reallocate(&screen->allbuf, (Char **)&screen->sbuf_address,
		 rows + savelines, cols,
		 screen->max_row + 1 + savelines, screen->max_col + 1);
		screen->buf = &screen->allbuf[2 * savelines];
		 
		screen->max_row = rows - 1;
		screen->max_col = cols - 1;
	
		/* adjust scrolling region */
		screen->top_marg = 0;
		screen->bot_marg = screen->max_row;
		*flags &= ~ORIGIN;
	
		if (screen->cur_row > screen->max_row)
			screen->cur_row = screen->max_row;
		if (screen->cur_col > screen->max_col)
			screen->cur_col = screen->max_col;
	
		screen->fullVwin.height = height - border;
		screen->fullVwin.width = width - border - screen->scrollbar;

	} else if(FullHeight(screen) == height && FullWidth(screen) == width)
	 	return(0);	/* nothing has changed at all */

	if(screen->scrollWidget)
		ResizeScrollBar(screen->scrollWidget, -1, -1, height);
	
	screen->fullVwin.fullheight = height;
	screen->fullVwin.fullwidth = width;
	ResizeSelection (screen, rows, cols);
#ifdef sun
#ifdef TIOCSSIZE
	/* Set tty's idea of window size */
	ts.ts_lines = rows;
	ts.ts_cols = cols;
	ioctl (screen->respond, TIOCSSIZE, &ts);
#ifdef SIGWINCH
	if(screen->pid > 1) {
		int	pgrp;
		
		if (ioctl (screen->respond, TIOCGPGRP, &pgrp) != -1)
			killpg(pgrp, SIGWINCH);
	}
#endif	/* SIGWINCH */
#endif	/* TIOCSSIZE */
#else	/* sun */
#ifdef TIOCSWINSZ
	/* Set tty's idea of window size */
	ws.ws_row = rows;
	ws.ws_col = cols;
	ws.ws_xpixel = width;
	ws.ws_ypixel = height;
	ioctl (screen->respond, TIOCSWINSZ, (char *)&ws);
#ifdef notdef	/* change to SIGWINCH if this doesn't work for you */
	if(screen->pid > 1) {
		int	pgrp;
		
		if (ioctl (screen->respond, TIOCGPGRP, &pgrp) != -1)
			killpg(pgrp, SIGWINCH);
	}
#endif	/* SIGWINCH */
#endif	/* TIOCSWINSZ */
#endif	/* sun */
	return (0);
}

