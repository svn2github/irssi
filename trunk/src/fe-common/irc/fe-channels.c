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

#include "irc.h"
#include "channels.h"
#include "channels-setup.h"
#include "nicklist.h"

#include "windows.h"
#include "window-items.h"

static void signal_channel_created(CHANNEL_REC *channel, gpointer automatic)
{
	if (window_item_find(channel->server, channel->name) == NULL) {
		window_item_create((WI_ITEM_REC *) channel,
				   GPOINTER_TO_INT(automatic));
	}
}

static void signal_channel_created_curwin(CHANNEL_REC *channel)
{
	g_return_if_fail(channel != NULL);

	window_add_item(active_win, (WI_ITEM_REC *) channel, FALSE);
}

static void signal_channel_destroyed(CHANNEL_REC *channel)
{
	WINDOW_REC *window;

	g_return_if_fail(channel != NULL);

	window = window_item_window((WI_ITEM_REC *) channel);
	if (window != NULL) {
		window_remove_item(window, (WI_ITEM_REC *) channel);

		if (windows->next != NULL && (!channel->joined || channel->left) &&
		    settings_get_bool("autoclose_windows")) {
			window_destroy(window);
		}
	}
}

static void signal_window_item_removed(WINDOW_REC *window, WI_ITEM_REC *item)
{
	CHANNEL_REC *channel;

	g_return_if_fail(window != NULL);

	channel = irc_item_channel(item);
        if (channel != NULL) channel_destroy(channel);
}

static void sig_disconnected(IRC_SERVER_REC *server)
{
	WINDOW_REC *window;
	GSList *tmp;

	g_return_if_fail(server != NULL);
	if (!irc_server_check(server))
		return;

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

	if (g_slist_length(window->items) > 1 && irc_item_channel(item)) {
		printformat(item->server, item->name, MSGLEVEL_CLIENTNOTICE,
			    IRCTXT_TALKING_IN, item->name);
                signal_stop();
	}
}

static void cmd_wjoin(const char *data, void *server, WI_ITEM_REC *item)
{
	signal_add("channel created", (SIGNAL_FUNC) signal_channel_created_curwin);
	signal_emit("command join", 3, data, server, item);
	signal_remove("channel created", (SIGNAL_FUNC) signal_channel_created_curwin);
}

static void cmd_channel_list_joined(void)
{
	CHANNEL_REC *channel;
	GString *nicks;
	GSList *nicklist, *tmp, *ntmp;
	char *mode;

	if (channels == NULL) {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_NOT_IN_CHANNELS);
		return;
	}

	/* print active channel */
	channel = irc_item_channel(active_win->active);
	if (channel != NULL)
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_CURRENT_CHANNEL, channel->name);

	/* print list of all channels, their modes, server tags and nicks */
	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, IRCTXT_CHANLIST_HEADER);
	for (tmp = channels; tmp != NULL; tmp = tmp->next) {
		channel = tmp->data;

		nicklist = nicklist_getnicks(channel);
		mode = channel_get_mode(channel);
		nicks = g_string_new(NULL);
		for (ntmp = nicklist; ntmp != NULL; ntmp = ntmp->next) {
			NICK_REC *rec = ntmp->data;

			g_string_sprintfa(nicks, "%s ", rec->nick);
		}

		if (nicks->len > 1) g_string_truncate(nicks, nicks->len-1);
		printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, IRCTXT_CHANLIST_LINE,
			    channel->name, mode, channel->server->tag, nicks->str);

		g_free(mode);
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
	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, IRCTXT_CHANSETUP_HEADER);
	for (tmp = setupchannels; tmp != NULL; tmp = tmp->next) {
		SETUP_CHANNEL_REC *rec = tmp->data;

		g_string_truncate(str, 0);
		if (rec->autojoin)
			g_string_append(str, "autojoin, ");
		if (rec->botmasks != NULL && *rec->botmasks != '\0')
			g_string_sprintfa(str, "bots: %s, ", rec->botmasks);
		if (rec->autosendcmd != NULL && *rec->autosendcmd != '\0')
			g_string_sprintfa(str, "botcmd: %s, ", rec->autosendcmd);

		if (str->len > 2) g_string_truncate(str, str->len-2);
		printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, IRCTXT_CHANSETUP_LINE,
			    rec->name, rec->ircnet == NULL ? "" : rec->ircnet,
			    rec->password == NULL ? "" : rec->password, str->str);
	}
	g_string_free(str, TRUE);
	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, IRCTXT_CHANSETUP_FOOTER);
}

