/*
 *	$XConsortium: misc.c,v 1.29 89/01/04 12:25:17 jim Exp $
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

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <ctype.h>
#include <pwd.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "ptyx.h"
#include "data.h"
#include "error.h"
#include <X11/cursorfont.h>
#include "gray.ic"
#include "wait.ic"
#include "waitmask.ic"
#include <X11/Shell.h>
#include <X11/Xmu.h>

extern char *malloc();
extern char *mktemp();
extern void exit();
extern void perror();
extern void abort();

#ifndef lint
static char rcs_id[] = "$XConsortium: misc.c,v 1.29 89/01/04 12:25:17 jim Exp $";
#endif	/* lint */

xevents()
{
	XEvent event;
	register TScreen *screen = &term->screen;

	if(screen->scroll_amt)
		FlushScroll(screen);
	XPending (screen->display);
	do {
		if (waitingForTrackInfo)
			return;
		XNextEvent (screen->display, &event);
		if (!event.xany.send_event ||
		    screen->allowSendEvents ||
		    ((event.xany.type != KeyPress) &&
		     (event.xany.type != KeyRelease) &&
		     (event.xany.type != ButtonPress) &&
		     (event.xany.type != ButtonRelease)))
		    XtDispatchEvent(&event);
	} while (QLength(screen->display) > 0);
}


Cursor make_colored_cursor (cursorindex, fg, bg)
	int cursorindex;			/* index into font */
	unsigned long fg, bg;			/* pixel value */
{
	register TScreen *screen = &term->screen;
	Cursor c;
	register Display *dpy = screen->display;
	XColor foreback[2];
	
	c = XCreateFontCursor (dpy, cursorindex);
	if (c == (Cursor) 0) return (c);

	recolor_cursor (c, fg, bg);
	return (c);
}

/* ARGSUSED */
void HandleKeyPressed(w, event, params, nparams)
    Widget w;
    XEvent *event;
    String *params;
    Cardinal *nparams;
{
    register TScreen *screen = &term->screen;

#ifdef ACTIVEWINDOWINPUTONLY
    if (w == (screen->TekEmu ? (Widget)tekWidget : (Widget)term))
#endif
	Input (&term->keyboard, screen, event);
}

/* ARGSUSED */
void HandleStringEvent(w, event, params, nparams)
    Widget w;
    XEvent *event;
    String *params;
    Cardinal *nparams;
{
    register TScreen *screen = &term->screen;

#ifdef ACTIVEWINDOWINPUTONLY
    if (w != (screen->TekEmu ? (Widget)tekWidget : (Widget)term)) return;
#endif

    if (*nparams != 1) return;

    if ((*params)[0] == '0' && (*params)[1] == 'x' && (*params)[2] != '\0') {
	char c, *p, hexval[2];
	hexval[0] = hexval[1] = 0;
	for (p = *params+2; (c = *p); p++) {
	    hexval[0] *= 16;
	    if (isupper(c)) c = tolower(c);
	    if (c >= '0' && c <= '9')
		hexval[0] += c - '0';
	    else if (c >= 'a' && c <= 'f')
		hexval[0] += c - 'a' + 10;
	    else break;
	}
	if (c == '\0')
	    StringInput (screen, hexval);
    }
    else {
	StringInput (screen, *params);
    }
}

/*ARGSUSED*/
void HandleEnterWindow(w, eventdata, event)
Widget w;
register XEnterWindowEvent *event;
caddr_t eventdata;
{
    register TScreen *screen = &term->screen;

#ifdef ACTIVEWINDOWINPUTONLY
    if (w == XtParent(screen->TekEmu ? (Widget)tekWidget : (Widget)term)) 
#endif
      if (((event->detail) != NotifyInferior) &&
	  event->focus &&
	  !(screen->select & FOCUS))
	selectwindow(screen, INWINDOW);
}

/*ARGSUSED*/
void HandleLeaveWindow(w, eventdata, event)
Widget w;
register XEnterWindowEvent *event;
caddr_t eventdata;
{
    register TScreen *screen = &term->screen;

#ifdef ACTIVEWINDOWINPUTONLY
    if (w == XtParent(screen->TekEmu ? (Widget)tekWidget : (Widget)term)) 
#endif
      if (((event->detail) != NotifyInferior) &&
	  event->focus &&
	  !(screen->select & FOCUS))
	unselectwindow(screen, INWINDOW);

}


