/*
 fe-settings.c : irssi

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
#include "module-formats.h"
#include "signals.h"
#include "commands.h"
#include "servers.h"
#include "misc.h"
#include "lib-config/iconfig.h"
#include "settings.h"

#include "levels.h"
#include "printtext.h"
#include "keyboard.h"

static void set_print(SETTINGS_REC *rec)
{
	const char *value;
	char value_int[MAX_INT_STRLEN];

	switch (rec->type) {
	case SETTING_TYPE_BOOLEAN:
		value = settings_get_bool(rec->key) ? "ON" : "OFF";
		break;
	case SETTING_TYPE_INT:
		ltoa(value_int, settings_get_int(rec->key));
		value = value_int;
		break;
	case SETTING_TYPE_STRING:
		value = settings_get_str(rec->key);
		break;
	default:
		value = "";
	}
	printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "%s = %s", rec->key, value);
}

static void set_boolean(const char *key, const char *value)
{
	if (g_strcasecmp(value, "ON") == 0)
		settings_set_bool(key, TRUE);
	else if (g_strcasecmp(value, "OFF") == 0)
		settings_set_bool(key, FALSE);
	else if (g_strcasecmp(value, "TOGGLE") == 0)
		settings_set_bool(key, !settings_get_bool(key));
	else
		printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_NOT_TOGGLE);
}

/* SYNTAX: SET [-clear] [<key> [<value>] */
static void cmd_set(char *data)
{
        GHashTable *optlist;
	GSList *sets, *tmp;
	const char *last_section;
	char *key, *value;
	void *free_arg;
	int found, clear;

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST | PARAM_FLAG_OPTIONS,
			    "set", &optlist, &key, &value))
		return;

	clear = g_hash_table_lookup(optlist, "clear") != NULL;

	last_section = ""; found = 0;
	sets = settings_get_sorted();
	for (tmp = sets; tmp != NULL; tmp = tmp->next) {
		SETTINGS_REC *rec = tmp->data;

		if (((clear || *value != '\0') && g_strcasecmp(rec->key, key) != 0) ||
		    (*value == '\0' && *key != '\0' && stristr(rec->key, key) == NULL))
			continue;

		if (strcmp(last_section, rec->section) != 0) {
			/* print section */
			printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "%_[ %s ]", rec->section);
			last_section = rec->section;
		}

		if (clear || *value != '\0') {
			/* change the setting */
			switch (rec->type) {
			case SETTING_TYPE_BOOLEAN:
                                if (clear)
					settings_set_bool(key, FALSE);
                                else
					set_boolean(key, value);
				break;
			case SETTING_TYPE_INT:
				settings_set_int(key, clear ? 0 : atoi(value));
				break;
			case SETTING_TYPE_STRING:
				settings_set_str(key, clear ? "" : value);
				break;
			}
			signal_emit("setup changed", 0);
		}

                set_print(rec);
		found = TRUE;

		if (clear || *value != '\0')
			break;
	}
	g_slist_free(sets);

        if (!found)
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR, "Unknown setting %s", key);

        cmd_params_free(free_arg);
}

/* SYNTAX: TOGGLE <key> [on|off|toggle] */
static void cmd_toggle(const char *data)
{
	char *key, *value;
	void *free_arg;
	int type;

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &key, &value))
		return;

        if (*key == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	type = settings_get_type(key);
        if (type == -1)
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR, "Unknown setting %_%s", key);
	else if (type != SETTING_TYPE_BOOLEAN)
		printtext(NULL, NULL, MSGLEVEL_CLIENTERROR, "Setting %_%s%_ isn't boolean, use /SET", key);
	else {
		set_boolean(key, *value != '\0' ? value : "TOGGLE");
                set_print(settings_get_record(key));
	}

        cmd_params_free(free_arg);
}

static int config_key_compare(CONFIG_NODE *node1, CONFIG_NODE *node2)
{
	return g_strcasecmp(node1->key, node2->key);
}

