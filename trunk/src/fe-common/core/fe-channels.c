/*
 fe-channels.c : irssi

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
#include "misc.h"
#include "settings.h"

#include "channels.h"
#include "channels-setup.h"
#include "nicklist.h"

#include "fe-windows.h"
#include "window-items.h"
#include "printtext.h"

static void signal_channel_created(CHANNEL_REC *channel, void *automatic)
{
	if (window_item_find(channel->server, channel->name) == NULL) {
		window_item_create((WI_ITEM_REC *) channel,
				   GPOINTER_TO_INT(automatic));
	}
}

static void signal_channel_created_curwin(CHANNEL_REC *channel)
{
	g_return_if_fail(channel != NULL);

	window_item_add(active_win, (WI_ITEM_REC *) channel, FALSE);
}

static void signal_channel_destroyed(CHANNEL_REC *channel)
{
	WINDOW_REC *window;

	g_return_if_fail(channel != NULL);

	window = window_item_window((WI_ITEM_REC *) channel);
	if (window != NULL) {
		window_item_destroy((WI_ITEM_REC *) channel);

		if (window->items == NULL && windows->next != NULL &&
		    (!channel->joined || channel->left) &&
		    settings_get_bool("autoclose_windows")) {
			window_destroy(window);
		}
	}
}

static void signal_window_item_destroy(WINDOW_REC *window, WI_ITEM_REC *item)
{
	CHANNEL_REC *channel;

	g_return_if_fail(window != NULL);

	channel = CHANNEL(item);
        if (channel != NULL) channel_destroy(channel);
}

static void sig_disconnected(SERVER_REC *server)
{
	WINDOW_REC *window;
	GSList *tmp;

	g_return_if_fail(IS_SERVER(server));

	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_REC *channel = tmp->data;

		window = window_item_window((WI_ITEM_REC *) channel);
		window->waiting_channels =
			g_slist_append(window->waiting_channels, g_strdup_printf("%s %s", server->tag, channel->name));
	}
}

static void signal_window_item_changed(WINDOW_REC *window, WI_ITEM_REC *item)
{
	g_return_if_fail(window != NULL);
	if (item == NULL) return;

	if (g_slist_length(window->items) > 1 && IS_CHANNEL(item)) {
		printformat(item->server, item->name, MSGLEVEL_CLIENTNOTICE,
			    TXT_TALKING_IN, item->name);
                signal_stop();
	}
}

static void cmd_wjoin_pre(const char *data)
{
	GHashTable *optlist;
	char *nick;
	void *free_arg;

	if (!cmd_get_params(data, &free_arg, 1 | PARAM_FLAG_OPTIONS |
			    PARAM_FLAG_UNKNOWN_OPTIONS | PARAM_FLAG_GETREST,
			    "join", &optlist, &nick))
                return;

	if (g_hash_table_lookup(optlist, "window") != NULL) {
		signal_add("channel created",
			   (SIGNAL_FUNC) signal_channel_created_curwin);
        }
	cmd_params_free(free_arg);
}

static void cmd_join(const char *data, SERVER_REC *server)
{
        CHANNEL_REC *channel;

        if (strchr(data, ' ') != NULL || strchr(data, ',') != NULL)
                return;

        channel = channel_find(server, data);
        if (channel != NULL)
                window_item_set_active(active_win, (WI_ITEM_REC *) channel);
}

static void cmd_wjoin_post(const char *data)
{
	GHashTable *optlist;
	char *nick;
	void *free_arg;

	if (!cmd_get_params(data, &free_arg, 1 | PARAM_FLAG_OPTIONS |
			    PARAM_FLAG_UNKNOWN_OPTIONS | PARAM_FLAG_GETREST,
			    "join", &optlist, &nick))
		return;

	if (g_hash_table_lookup(optlist, "window") != NULL) {
		signal_remove("channel created",
			   (SIGNAL_FUNC) signal_channel_created_curwin);
	}
	cmd_params_free(free_arg);
}

static void cmd_channel_list_joined(void)
{
	CHANNEL_REC *channel;
	GString *nicks;
	GSList *nicklist, *tmp, *ntmp;

	if (channels == NULL) {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_NOT_IN_CHANNELS);
		return;
	}

	/* print active channel */
	channel = CHANNEL(active_win->active);
	if (channel != NULL)
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_CURRENT_CHANNEL, channel->name);

	/* print list of all channels, their modes, server tags and nicks */
	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_CHANLIST_HEADER);
	for (tmp = channels; tmp != NULL; tmp = tmp->next) {
		channel = tmp->data;

		nicklist = nicklist_getnicks(channel);
		nicks = g_string_new(NULL);
		for (ntmp = nicklist; ntmp != NULL; ntmp = ntmp->next) {
			NICK_REC *rec = ntmp->data;

			g_string_sprintfa(nicks, "%s ", rec->nick);
		}

		if (nicks->len > 1) g_string_truncate(nicks, nicks->len-1);
		printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_CHANLIST_LINE,
			    channel->name, channel->mode, channel->server->tag, nicks->str);

		g_slist_free(nicklist);
		g_string_free(nicks, TRUE);
	}
}