/*ARGSUSED*/
void HandleFocusChange(w, eventdata, event)
Widget w;
register XFocusChangeEvent *event;
caddr_t eventdata;
{
        register TScreen *screen = &term->screen;

        if(event->type == FocusIn)
                selectwindow(screen,
			     (event->detail == NotifyPointer) ? INWINDOW :
								FOCUS);
        else {
                unselectwindow(screen,
			       (event->detail == NotifyPointer) ? INWINDOW :
								  FOCUS);
		if (screen->grabbedKbd && (event->mode == NotifyUngrab)) {
		    screen->grabbedKbd = FALSE;
		    ReverseVideo(term);
		    XBell(screen->display, 100);
		}
	}
}



selectwindow(screen, flag)
register TScreen *screen;
register int flag;
{
	if(screen->TekEmu) {
		TekSelect();
		if(!Ttoggled)
			TCursorToggle(TOGGLE);
		screen->select |= flag;
#ifdef obsolete
		if(screen->cellsused) {
			screen->colorcells[2].pixel =
			 screen->Tcursorcolor;
			XStoreColor(screen->display, 
			 DefaultColormap(screen->display,
				DefaultScreen(screen->display)),
			 &screen->colorcells[2]);
		}
#endif /* obsolete */
		if(!Ttoggled)
			TCursorToggle(TOGGLE);
		return;
	} else {
		VTSelect();
		if(screen->cursor_state &&
		   (screen->cursor_col != screen->cur_col ||
		    screen->cursor_row != screen->cur_row))
		    HideCursor();
		screen->select |= flag;
		if(screen->cursor_state)
			ShowCursor();
		return;
	}
}

unselectwindow(screen, flag)
register TScreen *screen;
register int flag;
{
    register int i;

    if (screen->always_highlight) return;

    if(screen->TekEmu) {
	if(!Ttoggled) TCursorToggle(TOGGLE);
	screen->select &= ~flag;
	TekUnselect();
#ifdef obsolete
			if(screen->cellsused) {
				i = (term->flags & REVERSE_VIDEO) == 0;
				screen->colorcells[i].pixel =
				 screen->Tcursorcolor;
				XStoreColor(screen->display, 
			         DefaultColormap(screen->display,
					       DefaultScreen(screen->display)),
				 &screen->colorcells[i]);
			}
#endif /* obsolete */
	if(!Ttoggled) TCursorToggle(TOGGLE);
    } else {
	screen->select &= ~flag;
	VTUnselect();
	if(screen->cursor_state &&
	   (screen->cursor_col != screen->cur_col ||
	    screen->cursor_row != screen->cur_row))
	      HideCursor();
	if(screen->cursor_state)
	  ShowCursor();
    }
}

/*ARGSUSED*/
reselectwindow(screen)
register TScreen *screen;
{
#ifdef obsolete
	Window root, win;
	int rootx, rooty, x, y;
	int doselect = 0;
	unsigned int mask;

	if(XQueryPointer(
	    screen->display, 
	    DefaultRootWindow(screen->display), 
	    &root, &win,
	    &rootx, &rooty,
	    &x, &y,
	    &mask)) {
		XtTranslateCoords(term, 0, 0, &x, &y);
		if ((rootx >= x) && (rootx < x + term->core.width) &&
		    (rooty >= y) && (rooty < y + term->core.height))
		    doselect = 1;
		else if (tekWidget) {
		    XtTranslateCoords(tekWidget, 0, 0, &x, &y);
		    if ((rootx >= x) && (rootx < x + tekWidget->core.width) &&
			(rooty >= y) && (rooty < y + tekWidget->core.height))
			doselect = 1;
		}
		if (doselect)
			selectwindow(screen, INWINDOW);
		else	unselectwindow(screen, INWINDOW);
	}
#endif /* obsolete */
}

Pixmap Make_tile(width, height, bits, foreground, background, depth)
	unsigned int width, height, depth;
	Pixel foreground, background;
	char *bits;
{
	register GC gc;
	register Pixmap pix;
	register TScreen *screen = &term->screen;
	XGCValues gcVals;
	XImage tileimage;

        pix = (Pixmap)XCreatePixmap(screen->display, 
	  DefaultRootWindow(screen->display), width, height, depth);
	gcVals.foreground = foreground;
	gcVals.background = background;
	gc = XCreateGC(screen->display, (Drawable) pix, 
	  GCForeground+GCBackground, &gcVals);
	tileimage.height = height;
	tileimage.width = width;
	tileimage.xoffset = 0;
	tileimage.format = XYBitmap;
	tileimage.data = bits;
	tileimage.byte_order = LSBFirst;
	tileimage.bitmap_unit = 8;
	tileimage.bitmap_bit_order = LSBFirst;
	tileimage.bitmap_pad = 8;
	tileimage.bytes_per_line = (width+7)>>3;
	tileimage.depth = 1;
        XPutImage(screen->display, pix, gc, &tileimage, 0, 0, 0, 0, width, height);
        XFreeGC (screen->display, gc);
	return(pix);
}


