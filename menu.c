#ifdef SCCS
static char sccsid[]="@(#)menu.c	1.7 Stellar 87/10/16";
#endif
/*
 *	$XConsortium: menu.c,v 1.11 88/11/16 13:47:32 rws Exp $
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

#include <stdio.h>
#ifdef MODEMENU
#include <X11/Xlib.h>
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include "menu.h"
#include <setjmp.h>
#include "ptyx.h"
#include "data.h"

#ifndef lint
static char rcs_id[] = "$XConsortium: menu.c,v 1.11 88/11/16 13:47:32 rws Exp $";
#endif	lint

#define DEFMENUBORDER	2
#define DEFMENUPAD	3

#define XOR(a,b)	((a&(~b)) | ((~a)&b))

#define	SetStateFlags(item)	item->itemFlags = (item->itemFlags &\
				 ~(itemStateMask | itemChanged)) |\
				 ((item->itemFlags & itemSetMask) >>\
				 itemSetMaskShift)


static char Check_MarkBits[] = {
   0x00, 0x01, 0x80, 0x01, 0xc0, 0x00, 0x60, 0x00,
   0x31, 0x00, 0x1b, 0x00, 0x0e, 0x00, 0x04, 0x00
};

static GC MenuGC, MenuInverseGC, MenuInvertGC, MenuGrayGC;
static int gotGCs = FALSE;

Pixmap Gray_Tile, Check_Normal_Tile, Check_Inverse_Tile, Check_Tile;
Menu Menu_Default;
Cursor Menu_DefaultCursor;
char *Menu_DefaultFont;

static int default_menuBorder = DEFMENUBORDER;
static int default_menuPad = DEFMENUPAD;

static XtResource resourceList[] = {
  {"menuBorder", "MenuBorder", XtRInt, sizeof(int),
     XtOffset (Menu *, menuBorderWidth), XtRInt, (caddr_t) &default_menuBorder},
  {"menuFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
     XtOffset (Menu *, menuFontInfo), XtRString, NULL},
  {"menuPad", "MenuPad", XtRInt, sizeof(int),
     XtOffset (Menu *, menuItemPad), XtRInt, (caddr_t) &default_menuPad}
};

/*
 * AddMenuItem() adds a menu item to an existing menu, at the end of the
 * list, which are number sequentially from zero.  The menuitem index is
 * return, or -1 if failed.
 */

AddMenuItem(menu, text)
register Menu *menu;
register char *text;
{
	register MenuItem *menuitem, **next;
	register int i;
	extern char *malloc();

	if(!menu || !text || (menuitem = (MenuItem *)malloc(sizeof(MenuItem)))
	 == (MenuItem *)0)
		return(-1);
	bzero((char *)menuitem, sizeof(MenuItem));
	menuitem->itemText = text;
	menuitem->itemTextLength = strlen(text);
	for(i = 0, next = &menu->menuItems ; *next ; i++)
		next = &(*next)->nextItem;
	*next = menuitem;
	menu->menuFlags |= menuChanged;
	return(i);
}

InitMenu(name)
register char *name;
{
	register XtermWidget xw = term;
	register char *cp;
	Display *dpy = XtDisplay(xw);
	Pixel background = xw->core.background_pixel;
	Pixel foreground = xw->screen.foreground;

	/*
	 * If the gray tile hasn't been set up, do it now.
	 */
	if(!Gray_Tile) 
		Gray_Tile = XtGrayPixmap(XtScreen(xw));
	if (!Check_Tile) {
	        Check_Normal_Tile = Make_tile(checkMarkWidth, checkMarkHeight,
		  Check_MarkBits, foreground, background,
		  DefaultDepth(dpy, DefaultScreen(dpy)));
	        Check_Inverse_Tile = Make_tile(checkMarkWidth, checkMarkHeight,
		  Check_MarkBits, background, foreground,
		  DefaultDepth(dpy, DefaultScreen(dpy)));
		Check_Tile = Check_Normal_Tile;
        }
	Menu_Default.menuFlags = menuChanged;
	Menu_Default.menuInitialItem = -1;
	XtGetSubresources(xw, (caddr_t)&Menu_Default, "menu", "Menu",
           resourceList, XtNumber(resourceList), NULL, 0);

	if (Menu_Default.menuFontInfo == NULL) {
	    if (xw->screen.fnt_norm) {
		Menu_Default.menuFontInfo = xw->screen.fnt_norm;
		MenugcFontMask = VTgcFontMask;
	    } else {
		Display *dpy = XtDisplay (xw);
		Menu_Default.menuFontInfo =
		  XQueryFont (dpy, DefaultGC (dpy, DefaultScreen (dpy))->gid);
		MenugcFontMask = 0;
	    }
	}
};

