/*
 set.c : irssi configuration - change settings in memory

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

static void cache_remove(CONFIG_REC *rec, CONFIG_NODE *node)
{
	char *path;

	path = g_hash_table_lookup(rec->cache_nodes, node);
	if (path != NULL) {
		g_hash_table_remove(rec->cache, path);
		g_hash_table_remove(rec->cache_nodes, node);
                g_free(path);
	}
}

void config_node_remove(CONFIG_REC *rec, CONFIG_NODE *parent, CONFIG_NODE *node)
{
	g_return_if_fail(parent != NULL);
	g_return_if_fail(node != NULL);

	cache_remove(rec, node);
	parent->value = g_slist_remove(parent->value, node);

	switch (node->type) {
	case NODE_TYPE_KEY:
	case NODE_TYPE_VALUE:
	case NODE_TYPE_COMMENT:
		g_free_not_null(node->value);
		break;
	case NODE_TYPE_BLOCK:
	case NODE_TYPE_LIST:
		while (node->value != NULL)
			config_node_remove(rec, node, ((GSList *) node->value)->data);
		break;
	}
	g_free_not_null(node->key);
        g_free(node);
}

/* Remove n'th node from a list */
void config_node_list_remove(CONFIG_REC *rec, CONFIG_NODE *node, int index)
{
	GSList *tmp;

	g_return_if_fail(node != NULL);
	g_return_if_fail(is_node_list(node));

	for (tmp = node->value; tmp != NULL; tmp = tmp->next, index--) {
		if (index == 0) {
                        config_node_remove(rec, node, tmp->data);
                        break;
		}
	}
}

void config_nodes_remove_all(CONFIG_REC *rec)
{
	g_return_if_fail(rec != NULL);

	while (rec->mainnode->value != NULL)
		config_node_remove(rec, rec->mainnode, ((GSList *) rec->mainnode->value)->data);
}

void config_node_set_str(CONFIG_REC *rec, CONFIG_NODE *parent, const char *key, const char *value)
{
	CONFIG_NODE *node;
	int no_key;

	g_return_if_fail(rec != NULL || value != NULL);
	g_return_if_fail(parent != NULL);

	no_key = key == NULL;
	node = no_key ? NULL : config_node_find(parent, key);

	if (value == NULL) {
                /* remove the key */
		if (node != NULL) config_node_remove(rec, parent, node);
		return;
	}

	if (node != NULL)
                g_free(node->value);
	else {
		node = g_new0(CONFIG_NODE, 1);
		parent->value = g_slist_append(parent->value, node);

		node->type = no_key ? NODE_TYPE_VALUE : NODE_TYPE_KEY;
		node->key = no_key ? NULL : g_strdup(key);
	}

	node->value = g_strdup(value);
}

void config_node_set_int(CONFIG_NODE *parent, const char *key, int value)
{
	char str[MAX_INT_STRLEN];

	g_snprintf(str, sizeof(str), "%d", value);
	config_node_set_str(NULL, parent, key, str);
}

void config_node_set_bool(CONFIG_NODE *parent, const char *key, int value)
{
	config_node_set_str(NULL, parent, key, value ? "yes" : "no");
}

int config_set_str(CONFIG_REC *rec, const char *section, const char *key, const char *value)
{
	CONFIG_NODE *parent;

	g_return_val_if_fail(rec != NULL, -1);
	g_return_val_if_fail(section != NULL, -1);

	parent = config_node_traverse(rec, section, TRUE);
	if (parent == NULL) return -1;

	config_node_set_str(rec, parent, key, value);
	return 0;
}

int config_set_int(CONFIG_REC *rec, const char *section, const char *key, int value)
{
	char str[MAX_INT_STRLEN];

	g_snprintf(str, sizeof(str), "%d", value);
	return config_set_str(rec, section, key, str);
}

int config_set_bool(CONFIG_REC *rec, const char *section, const char *key, int value)
{
	return config_set_str(rec, section, key, value ? "yes" : "no");
}

/* Add all values in `array' to `node' */
void config_node_add_list(CONFIG_NODE *node, char **array)
{
	char **tmp;

	for (tmp = array; *tmp != NULL; tmp++)
                config_node_set_str(NULL, node, NULL, *tmp);
}
