/*
 fe-ircnet.c : irssi

    Copyright (C) 2000 Timo Sirainen

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
#include "signals.h"
#include "commands.h"
#include "levels.h"
#include "misc.h"
#include "chatnets.h"

#include "irc-servers.h"
#include "irc-chatnets.h"
#include "printtext.h"

static void cmd_ircnet_list(void)
{
	GString *str;
	GSList *tmp;

	str = g_string_new(NULL);
	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, IRCTXT_IRCNET_HEADER);
	for (tmp = chatnets; tmp != NULL; tmp = tmp->next) {
		IRC_CHATNET_REC *rec = tmp->data;

		if (!IS_IRCNET(rec))
                        continue;

		g_string_truncate(str, 0);
		if (rec->nick != NULL)
			g_string_sprintfa(str, "nick: %s, ", rec->nick);
		if (rec->username != NULL)
			g_string_sprintfa(str, "username: %s, ", rec->username);
		if (rec->realname != NULL)
			g_string_sprintfa(str, "realname: %s, ", rec->realname);
		if (rec->own_host != NULL)
			g_string_sprintfa(str, "host: %s, ", rec->own_host);
		if (rec->autosendcmd != NULL)
			g_string_sprintfa(str, "autosendcmd: %s, ", rec->autosendcmd);

		if (rec->cmd_queue_speed > 0)
			g_string_sprintfa(str, "cmdspeed: %d, ", rec->cmd_queue_speed);
		if (rec->max_cmds_at_once > 0)
			g_string_sprintfa(str, "cmdmax: %d, ", rec->max_cmds_at_once);
		if (rec->max_query_chans > 0)
			g_string_sprintfa(str, "querychans: %d, ", rec->max_query_chans);

		if (rec->max_kicks > 0)
			g_string_sprintfa(str, "max_kicks: %d, ", rec->max_kicks);
		if (rec->max_msgs > 0)
			g_string_sprintfa(str, "max_msgs: %d, ", rec->max_msgs);
		if (rec->max_modes > 0)
			g_string_sprintfa(str, "max_modes: %d, ", rec->max_modes);
		if (rec->max_whois > 0)
			g_string_sprintfa(str, "max_whois: %d, ", rec->max_whois);

		if (str->len > 1) g_string_truncate(str, str->len-2);
		printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP,
			    IRCTXT_IRCNET_LINE, rec->name, str->str);
	}
	g_string_free(str, TRUE);
	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, IRCTXT_IRCNET_FOOTER);
}

/* SYNTAX: IRCNET ADD [-nick <nick>] [-user <user>] [-realname <name>]
                      [-host <host>] [-autosendcmd <cmd>]
		      [-querychans <count>] [-whois <count>] [-msgs <count>]
		      [-kicks <count>] [-modes <count>]
		      [-cmdspeed <ms>] [-cmdmax <count>] <name> */
static void cmd_ircnet_add(const char *data)
{
	GHashTable *optlist;
	char *name, *value;
	void *free_arg;
	IRC_CHATNET_REC *rec;

	if (!cmd_get_params(data, &free_arg, 1 | PARAM_FLAG_OPTIONS,
			    "ircnet add", &optlist, &name))
		return;
	if (*name == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	rec = ircnet_find(name);
	if (rec == NULL) {
		rec = g_new0(IRC_CHATNET_REC, 1);
		rec->name = g_strdup(name);
	} else {
		if (g_hash_table_lookup(optlist, "nick")) g_free_and_null(rec->nick);
		if (g_hash_table_lookup(optlist, "user")) g_free_and_null(rec->username);
		if (g_hash_table_lookup(optlist, "realname")) g_free_and_null(rec->realname);
		if (g_hash_table_lookup(optlist, "host")) {
			g_free_and_null(rec->own_host);
                        rec->own_ip4 = rec->own_ip6 = NULL;
		}
		if (g_hash_table_lookup(optlist, "autosendcmd")) g_free_and_null(rec->autosendcmd);
	}

	value = g_hash_table_lookup(optlist, "kicks");
	if (value != NULL) rec->max_kicks = atoi(value);
	value = g_hash_table_lookup(optlist, "msgs");
	if (value != NULL) rec->max_msgs = atoi(value);
	value = g_hash_table_lookup(optlist, "modes");
	if (value != NULL) rec->max_modes = atoi(value);
	value = g_hash_table_lookup(optlist, "whois");
	if (value != NULL) rec->max_whois = atoi(value);

	value = g_hash_table_lookup(optlist, "cmdspeed");
	if (value != NULL) rec->cmd_queue_speed = atoi(value);
	value = g_hash_table_lookup(optlist, "cmdmax");
	if (value != NULL) rec->max_cmds_at_once = atoi(value);
	value = g_hash_table_lookup(optlist, "querychans");
	if (value != NULL) rec->max_query_chans = atoi(value);

	value = g_hash_table_lookup(optlist, "nick");
	if (value != NULL && *value != '\0') rec->nick = g_strdup(value);
	value = g_hash_table_lookup(optlist, "user");
	if (value != NULL && *value != '\0') rec->username = g_strdup(value);
	value = g_hash_table_lookup(optlist, "realname");
	if (value != NULL && *value != '\0') rec->realname = g_strdup(value);

	value = g_hash_table_lookup(optlist, "host");
	if (value != NULL && *value != '\0') {
		rec->own_host = g_strdup(value);
		rec->own_ip4 = rec->own_ip6 = NULL;
	}

	value = g_hash_table_lookup(optlist, "autosendcmd");
	if (value != NULL && *value != '\0') rec->autosendcmd = g_strdup(value);

	ircnet_create(rec);
	printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_IRCNET_ADDED, name);

	cmd_params_free(free_arg);
}

/* SYNTAX: IRCNET REMOVE <ircnet> */
static void cmd_ircnet_remove(const char *data)
{
	IRC_CHATNET_REC *rec;

	if (*data == '\0') cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	rec = ircnet_find(data);
	if (rec == NULL)
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_IRCNET_NOT_FOUND, data);
	else {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_IRCNET_REMOVED, data);
		chatnet_remove(CHATNET(rec));
	}
}

static void cmd_ircnet(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	if (*data == '\0')
		cmd_ircnet_list();
	else
		command_runsub("ircnet", data, server, item);
}

void fe_ircnet_init(void)
{
	command_bind("ircnet", NULL, (SIGNAL_FUNC) cmd_ircnet);
	command_bind("ircnet list", NULL, (SIGNAL_FUNC) cmd_ircnet_list);
	command_bind("ircnet add", NULL, (SIGNAL_FUNC) cmd_ircnet_add);
	command_bind("ircnet remove", NULL, (SIGNAL_FUNC) cmd_ircnet_remove);

	command_set_options("ircnet add", "-kicks -msgs -modes -whois -cmdspeed -cmdmax -nick -user -realname -host -autosendcmd -querychans");
}

void fe_ircnet_deinit(void)
{
	command_unbind("ircnet", (SIGNAL_FUNC) cmd_ircnet);
	command_unbind("ircnet list", (SIGNAL_FUNC) cmd_ircnet_list);
	command_unbind("ircnet add", (SIGNAL_FUNC) cmd_ircnet_add);
	command_unbind("ircnet remove", (SIGNAL_FUNC) cmd_ircnet_remove);
}