/*
 * ItemFlags returns the state of item "n" of the menu.
 */
ItemFlags(menu, n)
register Menu *menu;
register int n;
{
	register MenuItem *item;

	if(!menu || !menu->menuItems || n < 0)
		return(-1);
	for(item = menu->menuItems ; n > 0 ; n--)
		if(!(item = item->nextItem))
			return(0);
	return((item->itemFlags & itemSetMask) >> itemSetMaskShift);
}

/*
 * ItemText changes the text of item "n" of the menu.
 */
ItemText(menu, n, text)
register Menu *menu;
register int n;
char *text;
{
	register MenuItem *item;

	if(!menu || !menu->menuItems || n < 0 || !text)
		return(0);
	for(item = menu->menuItems ; n > 0 ; n--)
		if(!(item = item->nextItem))
			return(0);
	item->itemText = text;
	menu->menuFlags |= menuChanged;
	return(1);
}

/*
 * NewMenu() returns a pointer to an initialized new Menu structure, or NULL
 * if failed.
 *
 * The Menu structure _menuDefault contains the default menu settings.
 */
Menu *NewMenu (name)
char *name;
{
	register Menu *menu;
	register XtermWidget xw = term;
	XGCValues xgc;
	extern char *malloc();
	extern XFontStruct *XLoadQueryFont();
	register Display *dpy = XtDisplay(xw);
	Pixel background = xw->core.background_pixel;
	Pixel foreground = xw->screen.foreground;

	/*
	 * If the GrayTile hasn't been defined, InitMenu() was never
	 * run, so exit.
	 */
	if(!Gray_Tile)
		return((Menu *)0);
	/*
	 * Allocate the memory for the menu structure.
	 */
	if((menu = (Menu *)malloc(sizeof(Menu))) == (Menu *)0)
		return((Menu *)0);
	/*
	 * Initialize to default values.
	 */
	*menu = Menu_Default;
	/*
	 * If the menu cursor hasn't been given, make a default one.
	 */
	if(!menu->menuCursor) {
		if(!Menu_DefaultCursor) {
			if(!(Menu_DefaultCursor =
			   XCreateFontCursor(dpy, XC_left_ptr)))
				return((Menu *)0);
		}
		menu->menuCursor = Menu_DefaultCursor;
	}
	/*
	 * Initialze the default background and border pixmaps and foreground
	 * and background colors (black and white).
	 */
	menu->menuFgColor = foreground;
	menu->menuBgColor = background;
	
	if(!gotGCs) {
	        xgc.foreground = menu->menuFgColor;
	        xgc.function = GXinvert;
	        xgc.plane_mask = XOR(menu->menuFgColor, menu->menuBgColor);
	        MenuInvertGC = XCreateGC(dpy, DefaultRootWindow(dpy),
        	  GCForeground+GCFunction+GCPlaneMask, &xgc);
	        xgc.foreground = menu->menuFgColor;
	        xgc.background = menu->menuBgColor;
	        xgc.font = menu->menuFontInfo->fid;
	        xgc.function = GXcopy;
	        xgc.fill_style = FillSolid;
	        MenuGC = XCreateGC(dpy, DefaultRootWindow(dpy),
		 MenugcFontMask+GCForeground+GCBackground+GCFunction+GCFillStyle,
		 &xgc);
	        xgc.foreground = menu->menuBgColor;
	        xgc.background = menu->menuFgColor;
	        xgc.font = menu->menuFontInfo->fid;
	        xgc.function = GXcopy;
	        xgc.fill_style = FillSolid;
	        MenuInverseGC = XCreateGC(dpy, DefaultRootWindow(dpy),
		 MenugcFontMask+GCForeground+GCBackground+GCFunction+GCFillStyle,
		 &xgc);
	        xgc.foreground = menu->menuFgColor;
	        xgc.background = menu->menuBgColor;
		xgc.function = GXcopy;
	        xgc.stipple = Gray_Tile;
	        xgc.fill_style = FillStippled;
	        MenuGrayGC = XCreateGC(dpy, DefaultRootWindow(dpy),
		 GCStipple+GCFillStyle+MenugcFontMask
				       +GCForeground+GCBackground+GCFunction, 
		 &xgc);
		gotGCs = TRUE;
	}
	/*
	 * Set the menu title.  If name is NULL or is an empty string, no
	 * title will be displayed.
	 */
	if(name && *name) {
		menu->menuTitleLength = strlen(menu->menuTitle = name);
		menu->menuTitleWidth = XTextWidth(menu->menuFontInfo, name, 
		 menu->menuTitleLength);
		menu->menuItemTop = menu->menuFontInfo->ascent + 
		 menu->menuFontInfo->descent + 2 * menu->menuItemPad + 1;
	} else
		menu->menuTitleLength = menu->menuTitleWidth =
		 menu->menuItemTop = 0;
	return(menu);
}

