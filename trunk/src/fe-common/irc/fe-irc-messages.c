/*
 fe-irc-messages.c : irssi

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
#include "levels.h"
#include "channels.h"
#include "ignore.h"

#include "irc.h"
#include "irc-channels.h"

#include "../core/module-formats.h"
#include "module-formats.h"
#include "printtext.h"
#include "fe-messages.h"

#include "fe-queries.h"
#include "window-items.h"

static void sig_message_own_public(SERVER_REC *server, const char *msg,
				   const char *target, const char *origtarget)
{
	char *nickmode;

	if (IS_IRC_SERVER(server) && target != NULL &&
	    *target == '@' && ischannel(target[1])) {
		/* Hybrid 6 feature, send msg to all ops in channel */
		nickmode = channel_get_nickmode(channel_find(server, target+1),
						server->nick);

		printformat_module("fe-common/core", server, target+1,
				   MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT |
				   MSGLEVEL_NO_ACT,
				   TXT_OWN_MSG_CHANNEL,
				   server->nick, target, msg, nickmode);
                signal_stop();
	}
}

/* received msg to all ops in channel */
static void sig_message_irc_op_public(SERVER_REC *server, const char *msg,
				      const char *nick, const char *address,
				      const char *target)
{
	char *nickmode, *optarget;

	nickmode = channel_get_nickmode(channel_find(server, target),
					server->nick);

        optarget = g_strconcat("@", target, NULL);
	printformat_module("fe-common/core", server, target,
			   MSGLEVEL_PUBLIC | MSGLEVEL_HILIGHT,
			   TXT_PUBMSG_ME_CHANNEL,
			   nick, optarget, msg, nickmode);
        g_free(optarget);
}

static void sig_message_own_wall(SERVER_REC *server, const char *msg,
				 const char *target)
{
        char *nickmode, *optarget;

	nickmode = channel_get_nickmode(channel_find(server, target),
					server->nick);

        optarget = g_strconcat("@", target, NULL);
	printformat_module("fe-common/core", server, target,
			   MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT |
			   MSGLEVEL_NO_ACT,
			   TXT_OWN_MSG_CHANNEL,
			   server->nick, optarget, msg, nickmode);
        g_free(optarget);
}

static void sig_message_own_action(IRC_SERVER_REC *server, const char *msg,
                                   const char *target)
{
	printformat(server, target, MSGLEVEL_ACTIONS |
		    (ischannel(*target) ? MSGLEVEL_PUBLIC : MSGLEVEL_MSGS) |
		    MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
		    IRCTXT_OWN_ME, server->nick, msg);
}

static void sig_message_irc_action(IRC_SERVER_REC *server, const char *msg,
				   const char *nick, const char *address,
				   const char *target)
{
	void *item;
	int level;

	level = MSGLEVEL_ACTIONS |
		(ischannel(*target) ? MSGLEVEL_PUBLIC : MSGLEVEL_MSGS);

	if (ignore_check(SERVER(server), nick, address, target, msg, level))
		return;

	if (ischannel(*target)) {
		/* channel action */
		item = irc_channel_find(server, target);

		if (window_item_is_active(item)) {
			/* message to active channel in window */
			printformat(server, target, level,
				    IRCTXT_ACTION_PUBLIC, nick, msg);
		} else {
			/* message to not existing/active channel */
			printformat(server, target, level,
				    IRCTXT_ACTION_PUBLIC_CHANNEL,
				    nick, target, msg);
		}
	} else {
		/* private action */
		item = privmsg_get_query(SERVER(server), nick, FALSE,
					 MSGLEVEL_MSGS);
		printformat(server, nick, MSGLEVEL_ACTIONS | MSGLEVEL_MSGS,
			    item == NULL ? IRCTXT_ACTION_PRIVATE :
			    IRCTXT_ACTION_PRIVATE_QUERY,
			    nick, address == NULL ? "" : address, msg);
	}
}

