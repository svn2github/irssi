/*
 irc-session.c : irssi

    Copyright (C) 2001 Timo Sirainen

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
#include "net-sendbuffer.h"
#include "lib-config/iconfig.h"

#include "irc-servers.h"
#include "irc-channels.h"
#include "irc-nicklist.h"

static void sig_session_save_server(IRC_SERVER_REC *server, CONFIG_REC *config,
				    CONFIG_NODE *node)
{
        GSList *tmp;

	if (!IS_IRC_SERVER(server))
		return;

        /* send all non-redirected commands to server immediately */
	for (tmp = server->cmdqueue; tmp != NULL; tmp = tmp->next->next) {
		const char *cmd = tmp->data;
                void *redirect = tmp->next->data;

		if (redirect == NULL) {
			if (net_sendbuffer_send(server->handle, cmd,
						strlen(cmd)) == -1)
				break;
		}
	}
        net_sendbuffer_flush(server->handle);

	config_node_set_str(config, node, "real_address", server->real_address);
	config_node_set_str(config, node, "userhost", server->userhost);
	config_node_set_str(config, node, "usermode", server->usermode);
	config_node_set_bool(config, node, "usermode_away", server->usermode_away);
	config_node_set_str(config, node, "away_reason", server->away_reason);
}

static void sig_session_restore_server(IRC_SERVER_REC *server,
				       CONFIG_NODE *node)
{
	if (!IS_IRC_SERVER(server))
		return;

        if (server->real_address == NULL)
		server->real_address = g_strdup(config_node_get_str(node, "real_address", NULL));
	server->userhost = g_strdup(config_node_get_str(node, "userhost", NULL));
	server->usermode = g_strdup(config_node_get_str(node, "usermode", NULL));
	server->usermode_away = config_node_get_bool(node, "usermode_away", FALSE);
	server->away_reason = g_strdup(config_node_get_str(node, "away_reason", NULL));

	/* FIXME: remove before .99 */
	g_free_not_null(server->connrec->channels);
	server->connrec->channels = g_strdup(config_node_get_str(node, "channels", NULL));
}

static void sig_session_restore_nick(IRC_CHANNEL_REC *channel,
				     CONFIG_NODE *node)
{
	const char *nick;
        int op, halfop, voice;
        NICK_REC *nickrec;

	if (!IS_IRC_CHANNEL(channel))
		return;

	nick = config_node_get_str(node, "nick", NULL);
	if (nick == NULL)
                return;

	op = config_node_get_bool(node, "op", FALSE);
        voice = config_node_get_bool(node, "voice", FALSE);
        halfop = config_node_get_bool(node, "halfop", FALSE);
	nickrec = irc_nicklist_insert(channel, nick, op, halfop, voice, FALSE);
}

static void session_restore_channel(IRC_CHANNEL_REC *channel)
{
	char *data;

	signal_emit("event join", 4, channel->server, channel->name,
		    channel->server->nick, channel->server->userhost);

	if (nicklist_find(CHANNEL(channel), channel->server->nick) == NULL) {
		/* FIXME: remove before .99 */
                irc_send_cmdv(channel->server, "NAMES %s", channel->name);
	} else {
		data = g_strconcat(channel->server->nick, " ", channel->name, NULL);
		signal_emit("event 366", 2, channel->server, data);
		g_free(data);
	}
}

static void sig_connected(IRC_SERVER_REC *server)
{
	GSList *tmp;
        char *str;

	if (!IS_IRC_SERVER(server) || !server->session_reconnect)
		return;

	str = g_strdup_printf("%s :Restoring connection to %s",
			      server->nick, server->connrec->address);
	signal_emit("event 001", 3, server, str, server->real_address);
        g_free(str);

	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		IRC_CHANNEL_REC *rec = tmp->data;

		rec->session_rejoin = TRUE; /* FIXME: remove after .99 */
		if (rec->session_rejoin)
                        session_restore_channel(rec);
	}
}

void irc_session_init(void)
{
	signal_add("session save server", (SIGNAL_FUNC) sig_session_save_server);
	signal_add("session restore server", (SIGNAL_FUNC) sig_session_restore_server);
	signal_add("session restore nick", (SIGNAL_FUNC) sig_session_restore_nick);

	signal_add("server connected", (SIGNAL_FUNC) sig_connected);
}

void irc_session_deinit(void)
{
	signal_remove("session save server", (SIGNAL_FUNC) sig_session_save_server);
	signal_remove("session restore server", (SIGNAL_FUNC) sig_session_restore_server);
	signal_remove("session restore nick", (SIGNAL_FUNC) sig_session_restore_nick);

	signal_remove("server connected", (SIGNAL_FUNC) sig_connected);
}
