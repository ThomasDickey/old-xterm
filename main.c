/*
 *	@Source: /orpheus/u1/X11/clients/xterm/RCS/main.c,v @
 *	@Header: main.c,v 1.29 87/08/03 17:05:39 swick Locked @
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

/* main.c */

#ifndef lint
static char rcs_id[] = "@Header: main.c,v 1.29 87/08/03 17:05:39 swick Locked @";
#endif	lint

#include <pwd.h>
#include <sgtty.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <sys/file.h>
#include <errno.h>
#include <signal.h>
#include <strings.h>
#include <setjmp.h>

#ifdef apollo
#include <sys/types.h>
#define ttyslot() 1
#define vhangup() ;
#endif

#include <utmp.h>
#include <sys/param.h>	/* for NOFILE */
#include <X11/Xlib.h>
#include <X11/Xtlib.h>
#include "ptyx.h"
#include "data.h"
#include "error.h"
#include "main.h"

extern Pixmap make_gray();
extern char *malloc();
extern char *calloc();
extern char *ttyname();
extern void exit();
extern void sleep();
extern void bcopy();
extern void vhangup();
extern long lseek();

XrmNameList nameList;
XrmClassList classList;

int switchfb[] = {0, 2, 1, 3};

static int reapchild ();

static char **command_to_exec;
static struct  sgttyb d_sg = {
        0, 0, 0177, CKILL, EVENP|ODDP|ECHO|XTABS|CRMOD
};
static struct  tchars d_tc = {
        CINTR, CQUIT, CSTART,
        CSTOP, CEOF, CBRK,
};
static struct  ltchars d_ltc = {
        CSUSP, CDSUSP, CRPRNT,
        CFLUSH, CWERASE, CLNEXT
};
static int d_disipline = NTTYDISC;
static long int d_lmode = LCRTBS|LCRTERA|LCRTKIL|LCTLECH;
static char display[256];
static char etc_utmp[] = "/etc/utmp";
static char *get_ty;
static int inhibit;
static int log_on;
static int login_shell;
static char passedPty[2];	/* name if pty if slave */
static int loginpty;
#ifdef TIOCCONS
static int Console;
#endif TIOCCONS
static int tslot;
static jmp_buf env;

#define	XtNxterm		"xterm"
#define	XtNwindowName		"windowName"
#define	XtNboldFont		"boldFont"
#define	XtNc132			"c132"
#define XtNtitle		"title"
#define	XtNcurses		"curses"
#define	XtNcursor		"cursor"
#define	XtNcursorShape		"cursorShape"
#define	XtNgeometry		"geometry"
#define	XtNiconStartup		"iconStartup"
#define	XtNinternalBorder	"internalBorder"
#define	XtNjumpScroll		"jumpScroll"
#define	XtNlogFile		"logFile"
#define	XtNlogging		"logging"
#define	XtNlogInhibit		"logInhibit"
#define	XtNloginShell		"loginShell"
#define	XtNmarginBell		"marginBell"
#define	XtNmouse		"mouse"
#define	XtNmultiScroll		"multiScroll"
#define	XtNnMarginBell		"nMarginBell"
#define	XtNreverseWrap		"reverseWrap"
#define	XtNsaveLines		"saveLines"
#define	XtNscrollBar		"scrollBar"
#define	XtNscrollInput		"scrollInput"
#define	XtNscrollKey		"scrollKey"
#define	XtNsignalInhibit	"signalInhibit"
#define	XtNtekInhibit		"tekInhibit"
#define	XtNtekStartup		"tekStartup"
#define	XtNvisualBell		"visualBell"

#define	XtCApp			"App"
#define	XtCC132			"C132"
#define	XtCCurses		"Curses"
#define	XtCBitmapbits		"Bitmapbits"
#define	XtCGeometry		"Geometry"
#define	XtCIconstartup		"Iconstartup"
#define	XtCJumpscroll		"Jumpscroll"
#define	XtCLogfile		"Logfile"
#define	XtCLogging		"Logging"
#define	XtCLoginhibit		"Loginhibit"
#define	XtCLoginshell		"Loginshell"
#define	XtCMarginbell		"Marginbell"
#define	XtCMultiscroll		"Multiscroll"
#define	XtCColumn		"Column"
#define	XtCReverseVideo		"ReverseVideo"
#define	XtCReverseWrap		"ReverseWrap"
#define	XtCRows			"Rows"
#define	XtCScrollbar		"ScrollBar"
#define	XtCScrollcond		"Scrollcond"
#define	XtCSignalInibit		"SignalInibit"
#define	XtCTekInhibit		"TekInhibit"
#define	XtCTekStartup		"TekStartup"
#define	XtCVisualbell		"Visualbell"


/* Defaults */
static  Boolean	defaultFALSE	   = FALSE;
static  Boolean	defaultTRUE	   = TRUE;
static	char	*defaultNULL	   = NULL;
static	int	defaultBorderWidth = DEFBORDERWIDTH;
static	int	defaultIntBorder   = DEFBORDER;
static  int	defaultSaveLines   = SAVELINES;
static	int	defaultNMarginBell = N_MARGINBELL;
static  char	*defaultFont	   = DEFFONT;
static	char	*defaultBoldFont  = DEFBOLDFONT;

/* term.screen.flags things that don't map directly from option */
static Boolean reverseWrap;
static Boolean logInhibit;
static Boolean signalInhibit;
static Boolean tekInhibit;

/* term.screen.xxx things that don't map directly from option */
static Boolean scrollbar;


