#ifndef lint
static char rcs_id[] = "@Header: main.c,v 1.23 88/02/26 09:14:01 swick Exp @";
#endif	/* lint */

/*
 * WARNING:  This code (particularly, the tty setup code) is a historical
 * relic and should not be confused with a real toolkit application or a
 * an example of how to do anything.  It really needs a rewrite.  Badly.
 */

#include <X11/copyright.h>

/***********************************************************
Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts,
and the Massachusetts Institute of Technology, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Digital or MIT not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/


/* main.c */

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <pwd.h>
#ifdef SYSV
#include <sys/ioctl.h>
#include <sys/termio.h>
#include <sys/ptyio.h>
#include <sys/stat.h>
#ifdef JOBCONTROL
#include <sys/bsdtty.h>
#endif	/* JOBCONTROL */
#else	/* !SYSV */
#include <sgtty.h>
#include <sys/wait.h>
#include <sys/resource.h>
#endif	/* !SYSV */
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#ifdef hpux
#include <sys/utsname.h>
#endif

#ifdef apollo
#define ttyslot() 1
#define vhangup() ;
#endif

#include <utmp.h>
#include <sys/param.h>	/* for NOFILE */

#include "ptyx.h"
#include "data.h"
#include "error.h"
#include "main.h"
#include <X11/Atoms.h>
#include <X11/Shell.h>

extern Pixmap make_gray();
extern char *malloc();
extern char *calloc();
extern char *ttyname();
extern void exit();
extern void sleep();
extern void bcopy();
extern void vhangup();
extern long lseek();

int switchfb[] = {0, 2, 1, 3};

static int reapchild ();

static Bool added_utmp_entry = False;

static char **command_to_exec;
#ifdef SYSV
/* The following structures are initialized in main() in order
** to eliminate any assumptions about the internal order of their
** contents.
*/
static struct termio d_tio;
#ifdef TIOCSLTC
static struct ltchars d_ltc;
#endif	/* TIOCSLTC */
#ifdef TIOCLSET
static unsigned int d_lmode;
#endif	/* TIOCLSET */
#else	/* !SYSV */
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
#endif	/* !SYSV */
#ifdef SYSV
extern struct utmp *getutent();
extern struct utmp *getutid();
extern struct utmp *getutline();
extern void pututline();
extern void setutent();
extern void endutent();
extern void utmpname();

extern struct passwd *getpwent();
extern struct passwd *getpwuid();
extern struct passwd *getpwnam();
extern void setpwent();
extern void endpwent();
extern struct passwd *fgetpwent();
#else	/* SYSV */
static char etc_utmp[] = "/etc/utmp";
#endif	/* SYSV */
static char *get_ty;
static int inhibit;
static int log_on;
static int login_shell;
static char passedPty[2];	/* name if pty if slave */
static int loginpty;
#ifdef TIOCCONS
static int Console;
#endif	/* TIOCCONS */
#ifndef SYSV
static int tslot;
#endif	/* !SYSV */
static jmp_buf env;

static char *icon_geometry;
static char *title;

/* used by VT (charproc.c) */

static XtResource application_resources[] = {
    {"name", "Name", XtRString, sizeof(char *),
	(Cardinal)&xterm_name, XtRString, "xterm"},
    {"iconGeometry", "IconGeometry", XtRString, sizeof(char *),
	(Cardinal)&icon_geometry, XtRString, (caddr_t) NULL},
    {XtNtitle, XtCTitle, XtRString, sizeof(char *),
	(Cardinal)&title, XtRString, (caddr_t) NULL},
};

/* Command line options table.  Only resources are entered here...there is a
   pass over the remaining options after XtParseCommand is let loose. */