/*
 * SetItemCheck sets the check state of item "n" of the menu to "state".
 */
SetItemCheck(menu, n, state)
register Menu *menu;
register int n;
int state;
{
	register MenuItem *item;

	if(!menu || !menu->menuItems || n < 0)
		return(0);
	for(item = menu->menuItems ; n > 0 ; n--)
		if(!(item = item->nextItem))
			return(0);
	if(state)
		item->itemFlags |= itemSetChecked;
	else
		item->itemFlags &= ~itemSetChecked;
	if(((item->itemFlags & itemSetMask) >> itemSetMaskShift) !=
	 (item->itemFlags & itemStateMask)) {
		item->itemFlags |= itemChanged;
		menu->menuFlags |= menuItemChanged;
	} else
		item->itemFlags &= ~itemChanged;
	return(1);
}

/*
 * SetItemDisable sets the disable state of item "n" of the menu to "state".
 */
SetItemDisable(menu, n, state)
register Menu *menu;
register int n;
int state;
{
	register MenuItem *item;

	if(!menu || !menu->menuItems || n < 0)
		return(0);
	for(item = menu->menuItems ; n > 0 ; n--)
		if(!(item = item->nextItem))
			return(0);
	if(state)
		item->itemFlags |= itemSetDisabled;
	else
		item->itemFlags &= ~itemSetDisabled;
	if(((item->itemFlags & itemSetMask) >> itemSetMaskShift) !=
	 (item->itemFlags & itemStateMask)) {
		item->itemFlags |= itemChanged;
		menu->menuFlags |= menuItemChanged;
	} else
		item->itemFlags &= ~itemChanged;
	return(1);
}

static Menu *menu;
static MenuItem *item;
static int i;
static MenuItem *hilited_item;
static int drawn;
static int changed;
int y, n, hilited_y, hilited_n, in_window;
static MenuItem *Mouse_InItem(), *Y_InItem();
static Unmap_Menu();


/*ARGSUSED*/
void MenuExposeWindow(w, closure, event)
Widget w;
caddr_t closure;
XEvent *event;
{
	register XtermWidget xw = (XtermWidget) w;
	/*
	 * If we have a saved pixmap, display it.  Otherwise
	 * redraw the menu and save it away.
	 */
	if (event->type == NoExpose) return;
	Draw_Menu(menu);

	/*
	 * If the menu has changed in any way and we want to
	 * save the menu, throw away any existing save menu
	 * image and make a new one.
	 */
	XFlush(XtDisplay(xw));

	/*
	 * See which item the cursor may currently be in.  If
	 * it is in a non-disabled item, hilite it.
	 */
	if(hilited_item = Mouse_InItem(menu, &hilited_y, &hilited_n))
		XFillRectangle(XtDisplay(xw), menu->menuWindow, 
		 MenuInvertGC, 0, hilited_y,
		 menu->menuWidth, hilited_item->itemHeight);
	drawn++;
}