/*static*/ Resource resourceList[] = {
{XtNbackground,	XtCColor,	XrmRPixel,	sizeof(Pixel),
	(caddr_t) &term.screen.background,	(caddr_t) &XtDefaultBGPixel},
{XtNforeground,	XtCColor,	XrmRPixel,	sizeof(Pixel),
	(caddr_t) &term.screen.foreground,	(caddr_t) &XtDefaultFGPixel},
{XtNfont,	XtCFont,	XrmRString,	sizeof(char *),
	(caddr_t) &f_n,				(caddr_t) &defaultNULL},
{XtNboldFont,	XtCFont,	XrmRString,	sizeof(char *),
	(caddr_t) &f_b,				(caddr_t) &defaultNULL},
{XtNborder,	XtCColor,	XrmRPixel,	sizeof(Pixel),
	(caddr_t) &term.screen.bordercolor,	(caddr_t) &XtDefaultFGPixel},
{XtNborderWidth,XtCBorderWidth,	XrmRInt,		sizeof(int),
	(caddr_t) &term.screen.borderwidth,	(caddr_t)&defaultBorderWidth},
{XtNc132,	XtCC132,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &term.screen.c132,		(caddr_t) &defaultFALSE},
{XtNcurses,	XtCCurses,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &term.screen.curses,		(caddr_t) &defaultFALSE},
{XtNcursor,	XtCColor,	XrmRPixel,	sizeof(Pixel),
	(caddr_t) &term.screen.cursorcolor,	
	(caddr_t) &term.screen.foreground},
{XtNcursorShape,XtCCursor,	XrmRString,	sizeof(char *),
	(caddr_t) &curs_shape,			(caddr_t) &defaultNULL},
{XtNgeometry,XtCGeometry,	XrmRString,	sizeof(char *),
	(caddr_t) &geo_metry,			(caddr_t) &defaultNULL},
{XtNiconStartup,XtCIconstartup,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &iconstartup,			(caddr_t) &defaultFALSE},
{XtNinternalBorder,XtCBorderWidth,XrmRInt,	sizeof(int),
	(caddr_t) &term.screen.border,		(caddr_t) &defaultIntBorder},
{XtNjumpScroll,	XtCJumpscroll,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &term.screen.jumpscroll,	(caddr_t) &defaultTRUE},
{XtNlogFile,	XtCLogfile,	XrmRString,	sizeof(char *),
	(caddr_t) &term.screen.logfile,		(caddr_t) &defaultNULL},
{XtNlogging,	XtCLogging,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &log_on,			(caddr_t) &defaultFALSE},
{XtNlogInhibit,	XtCLoginhibit,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &logInhibit,			(caddr_t) &defaultFALSE},
{XtNloginShell,	XtCLoginshell,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &login_shell,			(caddr_t) &defaultFALSE},
{XtNmarginBell,	XtCMarginbell,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &term.screen.marginbell,	(caddr_t) &defaultFALSE},
{XtNmouse,	XtCColor,	XrmRPixel,	sizeof(Pixel),
/*	(caddr_t) &term.screen.mousecolor,	(caddr_t) &XtDefaultFGPixel},
*/
	(caddr_t) &term.screen.mousecolor,	
	(caddr_t) &term.screen.cursorcolor},
{XtNmultiScroll,XtCMultiscroll,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &term.screen.multiscroll,	(caddr_t) &defaultFALSE},
{XtNnMarginBell,XtCColumn,	XrmRInt,	sizeof(int),
	(caddr_t) &term.screen.nmarginbell,	(caddr_t) &defaultNMarginBell},
{XtNreverseVideo,XtCReverseVideo,XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &re_verse,			(caddr_t) &defaultFALSE},
{XtNreverseWrap,XtCReverseWrap,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &reverseWrap,			(caddr_t) &defaultFALSE},
{XtNsaveLines,	XtCRows,	XrmRInt,	sizeof(int),
	(caddr_t) &save_lines,			(caddr_t) &defaultSaveLines},
{XtNscrollBar,	XtCScrollbar,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &scrollbar,			(caddr_t) &defaultFALSE},
{XtNscrollInput,XtCScrollcond,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &term.screen.scrollinput,	(caddr_t) &defaultTRUE},
{XtNscrollKey,	XtCScrollcond,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &term.screen.scrollkey,	(caddr_t) &defaultFALSE},
{XtNsignalInhibit,XtCSignalInibit,XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &signalInhibit,		(caddr_t) &defaultFALSE},
{XtNtekInhibit,	XtCTekInhibit,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &tekInhibit,			(caddr_t) &defaultFALSE},
{XtNtekStartup,	XtCTekStartup,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &term.screen.TekEmu,		(caddr_t) &defaultFALSE},
{XtNtitle,	XtCString,	XrmRString,	sizeof(char *),
	(caddr_t) &title_name,			(caddr_t) &defaultNULL},
{XtNvisualBell,XtCVisualbell,	XrmRBoolean,	sizeof(Boolean),
	(caddr_t) &term.screen.visualbell,	(caddr_t) &defaultFALSE},
{XtNwindowName,	XtCString,	XrmRString,	sizeof(char *),
	(caddr_t) &window_name,			(caddr_t) &defaultNULL},

};


/* Command line options table.  Only resources are entered here...there is a
   pass over the remaining options after XtParseCommand is let loose. */

