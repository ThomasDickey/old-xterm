/*
 *	@Source: /u1/X11/clients/xterm/RCS/menu.c,v @
 *	@Header: menu.c,v 1.28 87/09/11 22:31:28 rws Exp @
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
#include <X11/Intrinsic.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xtlib.h>
#include "menu.h"
#include <setjmp.h>
#include "ptyx.h"
#include "data.h"

#ifndef lint
static char rcs_id[] = "@Header: menu.c,v 1.28 87/09/11 22:31:28 rws Exp @";
#endif	lint

#define DEFMENUBORDER	2
#define DEFMENUPAD	3

#define  XtNxterm                "xterm"
#define  XtCApp                  "App"

#define XOR(a,b)	((a&(~b)) | ((~a)&b))

#define	SetStateFlags(item)	item->itemFlags = (item->itemFlags &\
				 ~(itemStateMask | itemChanged)) |\
				 ((item->itemFlags & itemSetMask) >>\
				 itemSetMaskShift)


static char Check_MarkBits[] = {
   0x00, 0x01, 0x80, 0x01, 0xc0, 0x00, 0x60, 0x00,
   0x31, 0x00, 0x1b, 0x00, 0x0e, 0x00, 0x04, 0x00
};
static char Default_CursorBits[] = {
   0x00, 0x00, 0x02, 0x00, 0x06, 0x00, 0x0e, 0x00,
   0x1e, 0x00, 0x3e, 0x00, 0x7e, 0x00, 0xfe, 0x00,
   0xfe, 0x01, 0x3e, 0x00, 0x36, 0x00, 0x62, 0x00,
   0x60, 0x00, 0xc0, 0x00, 0xc0, 0x00, 0x00, 0x00
};
static char Default_MaskBits[] = {
   0x03, 0x00, 0x07, 0x00, 0x0f, 0x00, 0x1f, 0x00,
   0x3f, 0x00, 0x7f, 0x00, 0xff, 0x00, 0xff, 0x01,
   0xff, 0x03, 0xff, 0x07, 0x7f, 0x00, 0xf7, 0x00,
   0xf3, 0x00, 0xe1, 0x01, 0xe0, 0x01, 0xc0, 0x01
};

static GC MenuGC, MenuInverseGC, MenuInvertGC, MenuGrayGC;
static int gotGCs = FALSE;

Pixmap Gray_Tile, Check_Tile;
Menu Menu_Default;
Cursor Menu_DefaultCursor;
char *Menu_DefaultFont;

extern XrmNameList nameList;
extern XrmClassList classList;
extern EventDoNothing();

static int default_menuBorder = DEFMENUBORDER;
static int default_menuPad = DEFMENUPAD;
static char *defaultNULL = NULL;

static Resource resourceList[] = {
  {"menuBorder", "MenuBorder", XrmRInt, sizeof(int),
     (caddr_t) &Menu_Default.menuBorderWidth, (caddr_t) &default_menuBorder},
  {"menuFont", XtCFont, XrmRString, sizeof(char *),
     (caddr_t) &Menu_DefaultFont, (caddr_t) &defaultNULL},
  {"menuPad", "MenuPad", XrmRInt, sizeof(int),
     (caddr_t) &Menu_Default.menuItemPad, (caddr_t) &default_menuPad}
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

/*
 * DisposeItem() releases the memory allocated for the given indexed
 * menuitem.  Nonzero is returned if an item was actual disposed of.
 */
DisposeItem(menu, i)
register Menu *menu;
register int i;
{
	register MenuItem **next, **last, *menuitem;

	if(!menu || i < 0)
		return(0);
	next = &menu->menuItems;
	do {
		if(!*next)
			return(0);
		last = next;
		next = &(*next)->nextItem;
	} while(i-- > 0);
	menuitem = *last;
	*last = *next;
	free(menuitem);
	return(1);
}

/*
 * DisposeMenu() releases the memory allocated for the given menu.
 */
DisposeMenu(menu)
register Menu *menu;
{
	register TScreen *screen = &term.screen;
	static Unmap_Menu();

	if(!menu)
		return;
	if(menu->menuFlags & menuMapped)
		Unmap_Menu(menu);
	while(DisposeItem(menu, 0));
	if(menu->menuWindow)
		XDestroyWindow(screen->display, menu->menuWindow);
	if(menu->menuSaved)
		XFreePixmap(screen->display, menu->menuSaved);
	free(menu);
}

