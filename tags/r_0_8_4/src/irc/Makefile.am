if BUILD_IRSSIBOT
BOT=bot
endif

if BUILD_IRSSIPROXY
PROXY=proxy
endif

SUBDIRS = core $(BOT) dcc flood notifylist $(PROXY)

noinst_LIBRARIES = libirc.a

libirc_a_SOURCES = irc.c

DISTFILES = $(DIST_COMMON) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

distclean-generic:
	rm -f irc.c