static XrmOptionDescRec optionDescList[] = {
{"=",		XtNgeometry,	XrmoptionIsArg,		(caddr_t) NULL},
{"-132",	XtNc132,	XrmoptionNoArg,		(caddr_t) "on"},
{"+132",	XtNc132,	XrmoptionNoArg,		(caddr_t) "off"},
{"-T",		XtNtitle,	XrmoptionSepArg,	(caddr_t) NULL},
{"-b",		XtNinternalBorder,XrmoptionSepArg,	(caddr_t) NULL},
{"-bd",		XtNborder,	XrmoptionSepArg,	(caddr_t) NULL},
{"-bg",		XtNbackground,	XrmoptionSepArg,	(caddr_t) NULL},
{"-bw",		XtNborderWidth,	XrmoptionSepArg,	(caddr_t) NULL},
{"-cr",		XtNcursor,	XrmoptionSepArg,	(caddr_t) NULL},
{"-cu",		XtNcurses,	XrmoptionNoArg,		(caddr_t) "on"},
{"+cu",		XtNcurses,	XrmoptionNoArg,		(caddr_t) "off"},
{"-e",		NULL,		XrmoptionSkipLine,	(caddr_t) NULL},
{"-fb",		XtNboldFont,	XrmoptionSepArg,	(caddr_t) NULL},
{"-fg",		XtNforeground,	XrmoptionSepArg,	(caddr_t) NULL},
{"-fn",		XtNfont,	XrmoptionSepArg,	(caddr_t) NULL},
{"-i",		XtNiconStartup,	XrmoptionNoArg,		(caddr_t) "on"},
{"-j",		XtNjumpScroll,	XrmoptionNoArg,		(caddr_t) "on"},
{"+j",		XtNjumpScroll,	XrmoptionNoArg,		(caddr_t) "off"},
{"-l",		XtNlogging,	XrmoptionNoArg,		(caddr_t) "on"},
{"+l",		XtNlogging,	XrmoptionNoArg,		(caddr_t) "off"},
{"-lf",		XtNlogFile,	XrmoptionSepArg,	(caddr_t) NULL},
{"-ls",		XtNloginShell,	XrmoptionNoArg,		(caddr_t) "on"},
{"+ls",		XtNloginShell,	XrmoptionNoArg,		(caddr_t) "off"},
{"-mb",		XtNmarginBell,	XrmoptionNoArg,		(caddr_t) "on"},
{"+mb",		XtNmarginBell,	XrmoptionNoArg,		(caddr_t) "off"},
{"-ms",		XtNmouse,	XrmoptionSepArg,	(caddr_t) NULL},
{"-n",		XtNwindowName,	XrmoptionSepArg,	(caddr_t) NULL},
{"-nb",		XtNnMarginBell,	XrmoptionSepArg,	(caddr_t) NULL},
{"-r",		XtNreverseVideo,XrmoptionNoArg,		(caddr_t) "on"},
{"+r",		XtNreverseVideo,XrmoptionNoArg,		(caddr_t) "off"},
{"-rv",		XtNreverseVideo,XrmoptionNoArg,		(caddr_t) "on"},
{"+rv",		XtNreverseVideo,XrmoptionNoArg,		(caddr_t) "off"},
{"-rw",		XtNreverseWrap,	XrmoptionNoArg,		(caddr_t) "on"},
{"+rw",		XtNreverseWrap,	XrmoptionNoArg,		(caddr_t) "off"},
{"-s",		XtNmultiScroll,	XrmoptionNoArg,		(caddr_t) "on"},
{"+s",		XtNmultiScroll,	XrmoptionNoArg,		(caddr_t) "off"},
{"-sb",		XtNscrollBar,	XrmoptionNoArg,		(caddr_t) "on"},
{"+sb",		XtNscrollBar,	XrmoptionNoArg,		(caddr_t) "off"},
{"-si",		XtNscrollInput,	XrmoptionNoArg,		(caddr_t) "off"},
{"+si",		XtNscrollInput,	XrmoptionNoArg,		(caddr_t) "on"},
{"-sk",		XtNscrollKey,	XrmoptionNoArg,		(caddr_t) "on"},
{"+sk",		XtNscrollKey,	XrmoptionNoArg,		(caddr_t) "off"},
{"-sl",		XtNsaveLines,	XrmoptionSepArg,	(caddr_t) NULL},
{"-t",		XtNtekStartup,	XrmoptionNoArg,		(caddr_t) "on"},
{"+t",		XtNtekStartup,	XrmoptionNoArg,		(caddr_t) "off"},
{"-vb",		XtNvisualBell,	XrmoptionNoArg,		(caddr_t) "on"},
{"+vb",		XtNvisualBell,	XrmoptionNoArg,		(caddr_t) "off"},
{"-w",		XtNborderWidth,	XrmoptionSepArg,	(caddr_t) NULL},
};

void XtGetUsersDataBase()
{
	XrmResourceDataBase resources, userResources;
	int uid;
	extern struct passwd *getpwuid();
	struct passwd *pw;
	char filename[1024];
	FILE *f;

	strcpy(filename, LIBDIR);
	strcat(filename, "/Xdefaults" );
	f = fopen(filename, "r");
	if (f) {
	        XrmGetDataBase(f, &resources);
		fclose(f);
	} else
		resources = NULL;

	/* Open .Xdefaults file and merge into existing data base */
	uid = getuid();
	pw = getpwuid(uid);
	if (pw) {
		strcpy(filename, pw->pw_dir);
		strcat(filename, "/.Xdefaults");
		f = fopen(filename, "r");
		if (f) {
			XrmGetDataBase(f, &userResources);
			if (resources)
			    XrmMergeDataBases(userResources, &resources);
			else
			    resources = userResources;
			fclose(f);
		}
		strcpy(filename, pw->pw_dir);
		strcat(filename, "/.X11defaults");
		f = fopen(filename, "r");
		if (f) {
			XrmGetDataBase(f, &userResources);
			if (resources)
			    XrmMergeDataBases(userResources, &resources);
			else
			    resources = userResources;
			fclose(f);
		}
	}
	if (resources) XrmSetCurrentDataBase(resources);
}


