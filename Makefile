# Makefile generated by imake - do not edit!
# $XConsortium: imake.c,v 1.51 89/12/12 12:37:30 jim Exp $

###########################################################################
# Makefile generated from "Imake.tmpl" and <Imakefile>
# $XConsortium: Imake.tmpl,v 1.77 89/12/18 17:01:37 jim Exp $
#
# Platform-specific parameters may be set in the appropriate .cf
# configuration files.  Site-wide parameters may be set in the file
# site.def.  Full rebuilds are recommended if any parameters are changed.
#
# If your C preprocessor doesn't define any unique symbols, you'll need
# to set BOOTSTRAPCFLAGS when rebuilding imake (usually when doing
# "make Makefile", "make Makefiles", or "make World").
#
# If you absolutely can't get imake to work, you'll need to set the
# variables at the top of each Makefile as well as the dependencies at the
# bottom (makedepend will do this automatically).
#

###########################################################################
# platform-specific configuration parameters - edit sony.cf to change

# platform:  $XConsortium: sony.cf,v 1.4 89/12/17 17:54:46 rws Exp $
# operating system:             NEWS-OS 3.2

###########################################################################
# site-specific configuration parameters - edit site.def to change

# site:  $XConsortium: site.def,v 1.21 89/12/06 11:46:50 jim Exp $

            SHELL = 	/bin/sh

              TOP = ../../.
      CURRENT_DIR = ./clients/xterm

               AR = ar cq
  BOOTSTRAPCFLAGS =
               CC = cc

         COMPRESS = compress
              CPP = /lib/cpp $(STD_CPP_DEFINES)
    PREPROCESSCMD = cc -E $(STD_CPP_DEFINES)
          INSTALL = install
               LD = ld
             LINT = lint
      LINTLIBFLAG = -C
         LINTOPTS = -axz
               LN = ln -s
             MAKE = make
               MV = mv
               CP = cp
           RANLIB = ranlib
  RANLIBINSTFLAGS =
               RM = rm -f
     STD_INCLUDES =
  STD_CPP_DEFINES =
      STD_DEFINES =
 EXTRA_LOAD_FLAGS =
  EXTRA_LIBRARIES =
             TAGS = ctags

   SIGNAL_DEFINES = -DSIGNALRETURNSINT

    PROTO_DEFINES =

     INSTPGMFLAGS =

     INSTBINFLAGS = -m 0755
     INSTUIDFLAGS = -m 4755
     INSTLIBFLAGS = -m 0664
     INSTINCFLAGS = -m 0444
     INSTMANFLAGS = -m 0444
     INSTDATFLAGS = -m 0444
    INSTKMEMFLAGS = -m 4755

          DESTDIR =

     TOP_INCLUDES = -I$(TOP)

      CDEBUGFLAGS = -O
        CCOPTIONS =
      COMPATFLAGS =

      ALLINCLUDES = $(STD_INCLUDES) $(TOP_INCLUDES) $(INCLUDES) $(EXTRA_INCLUDES)
       ALLDEFINES = $(ALLINCLUDES) $(STD_DEFINES) $(PROTO_DEFINES) $(DEFINES) $(COMPATFLAGS)
           CFLAGS = $(CDEBUGFLAGS) $(CCOPTIONS) $(ALLDEFINES)
        LINTFLAGS = $(LINTOPTS) -DLINT $(ALLDEFINES)
           LDLIBS = $(SYS_LIBRARIES) $(EXTRA_LIBRARIES)
        LDOPTIONS = $(CDEBUGFLAGS) $(CCOPTIONS)
   LDCOMBINEFLAGS = -X -r

        MACROFILE = sony.cf
           RM_CMD = $(RM) *.CKP *.ln *.BAK *.bak *.o core errs ,* *~ *.a .emacs_* tags TAGS make.log MakeOut

    IMAKE_DEFINES =

         IRULESRC = $(CONFIGSRC)
        IMAKE_CMD = $(NEWTOP)$(IMAKE) -I$(NEWTOP)$(IRULESRC) $(IMAKE_DEFINES)

     ICONFIGFILES = $(IRULESRC)/Imake.tmpl $(IRULESRC)/Imake.rules \
			$(IRULESRC)/Project.tmpl $(IRULESRC)/site.def \
			$(IRULESRC)/$(MACROFILE) $(EXTRA_ICONFIGFILES)

###########################################################################
# X Window System Build Parameters
# $XConsortium: Project.tmpl,v 1.63 89/12/18 16:46:44 jim Exp $

