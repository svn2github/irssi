/*
 irc-nicklist.c : irssi

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
#include "misc.h"

#include "irc-servers.h"
#include "irc-channels.h"
#include "irc-masks.h"
#include "irc-nicklist.h"
#include "modes.h"
#include "servers.h"

/* Add new nick to list */
NICK_REC *irc_nicklist_insert(IRC_CHANNEL_REC *channel, const char *nick,
			      int op, int halfop, int voice, int send_massjoin)
{
	NICK_REC *rec;

	g_return_val_if_fail(IS_IRC_CHANNEL(channel), NULL);
	g_return_val_if_fail(nick != NULL, NULL);

	rec = g_new0(NICK_REC, 1);
	rec->nick = g_strdup(nick);

	if (op) rec->op = TRUE;
	if (halfop) rec->halfop = TRUE;
	if (voice) rec->voice = TRUE;
	rec->send_massjoin = send_massjoin;

	nicklist_insert(CHANNEL(channel), rec);
	return rec;
}

#define isnickchar(a) \
	(i_isalnum(a) || (a) == '`' || (a) == '-' || (a) == '_' || \
	(a) == '[' || (a) == ']' || (a) == '{' || (a) == '}' || \
	(a) == '|' || (a) == '\\' || (a) == '^')

/* Remove all "extra" characters from `nick'. Like _nick_ -> nick */
char *irc_nick_strip(const char *nick)
{
	char *stripped, *spos;

	g_return_val_if_fail(nick != NULL, NULL);

	spos = stripped = g_strdup(nick);
	while (isnickchar(*nick)) {
		if (i_isalnum(*nick))
			*spos++ = *nick;
		nick++;
	}
	if ((unsigned char) *nick >= 128)
		*spos++ = *nick; /* just add it so that nicks won't match.. */
	*spos = '\0';
	return stripped;
}

static void event_names_list(IRC_SERVER_REC *server, const char *data)
{
	IRC_CHANNEL_REC *chanrec;
	char *params, *type, *channel, *names, *ptr;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 4, NULL, &type, &channel, &names);

	chanrec = irc_channel_find(server, channel);
	if (chanrec == NULL || chanrec->names_got) {
		/* unknown channel / names list already read */
		g_free(params);
		return;
	}

	/* type = '=' = public, '*' = private, '@' = secret.

	   This is actually pretty useless to check here, but at least we
	   get to know if the channel is +p or +s a few seconds before
	   we receive the MODE reply...

	   If the channel key is set, assume the channel is +k also until
           we know better, so parse_channel_modes() won't clear the key */
	if (*type == '*') {
		parse_channel_modes(chanrec, NULL,
				    chanrec->key ? "+kp" : "+p");
	} else if (*type == '@') {
		parse_channel_modes(chanrec, NULL,
				    chanrec->key ? "+ks" : "+s");
	}

	while (*names != '\0') {
		while (*names == ' ') names++;
		ptr = names;
		while (*names != '\0' && *names != ' ') names++;
		if (*names != '\0') *names++ = '\0';

		irc_nicklist_insert(chanrec, ptr+isnickflag(*ptr),
				    *ptr == '@', *ptr == '%', *ptr == '+',
				    FALSE);
	}

	g_free(params);
}

static void event_end_of_names(IRC_SERVER_REC *server, const char *data)
{
	char *params, *channel;
	IRC_CHANNEL_REC *chanrec;
	NICK_REC *ownnick;
	int nicks;
	
	g_return_if_fail(server != NULL);

	params = event_get_params(data, 2, NULL, &channel);

	chanrec = irc_channel_find(server, channel);
	if (chanrec != NULL && !chanrec->names_got) {
		ownnick = nicklist_find(CHANNEL(chanrec), server->nick);
		if (ownnick == NULL) {
			/* stupid server - assume we have ops
			   if channel is empty */
			nicks = g_hash_table_size(chanrec->nicks);
			ownnick = irc_nicklist_insert(chanrec, server->nick,
						      nicks == 0, FALSE,
						      FALSE, FALSE);
		}
		nicklist_set_own(CHANNEL(chanrec), ownnick);
                chanrec->chanop = chanrec->ownnick->op;
		chanrec->names_got = TRUE;
		signal_emit("channel joined", 1, chanrec);
	}

	g_free(params);
}