OpenDisplay()
{
	register TScreen *screen = &term.screen;
	register int try;
	for (try = 10 ; ; ) {
	    if (screen->display = XOpenDisplay(display))
		break;
	    if (!get_ty) {
		fprintf(stderr, "%s: No such display server %s\n", xterm_name,
		 XDisplayName(display));
		exit(ERROR_NOX);
	    }
	    if (--try <= 0)  {
		fprintf (stderr, "%s: Can't connect to display server %s\n",
		 xterm_name,  XDisplayName(display));
		exit (ERROR_NOX2);
	    }	    
	    sleep (5);
	}

	if (screen->display->fd > 31) {
		fprintf(stderr, 
		 "%s: Display server returned bogus file descriptor %d\n",
		  xterm_name, screen->display->fd);
		exit (ERROR_NOX3);
	};
}

/* ||| */
struct timeval startT, initT, endT;
struct timezone tz;

main (argc, argv)
int argc;
char **argv;
{
	register TScreen *screen = &term.screen;
	register int i, pty;
	int Xsocket, mode;
	char *malloc();
	char *basename();
	int xerror(), xioerror();

	xterm_name = (XStrCmp(*argv, "-") == 0) ? "xterm" : basename(*argv);

/* ||| 
gettimeofday(&startT, &tz);
*/
	/* Init the Toolkit. */
	XtInitialize();
/* ||| 
gettimeofday(&initT, &tz);
*/
	/* Parse the command line for resources */
	XtGetUsersDataBase();
	XrmParseCommand(optionDescList, XtNumber(optionDescList), XtNxterm,
	 &argc, argv);

	/* Parse the rest of the command line */
	display[0] = '\0';
	for (argc--, argv++ ; argc > 0 ; argc--, argv++) {
	    if (**argv == '%') {
		T_geometry = *argv;
		*T_geometry = '=';
		continue;
	    }

	    if (**argv == '#') {
		icon_geom = *argv;
		*icon_geom = '=';
		continue;
	    }

	    if(index(*argv, ':') != NULL) {
		strncpy(display, *argv, sizeof(display));
		continue;
	    }

	    if(!(i = (**argv == '-'))) Syntax (*argv);

	    switch(argv[0][1]) {
#ifdef TIOCCONS
	     case 'C':
		Console = TRUE;
		continue;
#endif TIOCCONS
	     case 'L':
		{
		char tt[32];

		L_flag = 1;
		get_ty = argv[--argc];
		strcpy(tt,"/dev/");
		strcat(tt, get_ty);
		tt[5] = 'p';
		loginpty = open( tt, O_RDWR, 0 );
		dup2( loginpty, 4 );
		close( loginpty );
		loginpty = 4;
		tt[5] = 't';
		chown(tt, 0, 0);
		chmod(tt, 0622);
		if (open(tt, O_RDWR, 0) < 0) {
			consolepr("open(%s) failed\n", tt);
		}
		signal(SIGHUP, SIG_IGN);
		vhangup();
		setpgrp(0,0);
		signal(SIGHUP, SIG_DFL);
		(void) close(0);
		open(tt, O_RDWR, 0);
		dup2(0, 1);
		dup2(0, 2);
		continue;
		}
	     case 'S':
		sscanf(*argv + 2, "%c%c%d", passedPty, passedPty+1,
		 &am_slave);
		if (am_slave <= 0) Syntax(*argv);
		continue;
#ifdef DEBUG
	     case 'D':
		debug = TRUE;
		continue;
#endif DEBUG
	     case 'e':
		if (argc <= 1) Syntax (*argv);
		command_to_exec = ++argv;
		break;
	     default:
		Syntax (*argv);
	    }
	    break;
	}


	/* Initialize the display connection */
	OpenDisplay();
	/* Get initial values from .Xdefaults file */
	XtGetResources(
	    screen->display,
	    resourceList,
	    XtNumber(resourceList), 
	    (ArgList) NULL,
	    0,
	    DefaultRootWindow(screen->display),
	    XtNxterm,
	    XtCApp,
	    &nameList,
	    &classList);

/* ||| 
    gettimeofday(&endT, &tz);
    printf("init: %8.3f,   total: %8.3f\n",
        ((initT.tv_sec*1000000.0+initT.tv_usec)
          - (startT.tv_sec*1000000.0+startT.tv_usec))/1000000.0,
        ((endT.tv_sec*1000000.0+endT.tv_usec)
          - (startT.tv_sec*1000000.0+startT.tv_usec))/1000000.0);
   exit(0);
*/
	/* Do additional processing on complex .Xdefaults stuff */

	if (!f_n) {
		if (f_b)
			f_n = f_b;
		else {
			f_n = defaultFont;
			f_b = defaultBoldFont;
		}
	}

	term.flags = WRAPAROUND | AUTOREPEAT;
	if (!screen->jumpscroll)	term.flags |= SMOOTHSCROLL;
	if (reverseWrap)		term.flags |= REVERSEWRAP;

	inhibit = 0;
	if (logInhibit)			inhibit |= I_LOG;
	if (signalInhibit)		inhibit |= I_SIGNAL;
	if (tekInhibit)			inhibit |= I_TEK;

	if (scrollbar)			screen->scrollbar = SCROLLBARWIDTH;

	screen->color = 0;
	if (screen->foreground != XtDefaultFGPixel)
		screen->color |= C_FOREGROUND;
	if (screen->background != XtDefaultBGPixel) {
		screen->color |= C_BACKGROUND;
	}
	if (screen->cursorcolor != XtDefaultFGPixel)
		screen->color |= C_CURSOR;
	if (screen->mousecolor != XtDefaultFGPixel)
		screen->color |= C_MOUSE;

	term.initflags = term.flags;

	if(!window_name)
		window_name = (get_ty ? "login" : (am_slave ? "xterm slave" :
		 (command_to_exec ? basename(command_to_exec[0]) :
		 xterm_name)));
	if (!title_name)
		title_name = window_name;
	if(inhibit & I_TEK)
		screen->TekEmu = FALSE;

	/* set up stderr properly */
	i = -1;
#ifdef DEBUG
	if(debug)
		i = open ("xterm.debug.log", O_WRONLY | O_CREAT | O_TRUNC,
		 0666);
	else
#endif DEBUG
	if(get_ty)
		i = open("/dev/console", O_WRONLY, 0);
	if(i >= 0)
		fileno(stderr) = i;
	if(fileno(stderr) != (NOFILE - 1)) {
		dup2(fileno(stderr), (NOFILE - 1));
		if(fileno(stderr) >= 3)
			close(fileno(stderr));
		fileno(stderr) = (NOFILE - 1);
	}

	signal (SIGCHLD, reapchild);

	/* open a terminal for client */
	get_terminal ();
	spawn ();

	Xsocket = screen->display->fd;
	pty = screen->respond;

	if (am_slave) { /* Write window id so master end can read and use */
	    write(pty, screen->TekEmu ? (char *)&TWindow(screen) :
	     (char *)&VWindow(screen), sizeof(Window));
	    write(pty, "\n", 1);
	}

	if(log_on) {
		log_on = FALSE;
		StartLog(screen);
	}
	screen->inhibit = inhibit;
	mode = 1;
	if (ioctl (pty, FIONBIO, (char *)&mode) == -1) SysError (ERROR_FIONBIO);
	
	pty_mask = 1 << pty;
	X_mask = 1 << Xsocket;
	Select_mask = pty_mask | X_mask;
	max_plus1 = (pty < Xsocket) ? (1 + Xsocket) : (1 + pty);

#ifdef DEBUG
	if (debug) printf ("debugging on\n");
#endif DEBUG
	XSetErrorHandler(xerror);
	XSetIOErrorHandler(xioerror);
	for( ; ; )
		if(screen->TekEmu)
			TekRun();
		else
			VTRun();
}