Pixmap make_gray(fg, bg, depth)
Pixel fg, bg;
{
	return(Make_tile(gray_width, gray_height, gray_bits, fg, bg, depth));
}

/* ARGSUSED */
Cursor make_tcross(fg, bg)
Pixel fg, bg;
{
	return (make_colored_cursor (XC_tcross, fg, bg));
}

static XColor background = { 0L, 65535, 65535, 65535 };
static XColor foreground = { 0L,    0,     0,     0 };

Cursor make_wait(fg, bg)
Pixel fg, bg;
{
	register TScreen *screen = &term->screen;
	register Display *dpy = screen->display;
	Pixmap source, mask;
	XColor foreback[2];

	source = Make_tile(wait_width, wait_height, wait_bits, 1L, 0L, 1);
	mask = Make_tile(waitmask_width, waitmask_height, waitmask_bits, 
	 1L, 0L, 1);

	foreback[0].pixel = fg;
	foreback[1].pixel = bg;
	XQueryColors (dpy, DefaultColormap (dpy, DefaultScreen (dpy)),
		      foreback, 2);

	return (XCreatePixmapCursor (dpy, source, mask, foreback, foreback+1,
	 wait_x_hot, wait_y_hot));
}

/* ARGSUSED */
Cursor make_arrow(fg, bg)
unsigned long fg, bg;

{
	return (make_colored_cursor (XC_left_ptr, fg, bg));
}

/* ARGSUSED */
Cursor make_xterm(fg, bg)
unsigned long fg, bg;

{
	return (make_colored_cursor (XC_xterm, fg, bg));
}

char *uniquesuffix(name)
char *name;
{
	register int *np, *fp, i;
	register Window *cp;
	register int temp, j, k, exact, *number;
	char *wname;
	Window *children, parent, root;
	unsigned int nchildren;
	static char *suffix, sufbuf[10];
	TScreen *screen = &term->screen;
	char *malloc();

	if(suffix)
		return(suffix);
	suffix = sufbuf;
	if(!XQueryTree(
	    screen->display, 
	    DefaultRootWindow(screen->display), 
	    &root, &parent,
	    &children, &nchildren) ||
	 nchildren < 1 || (number = (int *)malloc((unsigned)nchildren * sizeof(int)))
	 == NULL)
		return(suffix);
	exact = FALSE;
	i = strlen(name);
	for(np = number, cp = children, j = nchildren ; j > 0 ; cp++, j--) {
		if(!XFetchName(screen->display, *cp, &wname) || wname == NULL)
			continue;
		if(strncmp(name, wname, i) == 0) {
			if(wname[i] == 0 || XStrCmp(&wname[i], " (Tek)") == 0)
				exact = TRUE;
			else if(strncmp(&wname[i], " #", 2) == 0)
				*np++ = atoi(&wname[i + 2]);
		}
		free(wname);
	}
	free((char *)children);
	if(exact) {
		if(np <= number)
			strcpy(suffix, " #2");
		else {
			exact = np - number;
			np = number;
			/* shell sort */
			for(i = exact / 2 ; i > 0 ; i /= 2)
				for(k = i ; k < exact ; k++)
					for(j = k - i ; j >= 0 &&
					 np[j] > np[j + i] ; j -= i) {
						temp = np[j];
						np[j] = np[j + i];
						np[j + i] = temp;
					}
			/* make numbers unique */
			for(fp = np + 1, i = exact - 1 ; i > 0 ; fp++, i--)
				if(*fp != *np)
					*++np = *fp;
			/* find least unique number */
			for(i = 2, fp = number ; fp <= np ; fp++) {
				if(i < *fp)
					break;
				if(i == *fp)
					i++;
			}
			sprintf(suffix, " #%d", i);
		}
	}
	free((char *)number);
	return(suffix);
}

