/*
 irc-core.c : irssi

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
#include "chat-protocols.h"

#include "irc-servers.h"
#include "irc-chatnets.h"
#include "irc-channels.h"
#include "irc-queries.h"

#include "ctcp.h"
#include "ignore.h"
#include "irc.h"
#include "netsplit.h"

void irc_commands_init(void);
void irc_commands_deinit(void);

void irc_rawlog_init(void);
void irc_rawlog_deinit(void);

void irc_special_vars_init(void);
void irc_special_vars_deinit(void);

void irc_log_init(void);
void irc_log_deinit(void);

void lag_init(void);
void lag_deinit(void);

void irc_channels_setup_init(void);
void irc_channels_setup_deinit(void);

void irc_core_init(void)
{
	CHAT_PROTOCOL_REC *rec;

	rec = g_new0(CHAT_PROTOCOL_REC, 1);
	rec->name = "IRC";
	rec->fullname = "Internet Relay Chat";
	rec->chatnet = "ircnet";

	chat_protocol_register(rec);

	irc_chatnets_init();
	irc_servers_init();
	irc_channels_init();
	irc_queries_init();

	ctcp_init();
	irc_channels_setup_init();
	irc_commands_init();
	irc_irc_init();
	lag_init();
	netsplit_init();
	ignore_init();
	irc_rawlog_init();
	irc_special_vars_init();
	irc_log_init();
}

void irc_core_deinit(void)
{
        irc_log_deinit();
	irc_special_vars_deinit();
	irc_rawlog_deinit();
	ignore_deinit();
	netsplit_deinit();
	lag_deinit();
	irc_commands_deinit();
	irc_channels_setup_deinit();
	ctcp_deinit();

	irc_queries_deinit();
	irc_channels_deinit();
	irc_irc_deinit();
	irc_servers_deinit();
	irc_chatnets_deinit();

	chat_protocol_unregister("IRC");
}
