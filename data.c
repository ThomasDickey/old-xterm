/*
 *	$XConsortium: data.c,v 1.4 88/09/06 17:08:01 jim Exp $
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

#include <setjmp.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include "ptyx.h"
#include "data.h"

#ifndef lint
static char rcs_id[] = "$XConsortium: data.c,v 1.4 88/09/06 17:08:01 jim Exp $";
#endif	/* lint */

XPoint T_boxlarge[NBOX] = {
	{0, 0},
	{8, 0},
	{0, 14},
	{-8, 0},
	{0, -14},
};
XPoint T_box2[NBOX] = {
	{0, 0},
	{7, 0},
	{0, 12},
	{-7, 0},
	{0, -12},
};
XPoint T_box3[NBOX] = {
	{0, 0},
	{5, 0},
	{0, 12},
	{-5, 0},
	{0, -12},
};
XPoint T_boxsmall[NBOX] = {
	{0, 0},
	{5, 0},
	{0, 9},
	{-5, 0},
	{0, -9},
};
jmp_buf Tekend;
int Tbcnt = 0;
char *Tbuffer;
char *Tbptr;
TekLink *TekRefresh;
char *Tpushb;
char *Tpushback;
int Ttoggled = 0;
int bcnt = 0;
char buffer[BUF_SIZE];
char *bptr = buffer;
jmp_buf VTend;
XPoint VTbox[NBOX] = {
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
};
T_fontsize Tfontsize[TEKNUMFONTS] = {
	{9, 15},	/* large */
	{8, 13},	/* #2 */
	{6, 13},	/* #3 */
	{6, 10},	/* small */
};


#ifdef DEBUG
int debug = 0; 		/* true causes error messages to be displayed */
#endif	/* DEBUG */
XtermWidget term;		/* master data structure for client */
char *xterm_name;	/* argv[0] */
int am_slave = 0;	/* set to 1 if running as a slave process */
int L_flag;
int max_plus1;
int pty_mask;
int Select_mask;
int X_mask;
char *ptydev;
char *ttydev;
char log_def_name[] = "XtermLog.XXXXX";
int T_lastx = -1;
int T_lasty = -1;

int waitingForTrackInfo = 0;
EventMode eventMode = NORMAL;

GC visualBellGC;

int VTgcFontMask = GCFont;
int TEKgcFontMask = GCFont;
int MenugcFontMask = GCFont;

TekWidget tekWidget;