/* SYNTAX: CHANNEL LIST */
static void cmd_channel_list(void)
{
	GString *str;
	GSList *tmp;

	str = g_string_new(NULL);
	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_CHANSETUP_HEADER);
	for (tmp = setupchannels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_SETUP_REC *rec = tmp->data;

		g_string_truncate(str, 0);
		if (rec->autojoin)
			g_string_append(str, "autojoin, ");
		if (rec->botmasks != NULL && *rec->botmasks != '\0')
			g_string_sprintfa(str, "bots: %s, ", rec->botmasks);
		if (rec->autosendcmd != NULL && *rec->autosendcmd != '\0')
			g_string_sprintfa(str, "botcmd: %s, ", rec->autosendcmd);

		if (str->len > 2) g_string_truncate(str, str->len-2);
		printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_CHANSETUP_LINE,
			    rec->name, rec->chatnet == NULL ? "" : rec->chatnet,
			    rec->password == NULL ? "" : rec->password, str->str);
	}
	g_string_free(str, TRUE);
	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_CHANSETUP_FOOTER);
}

static void cmd_channel(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	if (*data == '\0')
		cmd_channel_list_joined();
	else
		command_runsub("channel", data, server, item);
}

/* SYNTAX: CHANNEL ADD [-auto | -noauto] [-bots <masks>] [-botcmd <command>]
                       <channel> <chatnet> [<password>] */
static void cmd_channel_add(const char *data)
{
	GHashTable *optlist;
	CHANNEL_SETUP_REC *rec;
	char *botarg, *botcmdarg, *chatnet, *channel, *password;
	void *free_arg;

	if (!cmd_get_params(data, &free_arg, 3 | PARAM_FLAG_OPTIONS,
			    "channel add", &optlist, &channel, &chatnet, &password))
		return;

	botarg = g_hash_table_lookup(optlist, "bots");
	botcmdarg = g_hash_table_lookup(optlist, "botcmd");

	if (*chatnet == '\0' || *channel == '\0')
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	rec = channels_setup_find(channel, chatnet);
	if (rec == NULL) {
		rec = g_new0(CHANNEL_SETUP_REC, 1);
		rec->name = g_strdup(channel);
		rec->chatnet = g_strdup(chatnet);
	} else {
		if (g_hash_table_lookup(optlist, "bots")) g_free_and_null(rec->botmasks);
		if (g_hash_table_lookup(optlist, "botcmd")) g_free_and_null(rec->autosendcmd);
		if (*password != '\0') g_free_and_null(rec->password);
	}
	if (g_hash_table_lookup(optlist, "auto")) rec->autojoin = TRUE;
	if (g_hash_table_lookup(optlist, "noauto")) rec->autojoin = FALSE;
	if (botarg != NULL && *botarg != '\0') rec->botmasks = g_strdup(botarg);
	if (botcmdarg != NULL && *botcmdarg != '\0') rec->autosendcmd = g_strdup(botcmdarg);
	if (*password != '\0' && strcmp(password, "-") != 0) rec->password = g_strdup(password);
	channels_setup_create(rec);
	printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_CHANSETUP_ADDED, channel, chatnet);

	cmd_params_free(free_arg);
}

