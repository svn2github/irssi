/*
 irc-commands.c : irssi

    Copyright (C) 1999 Timo Sirainen

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
#include "network.h"
#include "commands.h"
#include "misc.h"
#include "special-vars.h"
#include "settings.h"
#include "window-item-def.h"

#include "servers-reconnect.h"
#include "servers-redirect.h"
#include "servers-setup.h"
#include "nicklist.h"

#include "bans.h"
#include "irc.h"
#include "irc-servers.h"
#include "irc-channels.h"
#include "irc-queries.h"

/* How often to check if there's anyone to be unbanned in knockout list */
#define KNOCKOUT_TIMECHECK 10000

/* /LIST: Max. number of channels in IRC network before -yes option
   is required */
#define LIST_MAX_CHANNELS_PASS 1000

typedef struct {
	IRC_CHANNEL_REC *channel;
	char *ban;
        int timeleft;
} KNOCKOUT_REC;

static GString *tmpstr;
static int knockout_tag;

static SERVER_REC *irc_connect_server(const char *data)
{
	SERVER_CONNECT_REC *conn;
	SERVER_REC *server;
	GHashTable *optlist;
	char *addr, *portstr, *password, *nick, *ircnet, *host;
	void *free_arg;

	g_return_val_if_fail(data != NULL, NULL);

	if (!cmd_get_params(data, &free_arg, 4 | PARAM_FLAG_OPTIONS,
			    "connect", &optlist, &addr, &portstr, &password, &nick))
		return NULL;
	if (*addr == '+') addr++;
	if (*addr == '\0') {
		signal_emit("error command", 1, GINT_TO_POINTER(CMDERR_NOT_ENOUGH_PARAMS));
		cmd_params_free(free_arg);
		return NULL;
	}

	if (strcmp(password, "-") == 0)
		*password = '\0';

	/* connect to server */
	conn = server_create_conn(addr, atoi(portstr), password, nick);
        ircnet = g_hash_table_lookup(optlist, "ircnet");
	if (ircnet != NULL && *ircnet != '\0') {
		g_free_not_null(conn->chatnet);
		conn->chatnet = g_strdup(ircnet);
	}
        host = g_hash_table_lookup(optlist, "host");
	if (host != NULL && *host != '\0') {
		IPADDR ip;

		if (net_gethostbyname(host, &ip) == 0) {
			if (conn->own_ip == NULL)
				conn->own_ip = g_new(IPADDR, 1);
			memcpy(conn->own_ip, &ip, sizeof(IPADDR));
		}
	}
	server = server_connect(conn);

	cmd_params_free(free_arg);
	return server;
}

/* SYNTAX: CONNECT [-ircnet <ircnet>] [-host <hostname>]
                   <address>|<ircnet> [<port> [<password> [<nick>]]] */
static void cmd_connect(const char *data)
{
	if (*data == '\0') cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);
	irc_connect_server(data);
}

static RECONNECT_REC *find_reconnect_server(const char *addr, int port)
{
	RECONNECT_REC *match;
	GSList *tmp;

	g_return_val_if_fail(addr != NULL, NULL);

	if (g_slist_length(reconnects) == 1) {
		/* only one reconnection, we probably want to use it */
		match = reconnects->data;
		return IS_IRC_SERVER_CONNECT(match->conn) ? match : NULL;
	}

	/* check if there's a reconnection to the same host and maybe even
	   the same port */
        match = NULL;
	for (tmp = reconnects; tmp != NULL; tmp = tmp->next) {
		RECONNECT_REC *rec = tmp->data;

		if (IS_IRC_SERVER_CONNECT(rec->conn) &&
		    g_strcasecmp(rec->conn->address, addr) == 0) {
			if (rec->conn->port == port)
				return rec;
			match = rec;
		}
	}

	return match;
}

/* SYNTAX: SERVER [-ircnet <ircnet>] [-host <hostname>]
                  [+]<address>|<ircnet> [<port> [<password> [<nick>]]] */
static void cmd_server(const char *data, IRC_SERVER_REC *server,
		       void *item)
{
	GHashTable *optlist;
	IRC_SERVER_CONNECT_REC *conn;
	char *addr, *port, *channels, *away_reason, *usermode, *ircnet;
	void *free_arg;
	int no_old_server;

	g_return_if_fail(data != NULL);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTIONS,
			    "connect", &optlist, &addr, &port))
		return;

	if (*addr == '\0' || strcmp(addr, "+") == 0)
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	conn = server == NULL ? NULL : server->connrec;
	if (*addr != '+' && conn == NULL) {
		/* check if there's a server waiting for removal in
		   reconnection queue.. */
		RECONNECT_REC *rec;

		rec = find_reconnect_server(addr, atoi(port));
		if (rec != NULL) {
			/* remove the reconnection.. */
                        conn = (IRC_SERVER_CONNECT_REC *) rec->conn;
			server_reconnect_destroy(rec, FALSE);
		}
	}

	no_old_server = server == NULL;
	ircnet = conn == NULL ? NULL : g_strdup(conn->chatnet);
	if (*addr == '+' || conn == NULL) {
		channels = away_reason = usermode = NULL;
	} else if (server != NULL) {
		channels = irc_server_get_channels((IRC_SERVER_REC *) server);
		if (*channels == '\0')
			g_free_and_null(channels);
		away_reason = !server->usermode_away ? NULL :
			g_strdup(server->away_reason);
		usermode = g_strdup(server->usermode);
		signal_emit("command disconnect", 3,
			    "* Changing server", server, item);
	} else {
		channels = g_strdup(conn->channels);
		away_reason = g_strdup(conn->away_reason);
		usermode = g_strdup(conn->usermode);
	}

	server = IRC_SERVER(irc_connect_server(data));
	if (*addr == '+' || server == NULL ||
	    (ircnet != NULL && server->connrec->chatnet != NULL &&
	     g_strcasecmp(ircnet, server->connrec->chatnet) != 0)) {
		g_free_not_null(channels);
		g_free_not_null(usermode);
		g_free_not_null(away_reason);
	} else if (server != NULL && conn != NULL) {
		server->connrec->reconnection = TRUE;
		server->connrec->channels = channels;
		server->connrec->usermode = usermode;
		server->connrec->away_reason = away_reason;
		if (no_old_server)
			server_connect_free(SERVER_CONNECT(conn));
	}
	g_free_not_null(ircnet);
	cmd_params_free(free_arg);
}

