/*
 *	@Header: VTparse.h,v 1.1 88/02/11 22:08:38 jim Exp @
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

/* @(#)VTparse.h	X10/6.6	11/6/86 */
#define	CASE_GROUND_STATE	0
#define	CASE_IGNORE_STATE	(CASE_GROUND_STATE+1)
#define	CASE_IGNORE_ESC		(CASE_IGNORE_STATE+1)
#define	CASE_IGNORE		(CASE_IGNORE_ESC+1)
#define	CASE_BELL		(CASE_IGNORE+1)
#define	CASE_BS			(CASE_BELL+1)
#define	CASE_CR			(CASE_BS+1)
#define	CASE_ESC		(CASE_CR+1)
#define	CASE_VMOT		(CASE_ESC+1)
#define	CASE_TAB		(CASE_VMOT+1)
#define	CASE_SI			(CASE_TAB+1)
#define	CASE_SO			(CASE_SI+1)
#define	CASE_SCR_STATE		(CASE_SO+1)
#define	CASE_SCS0_STATE		(CASE_SCR_STATE+1)
#define	CASE_SCS1_STATE		(CASE_SCS0_STATE+1)
#define	CASE_SCS2_STATE		(CASE_SCS1_STATE+1)
#define	CASE_SCS3_STATE		(CASE_SCS2_STATE+1)
#define	CASE_ESC_IGNORE		(CASE_SCS3_STATE+1)
#define	CASE_ESC_DIGIT		(CASE_ESC_IGNORE+1)
#define	CASE_ESC_SEMI		(CASE_ESC_DIGIT+1)
#define	CASE_DEC_STATE		(CASE_ESC_SEMI+1)
#define	CASE_ICH		(CASE_DEC_STATE+1)
#define	CASE_CUU		(CASE_ICH+1)
#define	CASE_CUD		(CASE_CUU+1)
#define	CASE_CUF		(CASE_CUD+1)
#define	CASE_CUB		(CASE_CUF+1)
#define	CASE_CUP		(CASE_CUB+1)
#define	CASE_ED			(CASE_CUP+1)
#define	CASE_EL			(CASE_ED+1)
#define	CASE_IL			(CASE_EL+1)
#define	CASE_DL			(CASE_IL+1)
#define	CASE_DCH		(CASE_DL+1)
#define	CASE_DA1		(CASE_DCH+1)
#define CASE_TRACK_MOUSE	(CASE_DA1+1)
#define	CASE_TBC		(CASE_TRACK_MOUSE+1)
#define	CASE_SET		(CASE_TBC+1)
#define	CASE_RST		(CASE_SET+1)
#define	CASE_SGR		(CASE_RST+1)
#define	CASE_CPR		(CASE_SGR+1)
#define	CASE_DECSTBM		(CASE_CPR+1)
#define	CASE_DECREQTPARM	(CASE_DECSTBM+1)
#define	CASE_DECSET		(CASE_DECREQTPARM+1)
#define CASE_DECRST             (CASE_DECSET+1)
#define	CASE_DECALN		(CASE_DECRST+1)
#define	CASE_GSETS		(CASE_DECALN+1)
#define	CASE_DECSC		(CASE_GSETS+1)
#define	CASE_DECRC		(CASE_DECSC+1)
#define	CASE_DECKPAM		(CASE_DECRC+1)
#define	CASE_DECKPNM		(CASE_DECKPAM+1)
#define	CASE_IND		(CASE_DECKPNM+1)
#define	CASE_NEL		(CASE_IND+1)
#define	CASE_HTS		(CASE_NEL+1)
#define	CASE_RI			(CASE_HTS+1)
#define	CASE_SS2		(CASE_RI+1)
#define	CASE_SS3		(CASE_SS2+1)
#define	CASE_CSI_STATE		(CASE_SS3+1)
#define	CASE_OSC		(CASE_CSI_STATE+1)
#define	CASE_RIS		(CASE_OSC+1)
#define	CASE_LS2		(CASE_RIS+1)
#define	CASE_LS3		(CASE_LS2+1)
#define	CASE_LS3R		(CASE_LS3+1)
#define	CASE_LS2R		(CASE_LS3R+1)
#define	CASE_LS1R		(CASE_LS2R+1)
#define	CASE_PRINT		(CASE_LS1R+1)
#define	CASE_XTERM_SAVE		(CASE_PRINT+1)
#define	CASE_XTERM_RESTORE	(CASE_XTERM_SAVE+1)
#define CASE_XTERM_TITLE	(CASE_XTERM_RESTORE+1)
