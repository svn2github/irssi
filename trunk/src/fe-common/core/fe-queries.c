/*
 fe-queries.c : irssi

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
#include "module-formats.h"
#include "modules.h"
#include "signals.h"
#include "commands.h"
#include "levels.h"
#include "settings.h"

#include "chat-protocols.h"
#include "queries.h"

#include "fe-windows.h"
#include "window-items.h"
#include "printtext.h"

static int queryclose_tag, query_auto_close, querycreate_level;

/* Return query where to put the private message. */
QUERY_REC *privmsg_get_query(SERVER_REC *server, const char *nick,
			     int own, int level)
{
	QUERY_REC *query;

	g_return_val_if_fail(IS_SERVER(server), NULL);
        g_return_val_if_fail(nick != NULL, NULL);

	query = query_find(server, nick);
	if (query == NULL && (querycreate_level & level) != 0 &&
	    (!own || settings_get_bool("autocreate_own_query"))) {
		query = CHAT_PROTOCOL(server)->
			query_create(server->tag, nick, TRUE);
	}

	return query;
}

static void signal_query_created(QUERY_REC *query, gpointer automatic)
{
	g_return_if_fail(IS_QUERY(query));

	if (window_item_window(query) == NULL) {
		window_item_create((WI_ITEM_REC *) query,
				   GPOINTER_TO_INT(automatic));
	}

	printformat(query->server, query->name, MSGLEVEL_CLIENTNOTICE,
		    TXT_QUERY_START, query->name);
}

static void signal_query_created_curwin(QUERY_REC *query)
{
	g_return_if_fail(IS_QUERY(query));

	window_item_add(active_win, (WI_ITEM_REC *) query, FALSE);
}

static void signal_query_destroyed(QUERY_REC *query)
{
	WINDOW_REC *window;

	g_return_if_fail(IS_QUERY(query));

	window = window_item_window((WI_ITEM_REC *) query);
	if (window != NULL) {
		printformat(query->server, query->name, MSGLEVEL_CLIENTNOTICE,
			    TXT_QUERY_STOP, query->name);

		window_item_destroy((WI_ITEM_REC *) query);

		if (!query->unwanted)
			window_auto_destroy(window);
	}
}

static void signal_query_server_changed(QUERY_REC *query)
{
	WINDOW_REC *window;

	g_return_if_fail(query != NULL);

	window = window_item_window((WI_ITEM_REC *) query);
	if (window->active == (WI_ITEM_REC *) query)
		window_change_server(window, query->server);
}

static void signal_query_nick_changed(QUERY_REC *query, const char *oldnick)
{
	g_return_if_fail(query != NULL);

	printformat(query->server, query->name, MSGLEVEL_NICKS,
		    TXT_NICK_CHANGED, oldnick, query->name, query->name);

	signal_emit("window item changed", 2,
		    window_item_window((WI_ITEM_REC *) query), query);
}

static void signal_window_item_server_changed(WINDOW_REC *window,
					      QUERY_REC *query)
{
	if (IS_QUERY(query)) {
		g_free_and_null(query->server_tag);
                if (query->server != NULL)
			query->server_tag = g_strdup(query->server->tag);
	}
}

static void sig_server_connected(SERVER_REC *server)
{
	GSList *tmp;

	if (!IS_SERVER(server))
		return;

	/* check if there's any queries without server */
	for (tmp = queries; tmp != NULL; tmp = tmp->next) {
		QUERY_REC *rec = tmp->data;

		if (rec->server == NULL &&
		    (rec->server_tag == NULL ||
		     g_strcasecmp(rec->server_tag, server->tag) == 0)) {
			window_item_change_server((WI_ITEM_REC *) rec, server);
			server->queries = g_slist_append(server->queries, rec);
		}
	}
}

static void cmd_window_server(const char *data)
{
	SERVER_REC *server;
        QUERY_REC *query;

	g_return_if_fail(data != NULL);

	server = server_find_tag(data);
        query = QUERY(active_win->active);
	if (server == NULL || query == NULL)
		return;

	/* /WINDOW SERVER used in a query window */
	query_change_server(query, server);
	printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		    TXT_QUERY_SERVER_CHANGED,
		    query->name, server->tag);
	signal_stop();
}

/* SYNTAX: UNQUERY [<nick>] */
static void cmd_unquery(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	QUERY_REC *query;

	g_return_if_fail(data != NULL);

	if (*data == '\0') {
		/* remove current query */
		query = QUERY(item);
		if (query == NULL) return;
	} else {
		query = query_find(server, data);
		if (query == NULL) {
			printformat(server, NULL, MSGLEVEL_CLIENTERROR,
				    TXT_NO_QUERY, data);
			return;
		}
	}

	query_destroy(query);
}

