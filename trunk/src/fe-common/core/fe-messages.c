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

#define ishighalnum(c) ((unsigned char) (c) >= 128 || isalnum(c))

/* convert _underlined_ and *bold* words (and phrases) to use real
   underlining or bolding */
char *expand_emphasis(WI_ITEM_REC *item, const char *text)
{
	GString *str;
	char *ret;
	int pos;

        g_return_val_if_fail(text != NULL, NULL);

	str = g_string_new(text);

	for (pos = 0; pos < str->len; pos++) {
		char type, *bgn, *end;

		bgn = str->str + pos;

		if (*bgn == '*') 
			type = 2; /* bold */
		else if (*bgn == '_') 
			type = 31; /* underlined */
		else
			continue;

		/* check that the beginning marker starts a word, and
		   that the matching end marker ends a word */
		if ((pos > 0 && !isspace(bgn[-1])) || !ishighalnum(bgn[1]))
			continue;
		if ((end = strchr(bgn+1, *bgn)) == NULL)
			continue;
		if (!ishighalnum(end[-1]) || ishighalnum(end[1]) ||
		    end[1] == type || end[1] == '*' || end[1] == '_')
			continue;

		if (IS_CHANNEL(item)) {
			/* check that this isn't a _nick_, we don't want to
			   use emphasis on them. */
			int found;
                        char c;

			c = end[1];
                        end[1] = '\0';
                        found = nicklist_find(CHANNEL(item), bgn) != NULL;
			end[1] = c;
			if (found) continue;
		}

		/* allow only *word* emphasis, not *multiple words* */
		if (!settings_get_bool("emphasis_multiword")) {
			char *c;
			for (c = bgn+1; c != end; c++) {
				if (!ishighalnum(*c))
					break;
			}
			if (c != end) continue;
		}

		if (settings_get_bool("emphasis_replace")) {
			*bgn = *end = type;
                        pos += (end-bgn);
		} else {
			g_string_insert_c(str, pos, type);
                        pos += (end - bgn) + 2;
			g_string_insert_c(str, pos++, type);
		}
	}

	ret = str->str;
	g_string_free(str, FALSE);
	return ret;
}

char *channel_get_nickmode(CHANNEL_REC *channel, const char *nick)
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
	char *color, *freemsg = NULL;

	/* NOTE: this may return NULL if some channel is just closed with
	   /WINDOW CLOSE and server still sends the few last messages */
	chanrec = channel_find(server, target);

	for_me = nick_match_msg(chanrec, msg, server->nick);
	color = for_me ? NULL :
		hilight_match_nick(server, target, nick, address, MSGLEVEL_PUBLIC, msg);

	print_channel = chanrec == NULL ||
		!window_item_is_active((WI_ITEM_REC *) chanrec);
	if (!print_channel && settings_get_bool("print_active_channel") &&
	    window_item_window((WI_ITEM_REC *) chanrec)->items->next != NULL)
		print_channel = TRUE;

	level = MSGLEVEL_PUBLIC | (for_me || color != NULL ?
				   MSGLEVEL_HILIGHT : MSGLEVEL_NOHILIGHT);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis((WI_ITEM_REC *) chanrec, msg);

	nickmode = channel_get_nickmode(chanrec, nick);
	if (!print_channel) {
		/* message to active channel in window */
		if (color != NULL) {
			/* highlighted nick */
			printformat(server, target, level,
				    TXT_PUBMSG_HILIGHT,
				    color, nick, msg, nickmode);
		} else {
			printformat(server, target, level,
				    for_me ? TXT_PUBMSG_ME : TXT_PUBMSG,
				    nick, msg, nickmode);
		}
	} else {
		/* message to not existing/active channel */
		if (color != NULL) {
			/* highlighted nick */
			printformat(server, target, level,
				    TXT_PUBMSG_HILIGHT_CHANNEL,
				    color, nick, target, msg, nickmode);
		} else {
			printformat(server, target, level,
				    for_me ? TXT_PUBMSG_ME_CHANNEL :
				    TXT_PUBMSG_CHANNEL,
				    nick, target, msg, nickmode);
		}
	}

        g_free_not_null(freemsg);
	g_free_not_null(color);
}