static void sig_message_own_notice(IRC_SERVER_REC *server, const char *msg,
				   const char *target)
{
	printformat(server, target, MSGLEVEL_NOTICES |
		    MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
		    IRCTXT_OWN_NOTICE, target, msg);
}

static void sig_message_irc_notice(SERVER_REC *server, const char *msg,
				   const char *nick, const char *address,
				   const char *target)
{
	int op_notice;

	if (address == NULL) {
		/* notice from server */
		if (!ignore_check(server, nick, "",
				  target, msg, MSGLEVEL_SNOTES)) {
			printformat(server, target, MSGLEVEL_SNOTES,
				    IRCTXT_NOTICE_SERVER, nick, msg);
		}
                return;
	}

	op_notice = *target == '@' && ischannel(target[1]);
	if (op_notice) target++;

	if (ignore_check(server, nick, address,
			 ischannel(*target) ? target : NULL,
			 msg, MSGLEVEL_NOTICES))
		return;

	if (ischannel(*target)) {
		/* notice in some channel */
		printformat(server, target, MSGLEVEL_NOTICES,
			    op_notice ? IRCTXT_NOTICE_PUBLIC_OPS :
			    IRCTXT_NOTICE_PUBLIC, nick, target, msg);
	} else {
		/* private notice */
		privmsg_get_query(SERVER(server), nick, FALSE,
				  MSGLEVEL_NOTICES);
		printformat(server, nick, MSGLEVEL_NOTICES,
			    IRCTXT_NOTICE_PRIVATE, nick, address, msg);
	}
}

static void sig_message_own_ctcp(IRC_SERVER_REC *server, const char *cmd,
				 const char *data, const char *target)
{
	printformat(server, target, MSGLEVEL_CTCPS |
		    MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
		    IRCTXT_OWN_CTCP, target, cmd, data);
}

static void sig_message_irc_ctcp(IRC_SERVER_REC *server, const char *msg,
				 const char *nick, const char *addr,
				 const char *target)
{
	printformat(server, ischannel(*target) ? target : nick, MSGLEVEL_CTCPS,
		    IRCTXT_CTCP_REQUESTED, nick, addr, msg, target);
}

void fe_irc_messages_init(void)
{
        signal_add("message own_public", (SIGNAL_FUNC) sig_message_own_public);
        signal_add("message irc op_public", (SIGNAL_FUNC) sig_message_irc_op_public);
        signal_add("message irc own_wall", (SIGNAL_FUNC) sig_message_own_wall);
        signal_add("message irc own_action", (SIGNAL_FUNC) sig_message_own_action);
        signal_add("message irc action", (SIGNAL_FUNC) sig_message_irc_action);
        signal_add("message irc own_notice", (SIGNAL_FUNC) sig_message_own_notice);
        signal_add("message irc notice", (SIGNAL_FUNC) sig_message_irc_notice);
        signal_add("message irc own_ctcp", (SIGNAL_FUNC) sig_message_own_ctcp);
        signal_add("message irc ctcp", (SIGNAL_FUNC) sig_message_irc_ctcp);
}

void fe_irc_messages_deinit(void)
{
        signal_remove("message own_public", (SIGNAL_FUNC) sig_message_own_public);
        signal_remove("message irc op_public", (SIGNAL_FUNC) sig_message_irc_op_public);
        signal_remove("message irc own_wall", (SIGNAL_FUNC) sig_message_own_wall);
        signal_remove("message irc own_action", (SIGNAL_FUNC) sig_message_own_action);
        signal_remove("message irc action", (SIGNAL_FUNC) sig_message_irc_action);
        signal_remove("message irc own_notice", (SIGNAL_FUNC) sig_message_own_notice);
        signal_remove("message irc notice", (SIGNAL_FUNC) sig_message_irc_notice);
        signal_remove("message irc own_ctcp", (SIGNAL_FUNC) sig_message_own_ctcp);
        signal_remove("message irc ctcp", (SIGNAL_FUNC) sig_message_irc_ctcp);
}