/* SYNTAX: CHANNEL REMOVE <channel> <chatnet> */
static void cmd_channel_remove(const char *data)
{
	CHANNEL_SETUP_REC *rec;
	char *chatnet, *channel;
	void *free_arg;

	if (!cmd_get_params(data, &free_arg, 2, &channel, &chatnet))
		return;
	if (*chatnet == '\0' || *channel == '\0')
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	rec = channels_setup_find(channel, chatnet);
	if (rec == NULL)
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_CHANSETUP_NOT_FOUND, channel, chatnet);
	else {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_CHANSETUP_REMOVED, channel, chatnet);
		channels_setup_destroy(rec);
	}
	cmd_params_free(free_arg);
}

static int get_nick_length(void *data)
{
        return strlen(((NICK_REC *) data)->nick);
}

static void display_sorted_nicks(CHANNEL_REC *channel, GSList *nicklist)
{
        WINDOW_REC *window;
	TEXT_DEST_REC dest;
	GString *str;
	GSList *tmp;
        char *format, *stripped;
	char *linebuf, nickmode[2] = { 0, 0 };
	int *columns, cols, rows, last_col_rows, col, row, max_width;
        int item_extra;

	window = window_find_closest(channel->server, channel->name,
				     MSGLEVEL_CLIENTCRAP);
        max_width = window->width;

        /* get the length of item extra stuff ("[ ] ") */
	format = format_get_text(MODULE_NAME, NULL,
				 channel->server, channel->name,
				 TXT_NAMES_NICK, " ", "");
	stripped = strip_codes(format);
	item_extra = strlen(stripped);
        g_free(stripped);
	g_free(format);

        /* remove width of timestamp from max_width */
	format_create_dest(&dest, channel->server, channel->name,
			   MSGLEVEL_CLIENTCRAP, NULL);
	format = format_get_line_start(current_theme, &dest, time(NULL));
	if (format != NULL) {
		stripped = strip_codes(format);
		max_width -= strlen(stripped);
		g_free(stripped);
		g_free(format);
	}

	/* calculate columns */
	if (settings_get_int("names_max_width") > 0 &&
	    max_width > settings_get_int("names_max_width"))
		max_width = settings_get_int("names_max_width");

	cols = get_max_column_count(nicklist, get_nick_length, max_width,
				    settings_get_int("names_max_columns"),
				    item_extra, 3, &columns, &rows);
	nicklist = columns_sort_list(nicklist, rows);

        /* rows in last column */
	last_col_rows = rows-(cols*rows-g_slist_length(nicklist));
	if (last_col_rows == 0)
                last_col_rows = rows;

	str = g_string_new(NULL);
	linebuf = g_malloc(max_width+1);

        col = 0; row = 0;
	for (tmp = nicklist; tmp != NULL; tmp = tmp->next) {
		NICK_REC *rec = tmp->data;

		nickmode[0] = rec->op ? '@' : rec->voice ? '+' : ' ';

		memset(linebuf, ' ', columns[col]-item_extra);
		linebuf[columns[col]-item_extra] = '\0';
		memcpy(linebuf, rec->nick, strlen(rec->nick));

		format = format_get_text(MODULE_NAME, NULL,
					 channel->server, channel->name,
					 TXT_NAMES_NICK, nickmode, linebuf);
		g_string_append(str, format);
		g_free(format);

		if (++col == cols) {
			printtext(channel->server, channel->name,
				  MSGLEVEL_CLIENTCRAP, "%s", str->str);
			g_string_truncate(str, 0);
			col = 0; row++;

			if (row == last_col_rows)
                                cols--;
		}
	}

	if (str->len != 0) {
		printtext(channel->server, channel->name,
			  MSGLEVEL_CLIENTCRAP, "%s", str->str);
	}

	g_slist_free(nicklist);
	g_string_free(str, TRUE);
	g_free(columns);
	g_free(linebuf);
}

