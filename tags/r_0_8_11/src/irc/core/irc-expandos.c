/*
 irc-expandos.c : irssi

    Copyright (C) 2000-2002 Timo Sirainen

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
#include "misc.h"
#include "expandos.h"
#include "settings.h"

#include "irc-servers.h"
#include "irc-channels.h"
#include "nicklist.h"

static char *last_join;

/* last person to join a channel you are on */
static char *expando_lastjoin(SERVER_REC *server, void *item, int *free_ret)
{
	return last_join;
}

/* current server numeric being processed */
static char *expando_server_numeric(SERVER_REC *server, void *item, int *free_ret)
{
	return current_server_event == NULL ||
		!is_numeric(current_server_event, 0) ? NULL :
		current_server_event;
}

/* current server name */
static char *expando_servername(SERVER_REC *server, void *item, int *free_ret)
{
	IRC_SERVER_REC *ircserver = IRC_SERVER(server);

	return ircserver == NULL ? "" : ircserver->real_address;
}

/* your /userhost $N address (user@host) */
static char *expando_userhost(SERVER_REC *server, void *item, int *free_ret)
{
	IRC_SERVER_REC *ircserver;
	const char *username;
	char hostname[100];

	ircserver = IRC_SERVER(server);

	/* prefer the _real_ /userhost reply */
	if (ircserver != NULL && ircserver->userhost != NULL)
		return ircserver->userhost;

	/* haven't received userhost reply yet. guess something */
	*free_ret = TRUE;
	if (ircserver == NULL)
		username = settings_get_str("user_name");
	else
		username = ircserver->connrec->username;

	if (gethostname(hostname, sizeof(hostname)) != 0 || *hostname == '\0')
		strcpy(hostname, "??");
	return g_strconcat(username, "@", hostname, NULL);;
}

/* user mode in active server */
static char *expando_usermode(SERVER_REC *server, void *item, int *free_ret)
{
	return IS_IRC_SERVER(server) ? IRC_SERVER(server)->usermode : "";
}

/* expands to your usermode on channel, op '@', halfop '%', "+" voice or other */
static char *expando_cumode(SERVER_REC *server, void *item, int *free_ret)
{
	if (IS_IRC_CHANNEL(item) && CHANNEL(item)->ownnick) {
		char other = NICK(CHANNEL(item)->ownnick)->other;
		if (other != '\0') {
			char *cumode = g_malloc(2);
			cumode[0] = other;
			cumode[1] = '\0';
			*free_ret = TRUE;
			return cumode;
		}

		return NICK(CHANNEL(item)->ownnick)->op ? "@" :
		       NICK(CHANNEL(item)->ownnick)->halfop ? "%" :
		       NICK(CHANNEL(item)->ownnick)->voice ? "+" : "";
	}
	return "";
}

/* expands to your usermode on channel,
   op '@', halfop '%', "+" voice, " " normal */
static char *expando_cumode_space(SERVER_REC *server, void *item, int *free_ret)
{
	char *ret;

	if (!IS_IRC_SERVER(server))
                return "";

	ret = expando_cumode(server, item, free_ret);
	return *ret == '\0' ? " " : ret;
}

static void event_join(IRC_SERVER_REC *server, const char *data,
		       const char *nick, const char *address)
{
	g_return_if_fail(nick != NULL);

	if (g_strcasecmp(nick, server->nick) != 0) {
		g_free_not_null(last_join);
		last_join = g_strdup(nick);
	}
}

void irc_expandos_init(void)
{
	last_join = NULL;

	expando_create(":", expando_lastjoin,
		       "event join", EXPANDO_ARG_SERVER, NULL);
	expando_create("H", expando_server_numeric,
		       "server event", EXPANDO_ARG_SERVER, NULL);
	expando_create("S", expando_servername,
		       "window changed", EXPANDO_ARG_NONE,
		       "window server changed", EXPANDO_ARG_WINDOW, NULL);
	expando_create("X", expando_userhost,
		       "window changed", EXPANDO_ARG_NONE,
		       "window server changed", EXPANDO_ARG_WINDOW, NULL);
	expando_create("usermode", expando_usermode,
		       "window changed", EXPANDO_ARG_NONE,
		       "window server changed", EXPANDO_ARG_WINDOW,
		       "user mode changed", EXPANDO_ARG_SERVER, NULL);
	expando_create("cumode", expando_cumode,
		       "window changed", EXPANDO_ARG_NONE,
		       "window item changed", EXPANDO_ARG_WINDOW,
		       "nick mode changed", EXPANDO_ARG_WINDOW_ITEM,
		       "channel joined", EXPANDO_ARG_WINDOW_ITEM, NULL);
	expando_create("cumode_space", expando_cumode_space,
		       "window changed", EXPANDO_ARG_NONE,
		       "window item changed", EXPANDO_ARG_WINDOW,
		       "nick mode changed", EXPANDO_ARG_WINDOW_ITEM,
		       "channel joined", EXPANDO_ARG_WINDOW_ITEM, NULL);

        expando_add_signal("I", "event invite", EXPANDO_ARG_SERVER);

	signal_add("event join", (SIGNAL_FUNC) event_join);
}

void irc_expandos_deinit(void)
{
	g_free_not_null(last_join);

	expando_destroy(":", expando_lastjoin);
	expando_destroy("H", expando_server_numeric);
	expando_destroy("S", expando_servername);
	expando_destroy("X", expando_userhost);
	expando_destroy("usermode", expando_usermode);
	expando_destroy("cumode", expando_cumode);

	signal_remove("event join", (SIGNAL_FUNC) event_join);
}