static void sig_message_private(SERVER_REC *server, const char *msg,
				const char *nick, const char *address)
{
	QUERY_REC *query;
        char *freemsg = NULL;

	query = query_find(server, nick);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis((WI_ITEM_REC *) query, msg);

	printformat(server, nick, MSGLEVEL_MSGS,
		    query == NULL ? TXT_MSG_PRIVATE :
		    TXT_MSG_PRIVATE_QUERY, nick, address, msg);

	g_free_not_null(freemsg);
}

static void print_own_channel_message(SERVER_REC *server, CHANNEL_REC *channel,
				      const char *target, const char *msg)
{
	WINDOW_REC *window;
	const char *nickmode;
        char *freemsg = NULL;
	int print_channel;

	nickmode = channel_get_nickmode(channel, server->nick);

	window = channel == NULL ? NULL :
		window_item_window((WI_ITEM_REC *) channel);

	print_channel = window == NULL ||
		window->active != (WI_ITEM_REC *) channel;

	if (!print_channel && settings_get_bool("print_active_channel") &&
	    window != NULL && g_slist_length(window->items) > 1)
		print_channel = TRUE;

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis((WI_ITEM_REC *) channel, msg);

	if (!print_channel) {
		printformat(server, target, MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
			    TXT_OWN_MSG, server->nick, msg, nickmode);
	} else {
		printformat(server, target, MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
			    TXT_OWN_MSG_CHANNEL, server->nick, target, msg, nickmode);
	}

	g_free_not_null(freemsg);
}

static void sig_message_own_public(SERVER_REC *server, const char *msg,
				   const char *target)
{
	CHANNEL_REC *channel;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);

	channel = channel_find(server, target);
	print_own_channel_message(server, channel, target, msg);
}

static void sig_message_own_private(SERVER_REC *server, const char *msg,
				    const char *target, const char *origtarget)
{
	QUERY_REC *query;
        char *freemsg = NULL;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);

	if (target == NULL) {
		/* this should only happen if some special target failed and
		   we should display some error message. currently the special
		   targets are only ',' and '.'. */
		g_return_if_fail(strcmp(origtarget, ",") == 0 ||
				 strcmp(origtarget, ".") == 0);

		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			    *origtarget == ',' ? TXT_NO_MSGS_GOT :
			    TXT_NO_MSGS_SENT);
		signal_stop();
		return;
	}

	query = privmsg_get_query(server, target, TRUE, MSGLEVEL_MSGS);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis((WI_ITEM_REC *) query, msg);

	printformat(server, target,
		    MSGLEVEL_MSGS | MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
		    query == NULL ? TXT_OWN_MSG_PRIVATE :
		    TXT_OWN_MSG_PRIVATE_QUERY, target, msg, server->nick);

	g_free_not_null(freemsg);
}

static void sig_message_join(SERVER_REC *server, const char *channel,
			     const char *nick, const char *address)
{
	printformat(server, channel, MSGLEVEL_JOINS,
		    TXT_JOIN, nick, address, channel);
}

static void sig_message_part(SERVER_REC *server, const char *channel,
			     const char *nick, const char *address,
			     const char *reason)
{
	printformat(server, channel, MSGLEVEL_PARTS,
		    TXT_PART, nick, address, channel, reason);
}

static void sig_message_quit(SERVER_REC *server, const char *nick,
			     const char *address, const char *reason)
{
	WINDOW_REC *window;
	GString *chans;
	GSList *tmp, *windows;
	char *print_channel;
	int once, count;

	if (ignore_check(server, nick, address, NULL, reason, MSGLEVEL_QUITS))
		return;

	print_channel = NULL;
	once = settings_get_bool("show_quit_once");

	count = 0; windows = NULL;
	chans = g_string_new(NULL);
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_REC *rec = tmp->data;

		if (!nicklist_find(rec, nick))
			continue;

		if (ignore_check(server, nick, address, rec->name,
				 reason, MSGLEVEL_QUITS)) {
			count++;
			continue;
		}

		if (print_channel == NULL ||
		    active_win->active == (WI_ITEM_REC *) rec)
			print_channel = rec->name;

		if (once)
			g_string_sprintfa(chans, "%s,", rec->name);
		else {
			window = window_item_window((WI_ITEM_REC *) rec);
			if (g_slist_find(windows, window) == NULL) {
				windows = g_slist_append(windows, window);
				printformat(server, rec->name, MSGLEVEL_QUITS,
					    TXT_QUIT, nick, address, reason,
					    rec->name);
			}
		}
		count++;
	}
	g_slist_free(windows);

	if (!once) {
		/* check if you had query with the nick and
		   display the quit there too */
		QUERY_REC *query = query_find(server, nick);
		if (query != NULL) {
			printformat(server, nick, MSGLEVEL_QUITS,
				    TXT_QUIT, nick, address, reason, "");
		}
	}

	if (once || count == 0) {
		if (chans->len > 0)
			g_string_truncate(chans, chans->len-1);
		printformat(server, print_channel, MSGLEVEL_QUITS,
			    count <= 1 ? TXT_QUIT : TXT_QUIT_ONCE,
			    nick, address, reason, chans->str);
	}
	g_string_free(chans, TRUE);
}

