if HAS_CURSES
TEXTUI=gui-text
endif

if BUILD_GNOMEUI
GNOMEUI=gui-gnome
endif

if BUILD_IRSSIBOT
BOTUI=gui-none
endif

noinst_HEADERS = \
	common.h \
	common-setup.h \
	irssi-plugin.h \
	irssi-plugin-gui.h

SUBDIRS = irc-base irc-extra ui-common lib-config lib-nongui lib-popt settings $(GNOMEUI) $(TEXTUI) $(BOTUI)

EXTRA_DIST = signal.doc