###########################################################################
# X Window System make variables; this need to be coordinated with rules
# $XConsortium: Project.tmpl,v 1.63 89/12/18 16:46:44 jim Exp $

          PATHSEP = /
        USRLIBDIR = $(DESTDIR)/usr/lib
           BINDIR = $(DESTDIR)/usr/bin/X11
          INCROOT = $(DESTDIR)/usr/include
     BUILDINCROOT = $(TOP)
      BUILDINCDIR = $(BUILDINCROOT)/X11
      BUILDINCTOP = ..
           INCDIR = $(INCROOT)/X11
           ADMDIR = $(DESTDIR)/usr/adm
           LIBDIR = $(USRLIBDIR)/X11
        CONFIGDIR = $(LIBDIR)/config
       LINTLIBDIR = $(USRLIBDIR)/lint

          FONTDIR = $(LIBDIR)/fonts
         XINITDIR = $(LIBDIR)/xinit
           XDMDIR = $(LIBDIR)/xdm
           AWMDIR = $(LIBDIR)/awm
           TWMDIR = $(LIBDIR)/twm
           GWMDIR = $(LIBDIR)/gwm
          MANPATH = $(DESTDIR)/usr/man
    MANSOURCEPATH = $(MANPATH)/man
           MANDIR = $(MANSOURCEPATH)n
        LIBMANDIR = $(MANSOURCEPATH)3
      XAPPLOADDIR = $(LIBDIR)/app-defaults

       FONTCFLAGS = -t

     INSTAPPFLAGS = $(INSTDATFLAGS)

            IMAKE = $(IMAKESRC)/imake
           DEPEND = $(DEPENDSRC)/makedepend
              RGB = $(RGBSRC)/rgb
            FONTC = $(BDFTOSNFSRC)/bdftosnf
        MKFONTDIR = $(MKFONTDIRSRC)/mkfontdir
        MKDIRHIER = 	/bin/sh $(SCRIPTSRC)/mkdirhier.sh

        CONFIGSRC = $(TOP)/config
        CLIENTSRC = $(TOP)/clients
          DEMOSRC = $(TOP)/demos
           LIBSRC = $(TOP)/lib
          FONTSRC = $(TOP)/fonts
       INCLUDESRC = $(TOP)/X11
        SERVERSRC = $(TOP)/server
          UTILSRC = $(TOP)/util
        SCRIPTSRC = $(UTILSRC)/scripts
       EXAMPLESRC = $(TOP)/examples
       CONTRIBSRC = $(TOP)/../contrib
           DOCSRC = $(TOP)/doc
           RGBSRC = $(TOP)/rgb
        DEPENDSRC = $(UTILSRC)/makedepend
         IMAKESRC = $(CONFIGSRC)
         XAUTHSRC = $(LIBSRC)/Xau
          XLIBSRC = $(LIBSRC)/X
           XMUSRC = $(LIBSRC)/Xmu
       TOOLKITSRC = $(LIBSRC)/Xt
       AWIDGETSRC = $(LIBSRC)/Xaw
       OLDXLIBSRC = $(LIBSRC)/oldX
      XDMCPLIBSRC = $(LIBSRC)/Xdmcp
      BDFTOSNFSRC = $(FONTSRC)/bdftosnf
     MKFONTDIRSRC = $(FONTSRC)/mkfontdir
     EXTENSIONSRC = $(TOP)/extensions

  DEPEXTENSIONLIB =  $(EXTENSIONSRC)/lib/libXext.a
     EXTENSIONLIB = 			   $(DEPEXTENSIONLIB)

          DEPXLIB = $(DEPEXTENSIONLIB)  $(XLIBSRC)/libX11.a
             XLIB = $(EXTENSIONLIB) 			  $(XLIBSRC)/libX11.a

      DEPXAUTHLIB =  $(XAUTHSRC)/libXau.a
         XAUTHLIB = 			  $(DEPXAUTHLIB)

        DEPXMULIB =  $(XMUSRC)/libXmu.a
           XMULIB = 			  $(DEPXMULIB)

       DEPOLDXLIB =  $(OLDXLIBSRC)/liboldX.a
          OLDXLIB = 			  $(DEPOLDXLIB)

      DEPXTOOLLIB =  $(TOOLKITSRC)/libXt.a
         XTOOLLIB = 			  $(DEPXTOOLLIB)

        DEPXAWLIB =  $(AWIDGETSRC)/libXaw.a
           XAWLIB = 			  $(DEPXAWLIB)

 LINTEXTENSIONLIB =  $(EXTENSIONSRC)/lib/llib-lXext.ln
         LINTXLIB =  $(XLIBSRC)/llib-lX11.ln
          LINTXMU =  $(XMUSRC)/llib-lXmu.ln
        LINTXTOOL =  $(TOOLKITSRC)/llib-lXt.ln
          LINTXAW =  $(AWIDGETSRC)/llib-lXaw.ln

          DEPLIBS = $(LOCAL_LIBRARIES)

         DEPLIBS1 = $(DEPLIBS)
         DEPLIBS2 = $(DEPLIBS)
         DEPLIBS3 = $(DEPLIBS)

###########################################################################
# Imake rules for building libraries, programs, scripts, and data files
# rules:  $XConsortium: Imake.rules,v 1.67 89/12/18 17:14:15 jim Exp $