/* SYNTAX: NOTICE <targets> <message> */
static void cmd_notice(const char *data, IRC_SERVER_REC *server)
{
	char *target, *msg;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &target, &msg))
		return;
	if (*target == '\0' || *msg == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	g_string_sprintf(tmpstr, "NOTICE %s :%s", target, msg);
	irc_send_cmd_split(server, tmpstr->str, 2, server->max_msgs_in_cmd);

	cmd_params_free(free_arg);
}

/* SYNTAX: CTCP <targets> <ctcp command> [<ctcp data>] */
static void cmd_ctcp(const char *data, IRC_SERVER_REC *server)
{
	char *target, *ctcpcmd, *ctcpdata;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 3 | PARAM_FLAG_GETREST, &target, &ctcpcmd, &ctcpdata))
		return;
	if (*target == '\0' || *ctcpcmd == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	g_strup(ctcpcmd);
	if (*ctcpdata == '\0')
		g_string_sprintf(tmpstr, "PRIVMSG %s :\001%s\001", target, ctcpcmd);
	else
		g_string_sprintf(tmpstr, "PRIVMSG %s :\001%s %s\001", target, ctcpcmd, ctcpdata);
	irc_send_cmd_split(server, tmpstr->str, 2, server->max_msgs_in_cmd);

	cmd_params_free(free_arg);
}

/* SYNTAX: NCTCP <targets> <ctcp command> [<ctcp data>] */
static void cmd_nctcp(const char *data, IRC_SERVER_REC *server)
{
	char *target, *ctcpcmd, *ctcpdata;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 3 | PARAM_FLAG_GETREST, &target, &ctcpcmd, &ctcpdata))
		return;
	if (*target == '\0' || *ctcpcmd == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	g_strup(ctcpcmd);
	g_string_sprintf(tmpstr, "NOTICE %s :\001%s %s\001", target, ctcpcmd, ctcpdata);
	irc_send_cmd_split(server, tmpstr->str, 2, server->max_msgs_in_cmd);

	cmd_params_free(free_arg);
}

/* SYNTAX: PART [<channels>] [<message>] */
static void cmd_part(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
	char *channame, *msg;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTCHAN | PARAM_FLAG_GETREST, item, &channame, &msg))
		return;
	if (*channame == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	if (*msg == '\0') msg = (char *) settings_get_str("part_message");
	irc_send_cmdv(server, *msg == '\0' ? "PART %s" : "PART %s :%s",
		      channame, msg);

	cmd_params_free(free_arg);
}

/* SYNTAX: KICK [<channel>] <nicks> [<reason>] */
static void cmd_kick(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
	char *channame, *nicks, *reason;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 3 | PARAM_FLAG_OPTCHAN | PARAM_FLAG_GETREST,
			    item, &channame, &nicks, &reason))
		return;

	if (*channame == '\0' || *nicks == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	if (!ischannel(*channame)) cmd_param_error(CMDERR_NOT_JOINED);

	g_string_sprintf(tmpstr, "KICK %s %s :%s", channame, nicks, reason);
	irc_send_cmd_split(server, tmpstr->str, 3, server->max_kicks_in_cmd);

	cmd_params_free(free_arg);
}

/* SYNTAX: TOPIC [-delete] [<channel>] [<topic>] */
static void cmd_topic(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
	GHashTable *optlist;
	char *channame, *topic;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTCHAN |
			    PARAM_FLAG_OPTIONS | PARAM_FLAG_GETREST,
			    item, "topic", &optlist, &channame, &topic))
		return;

	irc_send_cmdv(server, *topic == '\0' &&
		      g_hash_table_lookup(optlist, "delete") == NULL ?
		      "TOPIC %s" : "TOPIC %s :%s", channame, topic);

	cmd_params_free(free_arg);
}

/* SYNTAX: INVITE <nick> [<channel>] */
static void cmd_invite(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
	char *nick, *channame;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2, &nick, &channame))
		return;

	if (*nick == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	if (*channame == '\0' || strcmp(channame, "*") == 0) {
		if (!IS_IRC_CHANNEL(item))
			cmd_param_error(CMDERR_NOT_JOINED);

		channame = item->name;
	}

	irc_send_cmdv(server, "INVITE %s %s", nick, channame);
	cmd_params_free(free_arg);
}

