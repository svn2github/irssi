/*
 channel-events.c : irssi

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

#include "misc.h"
#include "channels.h"
#include "irc.h"

static void event_cannot_join(const char *data, IRC_SERVER_REC *server)
{
	CHANNEL_REC *chanrec;
	char *params, *channel;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 2, NULL, &channel);

	if (channel[0] == '!' && channel[1] == '!')
		channel++; /* server didn't understand !channels */

	chanrec = channel_find(server, channel);
	if (chanrec != NULL && !chanrec->names_got) {
		chanrec->left = TRUE;
		channel_destroy(chanrec);
	}

	g_free(params);
}

static void event_target_unavailable(const char *data, IRC_SERVER_REC *server)
{
	char *params, *channel;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 2, NULL, &channel);
	if (ischannel(*channel)) {
		/* channel is unavailable - try to join again a bit later */
		event_cannot_join(data, server);
	}

	g_free(params);
}

static void channel_change_topic(IRC_SERVER_REC *server, const char *channel, const char *topic)
{
	CHANNEL_REC *chanrec;

	chanrec = channel_find(server, channel);
	if (chanrec != NULL) {
		g_free_not_null(chanrec->topic);
		chanrec->topic = *topic == '\0' ? NULL : g_strdup(topic);

		signal_emit("channel topic changed", 1, chanrec);
	}
}
static void event_topic_get(const char *data, IRC_SERVER_REC *server)
{
	char *params, *channel, *topic;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 3, NULL, &channel, &topic);
	channel_change_topic(server, channel, topic);
	g_free(params);
}

static void event_topic(const char *data, IRC_SERVER_REC *server)
{
	char *params, *channel, *topic;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 2, &channel, &topic);
	channel_change_topic(server, channel, topic);
	g_free(params);
}

/* Find any unjoined channel that matches `channel'. Long channel names are
   also a bit problematic, so find a channel where start of the name matches. */
static CHANNEL_REC *channel_find_unjoined(IRC_SERVER_REC *server, const char *channel)
{
	GSList *tmp;
	int len;

	len = strlen(channel);
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_REC *rec = tmp->data;

		if (rec->joined) continue;

		if (g_strncasecmp(channel, rec->name, len) == 0 &&
		    (len > 20 || rec->name[len] == '\0'))
			return rec;
	}

	return NULL;
}

static void event_join(const char *data, IRC_SERVER_REC *server, const char *nick, const char *address)
{
	char *params, *channel, *tmp;
	CHANNEL_REC *chanrec;

	g_return_if_fail(data != NULL);

	if (g_strcasecmp(nick, server->nick) != 0) {
		/* someone else joined channel, no need to do anything */
		return;
	}

	if (server->userhost == NULL)
		server->userhost = g_strdup(address);

	params = event_get_params(data, 1, &channel);
	tmp = strchr(channel, 7); /* ^G does something weird.. */
	if (tmp != NULL) *tmp = '\0';

	if (*channel == '!') {
		/* !channels have 5 chars long identification string before
		   it's name, it's not known when /join is called so rename
		   !channel here to !ABCDEchannel */
		char *shortchan;

		shortchan = g_strdup_printf("!%s", channel+6);
		chanrec = channel_find_unjoined(server, shortchan);
		if (chanrec != NULL) {
			g_free(chanrec->name);
			chanrec->name = g_strdup(channel);
		}

		g_free(shortchan);
	}

	chanrec = channel_find_unjoined(server, channel);
	if (chanrec == NULL) {
		/* didn't get here with /join command.. */
		chanrec = channel_create(server, channel, TRUE);
	}
	chanrec->joined = TRUE;
	if (strcmp(chanrec->name, channel) != 0) {
                g_free(chanrec->name);
		chanrec->name = g_strdup(channel);
	}

	g_free(params);
}

