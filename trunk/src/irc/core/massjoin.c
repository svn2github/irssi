/*
 massjoin.c : irssi

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
#include "common-setup.h"

#include "channels.h"
#include "irc.h"
#include "nicklist.h"
#include "irc-server.h"

static int massjoin_tag;

/* Massjoin support - really useful when trying to do things (like op/deop)
   to people after netjoins. It sends
   "massjoin #channel nick!user@host nick2!user@host ..." signals */
static void event_join(const char *data, IRC_SERVER_REC *server, const char *nick, const char *address)
{
	char *params, *channel, *ptr;
	CHANNEL_REC *chanrec;
	NICK_REC *nickrec;
	GSList *nicks, *tmp;

	g_return_if_fail(data != NULL);

	if (g_strcasecmp(nick, server->nick) == 0) {
		/* You joined, no need to do anything here */
		return;
	}

	params = event_get_params(data, 1, &channel);
	ptr = strchr(channel, 7); /* ^G does something weird.. */
	if (ptr != NULL) *ptr = '\0';

	/* find channel */
	chanrec = channel_find(server, channel);
	g_free(params);
	if (chanrec == NULL) return;

	/* add user to nicklist */
	nickrec = nicklist_insert(chanrec, nick, FALSE, FALSE, TRUE);
	nickrec->host = g_strdup(address);

	if (chanrec->massjoins == 0) {
		/* no nicks waiting in massjoin queue */
		chanrec->massjoin_start = time(NULL);
		chanrec->last_massjoins = 0;
	}

	/* Check if user is already in some other channel,
	   get the realname from there */
	nicks = nicklist_get_same(server, nick);
	for (tmp = nicks; tmp != NULL; tmp = tmp->next->next) {
		NICK_REC *rec = tmp->next->data;

		if (rec->realname != NULL) {
			nickrec->last_check = rec->last_check;
			nickrec->realname = g_strdup(rec->realname);
			nickrec->gone = rec->gone;
		}
	}
	g_slist_free(nicks);

	chanrec->massjoins++;
}

static void event_part(const char *data, IRC_SERVER_REC *server, const char *nick, const char *addr)
{
	char *params, *channel, *reason;
	CHANNEL_REC *chanrec;
	NICK_REC *nickrec;

	g_return_if_fail(data != NULL);

	if (g_strcasecmp(nick, server->nick) == 0) {
		/* you left channel, no need to do anything here */
		return;
	}

	params = event_get_params(data, 2, &channel, &reason);

	/* find channel */
	chanrec = channel_find(server, channel);
	if (chanrec == NULL) {
		g_free(params);
		return;
	}

	/* remove user from nicklist */
	nickrec = nicklist_find(chanrec, nick);
	if (nickrec != NULL) {
		if (nickrec->send_massjoin) {
			/* quick join/part after which it's useless to send
			   nick in massjoin */
			chanrec->massjoins--;
		}
		nicklist_remove(chanrec, nickrec);
	}
	g_free(params);
}

static void event_quit(const char *data, IRC_SERVER_REC *server, const char *nick)
{
        CHANNEL_REC *channel;
	NICK_REC *nickrec;
	GSList *nicks, *tmp;

	g_return_if_fail(data != NULL);

	if (g_strcasecmp(nick, server->nick) == 0) {
		/* you quit, don't do anything here */
		return;
	}

	/* Remove nick from all channels */
	nicks = nicklist_get_same(server, nick);
	for (tmp = nicks; tmp != NULL; tmp = tmp->next->next) {
                channel = tmp->data;
		nickrec = tmp->next->data;

		if (nickrec->send_massjoin) {
			/* quick join/quit after which it's useless to
			   send nick in massjoin */
			channel->massjoins--;
		}
		nicklist_remove(channel, nickrec);
	}
	g_slist_free(nicks);
}

static void event_kick(const char *data, IRC_SERVER_REC *server)
{
	char *params, *channel, *nick, *reason;
	CHANNEL_REC *chanrec;
	NICK_REC *nickrec;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 3, &channel, &nick, &reason);

	if (g_strcasecmp(nick, server->nick) == 0) {
		/* you were kicked, no need to do anything */
		g_free(params);
		return;
	}

	/* Remove user from nicklist */
	chanrec = channel_find(server, channel);
	nickrec = chanrec == NULL ? NULL : nicklist_find(chanrec, nick);
	if (chanrec != NULL && nickrec != NULL) {
		if (nickrec->send_massjoin) {
			/* quick join/kick after which it's useless to
			   send nick in massjoin */
			chanrec->massjoins--;
		}
		nicklist_remove(chanrec, nickrec);
	}

	g_free(params);
}

static void massjoin_send_hash(gpointer key, NICK_REC *nick, GSList **list)
{
	if (nick->send_massjoin) {
		nick->send_massjoin = FALSE;
		*list = g_slist_append(*list, nick);
	}
}

/* Send channel's massjoin list signal */
static void massjoin_send(CHANNEL_REC *channel)
{
	GSList *list;

	list = NULL;
	g_hash_table_foreach(channel->nicks, (GHFunc) massjoin_send_hash, &list);

	channel->massjoins = 0;
	signal_emit("massjoin", 2, channel, list);
	g_slist_free(list);
}

static void server_check_massjoins(IRC_SERVER_REC *server, time_t max)
{
	GSList *tmp;

	/* Scan all channels through for massjoins */
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_REC *rec = tmp->data;

		if (rec->massjoins <= 0)
			continue;

		if (rec->massjoin_start < max || /* We've waited long enough */
		    rec->massjoins-5 < rec->last_massjoins) { /* Less than 5 joins since last check */
			/* send them */
			massjoin_send(rec);
		} else {
			/* Wait for some more.. */
			rec->last_massjoins = rec->massjoins;
		}
	}

}

static int sig_massjoin_timeout(void)
{
	GSList *tmp;
	time_t max;

	max = time(NULL)-MAX_MASSJOIN_WAIT;
	for (tmp = servers; tmp != NULL; tmp = tmp->next)
                server_check_massjoins(tmp->data, max);

	return 1;
}

void massjoin_init(void)
{
	massjoin_tag = g_timeout_add(1000, (GSourceFunc) sig_massjoin_timeout, NULL);

	signal_add("event join", (SIGNAL_FUNC) event_join);
	signal_add("event part", (SIGNAL_FUNC) event_part);
	signal_add("event kick", (SIGNAL_FUNC) event_kick);
	signal_add("event quit", (SIGNAL_FUNC) event_quit);
}

void massjoin_deinit(void)
{
	g_source_remove(massjoin_tag);

	signal_remove("event join", (SIGNAL_FUNC) event_join);
	signal_remove("event part", (SIGNAL_FUNC) event_part);
	signal_remove("event kick", (SIGNAL_FUNC) event_kick);
	signal_remove("event quit", (SIGNAL_FUNC) event_quit);
}