static XrmOptionDescRec optionDescList[] = {
{"-geometry",	"*vt100.geometry",XrmoptionSepArg,	(caddr_t) NULL},
{"-132",	"*c132",	XrmoptionNoArg,		(caddr_t) "on"},
{"+132",	"*c132",	XrmoptionNoArg,		(caddr_t) "off"},
{"-b",		"*internalBorder",XrmoptionSepArg,	(caddr_t) NULL},
{"-cr",		"*cursorColor",	XrmoptionSepArg,	(caddr_t) NULL},
{"-cu",		"*curses",	XrmoptionNoArg,		(caddr_t) "on"},
{"+cu",		"*curses",	XrmoptionNoArg,		(caddr_t) "off"},
{"-e",		NULL,		XrmoptionSkipLine,	(caddr_t) NULL},
{"-fb",		"*boldFont",	XrmoptionSepArg,	(caddr_t) NULL},
{"-j",		"*jumpScroll",	XrmoptionNoArg,		(caddr_t) "on"},
{"+j",		"*jumpScroll",	XrmoptionNoArg,		(caddr_t) "off"},
{"-l",		"*logging",	XrmoptionNoArg,		(caddr_t) "on"},
{"+l",		"*logging",	XrmoptionNoArg,		(caddr_t) "off"},
{"-lf",		"*logFile",	XrmoptionSepArg,	(caddr_t) NULL},
{"-ls",		"*loginShell",	XrmoptionNoArg,		(caddr_t) "on"},
{"+ls",		"*loginShell",	XrmoptionNoArg,		(caddr_t) "off"},
{"-mb",		"*marginBell",	XrmoptionNoArg,		(caddr_t) "on"},
{"+mb",		"*marginBell",	XrmoptionNoArg,		(caddr_t) "off"},
{"-ms",		"*pointerColor",XrmoptionSepArg,	(caddr_t) NULL},
{"-nb",		"*nMarginBell",	XrmoptionSepArg,	(caddr_t) NULL},
{"-rw",		"*reverseWrap",	XrmoptionNoArg,		(caddr_t) "on"},
{"+rw",		"*reverseWrap",	XrmoptionNoArg,		(caddr_t) "off"},
{"-s",		"*multiScroll",	XrmoptionNoArg,		(caddr_t) "on"},
{"+s",		"*multiScroll",	XrmoptionNoArg,		(caddr_t) "off"},
{"-sb",		"*scrollBar",	XrmoptionNoArg,		(caddr_t) "on"},
{"+sb",		"*scrollBar",	XrmoptionNoArg,		(caddr_t) "off"},
{"-si",		"*scrollInput",	XrmoptionNoArg,		(caddr_t) "off"},
{"+si",		"*scrollInput",	XrmoptionNoArg,		(caddr_t) "on"},
{"-sk",		"*scrollKey",	XrmoptionNoArg,		(caddr_t) "on"},
{"+sk",		"*scrollKey",	XrmoptionNoArg,		(caddr_t) "off"},
{"-sl",		"*saveLines",	XrmoptionSepArg,	(caddr_t) NULL},
{"-t",		"*tekStartup",	XrmoptionNoArg,		(caddr_t) "on"},
{"+t",		"*tekStartup",	XrmoptionNoArg,		(caddr_t) "off"},
{"-vb",		"*visualBell",	XrmoptionNoArg,		(caddr_t) "on"},
{"+vb",		"*visualBell",	XrmoptionNoArg,		(caddr_t) "off"},
/* bogus old compatibility stuff for which there are
   standard XtInitialize options now */
#ifndef TRASHEQUALGEOMETRY
{"=",		"*vt100.geometry",XrmoptionStickyArg,	(caddr_t) NULL},
#endif
{"%",		"*tekGeometry",	XrmoptionStickyArg,	(caddr_t) NULL},
{"#",		".iconGeometry",XrmoptionStickyArg,	(caddr_t) NULL},
{"-T",		"*title",	XrmoptionSepArg,	(caddr_t) NULL},
{"-n",		"*iconName",	XrmoptionSepArg,	(caddr_t) NULL},
{"-r",		"*reverseVideo",XrmoptionNoArg,		(caddr_t) "on"},
{"+r",		"*reverseVideo",XrmoptionNoArg,		(caddr_t) "off"},
{"-rv",		"*reverseVideo",XrmoptionNoArg,		(caddr_t) "on"},
{"+rv",		"*reverseVideo",XrmoptionNoArg,		(caddr_t) "off"},
{"-w",		".TopLevelShell.borderWidth", XrmoptionSepArg, (caddr_t) NULL},
};

extern WidgetClass xtermWidgetClass;

Arg ourTopLevelShellArgs[] = {
	{ XtNallowShellResize, (XtArgVal) TRUE },	
	{ XtNinput, (XtArgVal) TRUE },
};
int number_ourTopLevelShellArgs = 2;
	
Widget toplevel;