/* SYNTAX: QUERY [-window] [-<server tag>] <nick> [<message>] */
static void cmd_query(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	GHashTable *optlist;
	QUERY_REC *query;
	char *nick, *msg;
	void *free_arg;

	g_return_if_fail(data != NULL);

	if (*data == '\0') {
		/* remove current query */
		cmd_unquery("", server, item);
		return;
	}

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST |
			    PARAM_FLAG_OPTIONS | PARAM_FLAG_UNKNOWN_OPTIONS,
			    "query", &optlist, &nick, &msg))
		return;
	if (*nick == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	server = cmd_options_get_server("query", optlist, server);
	if (server == NULL) {
		cmd_params_free(free_arg);
                return;
	}

	if (*nick != '=' && (server == NULL || !server->connected))
		cmd_param_error(CMDERR_NOT_CONNECTED);

	if (g_hash_table_lookup(optlist, "window") != NULL) {
		signal_add("query created",
			   (SIGNAL_FUNC) signal_query_created_curwin);
	}

	query = query_find(server, nick);
	if (query == NULL)
		CHAT_PROTOCOL(server)->query_create(server->tag, nick, FALSE);
	else {
		/* query already exists */
		WINDOW_REC *window = window_item_window(query);

		if (window == active_win) {
                        /* query is in active window, set it active */
			window_item_set_active(active_win,
					       (WI_ITEM_REC *) query);
		} else {
			/* notify user how to move the query to active
			   window. this was used to be done automatically
			   but it just confused everyone who did it
			   accidentally */
			printformat_window(active_win, MSGLEVEL_CLIENTNOTICE,
					   TXT_QUERY_MOVE_NOTIFY, query->name,
					   window->refnum);
		}
	}

	if (g_hash_table_lookup(optlist, "window") != NULL) {
		signal_remove("query created",
			      (SIGNAL_FUNC) signal_query_created_curwin);
	}

	if (*msg != '\0') {
		/* FIXME: we'll need some function that does both
		   of these. and separate the , and . target handling
		   from own_private messagge.. */
		server->send_message(server, nick, msg);

		signal_emit("message own_private", 4,
			    server, msg, nick, nick);
	}

	cmd_params_free(free_arg);
}

static int window_has_query(WINDOW_REC *window)
{
	GSList *tmp;

	g_return_val_if_fail(window != NULL, FALSE);

	for (tmp = window->items; tmp != NULL; tmp = tmp->next) {
		if (IS_QUERY(tmp->data))
			return TRUE;
	}

	return FALSE;
}

static void sig_window_changed(WINDOW_REC *window, WINDOW_REC *old_window)
{
	if (query_auto_close <= 0)
		return;

	/* reset the window's last_line timestamp so that query doesn't get
	   closed immediately after switched to the window, or after changed
	   to some other window from it */
	if (window != NULL && window_has_query(window))
		window->last_line = time(NULL);
	if (old_window != NULL && window_has_query(old_window))
		old_window->last_line = time(NULL);
}

static int sig_query_autoclose(void)
{
	WINDOW_REC *window;
	GSList *tmp, *next;
	time_t now;

	now = time(NULL);
	for (tmp = queries; tmp != NULL; tmp = next) {
		QUERY_REC *rec = tmp->data;

		next = tmp->next;
		window = window_item_window((WI_ITEM_REC *) rec);
		if (window != active_win && rec->data_level == 0 &&
		    now-window->last_line > query_auto_close)
			query_destroy(rec);
	}
        return 1;
}

static void sig_message_private(SERVER_REC *server, const char *msg,
				const char *nick, const char *address)
{
	/* create query window if needed */
	privmsg_get_query(server, nick, FALSE, MSGLEVEL_MSGS);
}

static void read_settings(void)
{
	querycreate_level = level2bits(settings_get_str("autocreate_query_level"));
	query_auto_close = settings_get_int("autoclose_query");
	if (query_auto_close > 0 && queryclose_tag == -1)
		queryclose_tag = g_timeout_add(5000, (GSourceFunc) sig_query_autoclose, NULL);
	else if (query_auto_close <= 0 && queryclose_tag != -1) {
		g_source_remove(queryclose_tag);
		queryclose_tag = -1;
	}
}

void fe_queries_init(void)
{
	settings_add_str("lookandfeel", "autocreate_query_level", "MSGS DCCMSGS");
	settings_add_bool("lookandfeel", "autocreate_own_query", TRUE);
	settings_add_int("lookandfeel", "autoclose_query", 0);

	queryclose_tag = -1;
	read_settings();

	signal_add("query created", (SIGNAL_FUNC) signal_query_created);
	signal_add("query destroyed", (SIGNAL_FUNC) signal_query_destroyed);
	signal_add("query server changed", (SIGNAL_FUNC) signal_query_server_changed);
	signal_add("query nick changed", (SIGNAL_FUNC) signal_query_nick_changed);
        signal_add("window item server changed", (SIGNAL_FUNC) signal_window_item_server_changed);
	signal_add("server connected", (SIGNAL_FUNC) sig_server_connected);
	signal_add("window changed", (SIGNAL_FUNC) sig_window_changed);
	signal_add_first("message private", (SIGNAL_FUNC) sig_message_private);
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);

	command_bind("query", NULL, (SIGNAL_FUNC) cmd_query);
	command_bind("unquery", NULL, (SIGNAL_FUNC) cmd_unquery);
	command_bind("window server", NULL, (SIGNAL_FUNC) cmd_window_server);

	command_set_options("query", "window");
}

void fe_queries_deinit(void)
{
	if (queryclose_tag != -1) g_source_remove(queryclose_tag);

	signal_remove("query created", (SIGNAL_FUNC) signal_query_created);
	signal_remove("query destroyed", (SIGNAL_FUNC) signal_query_destroyed);
	signal_remove("query server changed", (SIGNAL_FUNC) signal_query_server_changed);
	signal_remove("query nick changed", (SIGNAL_FUNC) signal_query_nick_changed);
        signal_remove("window item server changed", (SIGNAL_FUNC) signal_window_item_server_changed);
	signal_remove("server connected", (SIGNAL_FUNC) sig_server_connected);
	signal_remove("window changed", (SIGNAL_FUNC) sig_window_changed);
	signal_remove("message private", (SIGNAL_FUNC) sig_message_private);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);

	command_unbind("query", (SIGNAL_FUNC) cmd_query);
	command_unbind("unquery", (SIGNAL_FUNC) cmd_unquery);
	command_unbind("window server", (SIGNAL_FUNC) cmd_window_server);
}
