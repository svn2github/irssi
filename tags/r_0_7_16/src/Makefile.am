if HAS_CURSES
TEXTUI=gui-text
endif

if BUILD_GNOMEUI
GNOMEUI=gui-gnome
endif

noinst_HEADERS = \
	common.h \
	common-setup.h \
	irssi-plugin.h \
	irssi-plugin-gui.h

SUBDIRS = irc-base irc-extra ui-common lib-config lib-nongui settings $(GNOMEUI) $(TEXTUI) gui-none

EXTRA_DIST = signal.doc
