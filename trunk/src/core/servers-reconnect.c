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

#include "chat-protocols.h"
#include "servers.h"
#include "servers-setup.h"
#include "servers-reconnect.h"

#include "settings.h"

GSList *reconnects;
static int last_reconnect_tag;
static int reconnect_timeout_tag;
static int reconnect_time;

void reconnect_save_status(SERVER_CONNECT_REC *conn, SERVER_REC *server)
{
	g_free_not_null(conn->away_reason);
	conn->away_reason = !server->usermode_away ? NULL :
		g_strdup(server->away_reason);

	signal_emit("server reconnect save status", 2, conn, server);
}

static void server_reconnect_add(SERVER_CONNECT_REC *conn,
				 time_t next_connect)
{
	RECONNECT_REC *rec;

	g_return_if_fail(IS_SERVER_CONNECT(conn));

	rec = g_new(RECONNECT_REC, 1);
	rec->tag = ++last_reconnect_tag;
	rec->conn = conn;
	rec->next_connect = next_connect;

	reconnects = g_slist_append(reconnects, rec);
}

void server_reconnect_destroy(RECONNECT_REC *rec, int free_conn)
{
	g_return_if_fail(rec != NULL);

	reconnects = g_slist_remove(reconnects, rec);

	signal_emit("server reconnect remove", 1, rec);
	if (free_conn) server_connect_free(rec->conn);
	g_free(rec);

	if (reconnects == NULL)
	    last_reconnect_tag = 0;
}

static int server_reconnect_timeout(void)
{
	SERVER_CONNECT_REC *conn;
	GSList *list, *tmp;
	time_t now;

	/* If server_connect() removes the next reconnection in queue,
	   we're screwed. I don't think this should happen anymore, but just
	   to be sure we don't crash, do this safely. */
	list = g_slist_copy(reconnects);
	now = time(NULL);
	for (tmp = list; tmp != NULL; tmp = tmp->next) {
		RECONNECT_REC *rec = tmp->data;

		if (g_slist_find(reconnects, rec) == NULL)
			continue;

		if (rec->next_connect <= now) {
			conn = rec->conn;
			server_reconnect_destroy(rec, FALSE);
			CHAT_PROTOCOL(conn)->server_connect(conn);
		}
	}

	g_slist_free(list);
	return 1;
}

static void sserver_connect(SERVER_SETUP_REC *rec, SERVER_CONNECT_REC *conn)
{
        conn->family = rec->family;
	conn->address = g_strdup(rec->address);
	if (conn->port == 0) conn->port = rec->port;

	server_setup_fill_reconn(conn, rec);
	if (rec->last_connect > time(NULL)-reconnect_time) {
		/* can't reconnect this fast, wait.. */
		server_reconnect_add(conn, rec->last_connect+reconnect_time);
	} else {
		/* connect to server.. */
		CHAT_PROTOCOL(conn)->server_connect(conn);
	}
}

static SERVER_CONNECT_REC *
server_connect_copy_skeleton(SERVER_CONNECT_REC *src, int connect_info)
{
	SERVER_CONNECT_REC *dest;

        dest = NULL;
	signal_emit("server connect copy", 2, &dest, src);
	g_return_val_if_fail(dest != NULL, NULL);

	dest->type = module_get_uniq_id("SERVER CONNECT", 0);
	dest->reconnection = src->reconnection;
	dest->proxy = g_strdup(src->proxy);
        dest->proxy_port = src->proxy_port;
	dest->proxy_string = g_strdup(src->proxy_string);

	if (connect_info) {
                dest->family = src->family;
		dest->address = g_strdup(src->address);
		dest->port = src->port;
		dest->password = g_strdup(src->password);
	}

	dest->chatnet = g_strdup(src->chatnet);
	dest->nick = g_strdup(src->nick);
	dest->username = g_strdup(src->username);
	dest->realname = g_strdup(src->realname);

	if (src->own_ip4 != NULL) {
		dest->own_ip4 = g_new(IPADDR, 1);
		memcpy(dest->own_ip4, src->own_ip4, sizeof(IPADDR));
	}
	if (src->own_ip6 != NULL) {
		dest->own_ip6 = g_new(IPADDR, 1);
		memcpy(dest->own_ip6, src->own_ip6, sizeof(IPADDR));
	}

	dest->channels = g_strdup(src->channels);
	dest->away_reason = g_strdup(src->away_reason);

	return dest;
}

#define server_should_reconnect(server) \
	((server)->connection_lost && ((server)->connrec->chatnet != NULL || \
				(!(server)->banned && !(server)->dns_error)))

