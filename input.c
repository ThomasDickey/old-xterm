/*
 *	@Source: /u1/X11/clients/xterm/RCS/input.c,v @
 *	@Header: input.c,v 1.14 87/09/11 08:16:45 toddb Exp @
 */

#ifndef lint
static char *rcsid_input_c = "@Header: input.c,v 1.14 87/09/11 08:16:45 toddb Exp @";
#endif	lint

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

/* input.c */

#ifndef lint
static char rcs_id[] = "@Header: input.c,v 1.14 87/09/11 08:16:45 toddb Exp @";
#endif	lint

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Intrinsic.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include "ptyx.h"

int MetaMode = 1;	/* prefix with ESC when Meta Key is down */

static XComposeStatus compose_status = {NULL, 0};
static char *kypd_num = " XXXXXXXX\tXXX\rXXXxxxxXXXXXXXXXXXXXXXXXXXXX*+,-.\\0123456789XXX=";
static char *kypd_apl = " ABCDEFGHIJKLMNOPQRSTUVWXYZ??????abcdefghijklmnopqrstuvwxyzXXX";
static char *cur = "DACB";

Input (keyboard, screen, event)
register TKeyboard	*keyboard;
register TScreen		*screen;
register XKeyPressedEvent *event;
{

#define STRBUFSIZE 100

	 char strbuf[STRBUFSIZE];
	register char *string;
	register int col, key = FALSE;
	int	pty	= screen->respond;
	int	nbytes;
	int 	keycode;
	ANSI	reply;

	nbytes = XLookupString (event, strbuf, STRBUFSIZE, 
		&keycode, &compose_status);

	string = &strbuf[0];
	reply.a_pintro = 0;
	reply.a_final = 0;
	reply.a_nparam = 0;
	reply.a_inters = 0;

	if (IsPFKey(keycode)) {
		reply.a_type = SS3;
		unparseseq(&reply, pty);
		unparseputc((char)(keycode-XK_KP_F1+'P'), pty);
		key = TRUE;
	} else if (IsKeypadKey(keycode)) {
	  	if (keyboard->flags & KYPD_APL)	{
			reply.a_type   = SS3;
			unparseseq(&reply, pty);
			unparseputc(kypd_apl[keycode-XK_KP_Space], pty);
		} else
			unparseputc(kypd_num[keycode-XK_KP_Space], pty);
		key = TRUE;
        } else if (IsCursorKey(keycode)) {
       		if (keyboard->flags & CURSOR_APL) {
			reply.a_type = SS3;
			unparseseq(&reply, pty);
			unparseputc(cur[keycode-XK_Left], pty);
		} else {
			reply.a_type = CSI;
			reply.a_final = cur[keycode-XK_Left];
			unparseseq(&reply, pty);
		}
		key = TRUE;
	 } else if (IsFunctionKey(keycode) || IsMiscFunctionKey(keycode)) {
		reply.a_type = CSI;
		reply.a_nparam = 1;
		reply.a_param[0] = funcvalue(keycode);
		reply.a_final = '~';
		if (reply.a_param[0] > 0)
			unparseseq(&reply, pty);
		key = TRUE;
	} else if (nbytes > 0) {
		if(screen->TekGIN) {
			TekEnqMouse(*string++);
			TekGINoff();
			nbytes--;
		}
		if ((nbytes == 1) && MetaMode && (event->state & Mod1Mask))
			unparseputc(033, pty);
		while (nbytes-- > 0)
			unparseputc(*string++, pty);
		key = TRUE;
	}
	if(key && !screen->TekEmu) {
		if(screen->scrollkey && screen->topline != 0)
			WindowScroll(screen, 0);
		if(screen->marginbell) {
			col = screen->max_col - screen->nmarginbell;
			if(screen->bellarmed >= 0) {
				if(screen->bellarmed == screen->cur_row) {
					if(screen->cur_col >= col) {
						if(screen->cur_col == col)
							Bell();
						screen->bellarmed = -1;
					}
				} else
					screen->bellarmed = screen->cur_col <
					 col ? screen->cur_row : -1;
			} else if(screen->cur_col < col)
				screen->bellarmed = screen->cur_row;
		}
	}
#ifdef ENABLE_PRINT
	if (keycode == XK_F2) TekPrint();
#endif
	return;
}

funcvalue(keycode)
{
	switch (keycode) {
		case XK_F1:	return(11);
		case XK_F2:	return(12);
		case XK_F3:	return(13);
		case XK_F4:	return(14);
		case XK_F5:	return(15);
		case XK_F6:	return(17);
		case XK_F7:	return(18);
		case XK_F8:	return(19);
		case XK_F9:	return(20);
		case XK_F10:	return(21);
		case XK_F11:	return(23);
		case XK_F12:	return(24);
		case XK_F13:	return(25);
		case XK_F14:	return(26);
		case XK_F15:	return(28);
		case XK_Help:	return(28);
		case XK_F16:	return(29);
		case XK_Menu:	return(29);
		case XK_F17:	return(31);
		case XK_F18:	return(32);
		case XK_F19:	return(33);
		case XK_F20:	return(34);

		case XK_Find :	return(1);
		case XK_Insert:	return(2);
		case XK_Delete:	return(3);
		case XK_Select:	return(4);
		case XK_Prior:	return(5);
		case XK_Next:	return(6);
		default:	return(-1);
	}
}
