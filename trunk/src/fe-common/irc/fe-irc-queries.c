/*
 fe-irc-queries.c : irssi

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
#include "signals.h"
#include "settings.h"
#include "servers.h"
#include "queries.h"
#include "nicklist.h"

static QUERY_REC *query_find_address(SERVER_REC *server, const char *address)
{
	GSList *tmp;

	g_return_val_if_fail(IS_SERVER(server), NULL);

	for (tmp = server->queries; tmp != NULL; tmp = tmp->next) {
		QUERY_REC *rec = tmp->data;

		if (*rec->name != '=' && rec->address != NULL &&
		    g_strcasecmp(address, rec->address) == 0)
			return rec;
	}

	return NULL;
}

static void event_privmsg(SERVER_REC *server, const char *data,
			  const char *nick, const char *address)
{
	QUERY_REC *query;

	g_return_if_fail(data != NULL);

	if (nick == NULL || address == NULL || ischannel(*data) ||
	    !settings_get_bool("query_trace_nick_changes"))
                return;

	query = query_find(server, nick);
	if (query == NULL) {
		/* check if there's query with another nick from the same
		   address. it was probably a nick change or reconnect to
		   server, so rename the query. */
		query = query_find_address(server, address);
		if (query != NULL)
			query_change_nick(query, nick);
	}
}

void fe_irc_queries_init(void)
{
        settings_add_bool("lookandfeel", "query_trace_nick_changes", TRUE);

	signal_add_first("event privmsg", (SIGNAL_FUNC) event_privmsg);
}

void fe_irc_queries_deinit(void)
{
	signal_remove("event privmsg", (SIGNAL_FUNC) event_privmsg);
}