static void event_who(SERVER_REC *server, const char *data)
{
	char *params, *nick, *channel, *user, *host, *stat, *realname, *hops;
	CHANNEL_REC *chanrec;
	NICK_REC *nickrec;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 8, NULL, &channel, &user, &host,
				  NULL, &nick, &stat, &realname);

	/* get hop count */
	hops = realname;
	while (*realname != '\0' && *realname != ' ') realname++;
	*realname++ = '\0';
	while (*realname == ' ') realname++;

	/* update host, realname, hopcount */
	chanrec = channel_find(server, channel);
	nickrec = chanrec == NULL ? NULL :
		nicklist_find(chanrec, nick);
	if (nickrec != NULL) {
		if (nickrec->host == NULL) {
                        char *str = g_strdup_printf("%s@%s", user, host);
			nicklist_set_host(chanrec, nickrec, str);
                        g_free(str);
		}
		if (nickrec->realname == NULL)
			nickrec->realname = g_strdup(realname);
		sscanf(hops, "%d", &nickrec->hops);
	}

	nicklist_update_flags(server, nick,
			      strchr(stat, 'G') != NULL, /* gone */
			      strchr(stat, '*') != NULL); /* ircop */

	g_free(params);
}

static void event_whois(IRC_SERVER_REC *server, const char *data)
{
	char *params, *nick, *realname;
	GSList *nicks, *tmp;
	NICK_REC *rec;

	g_return_if_fail(data != NULL);

	/* first remove the gone-flag, if user is gone
	   it will be set later.. */
	params = event_get_params(data, 6, NULL, &nick, NULL,
				  NULL, NULL, &realname);

	nicks = nicklist_get_same(SERVER(server), nick);
	for (tmp = nicks; tmp != NULL; tmp = tmp->next->next) {
		rec = tmp->next->data;

		if (rec->realname == NULL)
			rec->realname = g_strdup(realname);
	}
	g_slist_free(nicks);

	/* reset gone and ircop status, we'll handle them in the following
	   WHOIS replies */
	nicklist_update_flags(SERVER(server), nick, FALSE, FALSE);
	g_free(params);
}

static void event_whois_away(SERVER_REC *server, const char *data)
{
	char *params, *nick, *awaymsg;

	g_return_if_fail(data != NULL);

	/* set user's gone flag.. */
	params = event_get_params(data, 3, NULL, &nick, &awaymsg);
	nicklist_update_flags(server, nick, TRUE, -1);
	g_free(params);
}

static void event_whois_ircop(SERVER_REC *server, const char *data)
{
	char *params, *nick, *awaymsg;

	g_return_if_fail(data != NULL);

	/* set user's gone flag.. */
	params = event_get_params(data, 3, NULL, &nick, &awaymsg);
	nicklist_update_flags(server, nick, -1, TRUE);
	g_free(params);
}

static void event_nick_in_use(IRC_SERVER_REC *server, const char *data)
{
	char *str;
	int n;

	g_return_if_fail(data != NULL);

	if (server->connected) {
		/* Already connected, no need to handle this anymore. */
		return;
	}

	/* nick already in use - need to change it .. */
	if (g_strcasecmp(server->nick, server->connrec->nick) == 0 &&
	    server->connrec->alternate_nick != NULL &&
	    g_strcasecmp(server->connrec->alternate_nick, server->nick) != 0) {
		/* first try, so try the alternative nick.. */
		g_free(server->nick);
		server->nick = g_strdup(server->connrec->alternate_nick);
	}
	else if (strlen(server->nick) < 9) {
		/* keep adding '_' to end of nick.. */
		str = g_strdup_printf("%s_", server->nick);
		g_free(server->nick);
		server->nick = str;
	} else {
		/* nick full, keep adding number at the end */
		for (n = 8; n > 0; n--) {
			if (server->nick[n] < '0' || server->nick[n] > '9') {
				server->nick[n] = '1';
				break;
			}

			if (server->nick[n] < '9') {
				server->nick[n]++;
				break;
			}
			server->nick[n] = '0';
		}
	}

	irc_send_cmdv(server, "NICK %s", server->nick);
}

static void event_target_unavailable(IRC_SERVER_REC *server, const char *data)
{
	char *params, *channel;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 2, NULL, &channel);
	if (!ischannel(*channel)) {
		/* nick is unavailable. */
		event_nick_in_use(server, data);
	}

	g_free(params);
}