void fe_channels_nicklist(CHANNEL_REC *channel)
{
	NICK_REC *nick;
	GSList *tmp, *nicklist, *sorted;
	int nicks, normal, voices, ops;

	nicks = normal = voices = ops = 0;
	nicklist = nicklist_getnicks(channel);
	sorted = NULL;

	/* sort the nicklist */
	for (tmp = nicklist; tmp != NULL; tmp = tmp->next) {
		nick = tmp->data;

		sorted = g_slist_insert_sorted(sorted, nick, (GCompareFunc) nicklist_compare);
		if (nick->op)
			ops++;
		else if (nick->voice)
			voices++;
		else
			normal++;
		nicks++;
	}
	g_slist_free(nicklist);

	/* display the nicks */
	printformat(channel->server, channel->name,
		    MSGLEVEL_CRAP, TXT_NAMES, channel->name, "");
	display_sorted_nicks(channel, sorted);
	g_slist_free(sorted);

	printformat(channel->server, channel->name,
		    MSGLEVEL_CRAP, TXT_ENDOFNAMES,
		    channel->name, nicks, ops, voices, normal);
}

static void cmd_names(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	CHANNEL_REC *channel;

	g_return_if_fail(data != NULL);

	if (server == NULL || !server->connected)
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (strcmp(data, "*") == 0) {
		if (!IS_CHANNEL(item))
			cmd_return_error(CMDERR_NOT_JOINED);

		data = item->name;
	}

	channel = channel_find(server, data);
	if (channel != NULL) {
		fe_channels_nicklist(channel);
                signal_stop();
	}
}

void fe_channels_init(void)
{
	settings_add_bool("lookandfeel", "autoclose_windows", TRUE);
	settings_add_int("lookandfeel", "names_max_columns", 6);
	settings_add_int("lookandfeel", "names_max_width", 0);

	signal_add("channel created", (SIGNAL_FUNC) signal_channel_created);
	signal_add("channel destroyed", (SIGNAL_FUNC) signal_channel_destroyed);
	signal_add_last("window item destroy", (SIGNAL_FUNC) signal_window_item_destroy);
	signal_add_last("window item changed", (SIGNAL_FUNC) signal_window_item_changed);
	signal_add_last("server disconnected", (SIGNAL_FUNC) sig_disconnected);

	command_bind_first("join", NULL, (SIGNAL_FUNC) cmd_wjoin_pre);
	command_bind("join", NULL, (SIGNAL_FUNC) cmd_join);
	command_bind_last("join", NULL, (SIGNAL_FUNC) cmd_wjoin_post);
	command_bind("channel", NULL, (SIGNAL_FUNC) cmd_channel);
	command_bind("channel add", NULL, (SIGNAL_FUNC) cmd_channel_add);
	command_bind("channel remove", NULL, (SIGNAL_FUNC) cmd_channel_remove);
	command_bind("channel list", NULL, (SIGNAL_FUNC) cmd_channel_list);
	command_bind("names", NULL, (SIGNAL_FUNC) cmd_names);

	command_set_options("channel add", "auto noauto -bots -botcmd");
	command_set_options("join", "window");
}

void fe_channels_deinit(void)
{
	signal_remove("channel created", (SIGNAL_FUNC) signal_channel_created);
	signal_remove("channel destroyed", (SIGNAL_FUNC) signal_channel_destroyed);
	signal_remove("window item destroy", (SIGNAL_FUNC) signal_window_item_destroy);
	signal_remove("window item changed", (SIGNAL_FUNC) signal_window_item_changed);
	signal_remove("server disconnected", (SIGNAL_FUNC) sig_disconnected);

	command_unbind("join", (SIGNAL_FUNC) cmd_wjoin_pre);
	command_unbind("join", (SIGNAL_FUNC) cmd_join);
	command_unbind("join", (SIGNAL_FUNC) cmd_wjoin_post);
	command_unbind("channel", (SIGNAL_FUNC) cmd_channel);
	command_unbind("channel add", (SIGNAL_FUNC) cmd_channel_add);
	command_unbind("channel remove", (SIGNAL_FUNC) cmd_channel_remove);
	command_unbind("channel list", (SIGNAL_FUNC) cmd_channel_list);
	command_unbind("names", (SIGNAL_FUNC) cmd_names);
}
