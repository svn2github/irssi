noinst_LIBRARIES = libcore.a

INCLUDES = \
	$(GLIB_CFLAGS) \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DMODULEDIR=\""$(libdir)/irssi/modules"\" \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/core

if BUILD_MEMDEBUG
memdebug_src=memdebug.c
else
memdebug_src=
endif

libcore_a_SOURCES = \
	args.c \
	channels.c \
	channels-setup.c \
	commands.c \
	chat-commands.c \
	chat-protocols.c \
	chatnets.c \
	core.c \
        ignore.c \
        levels.c \
	line-split.c \
	log.c \
	masks.c \
        $(memdebug_src) \
	misc.c \
	modules.c \
	net-disconnect.c \
	net-nonblock.c \
	net-sendbuffer.c \
	network.c \
	nicklist.c \
	pidwait.c \
	queries.c \
	rawlog.c \
	servers.c \
	servers-reconnect.c \
	servers-redirect.c \
	servers-setup.c \
	settings.c \
	signals.c \
	special-vars.c

structure_headers = \
	chatnet-rec.h \
	channel-rec.h \
	query-rec.h \
	server-rec.h \
	server-setup-rec.h \
	server-connect-rec.h \
	window-item-rec.h

noinst_HEADERS = \
	args.h \
	channels.h \
	channels-setup.h \
	commands.h \
	chat-protocols.h \
	chatnets.h \
	core.h \
        ignore.h \
        levels.h \
	line-split.h \
	log.h \
	masks.h \
        memdebug.h \
	misc.h \
	module.h \
	modules.h \
	net-disconnect.h \
	net-internal.h \
	net-nonblock.h \
	net-sendbuffer.h \
	network.h \
	nicklist.h \
	pidwait.h \
	queries.h \
	rawlog.h \
	servers.h \
	servers-reconnect.h \
	servers-redirect.h \
	servers-setup.h \
	settings.h \
	signals.h \
	special-vars.h \
	window-item-def.h \
	$(structure_headers)

EXTRA_DIST = \
	memdebug.c
