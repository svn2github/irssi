/*

 flood.c : Flood protection (see also ctcp.c)

    Copyright (C) 1999 Timo Sirainen

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
#include "modules.h"
#include "signals.h"
#include "levels.h"
#include "misc.h"
#include "settings.h"

#include "irc.h"
#include "irc-server.h"
#include "autoignore.h"
#include "ignore.h"

typedef struct {
	char *nick;
	int level;
	int msgcount;
} FLOOD_REC;

static int flood_tag;
static int flood_max_msgs;

static int flood_hash_deinit(const char *key, FLOOD_REC *rec)
{
	g_return_val_if_fail(key != NULL, FALSE);
	g_return_val_if_fail(rec != NULL, FALSE);

	g_free(rec->nick);
	g_free(rec);
	return TRUE;
}

/* timeout function: flood protection */
static int flood_timeout(void)
{
	MODULE_SERVER_REC *mserver;
	GSList *tmp;

	/* remove everyone from flood list */
	for (tmp = servers; tmp != NULL; tmp = tmp->next) {
		IRC_SERVER_REC *rec = tmp->data;

		mserver = MODULE_DATA(rec);
		g_hash_table_foreach_remove(mserver->floodlist, (GHRFunc) flood_hash_deinit, NULL);
	}
	return 1;
}

/* Initialize flood protection */
static void flood_init_server(IRC_SERVER_REC *server)
{
	MODULE_SERVER_REC *rec;

	g_return_if_fail(server != NULL);

	rec = g_new0(MODULE_SERVER_REC, 1);
	MODULE_DATA_SET(server, rec);

	rec->floodlist = g_hash_table_new((GHashFunc) g_istr_hash, (GCompareFunc) g_istr_equal);
}

/* Deinitialize flood protection */
static void flood_deinit_server(IRC_SERVER_REC *server)
{
	MODULE_SERVER_REC *mserver;

	g_return_if_fail(server != NULL);

	mserver = MODULE_DATA(server);
	if (mserver != NULL && mserver->floodlist != NULL) {
		g_hash_table_freeze(mserver->floodlist);
		g_hash_table_foreach(mserver->floodlist, (GHFunc) flood_hash_deinit, NULL);
		g_hash_table_thaw(mserver->floodlist);
		g_hash_table_destroy(mserver->floodlist);
	}
	g_free(mserver);
}

/* All messages should go through here.. */
static void flood_newmsg(IRC_SERVER_REC *server, int level, const char *nick, const char *host, const char *target)
{
	MODULE_SERVER_REC *mserver;
	FLOOD_REC *rec;
	char *levelstr;

	g_return_if_fail(server != NULL);
	g_return_if_fail(nick != NULL);

	mserver = MODULE_DATA(server);
	rec = g_hash_table_lookup(mserver->floodlist, nick);
	if (rec != NULL) {
		if (++rec->msgcount > flood_max_msgs) {
			/* flooding! */
                        levelstr = bits2level(rec->level);
			signal_emit("flood", 5, server, nick, host, levelstr, target);
			g_free(levelstr);
		}
		return;
	}

	rec = g_new(FLOOD_REC, 1);
	rec->level = level;
	rec->msgcount = 1;
	rec->nick = g_strdup(nick);

	g_hash_table_insert(mserver->floodlist, rec->nick, rec);
}

static void flood_privmsg(const char *data, IRC_SERVER_REC *server, const char *nick, const char *addr)
{
	int publiclevel;
	char *params, *target, *text;

	g_return_if_fail(data != NULL);
	g_return_if_fail(server != NULL);

	if (nick == NULL) {
		/* don't try to ignore server messages.. */
		return;
	}

	params = event_get_params(data, 2, &target, &text);

	if (*text == 1) {
		/* CTCP */
		if (!ignore_check(server, nick, addr, target, text, MSGLEVEL_CTCPS))
			flood_newmsg(server, MSGLEVEL_CTCPS, nick, addr, target);
	} else {
		publiclevel = ischannel(*target) ? MSGLEVEL_PUBLIC : MSGLEVEL_MSGS;

		if (addr != NULL && !ignore_check(server, nick, addr, target, text, publiclevel))
			flood_newmsg(server, publiclevel, nick, addr, target);
	}

	g_free(params);
}

static void flood_notice(const char *data, IRC_SERVER_REC *server, const char *nick, const char *addr)
{
	char *params, *target, *text;

	g_return_if_fail(text != NULL);
	g_return_if_fail(server != NULL);

	if (nick == NULL) {
		/* don't try to ignore server messages.. */
		return;
	}

	params = event_get_params(data, 2, &target, &text);
	if (addr != NULL && !ignore_check(server, nick, addr, target, text, MSGLEVEL_NOTICES))
		flood_newmsg(server, MSGLEVEL_NOTICES | ischannel(*target) ? MSGLEVEL_PUBLIC : MSGLEVEL_MSGS, nick, addr, target);

	g_free(params);
}

static void read_settings(void)
{
	if (flood_tag != -1) g_source_remove(flood_tag);
	flood_tag = g_timeout_add(settings_get_int("flood_timecheck"), (GSourceFunc) flood_timeout, NULL);

	flood_max_msgs = settings_get_int("flood_max_msgs");
}

void flood_init(void)
{
	settings_add_int("flood", "flood_timecheck", 1000);
	settings_add_int("flood", "flood_max_msgs", 5);

	flood_tag = -1;
	read_settings();
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);
	signal_add_first("server connected", (SIGNAL_FUNC) flood_init_server);
	signal_add("server disconnected", (SIGNAL_FUNC) flood_deinit_server);
	signal_add("event privmsg", (SIGNAL_FUNC) flood_privmsg);
	signal_add("event notice", (SIGNAL_FUNC) flood_notice);

	autoignore_init();
}

void flood_deinit(void)
{
	autoignore_deinit();

	if (flood_tag != -1) g_source_remove(flood_tag);

	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
	signal_remove("server connected", (SIGNAL_FUNC) flood_init_server);
	signal_remove("server disconnected", (SIGNAL_FUNC) flood_deinit_server);
	signal_remove("event privmsg", (SIGNAL_FUNC) flood_privmsg);
	signal_remove("event notice", (SIGNAL_FUNC) flood_notice);
}