/* SYNTAX: LIST [-yes] [<channel>] */
static void cmd_list(const char *data, IRC_SERVER_REC *server,
		     WI_ITEM_REC *item)
{
	GHashTable *optlist;
	char *str;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 1 | PARAM_FLAG_OPTIONS |
			    PARAM_FLAG_GETREST, "list", &optlist, &str))
		return;

	if (*str == '\0' && g_hash_table_lookup(optlist, "yes") == NULL &&
	    (server->channels_formed <= 0 ||
	     server->channels_formed > LIST_MAX_CHANNELS_PASS))
		cmd_param_error(CMDERR_NOT_GOOD_IDEA);

	irc_send_cmdv(server, "LIST %s", str);
	cmd_params_free(free_arg);

	/* add default redirection */
	server_redirect_default((SERVER_REC *) server, "bogus command list");
}

/* SYNTAX: WHO <nicks>|<channels>|** */
static void cmd_who(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
	char *channel, *rest;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &channel, &rest))
		return;

	if (strcmp(channel, "*") == 0 || *channel == '\0') {
		if (!IS_IRC_CHANNEL(item))
                        cmd_param_error(CMDERR_NOT_JOINED);

		channel = item->name;
	}
	if (strcmp(channel, "**") == 0) {
		/* ** displays all nicks.. */
		*channel = '\0';
	}

	irc_send_cmdv(server, *rest == '\0' ? "WHO %s" : "WHO %s %s",
		      channel, rest);
	cmd_params_free(free_arg);

	/* add default redirection */
	server_redirect_default((SERVER_REC *) server, "bogus command who");
}

/* SYNTAX: NAMES [-yes] [<channels>] */
static void cmd_names(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
	g_return_if_fail(data != NULL);

	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);
	if (*data == '\0') cmd_return_error(CMDERR_NOT_GOOD_IDEA);

	if (strcmp(data, "*") == 0) {
		if (!IS_IRC_CHANNEL(item))
			cmd_return_error(CMDERR_NOT_JOINED);

		data = item->name;
	}

	if (g_strcasecmp(data, "-YES") == 0)
		irc_send_cmd(server, "NAMES");
	else
		irc_send_cmdv(server, "NAMES %s", data);
}

/* SYNTAX: NICK <new nick> */
static void cmd_nick(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
        char *nick;
	void *free_arg;

	g_return_if_fail(data != NULL);

	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 1, &nick))
		return;

	server->nick_changing = TRUE;
	irc_send_cmdv(server, "NICK %s", nick);

	nick = g_strdup_printf("%s :%s", nick, nick);
	server_redirect_event(SERVER(server), nick, 5,
			      "event nick", "nickchange over", 0,
			      "event 433", "nickchange over", 1,
			      /* 437: ircnet = target unavailable,
				      dalnet = banned in channel,
				               can't change nick */
			      "event 437", "nickchange over", -1,
			      "event 432", "nickchange over", 1,
			      "event 438", "nickchange over", 1, NULL);
        g_free(nick);
	cmd_params_free(free_arg);
}

static void sig_nickchange_over(IRC_SERVER_REC *server, const char *data,
				const char *nick, const char *addr)
{
	char *signal;

	server->nick_changing = FALSE;

	signal = g_strconcat("event ", current_server_event, NULL);
	g_strdown(signal+6);
	signal_emit(signal, 4, server, data, nick, addr);
	g_free(signal);
}

static char *get_redirect_nicklist(const char *nicks, int *free)
{
	char *str, *ret;

	if (strchr(nicks, ',') == NULL) {
		*free = FALSE;
		return (char *) nicks;
	}

	*free = TRUE;

	str = g_strdup(nicks);
	g_strdelimit(str, ",", ' ');
	ret = g_strconcat(str, " ", nicks, NULL);
	g_free(str);

	return ret;
}

/* SYNTAX: WHOIS [<server>] [<nicks>] */
static void cmd_whois(const char *data, IRC_SERVER_REC *server,
		      WI_ITEM_REC *item)
{
	GHashTable *optlist;
	char *qserver, *query, *event_402, *str;
	void *free_arg;
	int free_nick;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTIONS,
			    "whois", &optlist, &qserver, &query))
		return;

	if (*query == '\0') {
		query = qserver;
		qserver = "";
	}
	if (*query == '\0') {
		QUERY_REC *queryitem = QUERY(item);
		query = queryitem != NULL ? queryitem->name : server->nick;
	}

	if (strcmp(query, "*") == 0 &&
	    g_hash_table_lookup(optlist, "yes") == NULL)
		cmd_param_error(CMDERR_NOT_GOOD_IDEA);

	event_402 = "event 402";
	if (*qserver == '\0')
		g_string_sprintf(tmpstr, "WHOIS %s", query);
	else {
		g_string_sprintf(tmpstr, "WHOIS %s %s", qserver, query);
		if (g_strcasecmp(qserver, query) == 0)
			event_402 = "whois event noserver";
	}

	server->whois_found = FALSE;
	irc_send_cmd_split(server, tmpstr->str, 2, server->max_whois_in_cmd);

	/* do automatic /WHOWAS if any of the nicks wasn't found */
	query = get_redirect_nicklist(query, &free_nick);

        str = g_strconcat(qserver, " ", query, NULL);
	server_redirect_event(SERVER(server), str, 2,
			      "event 318", "event 318", 1,
			      "event 402", event_402, 1,
			      "event 401", "whois not found", 1,
			      "event 311", "whois event", 1, NULL);
        g_free(str);

	if (free_nick) g_free(query);
	cmd_params_free(free_arg);
}