char *basename(name)
char *name;
{
	register char *cp;
	char *rindex();

	return((cp = rindex(name, '/')) ? cp + 1 : name);
}

static char *ustring[] = {
"Usage: xterm [-132] [-b inner_border_width] [-bd border_color] \\\n",
#ifdef TIOCCONS
" [-bg backgrnd_color] [-bw border_width] [-C] [-cr cursor_color] [-cu] \\\n",
#else TIOCCONS
" [-bg backgrnd_color] [-bw border_width] [-cr cursor_color] [-cu] \\\n",
#endif TIOCCONS
" [-fb bold_font] [-fg foregrnd_color] [-fn norm_font] \\\n",
" [-i] [-j] [-l] [-lf logfile] [-ls] [-mb] [-ms mouse_color] \\\n",
" [-n name] [-nb bell_margin] [-rv] [-rw] [-s] \\\n",
" [-sb] [-si] [-sk] [-sl save_lines] [-sn] [-st] [-T title] [-t] [-tb] \\\n",
" [-vb] [=[width]x[height][[+-]xoff[[+-]yoff]]] \\\n",
" [%[width]x[height][[+-]xoff[[+-]yoff]]] [#[+-]xoff[[+-]yoff]] \\\n",
" [-e command_to_exec]\n\n",
"Fonts must be of fixed width and of same size;\n",
"If only one font is specified, it will be used for normal and bold text\n",
"The -132 option allows 80 <-> 132 column escape sequences\n",
#ifdef TIOCCONS
"The -C option forces output to /dev/console to appear in this window\n",
#endif TIOCCONS
"The -cu option turns a curses bug fix on\n",
"The -i  option enables iconic startup\n",
"The -j  option enables jump scroll\n",
"The -l  option enables logging\n",
"The -ls option makes the shell a login shell\n",
"The -mb option turns the margin bell on\n",
"The -rv option turns reverse video on\n",
"The -rw option turns reverse wraparound on\n",
"The -s  option enables asynchronous scrolling\n",
"The -sb option enables the scrollbar\n",
"The -si option disables re-positioning the scrollbar at the bottom on input\n",
"The -sk option causes the scrollbar to position at the bottom on a key\n",
"The -t  option starts Tektronix mode\n",
"The -vb option enables visual bell\n",
0
};

Syntax (badOption)
char	*badOption;
{
	register char **us = ustring;

	fprintf(stderr, "Unknown option \"%s\"\n\n", badOption);
	while (*us) fputs(*us++, stderr);
	exit (1);
}

get_pty (pty, tty)
/*
   opens a pty, storing fildes in pty and tty.
 */
int *pty, *tty;
{
	int devindex, letter = 0;

	while (letter < 11) {
	    ttydev [8] = ptydev [8] = "pqrstuvwxyz" [letter++];
	    devindex = 0;

	    while (devindex < 16) {
		ttydev [9] = ptydev [9] = "0123456789abcdef" [devindex++];
		if ((*pty = open (ptydev, O_RDWR)) < 0)
			continue;
		if ((*tty = open (ttydev, O_RDWR)) < 0) {
			close(*pty);
			continue;
		}
		return;
	    }
	}
	
	fprintf (stderr, "%s: Not enough available pty's\n", xterm_name);
	exit (ERROR_PTYS);
}

get_terminal ()
/* 
 * sets up X and initializes the terminal structure except for term.buf.fildes.
 */
{
	register TScreen *screen = &term.screen;
	char *malloc();
	
	screen->graybordertile = make_gray(screen->bordercolor,
		screen->background,
		DefaultDepth(screen->display, DefaultScreen(screen->display)));

	screen->arrow = make_arrow(screen->mousecolor, screen->background);

	XAutoRepeatOn(screen->display);

	if((screen->iconname = malloc((unsigned) strlen(window_name) + 10)) == NULL)
		Error(ERROR_WINNAME);
	strcpy(screen->iconname, window_name);
	screen->iconnamelen = strlen(screen->iconname);
	if((screen->titlename = malloc((unsigned) strlen(title_name) + 10)) == NULL)
		Error(ERROR_WINNAME);
	strcpy(screen->titlename, title_name);
	screen->titlenamelen = strlen(screen->titlename);

}

