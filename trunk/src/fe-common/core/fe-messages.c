/*
 fe-messages.c : irssi

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
#include "special-vars.h"
#include "settings.h"

#include "window-items.h"
#include "fe-queries.h"
#include "channels.h"
#include "nicklist.h"
#include "hilight-text.h"
#include "ignore.h"
#include "printtext.h"

static char *get_nickmode(CHANNEL_REC *channel, const char *nick)
{
        NICK_REC *nickrec;
        char *emptystr;

	g_return_val_if_fail(nick != NULL, NULL);

	if (!settings_get_bool("show_nickmode"))
                return "";

        emptystr = settings_get_bool("show_nickmode_empty") ? " " : "";

	nickrec = channel == NULL ? NULL :
		nicklist_find(channel, nick);
	return nickrec == NULL ? emptystr :
		(nickrec->op ? "@" : (nickrec->voice ? "+" : emptystr));
}

static void sig_message_public(SERVER_REC *server, const char *msg,
			       const char *nick, const char *address,
			       const char *target)
{
	CHANNEL_REC *chanrec;
	const char *nickmode;
	int for_me, print_channel, level;
	char *color;

	chanrec = channel_find(server, target);
	for_me = nick_match_msg(server, msg, server->nick);
	color = for_me ? NULL :
		hilight_find_nick(target, nick, address, MSGLEVEL_PUBLIC, msg);

	print_channel = !window_item_is_active((WI_ITEM_REC *) chanrec);
	if (!print_channel && settings_get_bool("print_active_channel") &&
	    window_item_window((WI_ITEM_REC *) chanrec)->items->next != NULL)
		print_channel = TRUE;

	level = MSGLEVEL_PUBLIC | (for_me || color != NULL ?
				   MSGLEVEL_HILIGHT : MSGLEVEL_NOHILIGHT);

	nickmode = get_nickmode(chanrec, nick);
	if (!print_channel) {
		/* message to active channel in window */
		if (color != NULL) {
			/* highlighted nick */
			printformat(server, target, level,
				    IRCTXT_PUBMSG_HILIGHT,
				    color, nick, msg, nickmode);
		} else {
			printformat(server, target, level,
				    for_me ? IRCTXT_PUBMSG_ME : IRCTXT_PUBMSG,
				    nick, msg, nickmode);
		}
	} else {
		/* message to not existing/active channel */
		if (color != NULL) {
			/* highlighted nick */
			printformat(server, target, level,
				    IRCTXT_PUBMSG_HILIGHT_CHANNEL,
				    color, nick, target, msg, nickmode);
		} else {
			printformat(server, target, level,
				    for_me ? IRCTXT_PUBMSG_ME_CHANNEL :
				    IRCTXT_PUBMSG_CHANNEL,
				    nick, target, msg, nickmode);
		}
	}

	g_free_not_null(color);
}

static void sig_message_private(SERVER_REC *server, const char *msg,
				const char *nick, const char *address)
{
	QUERY_REC *query;

	query = query_find(server, nick);
	printformat(server, nick, MSGLEVEL_MSGS,
		    query == NULL ? IRCTXT_MSG_PRIVATE :
		    IRCTXT_MSG_PRIVATE_QUERY, nick, address, msg);
}

static void print_own_channel_message(SERVER_REC *server, CHANNEL_REC *channel,
				      const char *target, const char *msg)
{
	WINDOW_REC *window;
	const char *nickmode;
	int print_channel;

	nickmode = get_nickmode(channel, server->nick);

	window = channel == NULL ? NULL :
		window_item_window((WI_ITEM_REC *) channel);

	print_channel = window == NULL ||
		window->active != (WI_ITEM_REC *) channel;

	if (!print_channel && settings_get_bool("print_active_channel") &&
	    window != NULL && g_slist_length(window->items) > 1)
		print_channel = TRUE;

	if (!print_channel) {
		printformat(server, target, MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
			    IRCTXT_OWN_MSG, server->nick, msg, nickmode);
	} else {
		printformat(server, target, MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
			    IRCTXT_OWN_MSG_CHANNEL, server->nick, target, msg, nickmode);
	}
}

