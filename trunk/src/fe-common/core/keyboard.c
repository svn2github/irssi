/*
 keyboard.c : irssi

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
#include "signals.h"
#include "commands.h"
#include "lib-config/iconfig.h"
#include "settings.h"

#include "keyboard.h"
#include "windows.h"

GSList *keyinfos;
static GHashTable *keys;

static void keyconfig_save(const char *id, const char *key, const char *data)
{
	CONFIG_NODE *node;

	g_return_if_fail(id != NULL);
	g_return_if_fail(key != NULL);

	/* remove old keyboard settings */
	node = iconfig_node_traverse("keyboard", TRUE);
	node = config_node_section(node, id, NODE_TYPE_BLOCK);

	iconfig_node_set_str(node, key, data == NULL ? "" : data);
}

static void keyconfig_clear(const char *id, const char *key)
{
	CONFIG_NODE *node;

	g_return_if_fail(id != NULL);

	/* remove old keyboard settings */
	node = iconfig_node_traverse("keyboard", TRUE);
	if (key == NULL)
		iconfig_node_set_str(node, id, NULL);
	else {
		node = config_node_section(node, id, 0);
		if (node != NULL) iconfig_node_set_str(node, key, NULL);
	}
}

KEYINFO_REC *key_info_find(const char *id)
{
	GSList *tmp;

	for (tmp = keyinfos; tmp != NULL; tmp = tmp->next) {
		KEYINFO_REC *rec = tmp->data;

		if (g_strcasecmp(rec->id, id) == 0)
			return rec;
	}

	return NULL;
}

/* Bind a key for function */
void key_bind(const char *id, const char *description,
	      const char *key_default, const char *data, SIGNAL_FUNC func)
{
	KEYINFO_REC *info;
	char *key;

	g_return_if_fail(id != NULL);
	g_return_if_fail(func != NULL);

	/* create key info record */
	info = key_info_find(id);
	if (info == NULL) {
		if (description == NULL)
			g_warning("key_bind(%s) should have description!", id);
		info = g_new0(KEYINFO_REC, 1);
		info->id = g_strdup(id);
		info->description = g_strdup(description);
		keyinfos = g_slist_append(keyinfos, info);

		/* add the signal */
		key = g_strconcat("key ", id, NULL);
		signal_add(key, func);
		g_free(key);

		signal_emit("keyinfo created", 1, info);
	}

	if (key_default != NULL && *key_default != '\0')
		key_configure_add(id, key_default, data);
}

static void keyinfo_remove(KEYINFO_REC *info)
{
	GSList *tmp;

	g_return_if_fail(info != NULL);

	keyinfos = g_slist_remove(keyinfos, info);
	signal_emit("keyinfo destroyed", 1, info);

	/* destroy all keys */
	for (tmp = info->keys; tmp != NULL; tmp = tmp->next) {
		KEY_REC *rec = tmp->data;

		g_hash_table_remove(keys, rec->key);
		g_free_not_null(rec->data);
		g_free(rec->key);
		g_free(rec);
	}

	/* destroy key info */
	g_slist_free(info->keys);
	g_free_not_null(info->description);
	g_free(info->id);
	g_free(info);
}

/* Unbind key */
void key_unbind(const char *id, SIGNAL_FUNC func)
{
	KEYINFO_REC *info;
	char *key;

	g_return_if_fail(id != NULL);
	g_return_if_fail(func != NULL);

	/* remove keys */
	info = key_info_find(id);
	if (info != NULL)
		keyinfo_remove(info);

	/* remove signal */
	key = g_strconcat("key ", id, NULL);
	signal_remove(key, func);
	g_free(key);
}

/* Configure new key */
void key_configure_add(const char *id, const char *key, const char *data)
{
	KEYINFO_REC *info;
	KEY_REC *rec;

	g_return_if_fail(id != NULL);
	g_return_if_fail(key != NULL && *key != '\0');

	info = key_info_find(id);
	if (info == NULL)
		return;

	key_configure_remove(key);

	rec = g_new0(KEY_REC, 1);
	rec->key = g_strdup(key);
	rec->info = info;
	rec->data = g_strdup(data);
	info->keys = g_slist_append(info->keys, rec);
	g_hash_table_insert(keys, rec->key, rec);

        keyconfig_save(id, key, data);
}

/* Remove key */
void key_configure_remove(const char *key)
{
	KEY_REC *rec;

	g_return_if_fail(key != NULL);

	rec = g_hash_table_lookup(keys, key);
	if (rec == NULL) return;

        keyconfig_clear(rec->info->id, key);

	rec->info->keys = g_slist_remove(rec->info->keys, rec);
	g_hash_table_remove(keys, key);

	g_free_not_null(rec->data);
	g_free(rec->key);
	g_free(rec);
}

int key_pressed(const char *key, void *data)
{
	KEY_REC *rec;
	char *str;
	int ret;

	g_return_val_if_fail(key != NULL, FALSE);

	rec = g_hash_table_lookup(keys, key);
	if (rec == NULL) return FALSE;

	str = g_strconcat("key ", rec->info->id, NULL);
	ret = signal_emit(str, 3, rec->data, data, rec->info);
	g_free(str);

	return ret;
}

static void sig_command(const char *data)
{
	signal_emit("send command", 3, data, active_win->active_server, active_win->active);
}

void read_keyinfo(KEYINFO_REC *info, CONFIG_NODE *node)
{
	GSList *tmp;

	g_return_if_fail(info != NULL);
	g_return_if_fail(node != NULL);
	g_return_if_fail(is_node_list(node));

	/* remove all old keys */
	while (info->keys != NULL)
		key_configure_remove(((KEY_REC *) info->keys->data)->key);

	/* add the new keys */
	for (tmp = node->value; tmp != NULL; tmp = tmp->next) {
		node = tmp->data;

		if (node->key != NULL)
			key_configure_add(info->id, node->value, node->key);
	}
}

static void read_keyboard_config(void)
{
	KEYINFO_REC *info;
	CONFIG_NODE *node;
	GSList *tmp;

	node = iconfig_node_traverse("keyboard", FALSE);
	if (node == NULL) return;

	for (tmp = node->value; tmp != NULL; tmp = tmp->next) {
		node = tmp->data;

		if (node->key == NULL || node->value == NULL)
			continue;

		info = key_info_find(node->key);
		if (info != NULL) read_keyinfo(info, node);
	}
}

void keyboard_init(void)
{
	keys = g_hash_table_new((GHashFunc) g_str_hash, (GCompareFunc) g_str_equal);
	keyinfos = NULL;

	key_bind("command", "Run any IRC command", NULL, NULL, (SIGNAL_FUNC) sig_command);

	read_keyboard_config();
	signal_add("setup reread", (SIGNAL_FUNC) read_keyboard_config);
}

void keyboard_deinit(void)
{
	while (keyinfos != NULL)
		keyinfo_remove(keyinfos->data);
	g_hash_table_destroy(keys);

        signal_remove("setup reread", (SIGNAL_FUNC) read_keyboard_config);
}