Bell()
{
	extern XtermWidget term;
	register TScreen *screen = &term->screen;
	register Pixel xorPixel = screen->foreground ^ term->core.background_pixel;
	XGCValues gcval;
	GC visualGC;

	if(screen->visualbell) {
		gcval.function = GXxor;
		gcval.foreground = xorPixel;
		visualGC = XtGetGC((Widget)term, GCFunction+GCForeground, &gcval);
		if(screen->TekEmu) {
			XFillRectangle(
			    screen->display,
			    TWindow(screen), 
			    visualGC,
			    0, 0,
			    (unsigned) TFullWidth(screen),
			    (unsigned) TFullHeight(screen));
			XFlush(screen->display);
			XFillRectangle(
			    screen->display,
			    TWindow(screen), 
			    visualGC,
			    0, 0,
			    (unsigned) TFullWidth(screen),
			    (unsigned) TFullHeight(screen));
		} else {
			XFillRectangle(
			    screen->display,
			    VWindow(screen), 
			    visualGC,
			    0, 0,
			    (unsigned) FullWidth(screen),
			    (unsigned) FullHeight(screen));
			XFlush(screen->display);
			XFillRectangle(
			    screen->display,
			    VWindow(screen), 
			    visualGC,
			    0, 0,
			    (unsigned) FullWidth(screen),
			    (unsigned) FullHeight(screen));
		}
	} else
		XBell(screen->display, 0);
}

Redraw()
{
	extern XtermWidget term;
	register TScreen *screen = &term->screen;
	XExposeEvent event;

	event.type = Expose;
	event.display = screen->display;
	event.x = 0;
	event.y = 0;
	event.count = 0; 
	
	if(VWindow(screen)) {
	        event.window = VWindow(screen);
		event.width = term->core.width;
		event.height = term->core.height;
		(*term->core.widget_class->core_class.expose)(term, &event, NULL);
		if(screen->scrollbar) 
			(*screen->scrollWidget->core.widget_class->core_class.expose)(screen->scrollWidget, &event, NULL);
		}

	if(TWindow(screen) && screen->Tshow) {
	        event.window = TWindow(screen);
		event.width = tekWidget->core.width;
		event.height = tekWidget->core.height;
		TekExpose (tekWidget, &event, NULL);
	}
}

#ifdef obsolete
SyncUnmap(win, mask)
register Window win;
register long int mask;
{
	XEvent ev;
	register XEvent *rep = &ev;
	register TScreen *screen = &term->screen;

	do { /* ignore events through unmap */
		XWindowEvent(screen->display, win, mask, rep);
	} while(rep->type != UnmapNotify);
	return;
}
#endif /* obsolete */

StartLog(screen)
register TScreen *screen;
{
	register char *cp;
	register int i;
	static char *log_default;
	char *malloc(), *rindex();
	extern logpipe();
#ifdef SYSV
	/* SYSV has another pointer which should be part of the
	** FILE structure but is actually a separate array.
	*/
	unsigned char *old_bufend;
#endif	/* SYSV */

	if(screen->logging || (screen->inhibit & I_LOG))
		return;
	if(screen->logfile == NULL || *screen->logfile == 0) {
		if(screen->logfile)
			free(screen->logfile);
		if(log_default == NULL)
			mktemp(log_default = log_def_name);
		if((screen->logfile = malloc((unsigned)strlen(log_default) + 1)) == NULL)
			return;
		strcpy(screen->logfile, log_default);
	}
	if(*screen->logfile == '|') {	/* exec command */
		int p[2];
		static char *shell;

		if(pipe(p) < 0 || (i = fork()) < 0)
			return;
		if(i == 0) {	/* child */
			close(p[1]);
			dup2(p[0], 0);
			close(p[0]);
			dup2(fileno(stderr), 1);
			dup2(fileno(stderr), 2);
#ifdef SYSV
			old_bufend = _bufend(stderr);
#endif	/* SYSV */
			close(fileno(stderr));
			fileno(stderr) = 2;
#ifdef SYSV
			_bufend(stderr) = old_bufend;
#endif	/* SYSV */
			close(screen->display->fd);
			close(screen->respond);
			if(!shell) {
				register struct passwd *pw;
				char *getenv(), *malloc();
				struct passwd *getpwuid();

				if(((cp = getenv("SHELL")) == NULL || *cp == 0)
				 && ((pw = getpwuid(screen->uid)) == NULL ||
				 *(cp = pw->pw_shell) == 0) ||
				 (shell = malloc((unsigned) strlen(cp) + 1)) == NULL)
					shell = "/bin/sh";
				else
					strcpy(shell, cp);
			}
			signal(SIGHUP, SIG_DFL);
			signal(SIGCHLD, SIG_DFL);
			setgid(screen->gid);
			setuid(screen->uid);
			execl(shell, shell, "-c", &screen->logfile[1], 0);
			fprintf(stderr, "%s: Can't exec `%s'\n", xterm_name,
			 &screen->logfile[1]);
			exit(ERROR_LOGEXEC);
		}
		close(p[0]);
		screen->logfd = p[1];
		signal(SIGPIPE, logpipe);
	} else {
		if(access(screen->logfile, F_OK) == 0) {
			if(access(screen->logfile, W_OK) < 0)
				return;
		} else if(cp = rindex(screen->logfile, '/')) {
			*cp = 0;
			i = access(screen->logfile, W_OK);
			*cp = '/';
			if(i < 0)
				return;
		} else if(access(".", W_OK) < 0)
			return;
		if((screen->logfd = open(screen->logfile, O_WRONLY | O_APPEND |
		 O_CREAT, 0644)) < 0)
			return;
		chown(screen->logfile, screen->uid, screen->gid);

	}
	screen->logstart = screen->TekEmu ? Tbptr : bptr;
	screen->logging = TRUE;
}