/*ARGSUSED*/
void MenuMouseMoved(w, closure, event)
Widget w;
caddr_t closure;
XMotionEvent *event;
{
	register XtermWidget xw = (XtermWidget) w;
	if(!drawn || !in_window)
		return;
	/*
	 * See which item the cursor may currently be in.  If
	 * the item has changed, unhilite the old one and
	 * then hilited the new one.
	 */
	y = event->y;
	if((item = Y_InItem(menu, &y, &n)) != hilited_item) {
		if(hilited_item)
			XFillRectangle(XtDisplay(xw), menu->menuWindow, 
			 MenuInvertGC, 0, hilited_y,
			 menu->menuWidth, hilited_item->itemHeight);
		if(hilited_item = item) {
			XFillRectangle(XtDisplay(xw), menu->menuWindow, 
			 MenuInvertGC, 0,
			 hilited_y = y, menu->menuWidth, item->itemHeight);
			hilited_n = n;
		}
	}
}

/*ARGSUSED*/
void MenuEnterWindow(w, closure, event)
Widget w;
caddr_t closure;
XEvent *event;
{
	in_window = TRUE;
	MenuMouseMoved(w, closure, event);
}

/*ARGSUSED*/
void MenuLeaveWindow(w, closure, event)
Widget w;
caddr_t closure;
XEvent *event;
{
	register XtermWidget xw = (XtermWidget) w;
	if(!drawn)
		return;
	/*
	 * Unhilite any window that is currently hilited.
	 */
	if(hilited_item) {
		XFillRectangle(XtDisplay(xw), menu->menuWindow, 
		 MenuInvertGC, 0, hilited_y,
		 menu->menuWidth, hilited_item->itemHeight);
		hilited_item = (MenuItem *)0;
	}
	in_window = FALSE;
}

/*ARGSUSED*/
void MenuButtonReleased(w, closure, event)
Widget w;
caddr_t closure;
XButtonEvent *event;
{
	register XtermWidget xw = (XtermWidget) w;
	extern FinishModeMenu();

	/*
	 * return the index number of any selected menu
	 * item.
	 */

	if (! AllButtonsUp(event->state, event->button))
		return;

	XUngrabPointer(XtDisplay(xw), CurrentTime);
	
	if(in_window) {
		y = event->y;
		if((item = Y_InItem(menu, &y, &n)) != hilited_item) {
		    if(hilited_item)
			XFillRectangle(XtDisplay(xw), menu->menuWindow, 
			 MenuInvertGC, 0,
			 hilited_y, menu->menuWidth,
			 hilited_item->itemHeight);
		    if(hilited_item = item) {
			XFillRectangle(XtDisplay(xw), menu->menuWindow, 
			 MenuInvertGC, 0,
			 hilited_y = y, menu->menuWidth,
			 hilited_item->itemHeight);
			hilited_n = n;
		    }
		}
	}
	XFlush(XtDisplay(xw));
	menu->menuFlags &= ~(menuChanged | menuItemChanged);
	Unmap_Menu(menu);
	drawn = 0;
	if(hilited_item)
		FinishModeMenu(menu->menuInitialItem = hilited_n,
			       event->time);
	else
		FinishModeMenu(-1, event->time);
}

/*
 * TrackMenu does most of the work of displaying the menu and tracking the
 * mouse.
 */
