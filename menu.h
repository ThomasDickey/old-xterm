/*		@(#)menu.h	1.2 Stellar 87/10/07	*/
/*
 *	$XConsortium: menu.h,v 1.3 88/09/06 17:08:17 jim Exp $
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

/* @(#)menu.h	X10/6.6	11/3/86 */
/*
 * Menu items are constructed as follows, starting from the left side:
 *
 *	menuItemPad
 *	space for check mark
 *	menuItemPad
 *	text + padding
 *	menuItemPad
 *
 * The padding for the text is that amount that this text is narrower than the
 * widest text.
 */

typedef struct _menuItem {
	int itemHeight;			/* total height of this item */
	int itemFlags;			/* flags of item */

#define	itemDisabled		0x0001	/* item is disabled */
#define	itemChecked		0x0002	/* item has check mark */
#define	itemStateMask		0x0003	/* mask for current state */
#define	itemSetDisabled		0x0004	/* item wants to be disabled */
#define	itemSetChecked		0x0008	/* item wants check mark */
#define	itemSetMask		0x000c	/* mask for desired state */
#define	itemSetMaskShift	2	/* for comparison with actual */
#define	itemChanged		0x0010	/* item desires change */

	char *itemText;			/* text of item */
	int itemTextWidth;		/* width of text */
	int itemTextLength;		/* length of text */
	struct _menuItem *nextItem;	/* next item in chain */
} MenuItem;

typedef struct _menu {
	int menuWidth;			/* full width of menu */
	int menuHeight;			/* full height of menu */
	int menuFlags;			/* flags of this menu */

# define	menuChanged	0x0001		/* menu changed, must redraw */
# define	menuItemChanged	0x0002		/* item changed, must redraw */
# define	menuMapped	0x0004		/* menu is now mapped */

	int menuMaxTextWidth;		/* width of widest text */
	int menuInitialItem;		/* < 0 none, >= 0 initial item */
	int menuBorderWidth;		/* width of border */
	int menuBgColor;		/* background color */
	int menuFgColor;		/* foreground color */
	XFontStruct *menuFontInfo;	/* font info for menu font */
	int menuItemPad;		/* pad amount */
	Widget menuWidget;
	Window menuWindow;		/* window of menu */
	Cursor menuCursor;		/* cursor used in menu */
	MenuItem *menuItems;		/* head of menu item chain */
	char *menuTitle;		/* title of menu */
	int menuTitleWidth;		/* width of title */
	int menuTitleLength;		/* length of title */
	int menuItemTop;		/* position of top of first item */
} Menu;

#define	checkMarkWidth		9
#define	checkMarkHeight		8
#define	defaultCursorWidth	16
#define	defaultCursorHeight	16
#define	defaultCursorX		1
#define	defaultCursorY		1
#define	grayHeight		16
#define	grayWidth		16
#define	lineSeparatorHeight	9

#define	CheckItem(menu,item)	SetItemCheck(menu,item,1)
#define	DisableItem(menu,item)	SetItemDisable(menu,item,1)
#define	EnableItem(menu,item)	SetItemDisable(menu,item,0)
#define	UncheckItem(menu,item)	SetItemCheck(menu,item,0)

extern Menu *NewMenu();