###########################################################################
# start of Imakefile

###########################################################################
#                                                                         #
#                         Attention xterm porters                         #
#                                                                         #
#                                                                         #
# Xterm assumes that bcopy can handle overlapping arguments.  If your     #
# bcopy (or memcpy) can't, write a routine called bcopy and link it in    #
# or add -Dbcopy=mybcopy to the DEFINES list below.                       #
#                                                                         #
###########################################################################

   TTYGROUPDEF = -DUSE_TTY_GROUP

   MAIN_DEFINES = -DUTMP $(TTYGROUPDEF) $(PUCCPTYDDEF)

          SRCS1 = button.c charproc.c cursor.c data.c input.c \
		  main.c menu.c misc.c screen.c scrollbar.c tabs.c \
		  TekPrsTbl.c Tekproc.c util.c VTPrsTbl.c
          OBJS1 = main.o input.o charproc.o cursor.o util.o tabs.o \
		  screen.o scrollbar.o button.o Tekproc.o misc.o \
		  VTPrsTbl.o TekPrsTbl.o data.o menu.o
          SRCS2 = resize.c
          OBJS2 = resize.o
           SRCS = $(SRCS1) $(SRCS2)
           OBJS = $(OBJS1) $(OBJS2)
       PROGRAMS = resize xterm
       DEPLIBS1 = $(DEPXAWLIB) $(DEPXMULIB) $(DEPXTOOLLIB) $(DEPXLIB)
       DEPLIBS2 =
       PROGRAMS = xterm resize

     TERMCAPLIB = -ltermcap

all:: $(PROGRAMS)

main.o:
	$(RM) $@
	$(CC) -c $(CFLAGS) $(MAIN_DEFINES) $*.c

xterm: $(OBJS1) $(DEPLIBS1)
	 $(RM) $@
	$(CC) -o $@ $(OBJS1) $(LDOPTIONS) $(XAWLIB) $(XMULIB) $(XTOOLLIB) $(XLIB) $(LDLIBS) $(TERMCAPLIB) $(PTYLIB) $(EXTRA_LOAD_FLAGS)

clean::
	$(RM) xterm

install:: xterm
	$(INSTALL) -c $(INSTPGMFLAGS)  $(INSTUIDFLAGS) xterm  $(BINDIR)

resize: $(OBJS2) $(DEPLIBS2)
	 $(RM) $@
	$(CC) -o $@ $(OBJS2) $(LDOPTIONS)   $(LDLIBS) $(TERMCAPLIB) $(EXTRA_LOAD_FLAGS)

clean::
	$(RM) resize

install:: resize
	$(INSTALL) -c $(INSTPGMFLAGS)   resize  $(BINDIR)

install:: XTerm.ad
	$(INSTALL) -c $(INSTAPPFLAGS) XTerm.ad $(XAPPLOADDIR)/XTerm

install.man:: xterm.man
	$(INSTALL) -c $(INSTMANFLAGS) xterm.man $(MANDIR)/xterm.n

install.man:: resize.man
	$(INSTALL) -c $(INSTMANFLAGS) resize.man $(MANDIR)/resize.n

depend:: $(DEPEND)

$(DEPEND):
	@echo "checking $@ over in $(DEPENDSRC) first..."; \
	cd $(DEPENDSRC); $(MAKE); \
	echo "okay, continuing in $(CURRENT_DIR)"

depend::
	$(DEPEND) -s "# DO NOT DELETE" -- $(ALLDEFINES) -- $(SRCS)

###########################################################################
# common rules for all Makefiles - do not edit

emptyrule::

clean::
	$(RM_CMD) \#*

Makefile:: $(IMAKE)

$(IMAKE):
	@(cd $(IMAKESRC); if [ -f Makefile ]; then \
	echo "checking $@ in $(IMAKESRC) first..."; $(MAKE) all; else \
	echo "bootstrapping $@ from Makefile.ini in $(IMAKESRC) first..."; \
	$(MAKE) -f Makefile.ini BOOTSTRAPCFLAGS=$(BOOTSTRAPCFLAGS); fi; \
	echo "okay, continuing in $(CURRENT_DIR)")

Makefile::
	-@if [ -f Makefile ]; then \
		echo "	$(RM) Makefile.bak; $(MV) Makefile Makefile.bak"; \
		$(RM) Makefile.bak; $(MV) Makefile Makefile.bak; \
	else exit 0; fi
	$(IMAKE_CMD) -DTOPDIR=$(TOP) -DCURDIR=$(CURRENT_DIR)

tags::
	$(TAGS) -w *.[ch]
	$(TAGS) -xw *.[ch] > TAGS

###########################################################################
# empty rules for directories that do not have SUBDIRS - do not edit

install::
	@echo "install in $(CURRENT_DIR) done"

install.man::
	@echo "install.man in $(CURRENT_DIR) done"

Makefiles::

includes::

###########################################################################
# dependencies generated by makedepend