static void cmd_channel(const char *data, IRC_SERVER_REC *server, WI_ITEM_REC *item)
{
	if (*data == '\0')
		cmd_channel_list_joined();
	else if (ischannel(*data))
		signal_emit("command join", 2, data, server);
	else
		command_runsub("channel", data, server, item);
}

/* SYNTAX: CHANNEL ADD [-auto | -noauto] [-bots <masks>] [-botcmd <command>]
                       <channel> <ircnet> [<password>] */
static void cmd_channel_add(const char *data)
{
	GHashTable *optlist;
	SETUP_CHANNEL_REC *rec;
	char *botarg, *botcmdarg, *ircnet, *channel, *password;
	void *free_arg;

	if (!cmd_get_params(data, &free_arg, 3 | PARAM_FLAG_OPTIONS,
			    "channel add", &optlist, &channel, &ircnet, &password))
		return;

	botarg = g_hash_table_lookup(optlist, "bots");
	botcmdarg = g_hash_table_lookup(optlist, "botcmd");

	if (*ircnet == '\0' || *channel == '\0')
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	rec = channels_setup_find(channel, ircnet);
	if (rec == NULL) {
		rec = g_new0(SETUP_CHANNEL_REC, 1);
		rec->name = g_strdup(channel);
		rec->ircnet = g_strdup(ircnet);
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
	printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_CHANSETUP_ADDED, channel, ircnet);

	cmd_params_free(free_arg);
}

/* SYNTAX: CHANNEL REMOVE <channel> <ircnet> */
static void cmd_channel_remove(const char *data)
{
	SETUP_CHANNEL_REC *rec;
	char *ircnet, *channel;
	void *free_arg;

	if (!cmd_get_params(data, &free_arg, 2, &channel, &ircnet))
		return;
	if (*ircnet == '\0' || *channel == '\0')
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	rec = channels_setup_find(channel, ircnet);
	if (rec == NULL)
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_CHANSETUP_NOT_FOUND, channel, ircnet);
	else {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_CHANSETUP_REMOVED, channel, ircnet);
		channels_setup_destroy(rec);
	}
	cmd_params_free(free_arg);
}

void fe_channels_init(void)
{
	settings_add_bool("lookandfeel", "autoclose_windows", TRUE);

	signal_add("channel created", (SIGNAL_FUNC) signal_channel_created);
	signal_add("channel destroyed", (SIGNAL_FUNC) signal_channel_destroyed);
	signal_add("window item remove", (SIGNAL_FUNC) signal_window_item_removed);
	signal_add_last("window item changed", (SIGNAL_FUNC) signal_window_item_changed);
	signal_add_last("server disconnected", (SIGNAL_FUNC) sig_disconnected);

	command_bind("wjoin", NULL, (SIGNAL_FUNC) cmd_wjoin);
	command_bind("channel", NULL, (SIGNAL_FUNC) cmd_channel);
	command_bind("channel add", NULL, (SIGNAL_FUNC) cmd_channel_add);
	command_bind("channel remove", NULL, (SIGNAL_FUNC) cmd_channel_remove);
	command_bind("channel list", NULL, (SIGNAL_FUNC) cmd_channel_list);

	command_set_options("channel add", "auto noauto -bots -botcmd");
}

void fe_channels_deinit(void)
{
	signal_remove("channel created", (SIGNAL_FUNC) signal_channel_created);
	signal_remove("channel destroyed", (SIGNAL_FUNC) signal_channel_destroyed);
	signal_remove("window item remove", (SIGNAL_FUNC) signal_window_item_removed);
	signal_remove("window item changed", (SIGNAL_FUNC) signal_window_item_changed);
	signal_remove("server disconnected", (SIGNAL_FUNC) sig_disconnected);

	command_unbind("wjoin", (SIGNAL_FUNC) cmd_wjoin);
	command_unbind("channel", (SIGNAL_FUNC) cmd_channel);
	command_unbind("channel add", (SIGNAL_FUNC) cmd_channel_add);
	command_unbind("channel remove", (SIGNAL_FUNC) cmd_channel_remove);
	command_unbind("channel list", (SIGNAL_FUNC) cmd_channel_list);
}