main (argc, argv)
int argc;
char **argv;
{
	register TScreen *screen;
	register int i, pty;
	int Xsocket, mode;
	char *malloc();
	char *basename();
	int xerror(), xioerror();
	int fd1 = -1;
	int fd2 = -1;
	int fd3 = -1;

	/* close any extra open (stray) file descriptors */
	for (i = 3; i < NOFILE; i++)
		(void) close(i);

#ifdef SYSV
	/* Initialization is done here rather than above in order
	** to prevent any assumptions about the order of the contents
	** of the various terminal structures (which may change from
	** implementation to implementation.
	*/
	d_tio.c_iflag = ICRNL|IXON;
	d_tio.c_oflag = OPOST|ONLCR|TAB3;
#ifdef BAUD_0
    	d_tio.c_cflag = CS8|CREAD|PARENB|HUPCL;
#else	/* !BAUD_0 */
    	d_tio.c_cflag = B9600|CS8|CREAD|PARENB|HUPCL;
#endif	/* !BAUD_0 */
    	d_tio.c_lflag = ISIG|ICANON|ECHO|ECHOE|ECHOK;
	d_tio.c_line = 0;
	d_tio.c_cc[VINTR] = 0x7f;		/* DEL  */
	d_tio.c_cc[VQUIT] = '\\' & 0x3f;	/* '^\'	*/
	d_tio.c_cc[VERASE] = '#';		/* '#'	*/
	d_tio.c_cc[VKILL] = '@';		/* '@'	*/
    	d_tio.c_cc[VEOF] = 'D' & 0x3f;		/* '^D'	*/
	d_tio.c_cc[VEOL] = '@' & 0x3f;		/* '^@'	*/
#ifdef VSWITCH
	d_tio.c_cc[VSWITCH] = '@' & 0x3f;	/* '^@'	*/
#endif	/* VSWITCH */
	/* now, try to inherit tty settings */
	{
	    int i;

	    for (i = 0; i <= 2; i++) {
		struct termio deftio;
		if (ioctl (i, TCGETA, &deftio) == 0) {
		    d_tio.c_cc[VINTR] = deftio.c_cc[VINTR];
		    d_tio.c_cc[VQUIT] = deftio.c_cc[VQUIT];
		    d_tio.c_cc[VERASE] = deftio.c_cc[VERASE];
		    d_tio.c_cc[VKILL] = deftio.c_cc[VKILL];
		    d_tio.c_cc[VEOF] = deftio.c_cc[VEOF];
		    d_tio.c_cc[VEOL] = deftio.c_cc[VEOL];
#ifdef VSWITCH
		    d_tio.c_cc[VSWITCH] = deftio.c_cc[TSWITCH];
#endif
		    break;
		}
	    }
	}
#ifdef TIOCSLTC
        d_ltc.t_suspc = '\000';		/* t_suspc */
        d_ltc.t_dsuspc = '\000';	/* t_dsuspc */
        d_ltc.t_rprntc = '\377';	/* reserved...*/
        d_ltc.t_flushc = '\377';
        d_ltc.t_werasc = '\377';
        d_ltc.t_lnextc = '\377';
#endif	/* TIOCSLTC */
#ifdef TIOCLSET
	d_lmode = 0;
#endif	/* TIOCLSET */
#endif	/* !SYSV */

	/* This is ugly.  When running under init, we need to make sure
	 * Xlib/Xt won't use file descriptors 0/1/2, because we need to
	 * stomp on them.  This check doesn't guarantee a -L found is
	 * really an option, but the opens don't hurt anyway.
	 */
	for (i = 1; i < argc; i++) {
	    if ((argv[i][0] == '-') && (argv[i][1] == 'L')) {
		fd1 = open ("/dev/null", O_RDONLY, 0);
		fd2 = open ("/dev/null", O_RDONLY, 0);
		fd3 = open ("/dev/null", O_RDONLY, 0);
		break;
	    }
	}
	/* Init the Toolkit. */
	toplevel = XtInitialize("main", "XTerm",
		optionDescList, XtNumber(optionDescList), &argc, argv);

	XtGetApplicationResources( toplevel, 0, application_resources,
				   XtNumber(application_resources), NULL, 0 );

	if (strcmp(xterm_name, "-") == 0) xterm_name = "xterm";
	if (icon_geometry != NULL) {
	    int scr, junk;
	    Arg args[2];

	    for(scr = 0;	/* yyuucchh */
		XtScreen(toplevel) != ScreenOfDisplay(XtDisplay(toplevel),scr);
		scr++);

	    args[0].name = XtNiconX;
	    args[1].name = XtNiconY;
	    XGeometry(XtDisplay(toplevel), scr, icon_geometry, "", 0, 0, 0,
		      0, 0, &args[0].value, &args[1].value, &junk, &junk);
	    XtSetValues( toplevel, args, 2);
	}

	XtSetValues (toplevel, ourTopLevelShellArgs, 
		     number_ourTopLevelShellArgs);

	/* Now that we are in control again, close any uglies. */
	if (fd1 >= 0)
	    (void)close(fd1);
	if (fd2 >= 0)
	    (void)close(fd2);
	if (fd3 >= 0)
	    (void)close(fd3);


	/* Parse the rest of the command line */
	for (argc--, argv++ ; argc > 0 ; argc--, argv++) {
	    if(**argv != '-') Syntax (*argv);

	    switch(argv[0][1]) {
#ifdef TIOCCONS
	     case 'C':
		Console = TRUE;
		continue;
#endif	/* TIOCCONS */
	     case 'L':
		{
		static char *t_ptydev = PTYDEV;
		static char *t_ttydev = TTYDEV;

		L_flag = 1;
		get_ty = argv[--argc];
		t_ptydev[strlen(t_ptydev) - 2] =
			t_ttydev[strlen(t_ttydev) - 2] =
			get_ty[strlen(get_ty) - 2];
		t_ptydev[strlen(t_ptydev) - 1] =
			t_ttydev[strlen(t_ttydev) - 1] =
			get_ty[strlen(get_ty) - 1];
#ifdef SYSV
		/* use the same tty name that everyone else will use
		** (from ttyname)
		*/
		{
			char *ptr;

			if (ptr = ttyname(t_ptydev)) {
				/* it may be bigger! */
				t_ptydev = malloc((unsigned) (strlen(ptr) + 1));
				(void) strcpy(t_ptydev, ptr);
			}
		}
#endif	/* SYSV */
		loginpty = open( t_ptydev, O_RDWR, 0 );
		dup2( loginpty, 4);
		close( loginpty );
		loginpty = 4;
		chown(t_ttydev, 0, 0);
		chmod(t_ttydev, 0622);
		if (open(t_ttydev, O_RDWR, 0) < 0) {
			consolepr("open(%s) failed\n", t_ttydev);
		}
		signal(SIGHUP, SIG_IGN);
#ifdef SYSV
#ifdef JOBCONTROL
		{
			/* kind of do a vhangup */
			int tty_pgrp;

			if (!ioctl(0, TIOCGPGRP, &tty_pgrp)) {
				(void) kill (-tty_pgrp, SIGHUP);
			}
		}
		setpgrp2(0,0);
#else	/* !JOBCONTROL */
		setpgrp();
#endif	/* !JOBCONTROL */
#else	/* !SYSV */
		vhangup();
		setpgrp(0,0);
#endif	/* !SYSV */
		signal(SIGHUP, SIG_DFL);
		(void) close(0);
		open(t_ttydev, O_RDWR, 0);
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
#endif	/* DEBUG */
	     case 'e':
		if (argc <= 1) Syntax (*argv);
		command_to_exec = ++argv;
		break;
	     default:
		Syntax (*argv);
	    }
	    break;
	}

        term = (XtermWidget) XtCreateManagedWidget(
	    "vt100", xtermWidgetClass, toplevel, NULL, 0);
            /* this causes the initialize method to be called */

        screen = &term->screen;

	term->flags = WRAPAROUND | AUTOREPEAT;
	if (!screen->jumpscroll)	term->flags |= SMOOTHSCROLL;
	if (term->misc.reverseWrap)		term->flags |= REVERSEWRAP;

	inhibit = 0;
	if (term->misc.logInhibit)			inhibit |= I_LOG;
	if (term->misc.signalInhibit)		inhibit |= I_SIGNAL;
	if (term->misc.tekInhibit)			inhibit |= I_TEK;

	if (term->misc.scrollbar)			screen->scrollbar = SCROLLBARWIDTH;

	term->initflags = term->flags;

	if ((get_ty || command_to_exec) && !title) {
	    char window_title[1024];
	    static Arg args[] = {
		{XtNtitle, NULL},
		{XtNiconName, NULL},
	    };
	    if (get_ty) {
#ifdef hpux
		struct utsname name;
#endif

		strcpy(window_title, "login(");

#ifdef hpux
		/* Why not use gethostname()?  Well, at least on my system, I've had to
		 * make an ugly kernel patch to get a name longer than 8 characters, and
		 * uname() lets me access to the whole string (it smashes release, you
		 * see), whereas gethostname() kindly truncates it for me.
		 */

		uname(&name);
		strcpy(window_title+6, name.nodename);
#else
		(void) gethostname(window_title+6, sizeof(window_title)-6);
#endif /* hpux */

		strcat( window_title, ")" );
		args[0].value = args[1].value = (XtArgVal)window_title;
	    }
	    else if (command_to_exec) {
		args[0].value = args[1].value =
		    (XtArgVal)basename(command_to_exec[0]);
	    }
	    XtSetValues( toplevel, args, 2 );
	}

	if(inhibit & I_TEK)
		screen->TekEmu = FALSE;

	if(screen->TekEmu && !TekInit())
		exit(ERROR_INIT);

	/* set up stderr properly */
	i = -1;
#ifdef DEBUG
	if(debug)
		i = open ("xterm.debug.log", O_WRONLY | O_CREAT | O_TRUNC,
		 0666);
	else
#endif	/* DEBUG */
	if(get_ty)
		i = open("/dev/console", O_WRONLY, 0);
	if(i >= 0)
		fileno(stderr) = i;
	if(fileno(stderr) != (NOFILE - 1)) {
#ifdef SYSV
		/* SYSV has another pointer which should be part of the
		** FILE structure but is actually a seperate array.
		*/
		unsigned char *old_bufend;

		dup2(fileno(stderr), (NOFILE - 1));
		old_bufend = (unsigned char *) _bufend(stderr);
		if(fileno(stderr) >= 3)
			close(fileno(stderr));
		fileno(stderr) = (NOFILE - 1);
		(unsigned char *) _bufend(stderr) = old_bufend;
#else	/* !SYSV */
		dup2(fileno(stderr), (NOFILE - 1));
		if(fileno(stderr) >= 3)
			close(fileno(stderr));
		fileno(stderr) = (NOFILE - 1);
#endif	/* !SYSV */
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

#ifdef SYSV
	if (0 > (mode = fcntl(pty, F_GETFL, 0)))
		Error();
	mode |= O_NDELAY;
	if (fcntl(pty, F_SETFL, mode))
		Error();
#else	/* SYSV */
	mode = 1;
	if (ioctl (pty, FIONBIO, (char *)&mode) == -1) SysError (ERROR_FIONBIO);
#endif	/* SYSV */
	
	pty_mask = 1 << pty;
	X_mask = 1 << Xsocket;
	Select_mask = pty_mask | X_mask;
	max_plus1 = (pty < Xsocket) ? (1 + Xsocket) : (1 + pty);

#ifdef DEBUG
	if (debug) printf ("debugging on\n");
#endif	/* DEBUG */
	XSetErrorHandler(xerror);
	XSetIOErrorHandler(xioerror);
	for( ; ; )
		if(screen->TekEmu) {
			TekRun();
		} else
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
#else	/* TIOCCONS */
" [-bg backgrnd_color] [-bw border_width] [-cr cursor_color] [-cu] \\\n",
#endif	/* TIOCCONS */
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
#endif	/* TIOCCONS */
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
	    ttydev [strlen(ttydev) - 2]  = ptydev [strlen(ptydev) - 2] =
		    PTYCHAR1 [letter++];
	    devindex = 0;

	    while (devindex < 16) {
		ttydev [strlen(ttydev) - 1] = ptydev [strlen(ptydev) - 1] =
			PTYCHAR2 [devindex++];
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
	register TScreen *screen = &term->screen;
	char *malloc();
	
	screen->graybordertile = make_gray(term->core.border_pixel,
		term->core.background_pixel,
		DefaultDepth(screen->display, DefaultScreen(screen->display)));


	{
	    unsigned long fg, bg;

	    fg = screen->mousecolor;
	    bg = (screen->mousecolor == term->core.background_pixel) ?
		screen->foreground : term->core.background_pixel;

	    screen->arrow = make_arrow (fg, bg);
	}

	XAutoRepeatOn(screen->display);


}

/*
 * The only difference in /etc/termcap between 4014 and 4015 is that 
 * the latter has support for switching character sets.  We support the
 * 4015 protocol, but ignore the character switches.  Therefore, we should
 * probably choose 4014 over 4015.
 */

static char *tekterm[] = {
	"tek4014",
	"tek4015",		/* has alternate character set switching */
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
	register TScreen *screen = &term->screen;
	int Xsocket = screen->display->fd;
	int index1, tty = -1;
	int discipline;
#ifdef SYSV
	struct termio tio;
	struct termio dummy_tio;
#ifdef TIOCLSET
	unsigned lmode;
#endif	/* TIOCLSET */
#ifdef TIOCSLTC
	struct ltchars ltc;
#endif	/* TIOCSLTC */
	struct request_info reqinfo;
	int one = 1;
	int zero = 0;
	int status;
#else	/* !SYSV */
	unsigned lmode;
	struct tchars tc;
	struct ltchars ltc;
	struct sgttyb sg;
#endif	/* !SYSV */

	char termcap [1024];
	char newtc [1024];
	char *ptr, *shname, *shname_minus;
	int i, no_dev_tty = FALSE;
#ifdef SYSV
	char *dev_tty_name = (char *) 0;
#endif	/* SYSV */
	char **envnew;		/* new environment */
	char buf[32];
	char *TermName = NULL;
	int ldisc = 0;
#ifdef sun
#ifdef TIOCSSIZE
	struct ttysize ts;
#endif	/* TIOCSSIZE */
#else	/* sun */
#ifdef TIOCSWINSZ
	struct winsize ws;
#endif	/* TIOCSWINSZ */
#endif	/* sun */
	struct passwd *pw = NULL;
#ifdef UTMP
	struct utmp utmp;
#endif	/* UTMP */
	extern int Exit();
	char *getenv();
	char *index (), *rindex (), *strindex ();

	screen->uid = getuid();
	screen->gid = getgid();

#if !defined(SYSV) || defined(JOBCONTROL)
	/* so that TIOCSWINSZ || TIOCSIZE doesn't block */
	signal(SIGTTOU,SIG_IGN);
#endif	/* !defined(SYSV) || defined(JOBCONTROL) */

	if (get_ty) {
		screen->respond = loginpty;
#ifndef SYSV
		if((tslot = ttyslot()) <= 0)
			SysError(ERROR_TSLOT);
#endif	/* SYSV */
	} else if (am_slave) {
		screen->respond = am_slave;
		ptydev[strlen(ptydev) - 2] = ttydev[strlen(ttydev) - 2] =
			passedPty[0];
		ptydev[strlen(ptydev) - 1] = ttydev[strlen(ttydev) - 1] =
			passedPty[1];

		/* use the same tty name that everyone else will use
		** (from ttyname)
		*/
		if (ptr = ttyname(ttydev)) {
			/* it may be bigger! */
			ttydev = malloc((unsigned) (strlen(ptr) + 1));
			(void) strcpy(ttydev, ptr);
		}
#ifndef SYSV
		if((tslot = ttyslot()) <= 0)
			SysError(ERROR_TSLOT2);
#endif	/* !SYSV */
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
#ifdef SYSV
				tio = d_tio;
#ifdef TIOCSLTC
				ltc = d_ltc;
#endif	/* TIOCSLTC */
#ifdef TIOCLSET
				lmode = d_lmode;
#endif	/* TIOCLSET */
#else	/* !SYSV */
				sg = d_sg;
				tc = d_tc;
				discipline = d_disipline;
				ltc = d_ltc;
				lmode = d_lmode;
#endif	/* !SYSV */
			}
		} else {
			/* get a copy of the current terminal's state */

#ifdef SYSV
			if(ioctl(tty, TCGETA, &tio) == -1)
				SysError(ERROR_TIOCGETP);
#ifdef TIOCSLTC
			if(ioctl(tty, TIOCGLTC, &ltc) == -1)
				SysError(ERROR_TIOCGLTC);
#endif	/* TIOCSLTC */
#ifdef TIOCLSET
			if(ioctl(tty, TIOCLGET, &lmode) == -1)
				SysError(ERROR_TIOCLGET);
#endif	/* TIOCLSET */
#else	/* !SYSV */
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
#endif	/* !SYSV */
			close (tty);

			/* close all std file descriptors */
			for (index1 = 0; index1 < 3; index1++)
				close (index1);
#ifndef SYSV
			if ((tty = open ("/dev/tty", O_RDWR, 0)) < 0)
				SysError (ERROR_OPDEVTTY2);

			if (ioctl (tty, TIOCNOTTY, (char *) NULL) == -1)
				SysError (ERROR_NOTTY);
			close (tty);
#endif	/* !SYSV */
		}

		get_pty (&screen->respond, &tty);

		/* use the same tty name that everyone else will use
		** (from ttyname)
		*/
		if (ptr = ttyname(tty)) {
			/* it may be bigger */
			ttydev = malloc((unsigned) (strlen(ptr) + 1));
			(void) strcpy(ttydev, ptr);
		}
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

#ifdef SYSV
		/* input: nl->nl, don't ignore cr, cr->nl */
		tio.c_iflag &= ~(INLCR|IGNCR);
		tio.c_iflag |= ICRNL;
		/* ouput: cr->cr, nl is not return, no delays, ln->cr/nl */
		tio.c_oflag &=
		 ~(OCRNL|ONLRET|NLDLY|CRDLY|TABDLY|BSDLY|VTDLY|FFDLY);
		tio.c_oflag |= ONLCR;
#ifdef BAUD_0
		/* baud rate is 0 (don't care) */
		tio.c_cflag &= ~(CBAUD);
#else	/* !BAUD_0 */
		/* baud rate is 9600 (nice default) */
		tio.c_cflag &= ~(CBAUD);
		tio.c_cflag |= B9600;
#endif	/* !BAUD_0 */
		/* enable signals, canonical processing (erase, kill, etc),
		** echo
		*/
		tio.c_lflag |= ISIG|ICANON|ECHO;
		/* reset EOL to defalult value */
		tio.c_cc[VEOL] = '@' & 0x3f;		/* '^@'	*/
		/* certain shells (ksh & csh) change EOF as well */
		tio.c_cc[VEOF] = 'D' & 0x3f;		/* '^D'	*/
		if (ioctl (tty, TCSETA, &tio) == -1)
			SysError (ERROR_TIOCSETP);
#ifdef TIOCSLTC
		if (ioctl (tty, TIOCSLTC, &ltc) == -1)
			SysError (ERROR_TIOCSETC);
#endif	/* TIOCSLTC */
#ifdef TIOCLSET
		if (ioctl (tty, TIOCLSET, (char *)&lmode) == -1)
			SysError (ERROR_TIOCLSET);
#endif	/* TIOCLSET */
#else	/* !SYSV */
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
#endif	/* TIOCCONS */
#endif	/* !SYSV */

		close (open ("/dev/null", O_RDWR, 0));

		for (index1 = 0; index1 < 3; index1++)
			dup2 (tty, index1);
#ifndef SYSV
		if((tslot = ttyslot()) <= 0)
			SysError(ERROR_TSLOT3);
#endif	/* !SYSV */
#ifdef UTMP
#ifdef SYSV
		if(pw = getpwuid(screen->uid)) {
			(void) setutent ();

			/* set up entry to search for */
			(void) strncpy(utmp.ut_id,ttydev + strlen(ttydev) - 2,
			 sizeof (utmp.ut_id));
			utmp.ut_type = DEAD_PROCESS;

			/* position to entry in utmp file */
			(void) getutid(&utmp);

			/* set up the new entry */
			utmp.ut_type = USER_PROCESS;
			utmp.ut_exit.e_exit = 2;
			(void) strncpy(utmp.ut_user, pw->pw_name,
			 sizeof(utmp.ut_user));
			(void) strncmp(utmp.ut_id, ttydev + strlen(ttydev) - 2,
			 sizeof(utmp.ut_id));

			(void) strncpy (utmp.ut_line,
			 ttydev + strlen("/dev/"), sizeof (utmp.ut_line));
			utmp.ut_pid = getpid();
			utmp.ut_time = time ((long *) 0);

			/* write out the entry */
			(void) pututline(&utmp);
			added_utmp_entry = True;

			/* close the file */
			(void) endutent();
		}
#else	/* !SYSV */
		if((pw = getpwuid(screen->uid)) &&
		 (i = open(etc_utmp, O_WRONLY)) >= 0) {
			bzero((char *)&utmp, sizeof(struct utmp));
			(void) strcpy(utmp.ut_line, ttydev + strlen("/dev/"));
			(void) strcpy(utmp.ut_name, pw->pw_name);
			(void) strcpy(utmp.ut_host, 
				      XDisplayString (screen->display));
			time(&utmp.ut_time);
			lseek(i, (long)(tslot * sizeof(struct utmp)), 0);
			write(i, (char *)&utmp, sizeof(struct utmp));
			added_utmp_entry = True;
			close(i);
		} else
			tslot = -tslot;
#endif	/* !SYSV */
#endif	/* UTMP */
	}

        /* Realize the Tek or VT widget, depending on which mode we're in.
           If VT mode, this calls VTRealize (the widget's Realize proc) */
        XtRealizeWidget (screen->TekEmu ? tekWidget->core.parent :
			 term->core.parent);

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
#endif	/* TIOCSSIZE */
#else	/* sun */
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
#endif	/* TIOCSWINSZ */
#endif	/* sun */
            
	if (!am_slave) {
#ifdef SYSV
	    (void) setpgrp();
	    (void) close(open(ttydev, O_RDWR, 0));
#endif	/* SYSV */
	    if ((screen->pid = fork ()) == -1)
		SysError (ERROR_FORK);
		
	    if (screen->pid == 0) {
		extern char **environ;
		int pgrp = getpid();
#ifdef SYSV
		char numbuf[12];
#endif	/* SYSV */

		close (Xsocket);
		close (screen->respond);
		if(fileno(stderr) >= 3)
			close (fileno(stderr));

		if (tty >= 0) close (tty);

		signal (SIGCHLD, SIG_DFL);
		signal (SIGHUP, SIG_IGN);
		/* restore various signals to their defaults */
		signal (SIGINT, SIG_DFL);
		signal (SIGQUIT, SIG_DFL);
		signal (SIGTERM, SIG_DFL);

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
#ifdef SYSV
		sprintf (numbuf, "%d", screen->max_col + 1);
		Setenv("COLUMNS=", numbuf);
		sprintf (numbuf, "%d", screen->max_row + 1);
		Setenv("LINES=", numbuf);
#else	/* !SYSV */
		Setenv ("TERMCAP=", newtc);
#endif	/* !SYSV */

		sprintf(buf, "%d", screen->TekEmu ? (int)TWindow(screen) :
		 (int)VWindow(screen));
		Setenv ("WINDOWID=", buf);
		/* put the display into the environment of the shell*/
		Setenv ("DISPLAY=", XDisplayString (screen->display));

		signal(SIGTERM, SIG_DFL);
#if !defined(SYSV) || defined(JOBCONTROL)
		ioctl(0, TIOCSPGRP, (char *)&pgrp);
		setpgrp (0, 0);
#endif	/* !defined(SYSV) || defined(JOBCONTROL) */
		close(open(ttyname(0), O_WRONLY, 0));
#if !defined(SYSV) || defined(JOBCONTROL)
		setpgrp (0, pgrp);
#endif	/* !defined(SYSV) || defined(JOBCONTROL) */

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
#ifdef SYSV
			int fd;

			(void) setutent ();
			/* set up entry to search for */
			(void) strncpy(utmp.ut_id,ttydev + strlen(ttydev) - 2,
			 sizeof (utmp.ut_id));
			utmp.ut_type = DEAD_PROCESS;

			/* position to entry in utmp file */
			(void) getutid(&utmp);

			/* set up the new entry */
			utmp.ut_type = LOGIN_PROCESS;
			(void) strncpy(utmp.ut_user, "GETTY",
			 sizeof(utmp.ut_user));
			(void) strncmp(utmp.ut_id, ttydev + strlen(ttydev) - 2,
			 sizeof(utmp.ut_id));
			(void) strncpy (utmp.ut_line,
			 ttydev + strlen("/dev/"), sizeof (utmp.ut_line));
			utmp.ut_pid = getpid();
			utmp.ut_time = time ((long *) 0);

			/* write out the entry */
			(void) pututline(&utmp);
			added_utmp_entry = True;

			/* close the file */
			(void) endutent();

			/* set wtmp entry if wtmp file exists */
			if (fd = open("/etc/wtmp", O_WRONLY | O_APPEND)) {
				(void) write(fd, &utmp, sizeof(utmp));
				(void) close(fd);
			}

			ioctl (0, TIOCTTY, &zero);
			execlp ("/etc/getty", "getty", get_ty, "Xwindow", 0);

#else	/* !SYSV */
			ioctl (0, TIOCNOTTY, (char *) NULL);
			execlp ("/etc/getty", "+", "Xwindow", get_ty, 0);
#endif	/* !SYSV */
		}
		signal(SIGHUP, SIG_DFL);

#ifdef UTMP
		if(((ptr = getenv("SHELL")) == NULL || *ptr == 0) &&
		 ((pw == NULL && (pw = getpwuid(screen->uid)) == NULL) ||
		 *(ptr = pw->pw_shell) == 0))
#else	/* UTMP */
		if(((ptr = getenv("SHELL")) == NULL || *ptr == 0) &&
		 ((pw = getpwuid(screen->uid)) == NULL ||
		 *(ptr = pw->pw_shell) == 0))
#endif	/* UTMP */
			ptr = "/bin/sh";
		if(shname = rindex(ptr, '/'))
			shname++;
		else
			shname = ptr;
		shname_minus = malloc(strlen(shname) + 2);
		(void) strcpy(shname_minus, "-");
		(void) strcat(shname_minus, shname);
#ifndef SYSV
		ldisc = XStrCmp("csh", shname + strlen(shname) - 3) == 0 ?
		 NTTYDISC : 0;
		ioctl(0, TIOCSETD, (char *)&ldisc);
#endif	/* !SYSV */
		execlp (ptr, login_shell ? shname_minus : shname, 0);
		fprintf (stderr, "%s: Could not exec %s!\n", xterm_name, ptr);
		sleep(5);
		exit(ERROR_EXEC);
	    }
	}

	if(tty >= 0) close (tty);
#ifdef SYSV
	/* the parent should not be associated with tty anymore */
	for (index1 = 0; index1 < 3; index1++)
		close(index1);
#endif	/* SYSV */
	signal(SIGHUP,SIG_IGN);

	if (!no_dev_tty) {
		if ((tty = open ("/dev/tty", O_RDWR, 0)) < 0)
			SysError(ERROR_OPDEVTTY3);
		for (index1 = 0; index1 < 3; index1++)
			dup2 (tty, index1);
		if (tty > 2) close (tty);
	}

#if !defined(SYSV) || defined(JOBCONTROL)

	signal(SIGINT, Exit); 
	signal(SIGQUIT, Exit);
	signal(SIGTERM, Exit);
#else	/* !defined(SYSV) || defined(JOBCONTROL) */
	signal(SIGINT, SIG_IGN); 
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
#endif	/* !defined(SYSV) || defined(JOBCONTROL) */
}