static char *tekterm[] = {
	"tek4015",
	"tek4014",
	"tek4013",
	"tek4010",
	"dumb",
	0
};

static char *vtterm[] = {
	"xterm",
	"vt102",
	"vt100",
	"ansi",
	"dumb",
	0
};

hungtty()
{
	longjmp(env, 1);
}

spawn ()
/* 
 *  Inits pty and tty and forks a login process.
 *  Does not close fd Xsocket.
 *  If getty,  execs getty rather than csh and uses std fd's rather
 *  than opening a pty/tty pair.
 *  If slave, the pty named in passedPty is already open for use
 */
{
	register TScreen *screen = &term.screen;
	int Xsocket = screen->display->fd;
	int index1, tty = -1;
	int discipline;
	unsigned lmode;
	struct tchars tc;
	struct ltchars ltc;
	struct sgttyb sg;

	char termcap [1024];
	char newtc [1024];
	char *ptr, *shname;
	int i, no_dev_tty = FALSE;
	char **envnew;		/* new environment */
	char buf[32];
	char *TermName = NULL;
	int ldisc = 0;
#ifdef sun
#ifdef TIOCSSIZE
	struct ttysize ts;
#endif TIOCSSIZE
#else sun
#ifdef TIOCSWINSZ
	struct winsize ws;
#endif TIOCSWINSZ
#endif sun
	struct passwd *pw = NULL;
#ifdef UTMP
	struct utmp utmp;
#endif UTMP
	extern int Exit();
	char *getenv();
	char *index (), *rindex (), *strindex ();

	screen->uid = getuid();
	screen->gid = getgid();

	/* so that TIOCSWINSZ || TIOCSIZE doesn't block */
	signal(SIGTTOU,SIG_IGN);
	if(!(screen->TekEmu ? TekInit() : VTInit()))
		exit(ERROR_INIT);

	if(screen->TekEmu) {
		envnew = tekterm;
		ptr = newtc;
	} else {
		envnew = vtterm;
		ptr = termcap;
	}
	while(*envnew) {
		if(tgetent(ptr, *envnew) == 1) {
			TermName = *envnew;
			if(!screen->TekEmu)
			    resize(screen, TermName, termcap, newtc);
			break;
		}
		envnew++;
	}

	if (get_ty) {
		screen->respond = loginpty;
		if((tslot = ttyslot()) <= 0)
			SysError(ERROR_TSLOT);
	} else if (am_slave) {
		screen->respond = am_slave;
		ptydev[8] = ttydev[8] = passedPty[0];
		ptydev[9] = ttydev[9] = passedPty[1];
		if((tslot = ttyslot()) <= 0)
			SysError(ERROR_TSLOT2);
		setgid (screen->gid);
		setuid (screen->uid);
	} else {
 		/*
 		 * Sometimes /dev/tty hangs on open (as in the case of a pty
 		 * that has gone away).  Simply make up some reasonable
 		 * defaults.
 		 */
 		signal(SIGALRM, hungtty);
 		alarm(1);		
 		if (! setjmp(env)) {
 			tty = open ("/dev/tty", O_RDWR, 0);
 			alarm(0);
 		} else {
 			tty = -1;
 			errno = ENXIO;
 		}
 		signal(SIGALRM, SIG_DFL);
 
 		if (tty < 0) {
			if (errno != ENXIO) SysError(ERROR_OPDEVTTY);
			else {
				no_dev_tty = TRUE;
				sg = d_sg;
				tc = d_tc;
				discipline = d_disipline;
				ltc = d_ltc;
				lmode = d_lmode;
			}
		} else {
			/* get a copy of the current terminal's state */

			if(ioctl(tty, TIOCGETP, (char *)&sg) == -1)
				SysError (ERROR_TIOCGETP);
			if(ioctl(tty, TIOCGETC, (char *)&tc) == -1)
				SysError (ERROR_TIOCGETC);
			if(ioctl(tty, TIOCGETD, (char *)&discipline) == -1)
				SysError (ERROR_TIOCGETD);
			if(ioctl(tty, TIOCGLTC, (char *)&ltc) == -1)
				SysError (ERROR_TIOCGLTC);
			if(ioctl(tty, TIOCLGET, (char *)&lmode) == -1)
				SysError (ERROR_TIOCLGET);
			close (tty);

			/* close all std file descriptors */
			for (index1 = 0; index1 < 3; index1++)
				close (index1);
			if ((tty = open ("/dev/tty", O_RDWR, 0)) < 0)
				SysError (ERROR_OPDEVTTY2);

			if (ioctl (tty, TIOCNOTTY, (char *) NULL) == -1)
				SysError (ERROR_NOTTY);
			close (tty);
		}

		get_pty (&screen->respond, &tty);

		if (screen->respond != Xsocket + 1) {
			dup2 (screen->respond, Xsocket + 1);
			close (screen->respond);
			screen->respond = Xsocket + 1;
		}

		/* change ownership of tty to real group and user id */
		chown (ttydev, screen->uid, screen->gid);

		/* change protection of tty */
		chmod (ttydev, 0622);

		if (tty != Xsocket + 2)	{
			dup2 (tty, Xsocket + 2);
			close (tty);
			tty = Xsocket + 2;
		}

		/* set the new terminal's state to be the old one's 
		   with minor modifications for efficiency */

		sg.sg_flags &= ~(ALLDELAY | XTABS | CBREAK | RAW);
		sg.sg_flags |= ECHO | CRMOD;
		/* make sure speed is set on pty so that editors work right*/
		sg.sg_ispeed = B9600;
		sg.sg_ospeed = B9600;
		/* reset t_brkc to default value */
		tc.t_brkc = -1;

		if (ioctl (tty, TIOCSETP, (char *)&sg) == -1)
			SysError (ERROR_TIOCSETP);
		if (ioctl (tty, TIOCSETC, (char *)&tc) == -1)
			SysError (ERROR_TIOCSETC);
		if (ioctl (tty, TIOCSETD, (char *)&discipline) == -1)
			SysError (ERROR_TIOCSETD);
		if (ioctl (tty, TIOCSLTC, (char *)&ltc) == -1)
			SysError (ERROR_TIOCSLTC);
		if (ioctl (tty, TIOCLSET, (char *)&lmode) == -1)
			SysError (ERROR_TIOCLSET);
#ifdef TIOCCONS
		if (Console) {
			int on = 1;
			if (ioctl (tty, TIOCCONS, (char *)&on) == -1)
				SysError(ERROR_TIOCCONS);
		}
#endif TIOCCONS

		close (open ("/dev/null", O_RDWR, 0));

		for (index1 = 0; index1 < 3; index1++)
			dup2 (tty, index1);
		if((tslot = ttyslot()) <= 0)
			SysError(ERROR_TSLOT3);
#ifdef UTMP
		if((pw = getpwuid(screen->uid)) &&
		 (i = open(etc_utmp, O_WRONLY)) >= 0) {
			bzero((char *)&utmp, sizeof(struct utmp));
			(void) strcpy(utmp.ut_line, &ttydev[5]);
			(void) strcpy(utmp.ut_name, pw->pw_name);
			(void) strcpy(utmp.ut_host, XDisplayName(display));
			time(&utmp.ut_time);
			lseek(i, (long)(tslot * sizeof(struct utmp)), 0);
			write(i, (char *)&utmp, sizeof(struct utmp));
			close(i);
		} else
			tslot = -tslot;
#endif UTMP
	}

#ifdef sun
#ifdef TIOCSSIZE
	/* tell tty how big window is */
	if(screen->TekEmu) {
		ts.ts_lines = 38;
		ts.ts_cols = 81;
	} else {
		ts.ts_lines = screen->max_row + 1;
		ts.ts_cols = screen->max_col + 1;
	}
	ioctl  (screen->respond, TIOCSSIZE, &ts);
#endif TIOCSSIZE
#else sun
#ifdef TIOCSWINSZ
	/* tell tty how big window is */
	if(screen->TekEmu) {
		ws.ws_row = 38;
		ws.ws_col = 81;
		ws.ws_xpixel = TFullWidth(screen);
		ws.ws_ypixel = TFullHeight(screen);
	} else {
		ws.ws_row = screen->max_row + 1;
		ws.ws_col = screen->max_col + 1;
		ws.ws_xpixel = FullWidth(screen);
		ws.ws_ypixel = FullHeight(screen);
	}
	ioctl (screen->respond, TIOCSWINSZ, (char *)&ws);
#endif TIOCSWINSZ
#endif sun

	if (!am_slave) {
	    if ((screen->pid = fork ()) == -1)
		SysError (ERROR_FORK);
		
	    if (screen->pid == 0) {
		extern char **environ;
		int pgrp = getpid();

		close (Xsocket);
		close (screen->respond);
		if(fileno(stderr) >= 3)
			close (fileno(stderr));

		if (tty >= 0) close (tty);

		signal (SIGCHLD, SIG_DFL);
		signal (SIGHUP, SIG_IGN);

		/* copy the environment before Setenving */
		for (i = 0 ; environ [i] != NULL ; i++) ;
		/*
		 * The `4' is the number of Setenv() calls which may add
		 * a new entry to the environment.  The `1' is for the
		 * NULL terminating entry.
		 */
		envnew = (char **) calloc ((unsigned) i + (4 + 1), sizeof(char *));
		bcopy((char *)environ, (char *)envnew, i * sizeof(char *));
		environ = envnew;
		Setenv ("TERM=", TermName);
		if(!TermName)
			*newtc = 0;
		Setenv ("TERMCAP=", newtc);
		sprintf(buf, "%d", screen->TekEmu ? (int)TWindow(screen) :
		 (int)VWindow(screen));
		Setenv ("WINDOWID=", buf);
		/* put the display into the environment of the shell*/
		if (display[0] != '\0') 
			Setenv ("DISPLAY=", XDisplayName(display));

		signal(SIGTERM, SIG_DFL);
		ioctl(0, TIOCSPGRP, (char *)&pgrp);
		setpgrp (0, 0);
		close(open(ttyname(0), O_WRONLY, 0));
		setpgrp (0, pgrp);

		setgid (screen->gid);
		setuid (screen->uid);

		if (command_to_exec) {
			execvp(*command_to_exec, command_to_exec);
			/* print error message on screen */
			fprintf(stderr, "%s: Can't execvp %s\n", xterm_name,
			 *command_to_exec);
		}
		signal(SIGHUP, SIG_IGN);
		if (get_ty) {
			ioctl (0, TIOCNOTTY, (char *) NULL);
			execl ("/etc/getty", "+", "Xwindow", get_ty, 0);
		}
		signal(SIGHUP, SIG_DFL);

#ifdef UTMP
		if(((ptr = getenv("SHELL")) == NULL || *ptr == 0) &&
		 ((pw == NULL && (pw = getpwuid(screen->uid)) == NULL) ||
		 *(ptr = pw->pw_shell) == 0))
#else UTMP
		if(((ptr = getenv("SHELL")) == NULL || *ptr == 0) &&
		 ((pw = getpwuid(screen->uid)) == NULL ||
		 *(ptr = pw->pw_shell) == 0))
#endif UTMP
			ptr = "/bin/sh";
		if(shname = rindex(ptr, '/'))
			shname++;
		else
			shname = ptr;
		ldisc = XStrCmp("csh", shname + strlen(shname) - 3) == 0 ?
		 NTTYDISC : 0;
		ioctl(0, TIOCSETD, (char *)&ldisc);
		execl (ptr, login_shell ? "-" : shname, 0);
		fprintf (stderr, "%s: Could not exec %s!\n", xterm_name, ptr);
		sleep(5);
		exit(ERROR_EXEC);
	    }
	}

	if(tty >= 0) close (tty);
	signal(SIGHUP,SIG_IGN);

	if (!no_dev_tty) {
		if ((tty = open ("/dev/tty", O_RDWR, 0)) < 0)
			SysError(ERROR_OPDEVTTY3);
		for (index1 = 0; index1 < 3; index1++)
			dup2 (tty, index1);
		if (tty > 2) close (tty);
	}

	signal(SIGINT, Exit); 
	signal(SIGQUIT, Exit);
	signal(SIGTERM, Exit);
}

