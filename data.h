/*
 *	@Source: /orpheus/u1/X11/clients/xterm/RCS/data.h,v @
 *	@Header: data.h,v 1.10 87/08/16 16:32:52 swick Exp @
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

/* @(#)data.h\tX10/6.6\t11/10/86 */

#include <X11/Intrinsic.h>

extern TekLink *TekRefresh;
extern Terminal term;
extern XPoint T_box2[];
extern XPoint T_box3[];
extern XPoint T_boxlarge[];
extern XPoint T_boxsmall[];
extern XPoint VTbox[];
extern T_fontsize Tfontsize[];
extern char *T_geometry;
extern char *Tbptr;
extern char *Tbuffer;
extern char *Tpushb;
extern char *Tpushback;
extern char *bptr;
extern char *curs_shape;
extern char *f_b;
extern char *f_n;
extern char *f_t;
extern char *geo_metry;
extern char *icon_geom;
extern Boolean iconstartup;
extern char log_def_name[];
extern char *ptydev;
extern char *ttydev;
extern char *window_name;
extern char *title_name;
extern char *xterm_name;
extern char buffer[];
extern int L_flag;
extern int Select_mask;
extern int T_lastx;
extern int T_lasty;
extern int Tbcnt;
extern int Ttoggled;
extern int X_mask;
extern int am_slave;
extern int bcnt;
#ifdef DEBUG
extern int debug;
#endif DEBUG
extern int errno;
extern int max_plus1;
extern int pty_mask;
extern int re_verse;
extern int save_lines;
extern int switchfb[];
extern jmp_buf Tekend;
extern jmp_buf VTend;

extern int waitingForTrackInfo;

extern EventMode eventMode;
extern Window menuWindow;

extern GC visualBellGC;

extern int VTgcFontMask;
extern int TEKgcFontMask;
extern int MenugcFontMask;