CloseLog(screen)
register TScreen *screen;
{
	if(!screen->logging || (screen->inhibit & I_LOG))
		return;
	FlushLog(screen);
	close(screen->logfd);
	screen->logging = FALSE;
}

FlushLog(screen)
register TScreen *screen;
{
	register char *cp;
	register int i;

	cp = screen->TekEmu ? Tbptr : bptr;
	if((i = cp - screen->logstart) > 0)
		write(screen->logfd, screen->logstart, i);
	screen->logstart = screen->TekEmu ? Tbuffer : buffer;
}

logpipe()
{
	register TScreen *screen = &term->screen;

#ifdef SYSV
	(void) signal(SIGPIPE, SIG_IGN);
#endif	/* SYSV */
	if(screen->logging)
		CloseLog(screen);
}


do_osc(func)
int (*func)();
{
	register TScreen *screen = &term->screen;
	register int mode, c;
	register char *cp;
	char buf[512];
	extern char *malloc();

	mode = 0;
	while(isdigit(c = (*func)()))
		mode = 10 * mode + (c - '0');
	cp = buf;
	while(isprint(c = (*func)()))
		*cp++ = c;
	*cp = 0;
	switch(mode) {
	 case 0:	/* new icon name and title*/
		Changename(buf);
		Changetitle(buf);
		break;

	 case 1:	/* new icon name only */
		Changename(buf);
		break;

	 case 2:	/* new title only */
		Changetitle(buf);
		break;


	 case 46:	/* new log file */
		if((cp = malloc((unsigned)strlen(buf) + 1)) == NULL)
			break;
		strcpy(cp, buf);
		if(screen->logfile)
			free(screen->logfile);
		screen->logfile = cp;
		break;
	}
}

static ChangeGroup(attribute, value)
     String attribute;
     XtArgVal value;
{
	extern Widget toplevel;
	Arg args[1];

	XtSetArg( args[0], attribute, value );
	XtSetValues( toplevel, args, 1 );
}

Changename(name)
register char *name;
{
    ChangeGroup( XtNiconName, (XtArgVal)name );
}

Changetitle(name)
register char *name;
{
    ChangeGroup( XtNtitle, (XtArgVal)name );
}

#ifndef DEBUG
/* ARGSUSED */
#endif
Panic(s, a)
char	*s;
int a;
{
#ifdef DEBUG
	if(debug) {
		fprintf(stderr, "%s: PANIC!	", xterm_name);
		fprintf(stderr, s, a);
		fputs("\r\n", stderr);
		fflush(stderr);
	}
#endif	/* DEBUG */
}

char *SysErrorMsg (n)
    int n;
{
    extern char *sys_errlist[];
    extern int sys_nerr;

    return ((n >= 0 && n < sys_nerr) ? sys_errlist[n] : "unknown error");
}


SysError (i)
int i;
{
	int oerrno;

	oerrno = errno;
	/* perror(3) write(2)s to file descriptor 2 */
	fprintf (stderr, "%s: Error %d, errno %d: ", xterm_name, i, oerrno);
	fprintf (stderr, "%s\n", SysErrorMsg (oerrno));
	Cleanup(i);
}

Error (i)
int i;
{
	fprintf (stderr, "%s: Error %d\n", xterm_name, i);
	Cleanup(i);
}


/*
 * cleanup by sending SIGHUP to client processes
 */
