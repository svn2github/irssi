/*
 fe-ignore.c : irssi

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
#include "module-formats.h"
#include "signals.h"
#include "commands.h"
#include "levels.h"
#include "misc.h"

#include "servers.h"
#include "ignore.h"
#include "printtext.h"

static char *ignore_get_key(IGNORE_REC *rec)
{
	char *chans, *ret;

	if (rec->channels == NULL)
		return g_strdup(rec->mask != NULL ? rec->mask : "*" );

	chans = g_strjoinv(",", rec->channels);
	if (rec->mask == NULL) return chans;

	ret = g_strdup_printf("%s %s", rec->mask, chans);
	g_free(chans);
	return ret;
}

static void ignore_print(int index, IGNORE_REC *rec)
{
	GString *options;
	char *key, *levels;

	key = ignore_get_key(rec);
	levels = bits2level(rec->level);

	options = g_string_new(NULL);
	if (rec->exception) g_string_sprintfa(options, "-except ");
	if (rec->regexp) g_string_sprintfa(options, "-regexp ");
	if (rec->fullword) g_string_sprintfa(options, "-full ");
	if (rec->replies) g_string_sprintfa(options, "-replies ");
	if (options->len > 1) g_string_truncate(options, options->len-1);

	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP,
		    TXT_IGNORE_LINE, index,
		    key != NULL ? key : "",
		    levels != NULL ? levels : "", options->str);
	g_string_free(options, TRUE);
        g_free(key);
	g_free(levels);
}

static void cmd_ignore_show(void)
{
	GSList *tmp;
	int index;

	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_IGNORE_HEADER);
	index = 1;
	for (tmp = ignores; tmp != NULL; tmp = tmp->next, index++) {
		IGNORE_REC *rec = tmp->data;

		ignore_print(index, rec);
	}
	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_IGNORE_FOOTER);
}

/* SYNTAX: IGNORE [-regexp | -full] [-pattern <pattern>] [-except] [-replies]
                  [-channels <channel>] [-time <secs>] <mask> [<levels>]
           IGNORE [-regexp | -full] [-pattern <pattern>] [-except] [-replies]
	          [-time <secs>] <channels> [<levels>] */
static void cmd_ignore(const char *data)
{
	GHashTable *optlist;
	IGNORE_REC *rec;
	char *patternarg, *chanarg, *mask, *levels, *timestr;
	char **channels;
	void *free_arg;
	int new_ignore;

	if (*data == '\0') {
		cmd_ignore_show();
		return;
	}

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTIONS | PARAM_FLAG_GETREST,
			    "ignore", &optlist, &mask, &levels))
		return;

	patternarg = g_hash_table_lookup(optlist, "pattern");
        chanarg = g_hash_table_lookup(optlist, "channels");

	if (*mask == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
        if (*levels == '\0') levels = "ALL";

	if (active_win->active_server != NULL &&
	    active_win->active_server->ischannel(mask)) {
		chanarg = mask;
		mask = NULL;
	}
	channels = (chanarg == NULL || *chanarg == '\0') ? NULL :
		g_strsplit(replace_chars(chanarg, ',', ' '), " ", -1);

	rec = ignore_find(NULL, mask, channels);
	new_ignore = rec == NULL;

	if (rec == NULL) {
		rec = g_new0(IGNORE_REC, 1);

		rec->mask = mask == NULL || *mask == '\0' ||
			strcmp(mask, "*") == 0 ? NULL : g_strdup(mask);
		rec->channels = channels;
	} else {
                g_free_and_null(rec->pattern);
		g_strfreev(channels);
	}

	rec->level = combine_level(rec->level, levels);

	if (new_ignore && rec->level == 0) {
		/* tried to unignore levels from nonexisting ignore */
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			    TXT_IGNORE_NOT_FOUND, rec->mask);
		g_free(rec->mask);
		g_strfreev(rec->channels);
		g_free(rec);
		cmd_params_free(free_arg);
                return;
	}

	rec->pattern = (patternarg == NULL || *patternarg == '\0') ?
		NULL : g_strdup(patternarg);
	rec->exception = g_hash_table_lookup(optlist, "except") != NULL;
	rec->regexp = g_hash_table_lookup(optlist, "regexp") != NULL;
	rec->fullword = g_hash_table_lookup(optlist, "full") != NULL;
	rec->replies = g_hash_table_lookup(optlist, "replies") != NULL;
	timestr = g_hash_table_lookup(optlist, "time");
        if (timestr != NULL)
		rec->unignore_time = time(NULL)+atoi(timestr);

	if (new_ignore)
		ignore_add_rec(rec);
	else
		ignore_update_rec(rec);

	cmd_params_free(free_arg);
}

/* SYNTAX: UNIGNORE <id>|<mask> */
static void cmd_unignore(const char *data)
{
	IGNORE_REC *rec;
	GSList *tmp;

	if (*data == '\0')
                cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	if (is_numeric(data, ' ')) {
		/* with index number */
		tmp = g_slist_nth(ignores, atoi(data)-1);
		rec = tmp == NULL ? NULL : tmp->data;
	} else {
		/* with mask */
		const char *chans[2] = { "*", NULL };

		if (active_win->active_server != NULL &&
		    active_win->active_server->ischannel(data)) {
			chans[0] = data;
			data = NULL;
		}
		rec = ignore_find("*", data, (char **) chans);
	}

	if (rec != NULL) {
		rec->level = 0;
		ignore_update_rec(rec);
	} else {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			    TXT_IGNORE_NOT_FOUND, data);
	}
}

static void sig_ignore_created(IGNORE_REC *rec)
{
	char *key, *levels;

	key = ignore_get_key(rec);
	levels = bits2level(rec->level);
	printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		    TXT_IGNORED, key, levels);
	g_free(key);
	g_free(levels);
}

static void sig_ignore_destroyed(IGNORE_REC *rec)
{
	char *key;

	key = ignore_get_key(rec);
	printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_UNIGNORED, key);
	g_free(key);
}

void fe_ignore_init(void)
{
	command_bind("ignore", NULL, (SIGNAL_FUNC) cmd_ignore);
	command_bind("unignore", NULL, (SIGNAL_FUNC) cmd_unignore);

	signal_add("ignore destroyed", (SIGNAL_FUNC) sig_ignore_destroyed);
	signal_add("ignore created", (SIGNAL_FUNC) sig_ignore_created);
	signal_add("ignore changed", (SIGNAL_FUNC) sig_ignore_created);

	command_set_options("ignore", "regexp full except replies -time -pattern -channels");
}

void fe_ignore_deinit(void)
{
	command_unbind("ignore", (SIGNAL_FUNC) cmd_ignore);
	command_unbind("unignore", (SIGNAL_FUNC) cmd_unignore);

	signal_remove("ignore destroyed", (SIGNAL_FUNC) sig_ignore_destroyed);
	signal_remove("ignore created", (SIGNAL_FUNC) sig_ignore_created);
	signal_remove("ignore changed", (SIGNAL_FUNC) sig_ignore_created);
}
