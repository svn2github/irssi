moduledir = $(libdir)/irssi/modules
module_LTLIBRARIES = libirc_flood.la

libirc_flood_la_LDFLAGS = -avoid-version

INCLUDES = $(GLIB_CFLAGS) \
	-I$(top_srcdir)/src -I$(top_srcdir)/src/core/ -I$(top_srcdir)/src/irc/core/

libirc_flood_la_SOURCES = \
	autoignore.c \
	flood.c

noinst_HEADERS = \
	autoignore.h \
	flood.h \
	module.h
