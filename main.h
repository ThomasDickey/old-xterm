/*
 *	@Source: /u1/X/xterm/RCS/main.h,v @
 *	@Header: main.h,v 10.100 86/12/01 14:39:34 jg Rel @
 */

/* @(#)main.h\tX10/6.6\t11/10/86 */
#define DEF_ACTIVEICON		0
#define DEF_ALLOWICONINPUT	(DEF_ACTIVEICON+1)
#define	DEF_AUTORAISE		(DEF_ALLOWICONINPUT+1)
#define	DEF_BACKGROUND		(DEF_AUTORAISE+1)
#define	DEF_BODYFONT		(DEF_BACKGROUND+1)
#define	DEF_BOLDFONT		(DEF_BODYFONT+1)
#define	DEF_BORDER		(DEF_BOLDFONT+1)
#define	DEF_BORDERWIDTH		(DEF_BORDER+1)
#define	DEF_C132		(DEF_BORDERWIDTH+1)
#define	DEF_CURSES		(DEF_C132+1)
#define	DEF_CURSOR		(DEF_CURSES+1)
#define	DEF_DEICONWARP		(DEF_CURSOR+1)
#define	DEF_FOREGROUND		(DEF_DEICONWARP+1)
#define	DEF_ICONBITMAP		(DEF_FOREGROUND+1)
#define DEF_ICONFONT		(DEF_ICONBITMAP+1)
#define	DEF_ICONSTARTUP		(DEF_ICONFONT+1)
#define	DEF_INTERNALBORDER	(DEF_ICONSTARTUP+1)
#define	DEF_JUMPSCROLL		(DEF_INTERNALBORDER+1)

#ifdef KEYBD
#define	DEF_KEYBOARD		(DEF_JUMPSCROLL+1)
#define	DEF_LOGFILE		(DEF_KEYBOARD+1)
#else KEYBD
#define	DEF_LOGFILE		(DEF_JUMPSCROLL+1)
#endif KEYBD

#define	DEF_LOGGING		(DEF_LOGFILE+1)
#define	DEF_LOGINHIBIT		(DEF_LOGGING+1)
#define	DEF_LOGINSHELL		(DEF_LOGINHIBIT+1)
#define	DEF_MARGINBELL		(DEF_LOGINSHELL+1)
#define	DEF_MOUSE		(DEF_MARGINBELL+1)
#define	DEF_NMARGINBELL		(DEF_MOUSE+1)
#define	DEF_PAGEOVERLAP		(DEF_NMARGINBELL+1)
#define	DEF_PAGESCROLL		(DEF_PAGEOVERLAP+1)
#define	DEF_REVERSEVIDEO	(DEF_PAGESCROLL+1)
#define	DEF_REVERSEWRAP		(DEF_REVERSEVIDEO+1)
#define	DEF_SAVELINES		(DEF_REVERSEWRAP+1)
#define	DEF_SCROLLBAR		(DEF_SAVELINES+1)
#define	DEF_SCROLLINPUT		(DEF_SCROLLBAR+1)
#define	DEF_SCROLLKEY		(DEF_SCROLLINPUT+1)
#define	DEF_SIGNALINHIBIT	(DEF_SCROLLKEY+1)
#define	DEF_STATUSLINE		(DEF_SIGNALINHIBIT+1)
#define	DEF_STATUSNORMAL	(DEF_STATUSLINE+1)
#define	DEF_TEKICONBITMAP	(DEF_STATUSNORMAL+1)
#define	DEF_TEKINHIBIT		(DEF_TEKICONBITMAP+1)
#define	DEF_TEXTUNDERICON	(DEF_TEKINHIBIT+1)
#define	DEF_TITLEBAR		(DEF_TEXTUNDERICON+1)
#define	DEF_TITLEFONT		(DEF_TITLEBAR+1)
#define	DEF_VISUALBELL		(DEF_TITLEFONT+1)

#define	ARG_132			0

#ifdef TIOCCONS
#define	ARG__C			(ARG_132+1)
#endif TIOCCONS

#ifdef ARG__C
#define	ARG__L			(ARG__C+1)
#else ARG__C
#define	ARG__L			(ARG_132+1)
#endif ARG__C

#define	ARG__S			(ARG__L+1)
#define	ARG_AR			(ARG__S+1)
#define	ARG_B			(ARG_AR+1)
#define	ARG_BD			(ARG_B+1)
#define	ARG_BG			(ARG_BD+1)
#define ARG_BI			(ARG_BG+1)
#define	ARG_BW			(ARG_BI+1)
#define	ARG_CR			(ARG_BW+1)
#define	ARG_CU			(ARG_CR+1)

#ifdef DEBUG
#define	ARG_D			(ARG_CU+1)
#define	ARG_DW			(ARG_D+1)
#else DEBUG
#define	ARG_DW			(ARG_CU+1)
#endif DEBUG

#define	ARG_E			(ARG_DW+1)
#define	ARG_FB			(ARG_E+1)
#define	ARG_FG			(ARG_FB+1)
#define ARG_FI			(ARG_FG+1)
#define	ARG_FN			(ARG_FI+1)
#define	ARG_FT			(ARG_FN+1)
#define	ARG_I			(ARG_FT+1)
#define	ARG_IB			(ARG_I+1)
#define	ARG_IT			(ARG_IB+1)
#define	ARG_J			(ARG_IT+1)

#ifdef KEYBD
#define	ARG_K			(ARG_J+1)
#define	ARG_L			(ARG_K+1)
#else KEYBD
#define	ARG_L			(ARG_J+1)
#endif KEYBD

#define	ARG_LF			(ARG_L+1)
#define	ARG_LS			(ARG_LF+1)
#define	ARG_MB			(ARG_LS+1)
#define	ARG_MS			(ARG_MB+1)
#define	ARG_N			(ARG_MS+1)
#define	ARG_NB			(ARG_N+1)
#define	ARG_PO			(ARG_NB+1)
#define	ARG_PS			(ARG_PO+1)
#define	ARG_RV			(ARG_PS+1)
#define	ARG_RW			(ARG_RV+1)
#define	ARG_S			(ARG_RW+1)
#define	ARG_SB			(ARG_S+1)
#define	ARG_SI			(ARG_SB+1)
#define	ARG_SK			(ARG_SI+1)
#define	ARG_SL			(ARG_SK+1)
#define	ARG_SN			(ARG_SL+1)
#define	ARG_ST			(ARG_SN+1)
#define	ARG_T			(ARG_ST+1)
#define	ARG_TB			(ARG_T+1)
#define	ARG_TI			(ARG_TB+1)
#define	ARG_VB			(ARG_TI+1)

#define	DEFBOLDFONT		"vtbold"
#define	DEFBORDER		1
#define	DEFBORDERWIDTH		2
#define	DEFFONT			"vtsingle"
#define DEFICONFONT		"nil2"
#define	DEFTITLEFONT		"vtsingle"
#define DEFACTIVEICON		TRUE
#define DEFSCROLLINPUT		TRUE
#define DEFTITLEBAR		TRUE
