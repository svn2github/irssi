MODULE = Irssi  PACKAGE = Irssi

void
notifies()
PREINIT:
	GSList *tmp;
	HV *stash;
PPCODE:
	stash = gv_stashpv("Irssi::Notifylist", 0);
	for (tmp = notifies; tmp != NULL; tmp = tmp->next) {
		XPUSHs(sv_2mortal(sv_bless(newRV_noinc(newSViv(GPOINTER_TO_INT(tmp->data))), stash)));
	}

Irssi::Notifylist
notifylist_add(mask, ircnets, away_check, idle_check_time)
	char *mask
	char *ircnets
	int away_check
	int idle_check_time

void
notifylist_remove(mask)
	char *mask

Irssi::Server
notifylist_ison(nick, serverlist)
	char *nick
	char *serverlist

Irssi::Notifylist
notifylist_find(mask, ircnet)
	char *mask
	char *ircnet

#*******************************
MODULE = Irssi  PACKAGE = Irssi::Server
#*******************************

int
notifylist_ison_server(server, nick)
	Irssi::Server server
	char *nick

#*******************************
MODULE = Irssi	PACKAGE = Irssi::Notifylist  PREFIX = notifylist_
#*******************************

void
values(notify)
	Irssi::Notifylist notify
PREINIT:
	HV *hv;
	AV *av;
	char **tmp;
PPCODE:
	hv = newHV();
	hv_store(hv, "mask", 4, new_pv(notify->mask), 0);
	hv_store(hv, "away_check", 10, newSViv(notify->away_check), 0);
	hv_store(hv, "idle_check_time", 15, newSViv(notify->idle_check_time), 0);

	av = newAV();
	for (tmp = notify->ircnets; *tmp != NULL; tmp++) {
		av_push(av, new_pv(*tmp));
	}
	hv_store(hv, "ircnets", 7, newRV_noinc((SV*)av), 0);
	XPUSHs(sv_2mortal(newRV_noinc((SV*)hv)));

int
notifylist_ircnets_match(rec, ircnet)
	Irssi::Notifylist rec
	char *ircnet