Exit(n)
int n;
{
	register TScreen *screen = &term.screen;
        int pty = term.screen.respond;  /* file descriptor of pty */
#ifdef UTMP
	register int i;
	struct utmp utmp;

	if(!am_slave && tslot > 0 && (i = open(etc_utmp, O_WRONLY)) >= 0) {
		bzero((char *)&utmp, sizeof(struct utmp));
		lseek(i, (long)(tslot * sizeof(struct utmp)), 0);
		write(i, (char *)&utmp, sizeof(struct utmp));
		close(i);
	}
#endif UTMP
        close(pty); /* close explicitly to avoid race with slave side */
	if(screen->logging)
		CloseLog(screen);

	if(!get_ty && !am_slave) {
		/* restore ownership of tty */
		chown (ttydev, 0, 0);

		/* restore modes of tty */
		chmod (ttydev, 0666);
	}
	exit(n);
}

resize(screen, TermName, oldtc, newtc)
TScreen *screen;
char *TermName;
register char *oldtc, *newtc;
{
	register char *ptr1, *ptr2;
	register int i;
	register int li_first = 0;
	register char *temp;
	char *index(), *strindex();

	if ((ptr1 = strindex (oldtc, "co#")) == NULL){
		fprintf(stderr, "%s: Can't find co# in termcap string %s\n",
			xterm_name, TermName);
		exit (ERROR_NOCO);
	}
	if ((ptr2 = strindex (oldtc, "li#")) == NULL){
		fprintf(stderr, "%s: Can't find li# in termcap string %s\n",
			xterm_name, TermName);
		exit (ERROR_NOLI);
	}
	if(ptr1 > ptr2) {
		li_first++;
		temp = ptr1;
		ptr1 = ptr2;
		ptr2 = temp;
	}
	ptr1 += 3;
	ptr2 += 3;
	strncpy (newtc, oldtc, i = ptr1 - oldtc);
	newtc += i;
	sprintf (newtc, "%d", li_first ? screen->max_row + 1 :
	 screen->max_col + 1);
	newtc += strlen(newtc);
	ptr1 = index (ptr1, ':');
	strncpy (newtc, ptr1, i = ptr2 - ptr1);
	newtc += i;
	sprintf (newtc, "%d", li_first ? screen->max_col + 1 :
	 screen->max_row + 1);
	ptr2 = index (ptr2, ':');
	strcat (newtc, ptr2);
}

