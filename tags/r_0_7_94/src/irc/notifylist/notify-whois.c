/*
 notify-whois.c : irssi

    Copyright (C) 1999-2000 Timo Sirainen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "module.h"
#include "signals.h"
#include "special-vars.h"

#include "irc.h"
#include "irc-server.h"
#include "masks.h"

#include "notifylist.h"

static char *last_notify_nick;

static void event_whois(const char *data, IRC_SERVER_REC *server)
{
        char *params, *nick, *user, *host, *realname;
	NOTIFY_NICK_REC *nickrec;
	NOTIFYLIST_REC *notify;

	g_return_if_fail(data != NULL);
	g_return_if_fail(server != NULL);

	params = event_get_params(data, 6, NULL, &nick, &user, &host, NULL, &realname);

	notify = notifylist_find(nick, server->connrec->ircnet);
	if (notify != NULL && !irc_mask_match(notify->mask, nick, user, host)) {
		/* user or host didn't match */
		g_free(params);
		return;
	}

	nickrec = notify_nick_find(server, nick);
	if (nickrec != NULL) {
                g_free_not_null(last_notify_nick);
		last_notify_nick = g_strdup(nick);

		g_free_not_null(nickrec->user);
		g_free_not_null(nickrec->host);
		g_free_not_null(nickrec->realname);
		g_free_and_null(nickrec->awaymsg);
		nickrec->user = g_strdup(user);
		nickrec->host = g_strdup(host);
		nickrec->realname = g_strdup(realname);

		nickrec->away = FALSE;
		nickrec->host_ok = TRUE;
		nickrec->idle_ok = TRUE;
	}
	g_free(params);
}

static void event_whois_idle(const char *data, IRC_SERVER_REC *server)
{
	NOTIFY_NICK_REC *nickrec;
	NOTIFYLIST_REC *notify;
	char *params, *nick, *secstr;
	long secs;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 3, NULL, &nick, &secstr);
	secs = atol(secstr);

	notify = notifylist_find(nick, server->connrec->ircnet);
	nickrec = notify_nick_find(server, nick);
	if (notify != NULL && nickrec != NULL) {
		nickrec->idle_changed = secs < nickrec->idle_time &&
			nickrec->idle_time > notify->idle_check_time;

		nickrec->idle_time = secs;
	}

	g_free(params);
}

static void event_whois_away(const char *data, IRC_SERVER_REC *server)
{
	NOTIFY_NICK_REC *nickrec;
	char *params, *nick, *awaymsg;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 3, NULL, &nick, &awaymsg);

	nickrec = notify_nick_find(server, nick);
	if (nickrec != NULL) {
		nickrec->awaymsg = g_strdup(awaymsg);
                nickrec->away = TRUE;
	}

	g_free(params);
}

/* All WHOIS replies got, now announce all the changes at once. */
static void event_whois_end(const char *data, IRC_SERVER_REC *server)
{
	MODULE_SERVER_REC *mserver;
	NOTIFYLIST_REC *notify;
	NOTIFY_NICK_REC *rec;
	GSList *tmp;
	const char *event;
	int away_ok;

	mserver = MODULE_DATA(server);
	for (tmp = mserver->notify_users; tmp != NULL; tmp = tmp->next) {
		rec = tmp->data;

		if (rec->realname == NULL)
			continue;

		notify = notifylist_find(rec->nick, server->connrec->ircnet);
		if (notify == NULL) continue;

		away_ok = !notify->away_check || !rec->away;

		event = NULL;
		if (!rec->join_announced) {
			rec->join_announced = TRUE;
			rec->idle_time = 0;
			if (away_ok) event = "notifylist joined";
		} else if (notify->away_check && rec->away_ok == rec->away)
			event = "notifylist away changed";
		else if (notify->idle_check_time > 0 && rec->idle_changed)
			event = "notifylist unidle";

		if (event != NULL) {
			signal_emit(event, 6, server, rec->nick,
				    rec->user, rec->host,
				    rec->realname, rec->awaymsg);
		}
		rec->idle_ok = notify->idle_check_time <= 0 ||
			rec->idle_time <= notify->idle_check_time;
		rec->idle_changed = FALSE;
                rec->away_ok = away_ok;
	}
}

/* last person that NOTIFY detected a signon for */
static char *expando_lastnotify(void *server, void *item, int *free_ret)
{
	return last_notify_nick;
}

void notifylist_whois_init(void)
{
	last_notify_nick = NULL;

	signal_add("notifylist event whois", (SIGNAL_FUNC) event_whois);
	signal_add("notifylist event whois away", (SIGNAL_FUNC) event_whois_away);
	signal_add("notifylist event whois idle", (SIGNAL_FUNC) event_whois_idle);
	signal_add("notifylist event whois end", (SIGNAL_FUNC) event_whois_end);
	expando_create("D", expando_lastnotify);
}

void notifylist_whois_deinit(void)
{
	g_free_not_null(last_notify_nick);

	signal_remove("notifylist event whois", (SIGNAL_FUNC) event_whois);
	signal_remove("notifylist event whois away", (SIGNAL_FUNC) event_whois_away);
	signal_remove("notifylist event whois idle", (SIGNAL_FUNC) event_whois_idle);
	signal_remove("notifylist event whois end", (SIGNAL_FUNC) event_whois_end);
	expando_destroy("D", expando_lastnotify);
}