static void sig_message_kick(SERVER_REC *server, const char *channel,
			     const char *nick, const char *kicker,
			     const char *address, const char *reason)
{
	printformat(server, channel, MSGLEVEL_KICKS,
		    TXT_KICK, nick, channel, kicker, reason);
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
		    ownnick ? TXT_YOUR_NICK_CHANGED : TXT_NICK_CHANGED,
		    oldnick, newnick, channel);
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

		if (nicklist_find(channel, newnick) == NULL ||
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
			    TXT_YOUR_NICK_CHANGED, oldnick, newnick, "");
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
		    TXT_INVITE, nick, str);
	g_free(str);
}

static void sig_message_topic(SERVER_REC *server, const char *channel,
			      const char *topic,
			      const char *nick, const char *address)
{
	printformat(server, channel, MSGLEVEL_TOPICS,
		    *topic != '\0' ? TXT_NEW_TOPIC : TXT_TOPIC_UNSET,
		    nick, channel, topic);
}

void fe_messages_init(void)
{
	settings_add_bool("lookandfeel", "emphasis", TRUE);
	settings_add_bool("lookandfeel", "emphasis_replace", FALSE);
	settings_add_bool("lookandfeel", "emphasis_multiword", FALSE);
	settings_add_bool("lookandfeel", "show_nickmode", TRUE);
	settings_add_bool("lookandfeel", "show_nickmode_empty", TRUE);
	settings_add_bool("lookandfeel", "print_active_channel", FALSE);
	settings_add_bool("lookandfeel", "show_quit_once", FALSE);

	signal_add("message public", (SIGNAL_FUNC) sig_message_public);
	signal_add("message private", (SIGNAL_FUNC) sig_message_private);
	signal_add("message own_public", (SIGNAL_FUNC) sig_message_own_public);
	signal_add("message own_private", (SIGNAL_FUNC) sig_message_own_private);
	signal_add("message join", (SIGNAL_FUNC) sig_message_join);
	signal_add("message part", (SIGNAL_FUNC) sig_message_part);
	signal_add("message quit", (SIGNAL_FUNC) sig_message_quit);
	signal_add("message kick", (SIGNAL_FUNC) sig_message_kick);
	signal_add("message nick", (SIGNAL_FUNC) sig_message_nick);
	signal_add("message own_nick", (SIGNAL_FUNC) sig_message_own_nick);
	signal_add("message invite", (SIGNAL_FUNC) sig_message_invite);
	signal_add("message topic", (SIGNAL_FUNC) sig_message_topic);
}

void fe_messages_deinit(void)
{
	signal_remove("message public", (SIGNAL_FUNC) sig_message_public);
	signal_remove("message private", (SIGNAL_FUNC) sig_message_private);
	signal_remove("message own_public", (SIGNAL_FUNC) sig_message_own_public);
	signal_remove("message own_private", (SIGNAL_FUNC) sig_message_own_private);
	signal_remove("message join", (SIGNAL_FUNC) sig_message_join);
	signal_remove("message part", (SIGNAL_FUNC) sig_message_part);
	signal_remove("message quit", (SIGNAL_FUNC) sig_message_quit);
	signal_remove("message kick", (SIGNAL_FUNC) sig_message_kick);
	signal_remove("message nick", (SIGNAL_FUNC) sig_message_nick);
	signal_remove("message own_nick", (SIGNAL_FUNC) sig_message_own_nick);
	signal_remove("message invite", (SIGNAL_FUNC) sig_message_invite);
	signal_remove("message topic", (SIGNAL_FUNC) sig_message_topic);
}