static reapchild ()
{
	union wait status;
	register int pid;
	
#ifdef DEBUG
	if (debug) fputs ("Exiting\n", stderr);
#endif DEBUG
	pid  = wait3 (&status, WNOHANG, (struct rusage *)NULL);
	if (!pid) return;
	if (pid != term.screen.pid) return;
	
	Cleanup(0);
}

/* VARARGS1 */
consolepr(fmt,x0,x1,x2,x3,x4,x5,x6,x7,x8,x9)
char *fmt;
{
	extern int errno;
	extern char *sys_errlist[];
	int oerrno;
	int f;
 	char buf[ BUFSIZ ];

	oerrno = errno;
 	strcpy(buf, "xterm: ");
 	sprintf(buf+strlen(buf), fmt, x0,x1,x2,x3,x4,x5,x6,x7,x8,x9);
 	strcat(buf, ": ");
 	strcat(buf, sys_errlist[oerrno]);
 	strcat(buf, "\n");	
	f = open("/dev/console",O_WRONLY);
	write(f, buf, strlen(buf));
	close(f);
	if ((f = open("/dev/tty", 2)) >= 0) {
		ioctl(f, TIOCNOTTY, (char *)NULL);
		close(f);
	}
}

checklogin()
{
	register int ts, i;
	register struct passwd *pw;
	struct utmp utmp;

	ts = tslot > 0 ? tslot : -tslot;
	if((i = open(etc_utmp, O_RDONLY)) < 0)
		return(FALSE);
	lseek(i, (long)(ts * sizeof(struct utmp)), 0);
	ts = read(i, (char *)&utmp, sizeof(utmp));
	close(i);
	if(ts != sizeof(utmp) || XStrCmp(get_ty, utmp.ut_line) != 0 ||
	 !*utmp.ut_name || (pw = getpwnam(utmp.ut_name)) == NULL)
		return(FALSE);
	chdir(pw->pw_dir);
	setgid(pw->pw_gid);
	setuid(pw->pw_uid);
	L_flag = 0;
	return(TRUE);
}