static void event_whois(IRC_SERVER_REC *server, const char *data,
			const char *nick, const char *addr)
{
	server->whois_found = TRUE;
	signal_emit("event 311", 4, server, data, nick, addr);
}

static void sig_whois_not_found(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 2, NULL, &nick);
	irc_send_cmdv(server, "WHOWAS %s 1", nick);

	server->whowas_found = FALSE;
	server_redirect_event(SERVER(server), nick, 1,
			      "event 369", "whowas event end", 1,
			      "event 314", "whowas event", 1,
			      "event 406", "event empty", 1, NULL);
	g_free(params);
}

static void event_whowas(IRC_SERVER_REC *server, const char *data, const char *nick, const char *addr)
{
	server->whowas_found = TRUE;
	signal_emit("event 314", 4, server, data, nick, addr);
}

/* SYNTAX: WHOWAS [<nicks> [<count>]] */
static void cmd_whowas(const char *data, IRC_SERVER_REC *server)
{
	char *nicks, *count;
	void *free_arg;
	int free_nick;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2, &nicks, &count))
		return;
	if (*nicks == '\0') nicks = server->nick;

	server->whowas_found = FALSE;
	irc_send_cmdv(server, *count == '\0' ? "WHOWAS %s" :
		      "WHOWAS %s %s", nicks, count);

	nicks = get_redirect_nicklist(nicks, &free_nick);
	server_redirect_event(SERVER(server), nicks, 1,
			      "event 369", "event 369", 1,
			      "event 314", "whowas event", 1, NULL);
	if (free_nick) g_free(nicks);
	cmd_params_free(free_arg);
}

/* SYNTAX: PING <nicks> */
static void cmd_ping(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
	GTimeVal tv;
        char *str;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (*data == '\0' || strcmp(data, "*") == 0) {
		if (!IS_IRC_ITEM(item))
                        cmd_return_error(CMDERR_NOT_JOINED);

		data = item->name;
	}

	g_get_current_time(&tv);

	str = g_strdup_printf("%s PING %ld %ld", data, tv.tv_sec, tv.tv_usec);
	signal_emit("command ctcp", 3, str, server, item);
	g_free(str);
}

static void server_send_away(IRC_SERVER_REC *server, const char *reason)
{
	if (!IS_IRC_SERVER(server))
		return;

	g_free_not_null(server->away_reason);
	server->away_reason = g_strdup(reason);

	irc_send_cmdv(server, "AWAY :%s", reason);
}

/* SYNTAX: AWAY [-one | -all] [<reason>] */
static void cmd_away(const char *data, IRC_SERVER_REC *server)
{
	GHashTable *optlist;
	char *reason;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 1 | PARAM_FLAG_OPTIONS |
			    PARAM_FLAG_GETREST, "away", &optlist, &reason)) return;

	if (g_hash_table_lookup(optlist, "one") != NULL)
		server_send_away(server, reason);
	else
		g_slist_foreach(servers, (GFunc) server_send_away, reason);

	cmd_params_free(free_arg);
}

/* SYNTAX: DEOP <nicks> */
static void cmd_deop(const char *data, IRC_SERVER_REC *server)
{
	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (*data == '\0')
		irc_send_cmdv(server, "MODE %s -o", server->nick);
}

/* SYNTAX: SCONNECT <new server> [[<port>] <existing server>] */
static void cmd_sconnect(const char *data, IRC_SERVER_REC *server)
{
	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);
	if (*data == '\0') cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	irc_send_cmdv(server, "CONNECT %s", data);
}

/* SYNTAX: QUOTE <data> */
static void cmd_quote(const char *data, IRC_SERVER_REC *server)
{
	g_return_if_fail(data != NULL);
	if (server == NULL || !IS_IRC_SERVER(server))
		cmd_return_error(CMDERR_NOT_CONNECTED);

	irc_send_cmd(server, data);
}

static void cmd_wall_hash(gpointer key, NICK_REC *nick, GSList **nicks)
{
	if (nick->op) *nicks = g_slist_append(*nicks, nick);
}

/* SYNTAX: WAIT [-<server tag>] <milliseconds> */
static void cmd_wait(const char *data, IRC_SERVER_REC *server)
{
	GHashTable *optlist;
	char *msecs;
	void *free_arg;
	int n;

	g_return_if_fail(data != NULL);
	if (!IS_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 1 | PARAM_FLAG_OPTIONS |
			    PARAM_FLAG_UNKNOWN_OPTIONS | PARAM_FLAG_GETREST,
			    NULL, &optlist, &msecs))
		return;

	if (*msecs == '\0')
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	/* -<server tag> */
	server = IRC_SERVER(cmd_options_get_server(NULL, optlist,
						   SERVER(server)));

	n = atoi(msecs);
	if (server != NULL && n > 0) {
		g_get_current_time(&server->wait_cmd);
		server->wait_cmd.tv_sec += n/1000;
		server->wait_cmd.tv_usec += n%1000;
		if (server->wait_cmd.tv_usec >= 1000) {
			server->wait_cmd.tv_sec++;
			server->wait_cmd.tv_usec -= 1000;
		}
	}
	cmd_params_free(free_arg);
}