#define sserver_connect_ok(rec, net) \
	(!(rec)->banned && !(rec)->dns_error && (rec)->chatnet != NULL && \
	g_strcasecmp((rec)->chatnet, (net)) == 0)

static void sig_reconnect(SERVER_REC *server)
{
	SERVER_CONNECT_REC *conn;
	SERVER_SETUP_REC *sserver;
	GSList *tmp;
	int use_next, through;
	time_t now;

	g_return_if_fail(IS_SERVER(server));

	if (reconnect_time == -1 || !server_should_reconnect(server))
		return;

	conn = server_connect_copy_skeleton(server->connrec, FALSE);
        g_return_if_fail(conn != NULL);

	/* save the server status */
	if (server->connected) {
		conn->reconnection = TRUE;

                reconnect_save_status(conn, server);
	}

	sserver = server_setup_find(server->connrec->address,
				    server->connrec->port);

	if (sserver != NULL) {
		/* save the last connection time/status */
		sserver->last_connect = server->connect_time == 0 ?
			time(NULL) : server->connect_time;
		sserver->last_failed = !server->connected;
		if (server->banned) sserver->banned = TRUE;
                if (server->dns_error) sserver->dns_error = TRUE;
	}

	if (sserver == NULL || conn->chatnet == NULL) {
		/* not in any chatnet, just reconnect back to same server */
                conn->family = server->connrec->family;
		conn->address = g_strdup(server->connrec->address);
		conn->port = server->connrec->port;
		conn->password = g_strdup(server->connrec->password);

		if (server->connect_time != 0 &&
		    time(NULL)-server->connect_time > reconnect_time) {
			/* there's been enough time since last connection,
			   reconnect back immediately */
			CHAT_PROTOCOL(conn)->server_connect(conn);
		} else {
			/* reconnect later.. */
			server_reconnect_add(conn, (server->connect_time == 0 ? time(NULL) :
						    server->connect_time) + reconnect_time);
		}
		return;
	}

	/* always try to first connect to the first on the list where we
	   haven't got unsuccessful connection attempts for the last half
	   an hour. */

	now = time(NULL);
	for (tmp = setupservers; tmp != NULL; tmp = tmp->next) {
		SERVER_SETUP_REC *rec = tmp->data;

		if (sserver_connect_ok(rec, conn->chatnet) &&
		    (!rec->last_connect || !rec->last_failed ||
		     rec->last_connect < now-FAILED_RECONNECT_WAIT)) {
			if (rec == sserver)
                                conn->port = server->connrec->port;
			sserver_connect(rec, conn);
			return;
		}
	}

	/* just try the next server in list */
	use_next = through = FALSE;
	for (tmp = setupservers; tmp != NULL; ) {
		SERVER_SETUP_REC *rec = tmp->data;

		if (!use_next && server->connrec->port == rec->port &&
		    g_strcasecmp(rec->address, server->connrec->address) == 0)
			use_next = TRUE;
		else if (use_next && sserver_connect_ok(rec, conn->chatnet)) {
			if (rec == sserver)
                                conn->port = server->connrec->port;
			sserver_connect(rec, conn);
			break;
		}

		if (tmp->next != NULL) {
			tmp = tmp->next;
			continue;
		}

		if (through) {
			/* shouldn't happen unless there's no servers in
			   this chatnet in setup.. */
                        server_connect_free(conn);
			break;
		}

		tmp = setupservers;
		use_next = through = TRUE;
	}
}

static void sig_connected(SERVER_REC *server)
{
	g_return_if_fail(IS_SERVER(server));
	if (!server->connrec->reconnection)
		return;

	if (server->connrec->channels != NULL)
		server->channels_join(server, server->connrec->channels, TRUE);
}

/* Remove all servers from reconnect list */
/* SYNTAX: RMRECONNS */
static void cmd_rmreconns(void)
{
	while (reconnects != NULL)
		server_reconnect_destroy(reconnects->data, TRUE);
}

static RECONNECT_REC *reconnect_find_tag(int tag)
{
	GSList *tmp;

	for (tmp = reconnects; tmp != NULL; tmp = tmp->next) {
		RECONNECT_REC *rec = tmp->data;

		if (rec->tag == tag)
			return rec;
	}

	return NULL;
}