TrackMenu(lmenu, event)
register Menu *lmenu;
register XButtonPressedEvent *event;
{
	register XtermWidget xw = term;
	XButtonReleasedEvent ev;
	XSetWindowAttributes attr;

	menu = lmenu;
	hilited_item = (MenuItem *)0;
	/*
	 * Check that things are reasonable.
	 */
	if(!menu || !event || !menu->menuItems || event->type != ButtonPress)
		return(-1);
	/*
	 * Set the changed flag and clear the menu changed flags.
	 */
	changed = menu->menuFlags & (menuChanged | menuItemChanged);
	/*
	 * If the entire menu has changed, throw away any saved pixmap and
	 * then call RecalcMenu().
	 */
	if(changed & menuChanged) {
		if(!Recalc_Menu(menu))
			return(-1);
		changed &= ~menuItemChanged;
	}
	/*
	 * Now if the window was never created, go ahead and make it.  Otherwise
	 * if the menu has changed, resize the window.
	 */
	if(!menu->menuWindow) {
	        menu->menuWidget = XtCreatePopupShell ("Xterm Menu", transientShellWidgetClass, xw, NULL, 0);
		XtResizeWidget (menu->menuWidget, menu->menuWidth, menu->menuHeight, menu->menuBorderWidth);
		XtRealizeWidget (menu->menuWidget);
		menu->menuWindow = XtWindow (menu->menuWidget);
	        attr.override_redirect = True;
		attr.border_pixmap = XtGrayPixmap(XtScreen(xw));
		attr.background_pixel = menu->menuBgColor;
		attr.cursor = menu->menuCursor;
		attr.save_under = True;
		XChangeWindowAttributes (XtDisplay(xw), menu->menuWindow, 
					 (CWBorderPixmap | CWBackPixel | 
					  CWOverrideRedirect | CWCursor |
					  CWSaveUnder), &attr);

		XtAddEventHandler(menu->menuWidget, ExposureMask, FALSE,
                   MenuExposeWindow, NULL);
		XtAddEventHandler(menu->menuWidget, EnterWindowMask, FALSE,
                   MenuEnterWindow, NULL);
		XtAddEventHandler(menu->menuWidget, LeaveWindowMask, FALSE,
                   MenuLeaveWindow, NULL);
		XtAddEventHandler(menu->menuWidget, PointerMotionMask, FALSE,
                   MenuMouseMoved, NULL);
		XtAddEventHandler(menu->menuWidget, ButtonReleaseMask, FALSE,
                   MenuButtonReleased, NULL);

	} else if(changed & menuChanged)
	 	XtResizeWidget(menu->menuWidget, menu->menuWidth, menu->menuHeight,
                    menu->menuBorderWidth);
	/*
	 * Figure out where the menu is supposed to go, from the initial button
	 * press, and move the window there.  Then map the menu.
	 */
	if(!Move_Menu(menu, event) || !Map_Menu(menu))
		return(-1);

	in_window = TRUE;
	XGrabPointer(XtDisplay(xw), menu->menuWindow, FALSE,
	 ExposureMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask
	 | ButtonReleaseMask | ButtonPressMask,
	 GrabModeAsync, GrabModeAsync, None, menu->menuCursor, CurrentTime
	 );
	return 0;
}

/*
 * Recalculate all of the various menu and item variables.
 */