/* SYNTAX: WALL [<channel>] <message> */
static void cmd_wall(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
	char *channame, *msg, *args;
	void *free_arg;
	IRC_CHANNEL_REC *chanrec;
	GSList *tmp, *nicks;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTCHAN |
			    PARAM_FLAG_GETREST, item, &channame, &msg))
		return;
	if (*msg == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	chanrec = irc_channel_find(server, channame);
	if (chanrec == NULL) cmd_param_error(CMDERR_CHAN_NOT_FOUND);

	/* send notice to all ops */
	nicks = NULL;
	g_hash_table_foreach(chanrec->nicks, (GHFunc) cmd_wall_hash, &nicks);

	args = g_strconcat(chanrec->name, " ", msg, NULL);
	msg = parse_special_string(settings_get_str("wall_format"),
				   SERVER(server), item, args, NULL, 0);
	g_free(args);

	for (tmp = nicks; tmp != NULL; tmp = tmp->next) {
		NICK_REC *rec = tmp->data;

		if (rec != chanrec->ownnick)
			irc_send_cmdv(server, "NOTICE %s :%s", rec->nick, msg);
	}
	g_free(msg);
	g_slist_free(nicks);

	cmd_params_free(free_arg);
}

/* SYNTAX: CYCLE [<channel>] [<message>] */
static void cmd_cycle(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
	IRC_CHANNEL_REC *chanrec;
	char *channame, *msg;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTCHAN, item, &channame, &msg))
		return;
	if (*channame == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	chanrec = irc_channel_find(server, channame);
	if (chanrec == NULL) cmd_param_error(CMDERR_CHAN_NOT_FOUND);

	irc_send_cmdv(server, *msg == '\0' ? "PART %s" : "PART %s :%s",
		      channame, msg);
	irc_send_cmdv(server, chanrec->key == NULL ? "JOIN %s" : "JOIN %s %s",
		      channame, chanrec->key);

	cmd_params_free(free_arg);
}

/* SYNTAX: KICKBAN [<channel>] <nicks> <reason> */
static void cmd_kickban(const char *data, IRC_SERVER_REC *server,
			WI_ITEM_REC *item)
{
        IRC_CHANNEL_REC *chanrec;
	char *channel, *nicks, *reason, *str;
        char **nicklist, *spacenicks;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 3 | PARAM_FLAG_OPTCHAN | PARAM_FLAG_GETREST,
			    item, &channel, &nicks, &reason))
		return;

	if (*channel == '\0' || *nicks == '\0')
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	chanrec = irc_channel_find(server, channel);
	if (chanrec == NULL)
		cmd_param_error(CMDERR_CHAN_NOT_FOUND);
	if (!chanrec->wholist)
		cmd_param_error(CMDERR_CHAN_NOT_SYNCED);

	nicklist = g_strsplit(nicks, ",", -1);
        spacenicks = g_strjoinv(" ", nicklist);
	g_strfreev(nicklist);

	str = g_strdup_printf("%s %s", chanrec->name, spacenicks);
	signal_emit("command ban", 3, str, server, channel);
	g_free(str);

	str = g_strdup_printf("%s %s %s", chanrec->name, nicks, reason);
	signal_emit("command kick", 3, str, server, channel);
	g_free(str);

        g_free(spacenicks);
	cmd_params_free(free_arg);
}

static void knockout_destroy(IRC_SERVER_REC *server, KNOCKOUT_REC *rec)
{
	server->knockoutlist = g_slist_remove(server->knockoutlist, rec);
	g_free(rec->ban);
	g_free(rec);
}

/* timeout function: knockout */
static void knockout_timeout_server(IRC_SERVER_REC *server)
{
	GSList *tmp, *next;
	time_t t;

	g_return_if_fail(server != NULL);

	if (!IS_IRC_SERVER(server))
		return;

	t = server->knockout_lastcheck == 0 ? 0 :
		time(NULL)-server->knockout_lastcheck;
	server->knockout_lastcheck = time(NULL);

	for (tmp = server->knockoutlist; tmp != NULL; tmp = next) {
		KNOCKOUT_REC *rec = tmp->data;

		next = tmp->next;
		if (rec->timeleft > t)
			rec->timeleft -= t;
		else {
			/* timeout, unban. */
			ban_remove(rec->channel, rec->ban);
			knockout_destroy(server, rec);
		}
	}
}

static int knockout_timeout(void)
{
	g_slist_foreach(servers, (GFunc) knockout_timeout_server, NULL);
	return 1;
}

