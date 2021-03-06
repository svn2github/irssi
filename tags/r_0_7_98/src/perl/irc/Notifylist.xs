MODULE = Irssi::Irc  PACKAGE = Irssi::Irc

void
notifies()
PREINIT:
	GSList *tmp;
	HV *stash;
PPCODE:
	stash = gv_stashpv("Irssi::Irc::Notifylist", 0);
	for (tmp = notifies; tmp != NULL; tmp = tmp->next) {
		push_bless(tmp->data, stash);
	}

Irssi::Irc::Notifylist
notifylist_add(mask, ircnets, away_check, idle_check_time)
	char *mask
	char *ircnets
	int away_check
	int idle_check_time

void
notifylist_remove(mask)
	char *mask

Irssi::Irc::Server
notifylist_ison(nick, serverlist)
	char *nick
	char *serverlist

Irssi::Irc::Notifylist
notifylist_find(mask, ircnet)
	char *mask
	char *ircnet

#*******************************
MODULE = Irssi::Irc  PACKAGE = Irssi::Irc::Server
#*******************************

int
notifylist_ison_server(server, nick)
	Irssi::Irc::Server server
	char *nick

#*******************************
MODULE = Irssi::Irc	PACKAGE = Irssi::Irc::Notifylist  PREFIX = notifylist_
#*******************************

int
notifylist_ircnets_match(rec, ircnet)
	Irssi::Irc::Notifylist rec
	char *ircnet
