bin_PROGRAMS = irssi

INCLUDES = \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/core/ \
	-I$(top_srcdir)/src/irc/core/ \
	-I$(top_srcdir)/src/fe-common/core/ \
	-I$(top_srcdir)/src/fe-common/irc/ \
	$(CURSES_INCLUDEDIR)

irssi_DEPENDENCIES = @COMMON_LIBS@

irssi_LDADD = \
	@COMMON_LIBS@ \
	$(PROG_LIBS) \
	$(PERL_LDFLAGS) \
        $(CURSES_LIBS)

irssi_SOURCES = \
        gui-entry.c \
        mainwindows.c \
        gui-printtext.c \
        gui-readline.c \
	gui-special-vars.c \
        statusbar.c \
        statusbar-items.c \
        gui-textwidget.c \
        gui-windows.c \
        irssi.c \
	module-formats.c \
        screen.c

noinst_HEADERS = \
        gui-entry.h \
        mainwindows.h \
        gui-printtext.h \
        gui-readline.h \
	gui-special-vars.h \
        statusbar.h \
        gui-textwidget.h \
        gui-windows.h \
        screen.h \
	module.h \
	module-formats.h
