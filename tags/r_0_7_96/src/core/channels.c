/*
 channel.c : irssi

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
#include "special-vars.h"

#include "servers.h"
#include "channels.h"
#include "channels-setup.h"
#include "nicklist.h"

GSList *channels; /* List of all channels */

/* Create a new channel */
CHANNEL_REC *channel_create(int chat_type, SERVER_REC *server,
			    const char *name, int automatic)
{
	CHANNEL_REC *channel;

	g_return_val_if_fail(server == NULL || IS_SERVER(server), NULL);
	g_return_val_if_fail(name != NULL, NULL);

	channel = NULL;
	signal_emit("channel create", 5, &channel, GINT_TO_POINTER(chat_type),
		    server, name, GINT_TO_POINTER(automatic));
	return channel;
}

void channel_init(CHANNEL_REC *channel, int automatic)
{
	g_return_if_fail(channel != NULL);
	g_return_if_fail(channel->name != NULL);

	channels = g_slist_append(channels, channel);
	if (channel->server != NULL) {
		channel->server->channels =
			g_slist_append(channel->server->channels, channel);
	}

        MODULE_DATA_INIT(channel);
	channel->type = module_get_uniq_id_str("WINDOW ITEM TYPE", "CHANNEL");
        channel->mode = g_strdup("");
	channel->createtime = time(NULL);

	signal_emit("channel created", 2, channel, GINT_TO_POINTER(automatic));
}

void channel_destroy(CHANNEL_REC *channel)
{
	g_return_if_fail(IS_CHANNEL(channel));

	if (channel->destroying) return;
	channel->destroying = TRUE;

	channels = g_slist_remove(channels, channel);
	if (channel->server != NULL)
		channel->server->channels = g_slist_remove(channel->server->channels, channel);
	signal_emit("channel destroyed", 1, channel);

        MODULE_DATA_DEINIT(channel);
	g_free_not_null(channel->topic);
	g_free_not_null(channel->key);
	g_free(channel->mode);
	g_free(channel->name);
	g_free(channel);
}

static CHANNEL_REC *channel_find_server(SERVER_REC *server,
					const char *name)
{
	GSList *tmp;

	g_return_val_if_fail(IS_SERVER(server), NULL);

	if (server->channel_find_func != NULL) {
		/* use the server specific channel find function */
		return server->channel_find_func(server, name);
	}

	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_REC *rec = tmp->data;

		if (rec->chat_type == server->chat_type &&
		    g_strcasecmp(name, rec->name) == 0)
			return rec;
	}

	return NULL;
}

CHANNEL_REC *channel_find(SERVER_REC *server, const char *name)
{
	g_return_val_if_fail(server == NULL || IS_SERVER(server), NULL);
	g_return_val_if_fail(name != NULL, NULL);

	if (server != NULL)
		return channel_find_server(server, name);

	/* find from any server */
	return gslist_foreach_find(servers,
				   (FOREACH_FIND_FUNC) channel_find_server,
				   (void *) name);
}

/* connected to server, autojoin to channels. */
static void event_connected(SERVER_REC *server)
{
	GString *chans;
	GSList *tmp;

	g_return_if_fail(SERVER(server));

	if (server->connrec->reconnection)
		return;

	/* join to the channels marked with autojoin in setup */
	chans = g_string_new(NULL);
	for (tmp = setupchannels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_SETUP_REC *rec = tmp->data;

		if (!rec->autojoin ||
		    !channel_chatnet_match(rec->chatnet,
					   server->connrec->chatnet))
			continue;

		g_string_sprintfa(chans, "%s,", rec->name);
	}

	if (chans->len > 0) {
		g_string_truncate(chans, chans->len-1);
		server->channels_join(server, chans->str, TRUE);
	}

	g_string_free(chans, TRUE);
}

static int match_nick_flags(SERVER_REC *server, NICK_REC *nick, char flag)
{
	const char *flags = server->get_nick_flags();

	return strchr(flags, flag) == NULL ||
		(flag == flags[0] && nick->op) ||
		(flag == flags[1] && (nick->voice || nick->halfop ||
				      nick->op)) ||
		(flag == flags[2] && (nick->halfop || nick->op));
}

/* Send the auto send command to channel */
void channel_send_autocommands(CHANNEL_REC *channel)
{
	CHANNEL_SETUP_REC *rec;
	NICK_REC *nick;
	char **bots, **bot;

	g_return_if_fail(IS_CHANNEL(channel));

	rec = channels_setup_find(channel->name, channel->server->connrec->chatnet);
	if (rec == NULL || rec->autosendcmd == NULL || !*rec->autosendcmd)
		return;

	if (rec->botmasks == NULL || !*rec->botmasks) {
		/* just send the command. */
		eval_special_string(rec->autosendcmd, "", channel->server, channel);
		return;
	}

	/* find first available bot.. */
	bots = g_strsplit(rec->botmasks, " ", -1);
	for (bot = bots; *bot != NULL; bot++) {
		const char *botnick = *bot;

		nick = nicklist_find(channel,
				     channel->server->isnickflag(*botnick) ?
				     botnick+1 : botnick);
		if (nick == NULL ||
		    !match_nick_flags(channel->server, nick, *botnick))
			continue;

		/* got one! */
		eval_special_string(rec->autosendcmd, nick->nick, channel->server, channel);
		break;
	}
	g_strfreev(bots);
}

void channels_init(void)
{
	channels_setup_init();

	signal_add("event connected", (SIGNAL_FUNC) event_connected);
}

void channels_deinit(void)
{
	channels_setup_deinit();

	signal_remove("event connected", (SIGNAL_FUNC) event_connected);
	module_uniq_destroy("CHANNEL");
}
