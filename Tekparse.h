/*
 *	@Header: Tekparse.h,v 1.1 88/02/11 22:08:38 jim Exp @
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


/* @(#)Tekparse.h	X10/6.6	11/7/86 */
#define	CASE_REPORT	0
#define	CASE_VT_MODE	(CASE_REPORT + 1)
#define	CASE_SPT_STATE	(CASE_VT_MODE + 1)
#define	CASE_GIN	(CASE_SPT_STATE + 1)
#define	CASE_BEL	(CASE_GIN + 1)
#define	CASE_BS		(CASE_BEL + 1)
#define	CASE_PT_STATE	(CASE_BS + 1)
#define	CASE_PLT_STATE	(CASE_PT_STATE + 1)
#define	CASE_TAB	(CASE_PLT_STATE + 1)
#define	CASE_IPL_STATE	(CASE_TAB + 1)
#define	CASE_ALP_STATE	(CASE_IPL_STATE + 1)
#define	CASE_UP		(CASE_ALP_STATE + 1)
#define	CASE_COPY	(CASE_UP + 1)
#define	CASE_PAGE	(CASE_COPY + 1)
#define	CASE_BES_STATE	(CASE_PAGE + 1)
#define	CASE_BYP_STATE	(CASE_BES_STATE + 1)
#define	CASE_IGNORE	(CASE_BYP_STATE + 1)
#define	CASE_ASCII	(CASE_IGNORE + 1)
#define	CASE_APL	(CASE_ASCII + 1)
#define	CASE_CHAR_SIZE	(CASE_APL + 1)
#define	CASE_BEAM_VEC	(CASE_CHAR_SIZE + 1)
#define	CASE_CURSTATE	(CASE_BEAM_VEC + 1)
#define	CASE_PENUP	(CASE_CURSTATE + 1)
#define	CASE_PENDOWN	(CASE_PENUP + 1)
#define	CASE_IPL_POINT	(CASE_PENDOWN + 1)
#define	CASE_PLT_VEC	(CASE_IPL_POINT + 1)
#define	CASE_PT_POINT	(CASE_PLT_VEC + 1)
#define	CASE_SPT_POINT	(CASE_PT_POINT + 1)
#define	CASE_CR		(CASE_SPT_POINT + 1)
#define	CASE_ESC_STATE	(CASE_CR + 1)
#define	CASE_LF		(CASE_ESC_STATE + 1)
#define	CASE_SP		(CASE_LF + 1)
#define	CASE_PRINT	(CASE_SP + 1)
#define	CASE_OSC	(CASE_PRINT + 1)
