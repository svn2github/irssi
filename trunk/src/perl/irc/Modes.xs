#include "module.h"

MODULE = Irssi::Irc::Modes	PACKAGE = Irssi::Irc
PROTOTYPES: ENABLE

void
modes_join(old, mode, channel)
	char *old
	char *mode
        int channel
PREINIT:
	char *ret;
PPCODE:
	ret = modes_join(old, mode, channel);
	xPUSHs(sv_2mortal(new_pv(ret)));
	g_free(ret);

#*******************************
MODULE = Irssi::Irc::Modes	PACKAGE = Irssi::Irc::Channel  PREFIX = channel_
#*******************************

void
ban_get_mask(channel, nick, ban_type)
	Irssi::Irc::Channel channel
	char *nick
	int ban_type
PREINIT:
	char *ret;
PPCODE:
	ret = ban_get_mask(channel, nick, ban_type);
	xPUSHs(sv_2mortal(new_pv(ret)));
	g_free(ret);

Irssi::Irc::Ban
banlist_add(channel, ban, nick, time)
	Irssi::Irc::Channel channel
	char *ban
	char *nick
	time_t time

void
banlist_remove(channel, ban)
	Irssi::Irc::Channel channel
	char *ban