static Recalc_Menu(menu)
register Menu *menu;
{
	register MenuItem *item;
	register int max, height, fontheight;

	/*
	 * We must have already gotten the menu font.
	 */
	if(!menu->menuFontInfo)
		return(0);
	/*
	 * Initialize the various max width variables.
	 */
	fontheight = menu->menuFontInfo->ascent + menu->menuFontInfo->descent;
	height = menu->menuItemTop;
	menu->menuMaxTextWidth = menu->menuTitleWidth;
	/*
	 * The item height is the maximum of the font height and the
	 * checkbox height.
	 */
	max = fontheight;
	if(checkMarkHeight > max)
		max = checkMarkHeight;
	/*
	 * Go through the menu item list.
	 */
	for(item = menu->menuItems ; item ; item = item->nextItem) {
		/*
		 * If the item text is a single dash, we assume this is
		 * a line separator and treat it special.
		 */
		if(XStrCmp(item->itemText, "-") == 0)
			height += (item->itemHeight = lineSeparatorHeight);
		else {
			height += (item->itemHeight = max);
			/*
			 * Check the text width with the max value stored in
			 * menu.
			 */
			if((item->itemTextWidth = XTextWidth(
			  menu->menuFontInfo, item->itemText,
			  strlen(item->itemText))) > menu->menuMaxTextWidth)
				menu->menuMaxTextWidth = item->itemTextWidth;
		}
		/*
		 * If the itemChanged flag is set, set the state bits.
		 */
		if(item->itemFlags & itemChanged) {
			item->itemFlags = (item->itemFlags & ~itemStateMask) |
			 ((item->itemFlags & itemSetMask) >> itemSetMaskShift);
			item->itemFlags &= ~itemChanged;
		}
	}
	/*
	 * Set the menu height and then set the menu width.
	 */
	menu->menuHeight = height;
	menu->menuWidth = 3 * menu->menuItemPad + menu->menuMaxTextWidth +
	 checkMarkWidth;
	return(1);
}

/*
 * Figure out where to popup the menu, relative to the where the button was
 * pressed.
 */
static Move_Menu(menu, ev)
register Menu *menu;
XButtonPressedEvent *ev;
{
	register int n, x, y;
	register XtermWidget xw = term;
	int total_width;
	/*
	 * Try to popup the menu so that the cursor is centered within the
	 * width of the menu, but compensate if that would run it outside
	 * the display area.
	 */
	total_width = menu->menuWidth + 2 * menu->menuBorderWidth;
	if((x = ev->x_root - total_width / 2) < 0)
		x = 0;
	else if(x + total_width > DisplayWidth(XtDisplay(xw),
					       DefaultScreen(XtDisplay(xw))))
		x = DisplayWidth(XtDisplay(xw),
				 DefaultScreen(XtDisplay(xw))) - total_width;
	/*
	 * If the menu extends above outside of the display, warp
	 * the mouse vertically so the menu will all show up.
	 */
	if((y = ev->y_root) < 0) {
/* don't bother warping pointer 
		XWarpPointer(XtDisplay(xw), None, 
		  DefaultRootWindow(XtDisplay(xw)), 0, 0, 0, 0, ev->x_root, 
		    0);
*/
		y = 0;
	} else if((n = y + menu->menuHeight + 2 * menu->menuBorderWidth - 
	  DisplayHeight(XtDisplay(xw), DefaultScreen(XtDisplay(xw)))) > 0) {
/* don't bother warping pointer 
		XWarpPointer(XtDisplay(xw), None,
		 DefaultRootWindow(XtDisplay(xw)), 0, 0, 0, 0, ev->x_root, 
		  ev->y_root - n);
*/
		y -= n;
	}
	XtMoveWidget(menu->menuWidget, x, y);
	return(1);
}

/*
 * Map the menu window.
 */
static Map_Menu(menu)
register Menu *menu;
{
	register XtermWidget xw = term;

	/*
	 * Actually map the window.
	 */
        XRaiseWindow (XtDisplay(xw), menu->menuWindow);
        XtPopup (menu->menuWidget, XtGrabNone);
	menu->menuFlags |= menuMapped;
	return(1);
}

/*
 * Draw the entire menu in the blank window.
 */