Exit(n)
int n;
{
	register TScreen *screen = &term->screen;
        int pty = term->screen.respond;  /* file descriptor of pty */
#ifdef UTMP
#ifdef SYSV
	struct utmp utmp;
	struct utmp *utptr;

	/* cleanup the utmp entry we forged earlier */
	/* unlike BSD, go ahead and cream any entries we didn't make */
	utmp.ut_type = USER_PROCESS;
	(void) strncpy(utmp.ut_id, ttydev + strlen(ttydev) - 2,
	 sizeof(utmp.ut_id));
	(void) setutent();
	utptr = getutid(&utmp);
	if (utptr) {
		utptr->ut_type = DEAD_PROCESS;
		utptr->ut_time = time((long *) 0);
		(void) pututline(utptr);
	}
	(void) endutent();
#else	/* !SYSV */
	register int i;
	struct utmp utmp;

	if (added_utmp_entry &&
	    (!am_slave && tslot > 0 && (i = open(etc_utmp, O_WRONLY)) >= 0)) {
		bzero((char *)&utmp, sizeof(struct utmp));
		lseek(i, (long)(tslot * sizeof(struct utmp)), 0);
		write(i, (char *)&utmp, sizeof(struct utmp));
		close(i);
	}
#endif	/* !SYSV */
#endif	/* UTMP */
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

#ifndef SYSV
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
#endif	/* !SYSV */
}

