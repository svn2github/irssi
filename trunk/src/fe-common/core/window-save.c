/*
 window-save.c : irssi

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
#include "misc.h"
#include "levels.h"
#include "lib-config/iconfig.h"
#include "settings.h"

#include "chat-protocols.h"
#include "servers.h"
#include "queries.h"

#include "themes.h"
#include "windows.h"
#include "window-items.h"

static void sig_window_restore_item(WINDOW_REC *window, const char *type,
				    CONFIG_NODE *node)
{
	char *name, *tag, *chat_type, *str;

	chat_type = config_node_get_str(node, "chat_type", NULL);
	name = config_node_get_str(node, "name", NULL);
	tag = config_node_get_str(node, "tag", NULL);
	if (name == NULL) return;

	if (g_strcasecmp(type, "CHANNEL") == 0) {
		/* add channel to "waiting channels" list */
		str = tag == NULL ? g_strdup(name) :
			g_strdup_printf("%s %s", tag, name);

		window->waiting_channels =
			g_slist_append(window->waiting_channels, str);
	} else if (g_strcasecmp(type, "QUERY") == 0) {
		/* create query immediately */
		QUERY_REC *query;
		SERVER_REC *server;

		if (chat_type == NULL)
			return;

		server = tag == NULL ? NULL : server_find_tag(tag);
		query = query_create(chat_protocol_lookup(chat_type),
				     server, name, TRUE);
		if (server == NULL && tag != NULL)
                        query->server_tag = g_strdup(tag);
	}
}

static void window_add_items(WINDOW_REC *window, CONFIG_NODE *node)
{
	GSList *tmp;
	char *type;

	if (node == NULL)
		return;

	for (tmp = node->value; tmp != NULL; tmp = tmp->next) {
		CONFIG_NODE *node = tmp->data;

		type = config_node_get_str(node->value, "type", NULL);
		if (type != NULL) {
			signal_emit("window restore item", 3,
				    window, type, node->value);
		}
	}
}

void windows_restore(void)
{
	WINDOW_REC *window;
	CONFIG_NODE *node;
	GSList *tmp;

	node = iconfig_node_traverse("windows", FALSE);
	if (node == NULL) return;

	for (tmp = node->value; tmp != NULL; tmp = tmp->next) {
		CONFIG_NODE *node = tmp->data;

		window = window_create(NULL, TRUE);
		window_set_refnum(window, atoi(node->key));
                window_set_name(window, config_node_get_str(node, "name", NULL));
		window_set_level(window, level2bits(config_node_get_str(node, "level", "")));

		window->theme_name = g_strdup(config_node_get_str(node, "theme", NULL));
		if (window->theme_name != NULL)
			window->theme = theme_load(window->theme_name);

		window_add_items(window, config_node_section(node, "items", -1));
		signal_emit("window restore", 2, window, node);
	}

	signal_emit("windows restored", 0);
}

static void window_save_items(WINDOW_REC *window, CONFIG_NODE *node)
{
	CONFIG_NODE *subnode;
	GSList *tmp;
	const char *type;

	node = config_node_section(node, "items", NODE_TYPE_LIST);
	for (tmp = window->items; tmp != NULL; tmp = tmp->next) {
		WI_ITEM_REC *rec = tmp->data;
		SERVER_REC *server = rec->server;

		type = module_find_id_str("WINDOW ITEM TYPE", rec->type);
		if (type == NULL) continue;

		subnode = config_node_section(node, NULL, NODE_TYPE_BLOCK);

		iconfig_node_set_str(subnode, "type", type);
		iconfig_node_set_str(subnode, "chat_type",
				     chat_protocol_get_name(rec->chat_type));
		iconfig_node_set_str(subnode, "name", rec->name);

		if (server != NULL)
			iconfig_node_set_str(subnode, "tag", server->tag);
	}
}

static void window_save(WINDOW_REC *window, CONFIG_NODE *node)
{
	char refnum[MAX_INT_STRLEN];

        ltoa(refnum, window->refnum);
	node = config_node_section(node, refnum, NODE_TYPE_BLOCK);

	if (window->name != NULL)
		iconfig_node_set_str(node, "name", window->name);
	if (window->level != 0) {
                char *level = bits2level(window->level);
		iconfig_node_set_str(node, "level", level);
		g_free(level);
	}
	if (window->theme_name != NULL)
		iconfig_node_set_str(node, "theme", window->theme_name);

	if (window->items != NULL)
		window_save_items(window, node);

	signal_emit("window save", 2, window, node);
}

void windows_save(void)
{
	CONFIG_NODE *node;

	iconfig_set_str(NULL, "windows", NULL);
	node = iconfig_node_traverse("windows", TRUE);

	g_slist_foreach(windows, (GFunc) window_save, node);
	signal_emit("windows saved", 0);
}

void window_save_init(void)
{
	signal_add("window restore item", (SIGNAL_FUNC) sig_window_restore_item);
}

void window_save_deinit(void)
{
	signal_remove("window restore item", (SIGNAL_FUNC) sig_window_restore_item);
}