static Draw_Menu(menu)
register Menu *menu;
{
	register MenuItem *item;
	register int top = menu->menuItemTop;
	register int x = menu->menuItemPad;
	register int dim;
	register XtermWidget xw = term;

	/*
	 * If we have a menu title, draw it first, centered and hilited.
	 */
	if(menu->menuTitleLength) {
		XFillRectangle(XtDisplay(xw), menu->menuWindow, 
		 MenuGC, 0, 0, menu->menuWidth, top - 1);
		XDrawImageString(XtDisplay(xw), menu->menuWindow, 
		 MenuInverseGC, (menu->menuWidth -
		 menu->menuTitleWidth) / 2, 
		 menu->menuItemPad+menu->menuFontInfo->ascent, 
		 menu->menuTitle, menu->menuTitleLength);
	}
	/*
	 * For each item in the list, first draw any check mark and then
	 * draw the rest of it.
	 */
	for(item = menu->menuItems ; item ; item = item->nextItem) {
		SetStateFlags(item);
		dim = (item->itemFlags & itemDisabled);
		/*
		 * Draw the check mark, possibly dimmed, wherever is necessary.
		 */
		if(item->itemFlags & itemChecked) {
			XCopyArea(XtDisplay(xw), 
			 Check_Tile, menu->menuWindow, 
			 dim ? MenuGrayGC : MenuGC,
			 0, 0, checkMarkWidth, checkMarkHeight, x, 
			 top + (item->itemHeight - checkMarkHeight) / 2);
		}
		/*
		 * Draw the item, possibly dimmed.
		 */
		Draw_Item(menu, item, top, dim);
		top += item->itemHeight;
	}
}

/*
 * Modify the item at vertical position y.  This routine is table driven and
 * the state and set bits are each 2 bits long, contiguous, the least
 * significant bits in the flag word and with the state bits in bits 0 & 1.
 */

#define	drawCheck	0x10
#define	removeCheck	0x08
#define	dimCheck	0x04
#define	drawItem	0x02
#define	dimItem		0x01

static char Modify_Table[] = {
	0x00, 0x02, 0x08, 0x0a, 0x01, 0x00, 0x09, 0x08,
	0x10, 0x12, 0x00, 0x12, 0x15, 0x14, 0x05, 0x00
};
	
static Modify_Item(menu, item, top)
register Menu *menu;
register MenuItem *item;
int top;
{
	register int x = menu->menuItemPad;
	register int y;
	register int center = top + item->itemHeight / 2;
	register int func = Modify_Table[item->itemFlags &
	 (itemStateMask | itemSetMask)];
	register XtermWidget xw = term;

	/*
	 * If we really won't be making a change, return.
	 */
	if(func == 0)
		return;
	/*
	 * Draw the check mark if needed, possibly dimmed.
	 */
	y = center - (checkMarkHeight / 2);
	if(func & (drawCheck | dimCheck))
		XCopyArea(XtDisplay(xw), 
		 Check_Tile, menu->menuWindow, 
		 (func & dimCheck) ? MenuGrayGC : MenuGC,
		 0, 0, checkMarkWidth, checkMarkHeight, x, 
		  y = top + (item->itemHeight - checkMarkHeight) / 2);
	/*
	 * Remove the check mark if needed.
	 */
	if(func & removeCheck)
		XClearArea(XtDisplay(xw), menu->menuWindow, 
		 x, y, checkMarkWidth, checkMarkHeight, FALSE);
	/*
	 * Call Draw_Item if we need to draw or dim the item.
	 */
	if((x = func & dimItem) || (func & drawItem))
		Draw_Item(menu, item, top, x);
	/*
	 * Update state flags.
	 */
	SetStateFlags(item);
}

/*
 * Draw the item (less check mark) at vertical position y.
 * Dim the item if "dim" is set.
 */
static Draw_Item(menu, item, y, dim)
register Menu *menu;
register MenuItem *item;
register int y;
int  dim;
{
	register int x = 2 * menu->menuItemPad + checkMarkWidth;
	register int center = y + item->itemHeight / 2;
	register XtermWidget xw = term;

	/*
	 * If the item text is a single dash, draw a separating line.
	 */
	if(XStrCmp(item->itemText, "-") == 0) {
		XDrawLine(XtDisplay(xw), menu->menuWindow,  MenuGC,
		 0, center, menu->menuWidth, center);
		return;
	}
	/*
	 * Draw and/or dim the text, centered vertically.
	 */
	y = center - 
	 ((menu->menuFontInfo->ascent + menu->menuFontInfo->descent)/ 2);
	if(dim) {
		XDrawString(XtDisplay(xw), menu->menuWindow, MenuGrayGC,
		 x, y+menu->menuFontInfo->ascent, 
		 item->itemText, item->itemTextLength);
	} else
		XDrawImageString(XtDisplay(xw), menu->menuWindow, 
		 MenuGC, x, y+menu->menuFontInfo->ascent, 
		 item->itemText, item->itemTextLength);
}