/* SYNTAX: KNOCKOUT [<seconds>] <nicks> <reason> */
static void cmd_knockout(const char *data, IRC_SERVER_REC *server,
			 IRC_CHANNEL_REC *channel)
{
	KNOCKOUT_REC *rec;
	char *nicks, *reason, *timeoutstr, *str;
        char **nicklist, *spacenicks, *banmasks;
	void *free_arg;
	int timeleft;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_CHANNEL(channel))
		cmd_return_error(CMDERR_NOT_JOINED);
	if (!channel->wholist)
		cmd_return_error(CMDERR_CHAN_NOT_SYNCED);

	if (is_numeric(data, ' ')) {
		/* first argument is the timeout */
		if (!cmd_get_params(data, &free_arg, 3 | PARAM_FLAG_GETREST,
				    &timeoutstr, &nicks, &reason))
                        return;
		timeleft = atoi(timeoutstr);
	} else {
		if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST,
				    &nicks, &reason))
			return;
                timeleft = 0;
	}

	if (timeleft == 0) timeleft = settings_get_int("knockout_time");
	if (*nicks == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	nicklist = g_strsplit(nicks, ",", -1);
        spacenicks = g_strjoinv(" ", nicklist);
	g_strfreev(nicklist);

	banmasks = ban_get_masks(channel, spacenicks);
	g_free(spacenicks);

	if (*banmasks != '\0') {
		str = g_strdup_printf("%s %s", channel->name, banmasks);
		signal_emit("command ban", 3, str, server, channel);
		g_free(str);
	}

	str = g_strdup_printf("%s %s %s", channel->name, nicks, reason);
	signal_emit("command kick", 3, str, server, channel);
	g_free(str);

	if (*banmasks == '\0')
		g_free(banmasks);
	else {
		/* create knockout record */
		rec = g_new(KNOCKOUT_REC, 1);
		rec->timeleft = timeleft;
		rec->channel = channel;
		rec->ban = banmasks;

		server->knockoutlist = g_slist_append(server->knockoutlist, rec);
	}

	cmd_params_free(free_arg);
}

/* destroy all knockouts in server */
static void sig_server_disconnected(IRC_SERVER_REC *server)
{
	g_return_if_fail(server != NULL);

	if (!IS_IRC_SERVER(server))
		return;

	while (server->knockoutlist != NULL)
		knockout_destroy(server, server->knockoutlist->data);
}

/* destroy all knockouts in channel */
static void sig_channel_destroyed(IRC_CHANNEL_REC *channel)
{
	GSList *tmp, *next;

	if (!IS_IRC_CHANNEL(channel) || !IS_IRC_SERVER(channel->server))
		return;

	for (tmp = channel->server->knockoutlist; tmp != NULL; tmp = next) {
		KNOCKOUT_REC *rec = tmp->data;

		next = tmp->next;
		if (rec->channel == channel)
			knockout_destroy(channel->server, rec);
	}
}

/* SYNTAX: OPER [<nick> [<password>]] */
static void cmd_oper(const char *data, IRC_SERVER_REC *server)
{
	char *nick, *password;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

        /* asking for password is handled by fe-common */
	if (!cmd_get_params(data, &free_arg, 2, &nick, &password))
		return;
        if (*password == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	irc_send_cmdv(server, "OPER %s %s", nick, password);
	cmd_params_free(free_arg);
}

/* SYNTAX: UNSILENCE <nick!user@host> */
static void cmd_unsilence(const char *data, IRC_SERVER_REC *server)
{
	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (*data == '\0') 
		cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	irc_send_cmdv(server, "SILENCE -%s", data);
}

static void command_self(const char *data, IRC_SERVER_REC *server)
{
	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	irc_send_cmdv(server, *data == '\0' ? "%s" : "%s %s", current_command, data);
}

static void command_1self(const char *data, IRC_SERVER_REC *server)
{
	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);
	if (*data == '\0') cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	irc_send_cmdv(server, "%s :%s", current_command, data);
}

static void command_2self(const char *data, IRC_SERVER_REC *server)
{
	char *target, *text;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (!IS_IRC_SERVER(server) || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &target, &text))
		return;
	if (*target == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	irc_send_cmdv(server, "%s %s :%s", current_command, target, text);
	cmd_params_free(free_arg);
}

static void sig_connected(IRC_SERVER_REC *server)
{
	g_return_if_fail(server != NULL);

	/* FIXME: these two aren't probably needed? this whole redirection
	   thing might need some rethinking :) */
	/* WHOIS */
	/*server_redirect_init(SERVER(server), "", 2,
			     "event 318", "event 402", "event 401",
			     "event 301", "event 311", "event 312", "event 313",
			     "event 317", "event 319", NULL);*/

	/* NICK */
	/*server_redirect_init(SERVER(server), "", 5,
			     "event nick", "event 433", "event 437",
			     "event 432", "event 438", NULL);*/

	/* problem (doesn't really apply currently since there's no GUI):

	   second argument of server_redirect_init() is the command that
	   generates the redirection automatically when it's called, but the
	   command handler doesn't really know about the redirection itself.

	   every time the command is called, this redirection is generated.
	   this is a problem if the redirection is wanted sometimes but not
	   always. for example /WHO #channel could create a window with a
	   list of people in channel redirecting WHO's events to it's own use,
	   but /WHO -nogui #channel would use the default WHO handler which
	   doesn't know anything about redirection. with GUI /WHO the
	   redirection would be done twice then..

	   so the kludgy workaround currently is this: make the default
	   handler handle the redirection always.. when default WHO/LIST
	   handler is called, they call
	   server_redirect_default("bogus command who") or ..list..

	   this is really a problem if some script/plugin wants to override
           some default command to use redirections.. */
	server_redirect_init(SERVER(server), "bogus command who", 2, "event 401", "event 315", "event 352", NULL);
	server_redirect_init(SERVER(server), "bogus command list", 1, "event 321", "event 322", "event 323", NULL);
}

