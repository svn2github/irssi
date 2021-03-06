/*
 servers-reconnect.c : irssi

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
#include "commands.h"
#include "network.h"
#include "signals.h"

#include "modes.h"
#include "irc-servers.h"

#include "settings.h"

static void sig_server_connect_copy(SERVER_CONNECT_REC **dest,
				    IRC_SERVER_CONNECT_REC *src)
{
	IRC_SERVER_CONNECT_REC *rec;

	g_return_if_fail(dest != NULL);
	if (!IS_IRC_SERVER_CONNECT(src))
		return;

	rec = g_new0(IRC_SERVER_CONNECT_REC, 1);
	rec->chat_type = IRC_PROTOCOL;
	rec->max_cmds_at_once = src->max_cmds_at_once;
	rec->cmd_queue_speed = src->cmd_queue_speed;
        rec->max_query_chans = src->max_query_chans;
	rec->max_kicks = src->max_kicks;
	rec->max_modes = src->max_modes;
	rec->max_msgs = src->max_msgs;
	rec->usermode = g_strdup(src->usermode);
	*dest = (SERVER_CONNECT_REC *) rec;
}

static void sig_server_reconnect_save_status(IRC_SERVER_CONNECT_REC *conn,
					     IRC_SERVER_REC *server)
{
	if (!IS_IRC_SERVER_CONNECT(conn) || !IS_IRC_SERVER(server))
		return;

	g_free_not_null(conn->channels);
	conn->channels = irc_server_get_channels(server);

	g_free_not_null(conn->usermode);
	conn->usermode = g_strdup(server->wanted_usermode);
}

static void sig_connected(IRC_SERVER_REC *server)
{
	if (!IS_IRC_SERVER(server) || !server->connrec->reconnection)
		return;

	if (server->connrec->away_reason != NULL)
		signal_emit("command away", 2, server->connrec->away_reason, server, NULL);
}

static void event_nick_collision(IRC_SERVER_REC *server, const char *data)
{
	time_t new_connect;

	if (!IS_IRC_SERVER(server))
		return;

	/* after server kills us because of nick collision, we want to
	   connect back immediately. but no matter how hard they kill us,
	   don't connect to the server more than once in every 10 seconds. */

	new_connect = server->connect_time+10 -
		settings_get_int("server_reconnect_time");
	if (server->connect_time > new_connect)
		server->connect_time = new_connect;

        server->nick_collision = TRUE;
}

static void event_kill(IRC_SERVER_REC *server, const char *data)
{
	if (!server->nick_collision) {
		/* don't reconnect if we were killed */
		server->no_reconnect = TRUE;
	}
}

void irc_servers_reconnect_init(void)
{
	signal_add("server connect copy", (SIGNAL_FUNC) sig_server_connect_copy);
	signal_add("server reconnect save status", (SIGNAL_FUNC) sig_server_reconnect_save_status);
	signal_add("event connected", (SIGNAL_FUNC) sig_connected);
	signal_add("event 436", (SIGNAL_FUNC) event_nick_collision);
	signal_add("event kill", (SIGNAL_FUNC) event_kill);
}

void irc_servers_reconnect_deinit(void)
{
	signal_remove("server connect copy", (SIGNAL_FUNC) sig_server_connect_copy);
	signal_remove("server reconnect save status", (SIGNAL_FUNC) sig_server_reconnect_save_status);
	signal_remove("event connected", (SIGNAL_FUNC) sig_connected);
	signal_remove("event 436", (SIGNAL_FUNC) event_nick_collision);
	signal_remove("event kill", (SIGNAL_FUNC) event_kill);
}
