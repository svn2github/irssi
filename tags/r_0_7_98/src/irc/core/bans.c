/*
 bans.c : irssi

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
#include "signals.h"
#include "commands.h"
#include "misc.h"

#include "irc-masks.h"
#include "modes.h"
#include "mode-lists.h"
#include "irc.h"
#include "nicklist.h"

static int bantype;

char *ban_get_mask(IRC_CHANNEL_REC *channel, const char *nick)
{
	NICK_REC *rec;
	char *str, *user, *host;

	g_return_val_if_fail(IS_IRC_CHANNEL(channel), NULL);
	g_return_val_if_fail(nick != NULL, NULL);

	rec = nicklist_find(CHANNEL(channel), nick);
	if (rec == NULL || rec->host == NULL) return NULL;

	str = irc_get_mask(nick, rec->host, bantype);

	/* there's a limit of 10 characters in user mask. so, banning
	   someone with user mask of 10 characters gives us "*1234567890",
	   which is one too much.. so, replace the 10th character with '*' */
	user = strchr(str, '!');
	if (user == NULL) return str;

	host = strchr(++user, '@');
	if (host == NULL) return str;

	if ((int) (host-user) > 10) {
		/* too long user mask */
		user[9] = '*';
		g_memmove(user+10, host, strlen(host)+1);
	}
	return str;
}

char *ban_get_masks(IRC_CHANNEL_REC *channel, const char *nicks)
{
	GString *str;
	char **ban, **banlist, *realban, *ret;

	str = g_string_new(NULL);
	banlist = g_strsplit(nicks, " ", -1);
	for (ban = banlist; *ban != NULL; ban++) {
		if (strchr(*ban, '!') != NULL) {
			/* explicit ban */
			g_string_sprintfa(str, "%s ", *ban);
			continue;
		}

		/* ban nick */
		realban = ban_get_mask(channel, *ban);
		if (realban != NULL) {
			g_string_sprintfa(str, "%s ", realban);
			g_free(realban);
		}
	}
	g_strfreev(banlist);

	if (str->len > 0)
		g_string_truncate(str, str->len-1);

	ret = str->str;
	g_string_free(str, FALSE);
        return ret;
}

void ban_set_type(const char *type)
{
	char bantypestr[5], **list;
	int n, max;

	g_return_if_fail(type != NULL);

	if (toupper(type[0]) == 'N') {
		bantype = IRC_MASK_USER | IRC_MASK_DOMAIN;
		strcpy(bantypestr, "UD");
	}
	else if (toupper(type[0]) == 'H') {
		bantype = IRC_MASK_HOST | IRC_MASK_DOMAIN;
		strcpy(bantypestr, "HD");
	}
	else if (toupper(type[0]) == 'D') {
		bantype = IRC_MASK_DOMAIN;
		strcpy(bantypestr, "D");
	}
	else if (toupper(type[0]) == 'C') {
		list = g_strsplit(type, " ", -1);

		max = strarray_length(list);
		bantype = 0;
                for (n = 1; n < max; n++) {
			if (toupper(list[n][0]) == 'N')
				bantype |= IRC_MASK_NICK;
			else if (toupper(list[n][0]) == 'U')
				bantype |= IRC_MASK_USER;
			else if (toupper(list[n][0]) == 'H')
				bantype |= IRC_MASK_HOST;
			else if (toupper(list[n][0]) == 'D')
				bantype |= IRC_MASK_DOMAIN;
		}
                g_strfreev(list);

		bantypestr[0] = '\0';
		if (bantype & IRC_MASK_NICK) strcat(bantypestr, "N");
		if (bantype & IRC_MASK_USER) strcat(bantypestr, "U");
		if (bantype & IRC_MASK_HOST) strcat(bantypestr, "H");
		if (bantype & IRC_MASK_DOMAIN) strcat(bantypestr, "D");
	}

	signal_emit("ban type changed", 1, bantypestr);
}

void ban_set(IRC_CHANNEL_REC *channel, const char *bans)
{
	char *masks;

	g_return_if_fail(bans != NULL);

	masks = ban_get_masks(channel, bans);
	channel_set_singlemode(channel->server, channel->name,
			       masks, "+b");
        g_free(masks);
}

void ban_remove(IRC_CHANNEL_REC *channel, const char *bans)
{
	GString *str;
	GSList *tmp;
	char **ban, **banlist;

	g_return_if_fail(bans != NULL);

	str = g_string_new(NULL);
	banlist = g_strsplit(bans, " ", -1);
	for (ban = banlist; *ban != NULL; ban++) {
		for (tmp = channel->banlist; tmp != NULL; tmp = tmp->next) {
			BAN_REC *rec = tmp->data;

			if (match_wildcards(*ban, rec->ban))
				g_string_sprintfa(str, "%s ", rec->ban);
		}
	}
	g_strfreev(banlist);

	if (str->len > 0)
		channel_set_singlemode(channel->server, channel->name,
				       str->str, "-b");
	g_string_free(str, TRUE);
}

static void command_set_ban(const char *data, IRC_SERVER_REC *server,
			    WI_ITEM_REC *item, int set)
{
	IRC_CHANNEL_REC *chanrec;
	char *channel, *nicks;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (server == NULL || !server->connected || !IS_IRC_SERVER(server))
		cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTCHAN | PARAM_FLAG_GETREST,
			    item, &channel, &nicks)) return;
	if (!ischannel(*channel)) cmd_param_error(CMDERR_NOT_JOINED);
	if (*nicks == '\0') {
		if (strcmp(data, "*") != 0)
			cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
                /* /BAN * or /UNBAN * - ban/unban everyone */
		nicks = (char *) data;
	}

	chanrec = irc_channel_find(server, channel);
	if (chanrec == NULL)
		cmd_param_error(CMDERR_CHAN_NOT_FOUND);
	if (!chanrec->wholist)
		cmd_param_error(CMDERR_CHAN_NOT_SYNCED);

	if (set)
		ban_set(chanrec, nicks);
	else {
		if (is_numeric(nicks, '\0')) {
			/* unban with ban number */
			BAN_REC *ban = g_slist_nth_data(chanrec->banlist,
							atoi(nicks)-1);
			if (ban != NULL)
                                nicks = ban->ban;
		}

		ban_remove(chanrec, nicks);
	}

	cmd_params_free(free_arg);
}

/* SYNTAX: BANTYPE normal|host|domain|custom
           BANTYPE custom [nick] [user] [host] [domain] */
static void cmd_bantype(const char *data)
{
        ban_set_type(data);
}

/* SYNTAX: BAN <nicks/masks> */
static void cmd_ban(const char *data, IRC_SERVER_REC *server, void *item)
{
	command_set_ban(data, server, item, TRUE);
}

/* SYNTAX: UNBAN <masks> */
static void cmd_unban(const char *data, IRC_SERVER_REC *server, void *item)
{
	command_set_ban(data, server, item, FALSE);
}

void bans_init(void)
{
	/* default bantype */
	bantype = IRC_MASK_USER | IRC_MASK_DOMAIN;
	command_bind("bantype", NULL, (SIGNAL_FUNC) cmd_bantype);
	command_bind("ban", NULL, (SIGNAL_FUNC) cmd_ban);
	command_bind("unban", NULL, (SIGNAL_FUNC) cmd_unban);
}

void bans_deinit(void)
{
	command_unbind("bantype", (SIGNAL_FUNC) cmd_bantype);
	command_unbind("ban", (SIGNAL_FUNC) cmd_ban);
	command_unbind("unban", (SIGNAL_FUNC) cmd_unban);
}
