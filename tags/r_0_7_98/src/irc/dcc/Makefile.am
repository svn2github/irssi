moduledir = $(libdir)/irssi/modules
module_LTLIBRARIES = libirc_dcc.la

libirc_dcc_la_LDFLAGS = -avoid-version

INCLUDES = $(GLIB_CFLAGS) \
	-I$(top_srcdir)/src -I$(top_srcdir)/src/core/ -I$(top_srcdir)/src/irc/core/

libirc_dcc_la_SOURCES = \
	dcc.c \
	dcc-chat.c \
	dcc-get.c \
	dcc-send.c \
	dcc-resume.c \
	dcc-autoget.c

noinst_HEADERS = \
	dcc-rec.h \
	dcc-file-rec.h \
	dcc.h \
	dcc-file.h \
	dcc-chat.h \
	dcc-get.h \
	dcc-send.h \
	module.h