void irc_commands_init(void)
{
	tmpstr = g_string_new(NULL);

	settings_add_str("misc", "quit_message", "leaving");
	settings_add_str("misc", "part_message", "");
	settings_add_int("misc", "knockout_time", 300);
	settings_add_str("misc", "wall_format", "[Wall/$0] $1-");

	knockout_tag = g_timeout_add(KNOCKOUT_TIMECHECK, (GSourceFunc) knockout_timeout, NULL);

	signal_add("server connected", (SIGNAL_FUNC) sig_connected);
	command_bind("server", NULL, (SIGNAL_FUNC) cmd_server);
	command_bind("connect", NULL, (SIGNAL_FUNC) cmd_connect);
	command_bind("notice", NULL, (SIGNAL_FUNC) cmd_notice);
	command_bind("ctcp", NULL, (SIGNAL_FUNC) cmd_ctcp);
	command_bind("nctcp", NULL, (SIGNAL_FUNC) cmd_nctcp);
	command_bind("part", NULL, (SIGNAL_FUNC) cmd_part);
	command_bind("kick", NULL, (SIGNAL_FUNC) cmd_kick);
	command_bind("topic", NULL, (SIGNAL_FUNC) cmd_topic);
	command_bind("invite", NULL, (SIGNAL_FUNC) cmd_invite);
	command_bind("list", NULL, (SIGNAL_FUNC) cmd_list);
	command_bind("who", NULL, (SIGNAL_FUNC) cmd_who);
	command_bind("names", NULL, (SIGNAL_FUNC) cmd_names);
	command_bind("nick", NULL, (SIGNAL_FUNC) cmd_nick);
	/* SYNTAX: NOTE <command> [&<password>] [+|-<flags>] [<arguments>] */
	command_bind("note", NULL, (SIGNAL_FUNC) command_self);
	command_bind("whois", NULL, (SIGNAL_FUNC) cmd_whois);
	command_bind("whowas", NULL, (SIGNAL_FUNC) cmd_whowas);
	command_bind("ping", NULL, (SIGNAL_FUNC) cmd_ping);
	/* SYNTAX: KILL <nick> <reason> */
	command_bind("kill", NULL, (SIGNAL_FUNC) command_2self);
	command_bind("away", NULL, (SIGNAL_FUNC) cmd_away);
	/* SYNTAX: ISON <nicks> */
	command_bind("ison", NULL, (SIGNAL_FUNC) command_1self);
	/* SYNTAX: ADMIN [<server>|<nickname>] */
	command_bind("admin", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: INFO [<server>] */
	command_bind("info", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: LINKS [[<server>] <mask>] */
	command_bind("links", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: LUSERS [<server mask> [<remote server>]] */
	command_bind("lusers", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: MAP */
	command_bind("map", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: MOTD [<server>|<nick>] */
	command_bind("motd", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: REHASH */
	command_bind("rehash", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: STATS <type> [<server>] */
	command_bind("stats", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: TIME [<server>|<nick>] */
	command_bind("time", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: TRACE [<server>|<nick>] */
	command_bind("trace", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: VERSION [<server>|<nick>] */
	command_bind("version", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: SERVLIST [<server mask>] */
	command_bind("servlist", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: SILENCE [[+|-]<nick!user@host>]
	           SILENCE [<nick>] */
	command_bind("silence", NULL, (SIGNAL_FUNC) command_self);
	command_bind("unsilence", NULL, (SIGNAL_FUNC) cmd_unsilence);
	command_bind("sconnect", NULL, (SIGNAL_FUNC) cmd_sconnect);
	/* SYNTAX: SQUERY <service> [<commands>] */
	command_bind("squery", NULL, (SIGNAL_FUNC) command_2self);
	command_bind("deop", NULL, (SIGNAL_FUNC) cmd_deop);
	/* SYNTAX: DIE */
	command_bind("die", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: HASH */
	command_bind("hash", NULL, (SIGNAL_FUNC) command_self);
	command_bind("oper", NULL, (SIGNAL_FUNC) cmd_oper);
	/* SYNTAX: RESTART */
	command_bind("restart", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: RPING <server> */
	command_bind("rping", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: SQUIT <server>|<mask> <reason> */
	command_bind("squit", NULL, (SIGNAL_FUNC) command_2self);
	/* SYNTAX: UPING <server> */
	command_bind("uping", NULL, (SIGNAL_FUNC) command_self);
	/* SYNTAX: USERHOST <nicks> */
	command_bind("userhost", NULL, (SIGNAL_FUNC) command_self);
	command_bind("quote", NULL, (SIGNAL_FUNC) cmd_quote);
	command_bind("wall", NULL, (SIGNAL_FUNC) cmd_wall);
	command_bind("wait", NULL, (SIGNAL_FUNC) cmd_wait);
	/* SYNTAX: WALLOPS <message> */
	command_bind("wallops", NULL, (SIGNAL_FUNC) command_1self);
	/* SYNTAX: WALLCHOPS <channel> <message> */
	command_bind("wallchops", NULL, (SIGNAL_FUNC) command_2self);
	command_bind("cycle", NULL, (SIGNAL_FUNC) cmd_cycle);
	command_bind("kickban", NULL, (SIGNAL_FUNC) cmd_kickban);
	command_bind("knockout", NULL, (SIGNAL_FUNC) cmd_knockout);

	signal_add("channel destroyed", (SIGNAL_FUNC) sig_channel_destroyed);
	signal_add("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);
	signal_add("nickchange over", (SIGNAL_FUNC) sig_nickchange_over);
	signal_add("whois not found", (SIGNAL_FUNC) sig_whois_not_found);
	signal_add("whois event", (SIGNAL_FUNC) event_whois);
	signal_add("whowas event", (SIGNAL_FUNC) event_whowas);

	command_set_options("connect", "+ircnet +host");
	command_set_options("topic", "delete");
	command_set_options("list", "yes");
	command_set_options("away", "one all");
	command_set_options("whois", "yes");
}

void irc_commands_deinit(void)
{
	g_source_remove(knockout_tag);

	signal_remove("server connected", (SIGNAL_FUNC) sig_connected);
	command_unbind("server", (SIGNAL_FUNC) cmd_server);
	command_unbind("connect", (SIGNAL_FUNC) cmd_connect);
	command_unbind("notice", (SIGNAL_FUNC) cmd_notice);
	command_unbind("ctcp", (SIGNAL_FUNC) cmd_ctcp);
	command_unbind("nctcp", (SIGNAL_FUNC) cmd_nctcp);
	command_unbind("part", (SIGNAL_FUNC) cmd_part);
	command_unbind("kick", (SIGNAL_FUNC) cmd_kick);
	command_unbind("topic", (SIGNAL_FUNC) cmd_topic);
	command_unbind("invite", (SIGNAL_FUNC) cmd_invite);
	command_unbind("list", (SIGNAL_FUNC) cmd_list);
	command_unbind("who", (SIGNAL_FUNC) cmd_who);
	command_unbind("names", (SIGNAL_FUNC) cmd_names);
	command_unbind("nick", (SIGNAL_FUNC) cmd_nick);
	command_unbind("note", (SIGNAL_FUNC) command_self);
	command_unbind("whois", (SIGNAL_FUNC) cmd_whois);
	command_unbind("whowas", (SIGNAL_FUNC) cmd_whowas);
	command_unbind("ping", (SIGNAL_FUNC) cmd_ping);
	command_unbind("kill", (SIGNAL_FUNC) command_2self);
	command_unbind("away", (SIGNAL_FUNC) cmd_away);
	command_unbind("ison", (SIGNAL_FUNC) command_1self);
	command_unbind("admin", (SIGNAL_FUNC) command_self);
	command_unbind("info", (SIGNAL_FUNC) command_self);
	command_unbind("links", (SIGNAL_FUNC) command_self);
	command_unbind("lusers", (SIGNAL_FUNC) command_self);
	command_unbind("map", (SIGNAL_FUNC) command_self);
	command_unbind("motd", (SIGNAL_FUNC) command_self);
	command_unbind("rehash", (SIGNAL_FUNC) command_self);
	command_unbind("stats", (SIGNAL_FUNC) command_self);
	command_unbind("time", (SIGNAL_FUNC) command_self);
	command_unbind("trace", (SIGNAL_FUNC) command_self);
	command_unbind("version", (SIGNAL_FUNC) command_self);
	command_unbind("servlist", (SIGNAL_FUNC) command_self);
	command_unbind("silence", (SIGNAL_FUNC) command_self);
	command_unbind("unsilence", (SIGNAL_FUNC) cmd_unsilence);
	command_unbind("sconnect", (SIGNAL_FUNC) cmd_sconnect);
	command_unbind("squery", (SIGNAL_FUNC) command_2self);
	command_unbind("deop", (SIGNAL_FUNC) cmd_deop);
	command_unbind("die", (SIGNAL_FUNC) command_self);
	command_unbind("hash", (SIGNAL_FUNC) command_self);
	command_unbind("oper", (SIGNAL_FUNC) cmd_oper);
	command_unbind("restart", (SIGNAL_FUNC) command_self);
	command_unbind("rping", (SIGNAL_FUNC) command_self);
	command_unbind("squit", (SIGNAL_FUNC) command_2self);
	command_unbind("uping", (SIGNAL_FUNC) command_self);
	command_unbind("userhost", (SIGNAL_FUNC) command_self);
	command_unbind("quote", (SIGNAL_FUNC) cmd_quote);
	command_unbind("wall", (SIGNAL_FUNC) cmd_wall);
	command_unbind("wait", (SIGNAL_FUNC) cmd_wait);
	command_unbind("wallops", (SIGNAL_FUNC) command_1self);
	command_unbind("wallchops", (SIGNAL_FUNC) command_2self);
	command_unbind("cycle", (SIGNAL_FUNC) cmd_cycle);
	command_unbind("kickban", (SIGNAL_FUNC) cmd_kickban);
	command_unbind("knockout", (SIGNAL_FUNC) cmd_knockout);
	signal_remove("channel destroyed", (SIGNAL_FUNC) sig_channel_destroyed);
	signal_remove("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);
	signal_remove("nickchange over", (SIGNAL_FUNC) sig_nickchange_over);
	signal_remove("whois not found", (SIGNAL_FUNC) sig_whois_not_found);
	signal_remove("whois event", (SIGNAL_FUNC) event_whois);
	signal_remove("whowas event", (SIGNAL_FUNC) event_whowas);

	g_string_free(tmpstr, TRUE);
}