static reapchild ()
{
#if defined(SYSV) && !defined(JOBCONTROL)
	int status, pid;

	pid = wait(&status);
	if (pid == -1)
		return;
#else	/* defined(SYSV) && !defined(JOBCONTROL) */
	union wait status;
	register int pid;
	
#ifdef DEBUG
	if (debug) fputs ("Exiting\n", stderr);
#endif	/* DEBUG */
	pid  = wait3 (&status, WNOHANG, (struct rusage *)NULL);
	if (!pid) return;
#endif	/* defined(SYSV) && !defined(JOBCONTROL) */
	if (pid != term->screen.pid) return;
	
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
#ifdef TIOCNOTTY
	if ((f = open("/dev/tty", 2)) >= 0) {
		ioctl(f, TIOCNOTTY, (char *)NULL);
		close(f);
	}
#endif	/* TIOCNOTTY */
}

checklogin()
{
	register int ts, i;
	register struct passwd *pw;
#ifdef SYSV
	char *name;
#else	/* !SYSV */
	struct utmp utmp;
#endif	/* !SYSV */

#ifdef SYSV
	name = cuserid((char *) 0);
	if (name)
		pw = getpwnam(name);
	else
		pw = getpwuid(getuid());
	if (pw == NULL)
		return(FALSE);
#else	/* !SYSV */
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
#endif	/* !SYSV */
	setgid(pw->pw_gid);
	setuid(pw->pw_uid);
	L_flag = 0;
	return(TRUE);
}