static void event_part(const char *data, IRC_SERVER_REC *server, const char *nick)
{
	char *params, *channel, *reason;
	CHANNEL_REC *chanrec;

	g_return_if_fail(data != NULL);

	if (g_strcasecmp(nick, server->nick) != 0) {
		/* someone else part, no need to do anything here */
		return;
	}

	params = event_get_params(data, 2, &channel, &reason);

	chanrec = channel_find(server, channel);
	if (chanrec != NULL) {
		chanrec->left = TRUE;
		channel_destroy(chanrec);
	}

	g_free(params);
}

static void event_kick(const char *data, IRC_SERVER_REC *server)
{
	CHANNEL_REC *chanrec;
	char *params, *channel, *nick, *reason;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 3, &channel, &nick, &reason);

	if (g_strcasecmp(nick, server->nick) != 0) {
		/* someone else was kicked, no need to do anything */
		g_free(params);
		return;
	}

	chanrec = channel_find(server, channel);
	if (chanrec != NULL) {
		chanrec->kicked = TRUE;
		channel_destroy(chanrec);
	}

	g_free(params);
}

static void event_invite(const char *data, IRC_SERVER_REC *server)
{
	char *params, *channel;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 2, NULL, &channel);
	g_free_not_null(server->last_invite);
	server->last_invite = g_strdup(channel);
	g_free(params);
}

void channel_events_init(void)
{
	signal_add_first("event 403", (SIGNAL_FUNC) event_cannot_join); /* no such channel */
	signal_add_first("event 405", (SIGNAL_FUNC) event_cannot_join); /* too many channels */
	signal_add_first("event 407", (SIGNAL_FUNC) event_cannot_join); /* duplicate channel */
	signal_add_first("event 471", (SIGNAL_FUNC) event_cannot_join); /* channel is full */
	signal_add_first("event 473", (SIGNAL_FUNC) event_cannot_join); /* invite only */
	signal_add_first("event 474", (SIGNAL_FUNC) event_cannot_join); /* banned */
	signal_add_first("event 475", (SIGNAL_FUNC) event_cannot_join); /* bad channel key */
	signal_add_first("event 476", (SIGNAL_FUNC) event_cannot_join); /* bad channel mask */

	signal_add("event topic", (SIGNAL_FUNC) event_topic);
	signal_add("event join", (SIGNAL_FUNC) event_join);
	signal_add("event part", (SIGNAL_FUNC) event_part);
	signal_add("event kick", (SIGNAL_FUNC) event_kick);
	signal_add("event invite", (SIGNAL_FUNC) event_invite);
	signal_add("event 332", (SIGNAL_FUNC) event_topic_get);
	signal_add_first("event 437", (SIGNAL_FUNC) event_target_unavailable); /* channel/nick unavailable */
}

void channel_events_deinit(void)
{
	signal_remove("event 403", (SIGNAL_FUNC) event_cannot_join); /* no such channel */
	signal_remove("event 405", (SIGNAL_FUNC) event_cannot_join); /* too many channels */
	signal_remove("event 407", (SIGNAL_FUNC) event_cannot_join); /* duplicate channel */
	signal_remove("event 471", (SIGNAL_FUNC) event_cannot_join); /* channel is full */
	signal_remove("event 473", (SIGNAL_FUNC) event_cannot_join); /* invite only */
	signal_remove("event 474", (SIGNAL_FUNC) event_cannot_join); /* banned */
	signal_remove("event 475", (SIGNAL_FUNC) event_cannot_join); /* bad channel key */
	signal_remove("event 476", (SIGNAL_FUNC) event_cannot_join); /* bad channel mask */

	signal_remove("event topic", (SIGNAL_FUNC) event_topic);
	signal_remove("event join", (SIGNAL_FUNC) event_join);
	signal_remove("event part", (SIGNAL_FUNC) event_part);
	signal_remove("event kick", (SIGNAL_FUNC) event_kick);
	signal_remove("event invite", (SIGNAL_FUNC) event_invite);
	signal_remove("event 332", (SIGNAL_FUNC) event_topic_get);
	signal_remove("event 437", (SIGNAL_FUNC) event_target_unavailable); /* channel/nick unavailable */
}
