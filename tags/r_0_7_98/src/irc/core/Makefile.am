noinst_LIBRARIES = libirc_core.a

INCLUDES = \
	$(GLIB_CFLAGS) \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-I$(top_srcdir)/src -I$(top_srcdir)/src/core

libirc_core_a_SOURCES = \
	bans.c \
        ctcp.c \
        channels-query.c \
        channel-events.c \
        channel-rejoin.c \
        irc.c \
        irc-core.c \
        irc-channels.c \
        irc-channels-setup.c \
	irc-chatnets.c \
        irc-commands.c \
        irc-expandos.c \
        irc-log.c \
        irc-masks.c \
        irc-nicklist.c \
        irc-queries.c \
        irc-rawlog.c \
        irc-servers.c \
        irc-servers-reconnect.c \
        irc-servers-setup.c \
        lag.c \
        massjoin.c \
        modes.c \
        mode-lists.c \
        netsplit.c \
        server-idle.c

noinst_HEADERS = \
	bans.h \
        ctcp.h \
        channel-rejoin.h \
        irc.h \
        irc-channels.h \
	irc-chatnets.h \
        irc-masks.h \
        irc-nicklist.h \
        irc-queries.h \
        irc-servers.h \
        irc-servers-setup.h \
        modes.h \
        mode-lists.h \
	module.h \
        netsplit.h \
        server-idle.h