static void cmd_msg(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	GHashTable *optlist;
	CHANNEL_REC *channel;
	char *target, *msg, *freestr, *newtarget;
	void *free_arg;
	int free_ret;

	g_return_if_fail(data != NULL);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTIONS |
			    PARAM_FLAG_UNKNOWN_OPTIONS | PARAM_FLAG_GETREST,
			    "msg", &optlist, &target, &msg))
		return;
	if (*target == '\0' || *msg == '\0')
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	server = cmd_options_get_server("msg", optlist, server);

	free_ret = FALSE;
	if (strcmp(target, ",") == 0 || strcmp(target, ".") == 0) {
                /* , and . are handled specially */
		newtarget = parse_special(&target, server, item,
					  NULL, &free_ret, NULL);
		if (newtarget == NULL) {
			printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
				    *target == ',' ? IRCTXT_NO_MSGS_GOT :
				    IRCTXT_NO_MSGS_SENT);
			cmd_params_free(free_arg);
			signal_stop();
			return;
		}
		target = newtarget;
	} else if (strcmp(target, "*") == 0 && item != NULL) {
                /* * means active channel */
		target = item->name;
	}

	if (server == NULL || !server->connected)
		cmd_param_error(CMDERR_NOT_CONNECTED);
	channel = channel_find(server, target);

	freestr = !free_ret ? NULL : target;
	if (*target == '@' && server->ischannel(target[1])) {
		/* Hybrid 6 feature, send msg to all ops in channel
		   FIXME: this shouldn't really be here in core.. */
		target++;
	}

	if (server->ischannel(*target)) {
		/* msg to channel */
		print_own_channel_message(server, channel, target, msg);
	} else {
		/* private message */
		QUERY_REC *query;

		query = privmsg_get_query(server, target, TRUE, MSGLEVEL_MSGS);
		printformat(server, target, MSGLEVEL_MSGS |
			    MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
			    query == NULL ? IRCTXT_OWN_MSG_PRIVATE :
			    IRCTXT_OWN_MSG_PRIVATE_QUERY,
			    target, msg, server->nick);
	}
	g_free_not_null(freestr);

	cmd_params_free(free_arg);
}

static void sig_message_join(SERVER_REC *server, const char *channel,
			     const char *nick, const char *address)
{
	printformat(server, channel, MSGLEVEL_JOINS,
		    IRCTXT_JOIN, nick, address, channel);
}

static void sig_message_part(SERVER_REC *server, const char *channel,
			     const char *nick, const char *address,
			     const char *reason)
{
	printformat(server, channel, MSGLEVEL_PARTS,
		    IRCTXT_PART, nick, address, channel, reason);
}

static void sig_message_quit(SERVER_REC *server, const char *nick,
			     const char *address, const char *reason)
{
	WINDOW_REC *window;
	GString *chans;
	GSList *tmp, *windows;
	char *print_channel;
	int once, count;

	print_channel = NULL;
	once = settings_get_bool("show_quit_once");

	count = 0; windows = NULL;
	chans = !once ? NULL : g_string_new(NULL);
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_REC *rec = tmp->data;

		if (!nicklist_find(rec, nick) ||
		    ignore_check(server, nick, address, rec->name,
				 reason, MSGLEVEL_QUITS))
			continue;

		if (print_channel == NULL ||
		    active_win->active == (WI_ITEM_REC *) rec)
			print_channel = rec->name;

		if (!once) {
			window = window_item_window((WI_ITEM_REC *) rec);
			if (g_slist_find(windows, window) == NULL) {
				windows = g_slist_append(windows, window);
				printformat(server, rec->name, MSGLEVEL_QUITS,
					    IRCTXT_QUIT, nick, address, reason);
			}
		} else {
			g_string_sprintfa(chans, "%s,", rec->name);
			count++;
		}
	}
	g_slist_free(windows);

	if (once && count > 0) {
		g_string_truncate(chans, chans->len-1);
		printformat(server, print_channel, MSGLEVEL_QUITS,
			    count == 1 ? IRCTXT_QUIT : IRCTXT_QUIT_ONCE,
			    nick, address, reason, chans->str);
	}
	if (chans != NULL)
		g_string_free(chans, TRUE);
}

static void sig_message_kick(SERVER_REC *server, const char *channel,
			     const char *nick, const char *kicker,
			     const char *address, const char *reason)
{
	printformat(server, channel, MSGLEVEL_KICKS,
		    IRCTXT_KICK, nick, channel, kicker, reason);
}

static void print_nick_change_channel(SERVER_REC *server, const char *channel,
				      const char *newnick, const char *oldnick,
				      const char *address,
				      int ownnick)
{
	if (ignore_check(server, oldnick, address,
			 channel, newnick, MSGLEVEL_NICKS))
		return;

	printformat(server, channel, MSGLEVEL_NICKS,
		    ownnick ? IRCTXT_YOUR_NICK_CHANGED : IRCTXT_NICK_CHANGED,
		    oldnick, newnick);
}