/*
 * Determine which enabled menu item the mouse is currently in.  Return the
 * top position of this item and its item number.  Set inwindow to whether
 * we are or not.
 */
static MenuItem *Mouse_InItem(menu, top, n)
register Menu *menu;
int *top, *n;
{
	int x, y, rootx, rooty, mask;
	Window subw, root;
	static MenuItem *Y_InItem();
	register XtermWidget xw = term;

	/*
	 * Find out where the mouse is.  If its not in the menu window,
	 * return NULL.
	 */
	XQueryPointer(XtDisplay(xw), menu->menuWindow, 
	&root, &subw, &rootx, &rooty, &x, &y, &mask);
	if((x <0) || (y < 0) || 
	   (x > menu->menuWidth) || (y > menu->menuHeight)) {
		return((MenuItem *)0);
	}
	/*
	 * Call Y_InItem().
	 */
	*top = y;
	return(Y_InItem(menu, top, n));
}

/*
 * Return which enabled item the locator is in.  Also return the
 * top position of this item and its item number.  Initial y passed
 * in top.
 */
static MenuItem *Y_InItem(menu, top, n)
register Menu *menu;
int *top, *n;
{
	register MenuItem *item;
	register int t, i;
	register int y = *top;

	/*
	 * Go through the item list.  "t" is the vertical position of the
	 * current item and "i" is its item number.
	 */
	t = menu->menuItemTop;
	/*
	 * If the mouse is before the first item, return.
	 */
	if(y < t)
		return((MenuItem *)0);
	for(i = 0, item = menu->menuItems ; item ; i++, item = item->nextItem) {
		/*
		 * If the y coordinate is within this menu item, then return.
		 * But don't return disable items.
		 */
		if(t + item->itemHeight > y) {
			if(item->itemFlags & itemDisabled)
				return((MenuItem *)0);
			*top = t;
			*n = i;
			return(item);
		}
		t += item->itemHeight;
	}
	/*
	 * Should never get here.
	 */
	return((MenuItem *)0);
}

/*
 * Unmap_Menu() unmaps a menu, if it is currently mapped.
 */
static Unmap_Menu(menu)
register Menu *menu;
{
	register int i;
	register XtermWidget xw = term;

	if(!menu || !(menu->menuFlags & menuMapped))
		return;
	XtPopdown (menu->menuWidget);
	menu->menuFlags &= ~menuMapped;
}

MenuResetGCs (bgp, fgp)
    Pixel *bgp, *fgp;
{
    register XtermWidget xw = term;
    Display *dpy = XtDisplay (xw);
    XGCValues xgc;

    *bgp = xw->core.background_pixel;
    *fgp = xw->screen.foreground;

    if (MenuInvertGC) {
	xgc.foreground = *fgp;
	xgc.plane_mask = XOR(*fgp, *bgp);
	XChangeGC (dpy, MenuInvertGC, (GCForeground | GCPlaneMask), &xgc);
    }

    if (MenuGC) {
	xgc.foreground = *fgp;
	xgc.background = *bgp;
	XChangeGC (dpy, MenuGC, (GCForeground | GCBackground), &xgc);
    }

    if (MenuInverseGC) {
	xgc.foreground = *bgp;
	xgc.background = *fgp;
	XChangeGC (dpy, MenuInverseGC, (GCForeground | GCBackground), &xgc);
    }

    if (MenuGrayGC) {
	xgc.foreground = *fgp;
	xgc.background = *bgp;
	xgc.function = *fgp ? GXor : GXand;
	XChangeGC (dpy, MenuGrayGC,
		   (GCForeground | GCBackground | GCFunction), &xgc);
    }

    if (Check_Tile) {
	Check_Tile = ((Check_Tile == Check_Normal_Tile) ? Check_Inverse_Tile :
		      Check_Normal_Tile);
    }

    return;
}

#endif MODEMENU