static void reconnect_all(void)
{
	GSList *list;
	SERVER_CONNECT_REC *conn;
	RECONNECT_REC *rec;

	/* first move reconnects to another list so if server_connect()
	   fails and goes to reconnection list again, we won't get stuck
	   here forever */
	list = NULL;
	while (reconnects != NULL) {
		rec = reconnects->data;

		list = g_slist_append(list, rec->conn);
		server_reconnect_destroy(rec, FALSE);
	}


	while (list != NULL) {
		conn = list->data;

		CHAT_PROTOCOL(conn)->server_connect(conn);
                list = g_slist_remove(list, conn);
	}
}

/* SYNTAX: RECONNECT <tag> */
static void cmd_reconnect(const char *data, SERVER_REC *server)
{
	SERVER_CONNECT_REC *conn;
	RECONNECT_REC *rec;
	int tag;

	if (*data == '\0' && server != NULL) {
		/* reconnect back to same server */
		conn = server_connect_copy_skeleton(server->connrec, TRUE);

		if (server->connected) {
			reconnect_save_status(conn, server);
			signal_emit("command disconnect", 2,
				    "* Reconnecting", server);
		}

		conn->reconnection = TRUE;
		CHAT_PROTOCOL(conn)->server_connect(conn);
                return;
	}

	if (g_strcasecmp(data, "all") == 0) {
		/* reconnect all servers in reconnect queue */
                reconnect_all();
                return;
	}

	if (*data == '\0') {
		/* reconnect to first server in reconnection list */
		if (reconnects == NULL)
			cmd_return_error(CMDERR_NOT_CONNECTED);
                rec = reconnects->data;
	} else {
		if (g_strncasecmp(data, "RECON-", 6) == 0)
			data += 6;

		tag = atoi(data);
		rec = tag <= 0 ? NULL : reconnect_find_tag(tag);

		if (rec == NULL) {
			signal_emit("server reconnect not found", 1, data);
                        return;
		}
	}

	conn = rec->conn;
	server_reconnect_destroy(rec, FALSE);
	CHAT_PROTOCOL(conn)->server_connect(conn);
}

static void cmd_disconnect(const char *data, SERVER_REC *server)
{
	RECONNECT_REC *rec;
	int tag;

	if (g_strncasecmp(data, "RECON-", 6) != 0)
		return; /* handle only reconnection removing */

	rec = sscanf(data+6, "%d", &tag) == 1 && tag > 0 ?
		reconnect_find_tag(tag) : NULL;

	if (rec == NULL)
		signal_emit("server reconnect not found", 1, data);
	else
		server_reconnect_destroy(rec, TRUE);
	signal_stop();
}

static void sig_chat_protocol_deinit(CHAT_PROTOCOL_REC *proto)
{
	GSList *tmp, *next;

	for (tmp = reconnects; tmp != NULL; tmp = next) {
		RECONNECT_REC *rec = tmp->data;

                next = tmp->next;
                if (rec->conn->chat_type == proto->id)
			server_reconnect_destroy(rec, TRUE);
	}
}

static void read_settings(void)
{
	reconnect_time = settings_get_int("server_reconnect_time");
}

void servers_reconnect_init(void)
{
	settings_add_int("server", "server_reconnect_time", 300);

	reconnects = NULL;
	last_reconnect_tag = 0;

	reconnect_timeout_tag = g_timeout_add(1000, (GSourceFunc) server_reconnect_timeout, NULL);
	read_settings();

	signal_add("server connect failed", (SIGNAL_FUNC) sig_reconnect);
	signal_add("server disconnected", (SIGNAL_FUNC) sig_reconnect);
	signal_add("event connected", (SIGNAL_FUNC) sig_connected);
	signal_add("chat protocol deinit", (SIGNAL_FUNC) sig_chat_protocol_deinit);
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);

	command_bind("rmreconns", NULL, (SIGNAL_FUNC) cmd_rmreconns);
	command_bind("reconnect", NULL, (SIGNAL_FUNC) cmd_reconnect);
	command_bind_first("disconnect", NULL, (SIGNAL_FUNC) cmd_disconnect);
}

void servers_reconnect_deinit(void)
{
	g_source_remove(reconnect_timeout_tag);

	signal_remove("server connect failed", (SIGNAL_FUNC) sig_reconnect);
	signal_remove("server disconnected", (SIGNAL_FUNC) sig_reconnect);
	signal_remove("event connected", (SIGNAL_FUNC) sig_connected);
	signal_remove("chat protocol deinit", (SIGNAL_FUNC) sig_chat_protocol_deinit);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);

	command_unbind("rmreconns", (SIGNAL_FUNC) cmd_rmreconns);
	command_unbind("reconnect", (SIGNAL_FUNC) cmd_reconnect);
	command_unbind("disconnect", (SIGNAL_FUNC) cmd_disconnect);
}