Cleanup (code)
int code;
{
	extern XtermWidget term;
	register TScreen *screen;

	screen = &term->screen;
	if (screen->pid > 1) {
	    (void) killpg (screen->pid, SIGHUP);
	}
	Exit (code);
}

/*
 * sets the value of var to be arg in the Unix 4.2 BSD environment env.
 * Var should end with '=' (bindings are of the form "var=value").
 * This procedure assumes the memory for the first level of environ
 * was allocated using calloc, with enough extra room at the end so not
 * to have to do a realloc().
 */
Setenv (var, value)
register char *var, *value;
{
	extern char **environ;
	register int index = 0;
	register int len = strlen(var);

	while (environ [index] != NULL) {
	    if (strncmp (environ [index], var, len) == 0) {
		/* found it */
		environ[index] = (char *)malloc ((unsigned)len + strlen (value) + 1);
		strcpy (environ [index], var);
		strcat (environ [index], value);
		return;
	    }
	    index ++;
	}

#ifdef DEBUG
	if (debug) fputs ("expanding env\n", stderr);
#endif	/* DEBUG */

	environ [index] = (char *) malloc ((unsigned)len + strlen (value) + 1);
	(void) strcpy (environ [index], var);
	strcat (environ [index], value);
	environ [++index] = NULL;
}

/*
 * returns a pointer to the first occurrence of s2 in s1,
 * or NULL if there are none.
 */
char *strindex (s1, s2)
register char	*s1, *s2;
{
	register char	*s3;
	char		*index();
	int s2len = strlen (s2);

	while ((s3=index(s1, *s2)) != NULL) {
		if (strncmp(s3, s2, s2len) == 0)
			return (s3);
		s1 = ++s3;
	}
	return (NULL);
}

/*ARGSUSED*/
xerror(d, ev)
Display *d;
register XErrorEvent *ev;
{
    fprintf (stderr, "%s:  warning, error event receieved:\n", xterm_name);
    (void) XmuPrintDefaultErrorMessage (d, ev, stderr);
    Exit (ERROR_XERROR);
}

/*ARGSUSED*/
xioerror(d)
Display *d;
{
	/* Perror(3) writes to fd 2.  What we want to do is
	** to write to stderr's fd.  Since the original code used
	** perror which uses write(2), let's not use fprintf(3).
	*/
	int oerrno = errno;
	char *msg = SysErrorMsg (oerrno);
	int stderrfd = fileno (stderr);
	
	write(stderrfd, xterm_name, strlen(xterm_name));
	write(stderrfd, ": ", 2);
	write(stderrfd, msg, strlen (msg));
	write(stderrfd, "\n", 1);

	Exit(ERROR_XIOERROR);
}

XStrCmp(s1, s2)
char *s1, *s2;
{
  if (s1 && s2) return(strcmp(s1, s2));
  if (s1 && *s1) return(1);
  if (s2 && *s2) return(-1);
  return(0);
}

void set_vt_visibility (on)
    Boolean on;
{
    register TScreen *screen = &term->screen;

    if (on) {
	if (!screen->Vshow && term) {
	    VTInit ();
	    XtMapWidget (term->core.parent);
	    screen->Vshow = TRUE;
	}
    } else {
	if (screen->Vshow && term) {
	    XtUnmapWidget (term->core.parent);
	    screen->Vshow = FALSE;
	}
    }
    return;
}

void set_tek_visibility (on)
    Boolean on;
{
    register TScreen *screen = &term->screen;

    if (on) {
	if (!screen->Tshow && (tekWidget || TekInit())) {
	    XtRealizeWidget (tekWidget->core.parent);
	    XtMapWidget (tekWidget->core.parent);
	    screen->Tshow = TRUE;
	}
    } else {
	if (screen->Tshow && tekWidget) {
	    XtUnmapWidget (tekWidget->core.parent);
	    screen->Tshow = FALSE;
	}
    }
    return;
}

void end_tek_mode ()
{
    register TScreen *screen = &term->screen;

    if (screen->TekEmu) {
	if (screen->logging) {
	    FlushLog (screen);
	    screen->logstart = buffer;
	}
	longjmp(Tekend, 1);
    } 
    return;
}

void end_vt_mode ()
{
    register TScreen *screen = &term->screen;

    if (!screen->TekEmu) {
	if(screen->logging) {
	    FlushLog(screen);
	    screen->logstart = Tbuffer;
	}
	screen->TekEmu = TRUE;
	longjmp(VTend, 1);
    } 
    return;
}