static void print_nick_change(SERVER_REC *server, const char *newnick,
			      const char *oldnick, const char *address,
			      int ownnick)
{
	GSList *tmp, *windows;
	int msgprint;

	msgprint = FALSE;

	/* Print to each channel/query where the nick is.
	   Don't print more than once to the same window. */
	windows = NULL;
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_REC *channel = tmp->data;
		WINDOW_REC *window =
			window_item_window((WI_ITEM_REC *) channel);

		if (nicklist_find(channel, oldnick) == NULL ||
		    g_slist_find(windows, window) != NULL)
			continue;

		windows = g_slist_append(windows, window);
		print_nick_change_channel(server, channel->name, newnick,
					  oldnick, address, ownnick);
		msgprint = TRUE;
	}

	for (tmp = server->queries; tmp != NULL; tmp = tmp->next) {
		QUERY_REC *query = tmp->data;
		WINDOW_REC *window =
			window_item_window((WI_ITEM_REC *) query);

		if (g_strcasecmp(query->name, oldnick) != 0 ||
		    g_slist_find(windows, window) != NULL)
			continue;

		windows = g_slist_append(windows, window);
		print_nick_change_channel(server, query->name, newnick,
					  oldnick, address, ownnick);
		msgprint = TRUE;
	}
	g_slist_free(windows);

	if (!msgprint && ownnick) {
		printformat(server, NULL, MSGLEVEL_NICKS,
			    IRCTXT_YOUR_NICK_CHANGED, oldnick, newnick);
	}
}

static void sig_message_nick(SERVER_REC *server, const char *newnick,
			     const char *oldnick, const char *address)
{
	print_nick_change(server, newnick, oldnick, address, FALSE);
}

static void sig_message_own_nick(SERVER_REC *server, const char *newnick,
				 const char *oldnick, const char *address)
{
	print_nick_change(server, newnick, oldnick, address, TRUE);
}

static void sig_message_invite(SERVER_REC *server, const char *channel,
			       const char *nick, const char *address)
{
	char *str;

	str = show_lowascii(channel);
	printformat(server, NULL, MSGLEVEL_INVITES,
		    IRCTXT_INVITE, nick, str);
	g_free(str);
}

static void sig_message_topic(SERVER_REC *server, const char *channel,
			      const char *topic,
			      const char *nick, const char *address)
{
	printformat(server, channel, MSGLEVEL_TOPICS,
		    *topic != '\0' ? IRCTXT_NEW_TOPIC : IRCTXT_TOPIC_UNSET,
		    nick, channel, topic);
}

void fe_messages_init(void)
{
	settings_add_bool("lookandfeel", "show_nickmode", TRUE);
	settings_add_bool("lookandfeel", "show_nickmode_empty", TRUE);
	settings_add_bool("lookandfeel", "print_active_channel", FALSE);
	settings_add_bool("lookandfeel", "show_quit_once", FALSE);

	signal_add("message public", (SIGNAL_FUNC) sig_message_public);
	signal_add("message private", (SIGNAL_FUNC) sig_message_private);
	signal_add("message join", (SIGNAL_FUNC) sig_message_join);
	signal_add("message part", (SIGNAL_FUNC) sig_message_part);
	signal_add("message quit", (SIGNAL_FUNC) sig_message_quit);
	signal_add("message kick", (SIGNAL_FUNC) sig_message_kick);
	signal_add("message nick", (SIGNAL_FUNC) sig_message_nick);
	signal_add("message own_nick", (SIGNAL_FUNC) sig_message_own_nick);
	signal_add("message invite", (SIGNAL_FUNC) sig_message_invite);
	signal_add("message topic", (SIGNAL_FUNC) sig_message_topic);
	command_bind_last("msg", NULL, (SIGNAL_FUNC) cmd_msg);
}

void fe_messages_deinit(void)
{
	signal_remove("message public", (SIGNAL_FUNC) sig_message_public);
	signal_remove("message private", (SIGNAL_FUNC) sig_message_private);
	signal_remove("message join", (SIGNAL_FUNC) sig_message_join);
	signal_remove("message part", (SIGNAL_FUNC) sig_message_part);
	signal_remove("message quit", (SIGNAL_FUNC) sig_message_quit);
	signal_remove("message kick", (SIGNAL_FUNC) sig_message_kick);
	signal_remove("message nick", (SIGNAL_FUNC) sig_message_nick);
	signal_remove("message own_nick", (SIGNAL_FUNC) sig_message_own_nick);
	signal_remove("message invite", (SIGNAL_FUNC) sig_message_invite);
	signal_remove("message topic", (SIGNAL_FUNC) sig_message_topic);
	command_unbind("msg", (SIGNAL_FUNC) cmd_msg);
}