static void event_nick(IRC_SERVER_REC *server, const char *data,
		       const char *orignick)
{
	char *params, *nick;

	g_return_if_fail(data != NULL);

	params = event_get_params(data, 1, &nick);

	if (g_strcasecmp(orignick, server->nick) == 0) {
		/* You changed your nick */
		if (server->last_nick != NULL &&
		    g_strcasecmp(server->last_nick, nick) == 0) {
                        /* changed with /NICK - keep it as wanted nick */
			g_free(server->connrec->nick);
			server->connrec->nick = g_strdup(nick);
		}

		server_change_nick(SERVER(server), nick);
	}

        nicklist_rename(SERVER(server), orignick, nick);
	g_free(params);
}

static void event_userhost(SERVER_REC *server, const char *data)
{
	char *params, *hosts, **phosts, **pos, *ptr;

	g_return_if_fail(data != NULL);

	/* set user's gone flag.. */
	params = event_get_params(data, 2, NULL, &hosts);

	phosts = g_strsplit(hosts, " ", -1);
	for (pos = phosts; *pos != NULL; pos++) {
		ptr = strchr(*pos, '=');
		if (ptr == NULL) continue;
		*ptr++ = '\0';

		nicklist_update_flags(server, *pos, *ptr == '-', -1);
	}
	g_strfreev(phosts);
	g_free(params);
}

static void sig_usermode(SERVER_REC *server)
{
	g_return_if_fail(IS_SERVER(server));

	nicklist_update_flags(server, server->nick, server->usermode_away, -1);
}

static const char *get_nick_flags(void)
{
        return "@+%";
}

static void sig_connected(IRC_SERVER_REC *server)
{
	if (IS_IRC_SERVER(server)) {
		server->get_nick_flags =
			(const char *(*)(void)) get_nick_flags;
	}
}

void irc_nicklist_init(void)
{
	signal_add_first("event nick", (SIGNAL_FUNC) event_nick);
	signal_add_first("event 352", (SIGNAL_FUNC) event_who);
	signal_add("silent event who", (SIGNAL_FUNC) event_who);
	signal_add("silent event whois", (SIGNAL_FUNC) event_whois);
	signal_add_first("event 311", (SIGNAL_FUNC) event_whois);
	signal_add_first("event 301", (SIGNAL_FUNC) event_whois_away);
	signal_add_first("event 313", (SIGNAL_FUNC) event_whois_ircop);
	signal_add_first("event 353", (SIGNAL_FUNC) event_names_list);
	signal_add_first("event 366", (SIGNAL_FUNC) event_end_of_names);
	signal_add_first("event 433", (SIGNAL_FUNC) event_nick_in_use);
	signal_add_first("event 437", (SIGNAL_FUNC) event_target_unavailable);
	signal_add_first("event 302", (SIGNAL_FUNC) event_userhost);
	signal_add("userhost event", (SIGNAL_FUNC) event_userhost);
	signal_add("user mode changed", (SIGNAL_FUNC) sig_usermode);
	signal_add("server connected", (SIGNAL_FUNC) sig_connected);
}

void irc_nicklist_deinit(void)
{
	signal_remove("event nick", (SIGNAL_FUNC) event_nick);
	signal_remove("event 352", (SIGNAL_FUNC) event_who);
	signal_remove("silent event who", (SIGNAL_FUNC) event_who);
	signal_remove("silent event whois", (SIGNAL_FUNC) event_whois);
	signal_remove("event 311", (SIGNAL_FUNC) event_whois);
	signal_remove("event 301", (SIGNAL_FUNC) event_whois_away);
	signal_remove("event 313", (SIGNAL_FUNC) event_whois_ircop);
	signal_remove("event 353", (SIGNAL_FUNC) event_names_list);
	signal_remove("event 366", (SIGNAL_FUNC) event_end_of_names);
	signal_remove("event 433", (SIGNAL_FUNC) event_nick_in_use);
	signal_remove("event 437", (SIGNAL_FUNC) event_target_unavailable);
	signal_remove("event 302", (SIGNAL_FUNC) event_userhost);
	signal_remove("userhost event", (SIGNAL_FUNC) event_userhost);
	signal_remove("user mode changed", (SIGNAL_FUNC) sig_usermode);
	signal_remove("server connected", (SIGNAL_FUNC) sig_connected);
}