InitMenu(name)
register char *name;
{
	register TScreen *screen = &term.screen;
	register char *cp;
	extern Pixmap make_gray();
	Display *dpy = screen->display;

	/*
	 * If the gray tile hasn't been set up, do it now.
	 */
	if(!Gray_Tile) 
		Gray_Tile = make_gray(WhitePixel(dpy, DefaultScreen(dpy)), 
		BlackPixel(dpy, DefaultScreen(dpy)), 1);
	if (!Check_Tile) {
	        Check_Tile = Make_tile(checkMarkWidth, checkMarkHeight,
		  Check_MarkBits, BlackPixel(dpy, DefaultScreen(dpy)),
		  WhitePixel(dpy, DefaultScreen(dpy)),
		  DefaultDepth(dpy, DefaultScreen(dpy)));
        }
	Menu_Default.menuFlags = menuChanged;
/*	if((cp = XGetDefault(dpy, name, "MenuFreeze")) && XStrCmp(cp, "on") == 0)
		Menu_Default.menuFlags |= menuFreeze;
	if((cp = XGetDefault(dpy, name, "MenuSave")) && XStrCmp(cp, "on") == 0)
		Menu_Default.menuFlags |= menuSaveMenu;
*/
	Menu_Default.menuInitialItem = -1;
	XtGetResources( dpy, resourceList, XtNumber(resourceList),
		        (ArgList) NULL, 0, DefaultRootWindow(dpy),
		        XtNxterm, XtCApp, &nameList, &classList );

	/* would be nice if (Xrm)CvtStringToFontStruct didn't complain
	 * when the font didn't exist! */
	if (Menu_DefaultFont != NULL) {
	    Menu_Default.menuFontInfo = XLoadQueryFont(dpy, Menu_DefaultFont);
	    if (!Menu_Default.menuFontInfo)
	        Menu_DefaultFont = NULL;
	}

	/* actually, we need to know whether the fid is valid anyway */
	if (Menu_DefaultFont == NULL) {
	    Menu_Default.menuFontInfo = screen->fnt_norm;
	    MenugcFontMask = VTgcFontMask;
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
Menu *NewMenu(name, reverse)
char *name;
int reverse;
{
	register Menu *menu;
	register int fg, bg;
	register TScreen *screen = &term.screen;
	XGCValues xgc;
	extern char *malloc();
	extern XFontStruct *XLoadQueryFont();
	register Display *dpy = screen->display;

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
			if(reverse) {
				fg = WhitePixel(dpy, 
					DefaultScreen(dpy));
				bg = BlackPixel(dpy, 
					DefaultScreen(dpy));
			} else {
				fg = BlackPixel(dpy, 
					DefaultScreen(dpy));
				bg = WhitePixel(dpy, 
					DefaultScreen(dpy));
			}
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
	if(reverse) {
		menu->menuFgColor = WhitePixel(dpy, 
			DefaultScreen(dpy));
		menu->menuBgColor = BlackPixel(dpy, 
			DefaultScreen(dpy));
	} else {
		menu->menuFgColor = BlackPixel(dpy, 
			DefaultScreen(dpy));
		menu->menuBgColor = WhitePixel(dpy, 
			DefaultScreen(dpy));
	}
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
	        xgc.function = menu->menuFgColor ? GXor : GXand;
/*		xgc.function = GXcopy;*/
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

XtEventReturnCode MenuExposeWindow(event, eventdata)
register XEvent *event;
caddr_t eventdata;
{
	register TScreen *screen = &term.screen;
	/*
	 * If we have a saved pixmap, display it.  Otherwise
	 * redraw the menu and save it away.
	 */
	if (event->type == NoExpose) return;
	if(menu->menuSaved) {
		XCopyArea(screen->display, menu->menuSaved, menu->menuWindow, 
		 MenuGC, 0, 0,
		 menu->menuWidth, menu->menuHeight, 0, 0);
		/*
		 * If the menuItemChanged flag is still set,
		 * then we need to redraw certain menu items.
		 * ("i" is the vertical position of the top
		 * of the current item.)
		 */
		if(changed & menuItemChanged) {
			i = menu->menuItemTop;
			for(item = menu->menuItems ; item ;
			 item = item->nextItem) {
				if(item->itemFlags & itemChanged)
					Modify_Item(menu, item, i);
				i += item->itemHeight;
			}
		}
	} else
		Draw_Menu(menu);
	/*
	 * If the menu has changed in any way and we want to
	 * save the menu, throw away any existing save menu
	 * image and make a new one.
	 */
	XFlush(screen->display);
	if(changed && (menu->menuFlags & menuSaveMenu)) {
		if(menu->menuSaved)
			XFreePixmap(screen->display, menu->menuSaved);
/*		menu->menuSaved = XPixmapSave(screen->display, 
		 menu->menuWindow, 0, 0, menu->menuWidth, menu->menuHeight);
*/
	}
	/*
	 * See which item the cursor may currently be in.  If
	 * it is in a non-disabled item, hilite it.
	 */
	if(hilited_item = Mouse_InItem(menu, &hilited_y, &hilited_n))
		XFillRectangle(screen->display, menu->menuWindow, 
		 MenuInvertGC, 0, hilited_y,
		 menu->menuWidth, hilited_item->itemHeight);
	drawn++;
	return (XteventHandled);
}

XtEventReturnCode MenuMouseMoved(event, eventdata)
XButtonEvent *event;
caddr_t eventdata;
{
	register TScreen *screen = &term.screen;
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
			XFillRectangle(screen->display, menu->menuWindow, 
			 MenuInvertGC, 0, hilited_y,
			 menu->menuWidth, hilited_item->itemHeight);
		if(hilited_item = item) {
			XFillRectangle(screen->display, menu->menuWindow, 
			 MenuInvertGC, 0,
			 hilited_y = y, menu->menuWidth, item->itemHeight);
			hilited_n = n;
		}
	}
	return (XteventHandled);
}

XtEventReturnCode MenuEnterWindow(event, eventdata)
XEvent *event;
caddr_t eventdata;
{
	in_window = TRUE;
	return (MenuMouseMoved(event, eventdata));
}

XtEventReturnCode MenuLeaveWindow(event, eventdata)
XEvent *event;
caddr_t eventdata;
{
	register TScreen *screen = &term.screen;
	if(!drawn)
		return (XteventHandled);
	/*
	 * Unhilite any window that is currently hilited.
	 */
	if(hilited_item) {
		XFillRectangle(screen->display, menu->menuWindow, 
		 MenuInvertGC, 0, hilited_y,
		 menu->menuWidth, hilited_item->itemHeight);
		hilited_item = (MenuItem *)0;
	}
	in_window = FALSE;
	return (XteventHandled);
}

XtEventReturnCode MenuButtonReleased(event, eventdata)
XButtonEvent *event;
caddr_t eventdata;
{
	register TScreen *screen = &term.screen;
	extern FinishModeMenu();

	/*
	 * return the index number of any selected menu
	 * item.
	 */

	if (! AllButtonsUp(event->state, event->button))
		return(XteventHandled);

	XUngrabPointer(screen->display, CurrentTime);
	
	if(in_window) {
		y = event->y;
		if((item = Y_InItem(menu, &y, &n)) != hilited_item) {
		    if(hilited_item)
			XFillRectangle(screen->display, menu->menuWindow, 
			 MenuInvertGC, 0,
			 hilited_y, menu->menuWidth,
			 hilited_item->itemHeight);
		    if(hilited_item = item) {
			XFillRectangle(screen->display, menu->menuWindow, 
			 MenuInvertGC, 0,
			 hilited_y = y, menu->menuWidth,
			 hilited_item->itemHeight);
			hilited_n = n;
		    }
		}
	}
	XFlush(screen->display);
	menu->menuFlags &= ~(menuChanged | menuItemChanged);
	Unmap_Menu(menu);
	drawn = 0;
	if(hilited_item)
		FinishModeMenu(menu->menuInitialItem = hilited_n);
	else
		FinishModeMenu(-1);
	return (XteventHandled);
}

/*
 * TrackMenu does most of the work of displaying the menu and tracking the
 * mouse.
 */
TrackMenu(lmenu, event)
register Menu *lmenu;
register XButtonPressedEvent *event;
{
	register TScreen *screen = &term.screen;
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
		if(menu->menuSaved)
			XFreePixmap(screen->display, menu->menuSaved);
		menu->menuSaved = (Pixmap)0;
		if(!Recalc_Menu(menu))
			return(-1);
		changed &= ~menuItemChanged;
	}
	/*
	 * Now if the window was never created, go ahead and make it.  Otherwise
	 * if the menu has changed, resize the window.
	 */
	if(!menu->menuWindow) {
	        attr.override_redirect = TRUE;
		attr.border_pixmap = make_gray(
		  WhitePixel(screen->display, DefaultScreen(screen->display)), 
		  BlackPixel(screen->display, DefaultScreen(screen->display)), 
		  DefaultDepth(screen->display, DefaultScreen(screen->display)));
		attr.background_pixel = menu->menuBgColor;
		attr.cursor = menu->menuCursor;
		if((menu->menuWindow = XCreateWindow(screen->display, 
		 DefaultRootWindow(screen->display), 0, 0,
		 menu->menuWidth, menu->menuHeight, menu->menuBorderWidth,
		 0, CopyFromParent, CopyFromParent, 
		 CWBorderPixmap+CWBackPixel+CWOverrideRedirect+CWCursor, 
		 &attr)) == (Window)0)
			return(-1);


		XtSetEventHandler(screen->display, menu->menuWindow,
		 (XtEventHandler) MenuExposeWindow, ExposureMask, 
		 (caddr_t)NULL);
		XtSetEventHandler(screen->display, menu->menuWindow,
		 (XtEventHandler) MenuEnterWindow, EnterWindowMask, 
		 (caddr_t)NULL);
		XtSetEventHandler(screen->display, menu->menuWindow,
		 (XtEventHandler) MenuLeaveWindow, LeaveWindowMask, 
		 (caddr_t)NULL);
		XtSetEventHandler(screen->display, menu->menuWindow,
		 (XtEventHandler) MenuMouseMoved, PointerMotionMask, 
		 (caddr_t)NULL);
		XtSetEventHandler(screen->display, menu->menuWindow,
		 (XtEventHandler) MenuButtonReleased, ButtonReleaseMask,
		 (caddr_t)NULL);
		XtSetEventHandler(screen->display, menu->menuWindow,
		 (XtEventHandler) EventDoNothing, ButtonPressMask, 
		 (caddr_t)NULL);
	} else if(changed & menuChanged)
	 	XResizeWindow(screen->display, menu->menuWindow, 
		 menu->menuWidth, menu->menuHeight);
	/*
	 * Figure out where the menu is supposed to go, from the initial button
	 * press, and move the window there.  Then map the menu.
	 */
	if(!Move_Menu(menu, event) || !Map_Menu(menu))
		return(-1);

	menuWindow = menu->menuWindow;
	in_window = TRUE;
	XGrabPointer(screen->display, menu->menuWindow, FALSE,
	 ExposureMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask
	 | ButtonReleaseMask | ButtonPressMask,
	 GrabModeAsync, GrabModeAsync, None, menu->menuCursor, CurrentTime
	 );
}

/*
 * Recalculate all of the various menu and item variables.
 */
static Recalc_Menu(menu)
register Menu *menu;
{
	register MenuItem *item;
	register int max, i, height, fontheight;

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
	register MenuItem *item;
	register int n, x, y;
	register TScreen *screen = &term.screen;
	int total_width;
	Window subw;
	/*
	 * Try to popup the menu so that the cursor is centered within the
	 * width of the menu, but compensate if that would run it outside
	 * the display area.
	 */
	total_width = menu->menuWidth + 2 * menu->menuBorderWidth;
	if((x = ev->x_root - total_width / 2) < 0)
		x = 0;
	else if(x + total_width > DisplayWidth(screen->display,
					       DefaultScreen(screen->display)))
		x = DisplayWidth(screen->display,
				 DefaultScreen(screen->display)) - total_width;
	/*
	 * If the menu extends above outside of the display, warp
	 * the mouse vertically so the menu will all show up.
	 */
	if((y = ev->y_root) < 0) {
/* don't bother warping pointer 
		XWarpPointer(screen->display, None, 
		  DefaultRootWindow(screen->display), 0, 0, 0, 0, ev->x_root, 
		    0);
*/
		y = 0;
	} else if((n = y + menu->menuHeight + 2 * menu->menuBorderWidth - 
	  DisplayHeight(screen->display, DefaultScreen(screen->display))) > 0) {
/* don't bother warping pointer 
		XWarpPointer(screen->display, None,
		 DefaultRootWindow(screen->display), 0, 0, 0, 0, ev->x_root, 
		  ev->y_root - n);
*/
		y -= n;
	}
	XMoveWindow(screen->display, menu->menuWindow, x, y);
	/*
	 * If we are in freeze mode, save what will be the coordinates of
	 * the save image.
	 */
	if(menu->menuFlags & menuFreeze) {
		menu->menuSavedImageX = x;
		menu->menuSavedImageY = y;
	}
	return(1);
}

/*
 * Map the menu window.
 */
static Map_Menu(menu)
register Menu *menu;
{
	register int i;
	register TScreen *screen = &term.screen;

	/*
	 * If we are in freeze mode, save the pixmap underneath where the menu
	 * will be (including the border).
	 */
	if(menu->menuFlags & menuFreeze) {
		XGrabServer(screen->display);
		i = 2 * menu->menuBorderWidth;
/*		if((menu->menuSavedImage = XPixmapSave(screen->display,
		 DefaultRootWindow(screen->display),
		 menu->menuSavedImageX, menu->menuSavedImageY, menu->menuWidth
		 + i, menu->menuHeight + i)) == (Pixmap)0)
*/
			return(0);
	}
	/*
	 * Actually map the window.
	 */
	XMapRaised(screen->display, menu->menuWindow);
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
	register int y, dim;
	register TScreen *screen = &term.screen;

	/*
	 * If we have a menu title, draw it first, centered and hilited.
	 */
	if(menu->menuTitleLength) {
		XFillRectangle(screen->display, menu->menuWindow, 
		 MenuGC, 0, 0, menu->menuWidth, top - 1);
		XDrawImageString(screen->display, menu->menuWindow, 
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
			XCopyArea(screen->display, 
			 Check_Tile, menu->menuWindow, 
			 dim ? MenuGrayGC : MenuGC,
			 0, 0, checkMarkWidth, checkMarkHeight, x, 
			 y = top + (item->itemHeight - checkMarkHeight) / 2);
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
	register TScreen *screen = &term.screen;

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
		XCopyArea(screen->display, 
		 Check_Tile, menu->menuWindow, 
		 (func & dimCheck) ? MenuGrayGC : MenuGC,
		 0, 0, checkMarkWidth, checkMarkHeight, x, 
		  y = top + (item->itemHeight - checkMarkHeight) / 2);
	/*
	 * Remove the check mark if needed.
	 */
	if(func & removeCheck)
		XClearArea(screen->display, menu->menuWindow, 
		 x, y, checkMarkWidth, checkMarkHeight);
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
	register TScreen *screen = &term.screen;

	/*
	 * If the item text is a single dash, draw a separating line.
	 */
	if(XStrCmp(item->itemText, "-") == 0) {
		XDrawLine(screen->display, menu->menuWindow,  MenuGC,
		 0, center, menu->menuWidth, center);
		return;
	}
	/*
	 * Draw and/or dim the text, centered vertically.
	 */
	y = center - 
	 ((menu->menuFontInfo->ascent + menu->menuFontInfo->descent)/ 2);
	if(dim) {
		XDrawString(screen->display, menu->menuWindow, MenuGrayGC,
		 x, y+menu->menuFontInfo->ascent, 
		 item->itemText, item->itemTextLength);
	} else
		XDrawImageString(screen->display, menu->menuWindow, 
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
	register TScreen *screen = &term.screen;

	/*
	 * Find out where the mouse is.  If its not in the menu window,
	 * return NULL.
	 */
	XQueryPointer(screen->display, menu->menuWindow, 
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
	Window subw;

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
	register TScreen *screen = &term.screen;

	if(!menu || !(menu->menuFlags & menuMapped))
		return;
	if(menu->menuFlags & menuFreeze) {
		XUnmapWindow(screen->display, menu->menuWindow);
		i = 2 * menu->menuBorderWidth;
		XCopyArea(screen->display, 
		 menu->menuSavedImage, DefaultRootWindow(screen->display), 
		 MenuGC, 0, 0, menu->menuWidth + i,
		 menu->menuHeight + i, menu->menuSavedImageX, 
		 menu->menuSavedImageY);
		XFreePixmap(screen->display, menu->menuSavedImage);
		XUngrabServer(screen->display);
	} else
		XUnmapWindow(screen->display, menu->menuWindow);
	menu->menuFlags &= ~menuMapped;
}
#endif MODEMENU