static void show_aliases(const char *alias)
{
	CONFIG_NODE *node;
	GSList *tmp, *list;
	int aliaslen;

	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_ALIASLIST_HEADER);

	node = iconfig_node_traverse("aliases", FALSE);
	tmp = node == NULL ? NULL : node->value;

	/* first get the list of aliases sorted */
	list = NULL;
	aliaslen = strlen(alias);
	for (; tmp != NULL; tmp = tmp->next) {
		CONFIG_NODE *node = tmp->data;

		if (node->type != NODE_TYPE_KEY)
			continue;

		if (aliaslen != 0 && g_strncasecmp(node->key, alias, aliaslen) != 0)
			continue;

		list = g_slist_insert_sorted(list, node, (GCompareFunc) config_key_compare);
	}

	/* print the aliases */
	for (tmp = list; tmp != NULL; tmp = tmp->next) {
		CONFIG_NODE *node = tmp->data;

		printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_ALIASLIST_LINE,
			    node->key, node->value);
	}
	g_slist_free(list);

	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_ALIASLIST_FOOTER);
}

static void alias_remove(const char *alias)
{
	if (iconfig_get_str("aliases", alias, NULL) == NULL)
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_ALIAS_NOT_FOUND, alias);
	else {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_ALIAS_REMOVED, alias);
		iconfig_set_str("aliases", alias, NULL);
	}
}

/* SYNTAX: ALIAS [[-]<alias> [<command>]] */
static void cmd_alias(const char *data)
{
	char *alias, *value;
	void *free_arg;

	g_return_if_fail(data != NULL);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &alias, &value))
		return;

	if (*alias == '-') {
		if (alias[1] != '\0') alias_remove(alias+1);
	} else if (*alias == '\0' || *value == '\0')
		show_aliases(alias);
	else {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_ALIAS_ADDED, alias);
		iconfig_set_str("aliases", alias, value);
	}
        cmd_params_free(free_arg);
}

/* SYNTAX: UNALIAS <alias> */
static void cmd_unalias(const char *data)
{
	g_return_if_fail(data != NULL);
	if (*data == '\0') cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	alias_remove(data);
}

/* SYNTAX: RELOAD [<file>] */
static void cmd_reload(const char *data)
{
	char *fname;

	fname = *data != '\0' ? g_strdup(data) :
		g_strdup_printf("%s/.irssi/config", g_get_home_dir());
	if (settings_reread(fname)) {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			    TXT_CONFIG_RELOADED, fname);
	}
	g_free(fname);
}

static void settings_save_fe(const char *fname)
{
	if (settings_save(fname)) {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
			    TXT_CONFIG_SAVED, fname);
	}
}

static void settings_save_confirm(const char *line, char *fname)
{
	if (g_strncasecmp(line, _("Y"), 1) == 0)
		settings_save_fe(fname);
	g_free(fname);
}

/* SYNTAX: SAVE [<file>] */
static void cmd_save(const char *data)
{
	if (*data == '\0')
		data = mainconfig->fname;

	if (!irssi_config_is_changed(data)) {
		settings_save_fe(data);
		return;
	}

	printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		    TXT_CONFIG_MODIFIED, data);
	keyboard_entry_redirect((SIGNAL_FUNC) settings_save_confirm,
				_("Overwrite config (y/N)?"),
				0, g_strdup(data));
}

void fe_settings_init(void)
{
	command_bind("set", NULL, (SIGNAL_FUNC) cmd_set);
	command_bind("toggle", NULL, (SIGNAL_FUNC) cmd_toggle);
	command_bind("alias", NULL, (SIGNAL_FUNC) cmd_alias);
	command_bind("unalias", NULL, (SIGNAL_FUNC) cmd_unalias);
	command_bind("reload", NULL, (SIGNAL_FUNC) cmd_reload);
	command_bind("save", NULL, (SIGNAL_FUNC) cmd_save);

	command_set_options("set", "clear");
}

void fe_settings_deinit(void)
{
	command_unbind("set", (SIGNAL_FUNC) cmd_set);
	command_unbind("toggle", (SIGNAL_FUNC) cmd_toggle);
	command_unbind("alias", (SIGNAL_FUNC) cmd_alias);
	command_unbind("unalias", (SIGNAL_FUNC) cmd_unalias);
	command_unbind("reload", (SIGNAL_FUNC) cmd_reload);
	command_unbind("save", (SIGNAL_FUNC) cmd_save);
}
