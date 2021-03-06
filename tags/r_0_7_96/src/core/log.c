/*
 log.c : irssi

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
#include "levels.h"
#include "misc.h"
#include "servers.h"
#include "log.h"

#include "lib-config/iconfig.h"
#include "settings.h"

#define DEFAULT_LOG_FILE_CREATE_MODE 644

#ifdef HAVE_FCNTL
static struct flock lock;
#endif

GSList *logs;

static const char *log_item_types[] = {
	"target",
	"window",

	NULL
};

const char *log_timestamp;
static int log_file_create_mode;
static int rotate_tag;

static int log_item_str2type(const char *type)
{
	int n;

	for (n = 0; log_item_types[n] != NULL; n++) {
		if (g_strcasecmp(log_item_types[n], type) == 0)
			return n;
	}

	return -1;
}

static void log_write_timestamp(int handle, const char *format,
				const char *suffix, time_t stamp)
{
	struct tm *tm;
	char str[256];

	g_return_if_fail(format != NULL);
	if (*format == '\0') return;

	tm = localtime(&stamp);

	str[sizeof(str)-1] = '\0';
	strftime(str, sizeof(str)-1, format, tm);

	write(handle, str, strlen(str));
	if (suffix != NULL) write(handle, suffix, strlen(suffix));
}

static char *log_filename(LOG_REC *log)
{
	char *str, fname[1024];
	struct tm *tm;
	time_t now;

	now = time(NULL);
	tm = localtime(&now);

	str = convert_home(log->fname);
	strftime(fname, sizeof(fname), str, tm);
	g_free(str);

	return g_strdup(fname);
}

int log_start_logging(LOG_REC *log)
{
	g_return_val_if_fail(log != NULL, FALSE);

	if (log->handle != -1)
		return TRUE;

	/* Append/create log file */
	g_free_not_null(log->real_fname);
	log->real_fname = log_filename(log);
	log->handle = open(log->real_fname, O_WRONLY | O_APPEND | O_CREAT,
			   log_file_create_mode);
	if (log->handle == -1) {
		signal_emit("log create failed", 1, log);
		log->failed = TRUE;
		return FALSE;
	}
#ifdef HAVE_FCNTL
        memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	if (fcntl(log->handle, F_SETLK, &lock) == -1) {
		close(log->handle);
		log->handle = -1;
		signal_emit("log locked", 1, log);
		log->failed = TRUE;
		return FALSE;
	}
#endif
	lseek(log->handle, 0, SEEK_END);

	log->opened = log->last = time(NULL);
	log_write_timestamp(log->handle,
			    settings_get_str("log_open_string"),
			    "\n", log->last);

	signal_emit("log started", 1, log);
	log->failed = FALSE;
	return TRUE;
}

void log_stop_logging(LOG_REC *log)
{
	g_return_if_fail(log != NULL);

	if (log->handle == -1)
		return;

	signal_emit("log stopped", 1, log);

	log_write_timestamp(log->handle,
			    settings_get_str("log_close_string"),
			    "\n", time(NULL));

#ifdef HAVE_FCNTL
        memset(&lock, 0, sizeof(lock));
	lock.l_type = F_UNLCK;
	fcntl(log->handle, F_SETLK, &lock);
#endif

	close(log->handle);
	log->handle = -1;
}

static void log_rotate_check(LOG_REC *log)
{
	char *new_fname;

	g_return_if_fail(log != NULL);

	if (log->handle == -1 || log->real_fname == NULL)
		return;

	new_fname = log_filename(log);
	if (strcmp(new_fname, log->real_fname) != 0) {
		/* rotate log */
		log_stop_logging(log);
		log_start_logging(log);
	}
	g_free(new_fname);
}

void log_write_rec(LOG_REC *log, const char *str)
{
	struct tm *tm;
	time_t now;
	int hour, day;

	g_return_if_fail(log != NULL);
	g_return_if_fail(str != NULL);

	if (log->handle == -1)
		return;

	now = time(NULL);
	tm = localtime(&now);
	hour = tm->tm_hour;
	day = tm->tm_mday;

	tm = localtime(&log->last);
	day -= tm->tm_mday; /* tm breaks in log_rotate_check() .. */
	if (tm->tm_hour != hour) {
		/* hour changed, check if we need to rotate log file */
                log_rotate_check(log);
	}

	if (day != 0) {
		/* day changed */
		log_write_timestamp(log->handle,
				    settings_get_str("log_day_changed"),
				    "\n", now);
	}

	log->last = now;

	log_write_timestamp(log->handle, log_timestamp, str, now);
	write(log->handle, "\n", 1);

	signal_emit("log written", 2, log, str);
}

LOG_ITEM_REC *log_item_find(LOG_REC *log, int type, const char *item,
			    SERVER_REC *server)
{
	GSList *tmp;

	g_return_val_if_fail(log != NULL, NULL);
	g_return_val_if_fail(item != NULL, NULL);

	for (tmp = log->items; tmp != NULL; tmp = tmp->next) {
		LOG_ITEM_REC *rec = tmp->data;

		if (rec->type == type && g_strcasecmp(rec->name, item) == 0 &&
		    (rec->servertag == NULL || (server != NULL &&
		    	g_strcasecmp(rec->servertag, server->tag) == 0)))
			return rec;
	}

	return NULL;
}

static void log_file_write(SERVER_REC *server, const char *item, int level,
			   const char *str, int no_fallbacks)
{
	GSList *tmp, *fallbacks;
	char *tmpstr;
	int found;

	g_return_if_fail(str != NULL);

	fallbacks = NULL; found = FALSE;

	for (tmp = logs; tmp != NULL; tmp = tmp->next) {
		LOG_REC *rec = tmp->data;

		if (rec->handle == -1)
			continue; /* log not opened yet */

		if ((level & rec->level) == 0)
			continue;

		if (rec->items == NULL)
			fallbacks = g_slist_append(fallbacks, rec);
		else if (item != NULL && log_item_find(rec, LOG_ITEM_TARGET,
						       item, server) != NULL)
			log_write_rec(rec, str);
	}

	if (!found && !no_fallbacks && fallbacks != NULL) {
		/* not found from any items, so write it to all main logs */
		tmpstr = NULL;
		if (level & MSGLEVEL_PUBLIC)
			tmpstr = g_strconcat(item, ": ", str, NULL);

		g_slist_foreach(fallbacks, (GFunc) log_write_rec,
				tmpstr ? tmpstr : (char *)str);
		g_free_not_null(tmpstr);
	}
        g_slist_free(fallbacks);
}

LOG_REC *log_find(const char *fname)
{
	GSList *tmp;

	for (tmp = logs; tmp != NULL; tmp = tmp->next) {
		LOG_REC *rec = tmp->data;

		if (strcmp(rec->fname, fname) == 0)
			return rec;
	}

	return NULL;
}

static void log_items_update_config(LOG_REC *log, CONFIG_NODE *parent)
{
	GSList *tmp;
	CONFIG_NODE *node;

	parent = config_node_section(parent, "items", NODE_TYPE_LIST);
	for (tmp = log->items; tmp != NULL; tmp = tmp->next) {
		LOG_ITEM_REC *rec = tmp->data;

                node = config_node_section(parent, NULL, NODE_TYPE_BLOCK);
		iconfig_node_set_str(node, "type", log_item_types[rec->type]);
		iconfig_node_set_str(node, "name", rec->name);
		iconfig_node_set_str(node, "server", rec->servertag);
	}
}

static void log_update_config(LOG_REC *log)
{
	CONFIG_NODE *node;
	char *levelstr;

	if (log->temp)
		return;

	node = iconfig_node_traverse("logs", TRUE);
	node = config_node_section(node, log->fname, NODE_TYPE_BLOCK);

	if (log->autoopen)
		config_node_set_bool(node, "auto_open", TRUE);
	else
		iconfig_node_set_str(node, "auto_open", NULL);

	levelstr = bits2level(log->level);
	iconfig_node_set_str(node, "level", levelstr);
	g_free(levelstr);

	iconfig_node_set_str(node, "items", NULL);

	if (log->items != NULL)
		log_items_update_config(log, node);
}

static void log_remove_config(LOG_REC *log)
{
	iconfig_set_str("logs", log->fname, NULL);
}

LOG_REC *log_create_rec(const char *fname, int level)
{
	LOG_REC *rec;

	g_return_val_if_fail(fname != NULL, NULL);

	rec = log_find(fname);
	if (rec == NULL) {
		rec = g_new0(LOG_REC, 1);
		rec->fname = g_strdup(fname);
		rec->handle = -1;
	}

	rec->level = level;
	return rec;
}

void log_item_add(LOG_REC *log, int type, const char *name,
		  SERVER_REC *server)
{
	LOG_ITEM_REC *rec;

	g_return_if_fail(log != NULL);
	g_return_if_fail(name != NULL);

	if (log_item_find(log, type, name, server))
		return;

	rec = g_new0(LOG_ITEM_REC, 1);
	rec->type = type;
	rec->name = g_strdup(name);
	rec->servertag = server == NULL ? NULL : g_strdup(server->tag);

	log->items = g_slist_append(log->items, rec);
}

void log_update(LOG_REC *log)
{
	g_return_if_fail(log != NULL);

	if (log_find(log->fname) == NULL) {
		logs = g_slist_append(logs, log);
		log->handle = -1;
	}

	log_update_config(log);
	signal_emit("log new", 1, log);
}

void log_item_destroy(LOG_REC *log, LOG_ITEM_REC *item)
{
	log->items = g_slist_remove(log->items, item);

	g_free(item->name);
	g_free_not_null(item->servertag);
	g_free(item);
}

static void log_destroy(LOG_REC *log)
{
	g_return_if_fail(log != NULL);

	if (log->handle != -1)
		log_stop_logging(log);

	logs = g_slist_remove(logs, log);
	signal_emit("log remove", 1, log);

	while (log->items != NULL)
		log_item_destroy(log, log->items->data);
	g_free(log->fname);
	g_free_not_null(log->real_fname);
	g_free(log);
}

void log_close(LOG_REC *log)
{
	g_return_if_fail(log != NULL);

	log_remove_config(log);
	log_destroy(log);
}

static void sig_printtext_stripped(void *window, SERVER_REC *server,
				   const char *item, gpointer levelp,
				   const char *str)
{
	char **items, **tmp;
	int level;

	g_return_if_fail(str != NULL);

	level = GPOINTER_TO_INT(levelp);
	if (logs == NULL || level == MSGLEVEL_NEVER)
		return;

        if (item == NULL)
		log_file_write(server, NULL, level, str, FALSE);
	else {
		/* there can be multiple items separated with comma */
		items = g_strsplit(item, ",", -1);
		for (tmp = items; *tmp != NULL; tmp++)
			log_file_write(server, *tmp, level, str, FALSE);
		g_strfreev(items);
	}
}

static int sig_rotate_check(void)
{
	static int last_hour = -1;
	struct tm tm;
	time_t now;

	/* don't do anything until hour is changed */
	now = time(NULL);
	memcpy(&tm, localtime(&now), sizeof(tm));
	if (tm.tm_hour != last_hour) {
		last_hour = tm.tm_hour;
		g_slist_foreach(logs, (GFunc) log_rotate_check, NULL);
	}
	return 1;
}

static void log_items_read_config(CONFIG_NODE *node, LOG_REC *log)
{
	LOG_ITEM_REC *rec;
	GSList *tmp;
	char *item;
	int type;

	for (tmp = node->value; tmp != NULL; tmp = tmp->next) {
		node = tmp->data;

		if (node->type != NODE_TYPE_BLOCK)
			continue;

		item = config_node_get_str(node, "name", NULL);
		type = log_item_str2type(config_node_get_str(node, "type", NULL));
		if (item == NULL || type == -1)
			continue;

		rec = g_new0(LOG_ITEM_REC, 1);
		rec->type = type;
		rec->name = g_strdup(item);
		rec->servertag = g_strdup(config_node_get_str(node, "server", NULL));

		log->items = g_slist_append(log->items, rec);
	}
}

static void log_read_config(void)
{
	CONFIG_NODE *node;
	LOG_REC *log;
	GSList *tmp, *next, *fnames;

	/* close old logs, save list of open logs */
	fnames = NULL;
	for (tmp = logs; tmp != NULL; tmp = next) {
		log = tmp->data;

		next = tmp->next;
		if (log->temp)
			continue;

		if (log->handle != -1)
			fnames = g_slist_append(fnames, g_strdup(log->fname));
		log_destroy(log);
	}

	node = iconfig_node_traverse("logs", FALSE);
	if (node == NULL) return;

	for (tmp = node->value; tmp != NULL; tmp = tmp->next) {
		node = tmp->data;

		if (node->type != NODE_TYPE_BLOCK)
			continue;

		log = g_new0(LOG_REC, 1);
		logs = g_slist_append(logs, log);

		log->handle = -1;
		log->fname = g_strdup(node->key);
		log->autoopen = config_node_get_bool(node, "auto_open", FALSE);
		log->level = level2bits(config_node_get_str(node, "level", 0));

		node = config_node_section(node, "items", -1);
		if (node != NULL)
			log_items_read_config(node, log);

		if (log->autoopen || gslist_find_string(fnames, log->fname))
			log_start_logging(log);
	}

	g_slist_foreach(fnames, (GFunc) g_free, NULL);
	g_slist_free(fnames);
}

static void read_settings(void)
{
	log_timestamp = settings_get_str("log_timestamp");
	log_file_create_mode = octal2dec(settings_get_int("log_create_mode"));
}

void log_init(void)
{
	rotate_tag = g_timeout_add(60000, (GSourceFunc) sig_rotate_check, NULL);
	logs = NULL;

	settings_add_int("log", "log_create_mode",
			 DEFAULT_LOG_FILE_CREATE_MODE);
	settings_add_str("log", "log_timestamp", "%H:%M ");
	settings_add_str("log", "log_open_string",
			 "--- Log opened %a %b %d %H:%M:%S %Y");
	settings_add_str("log", "log_close_string",
			 "--- Log closed %a %b %d %H:%M:%S %Y");
	settings_add_str("log", "log_day_changed",
			 "--- Day changed %a %b %d %Y");

	read_settings();
	log_read_config();
        signal_add("setup changed", (SIGNAL_FUNC) read_settings);
        signal_add("setup reread", (SIGNAL_FUNC) log_read_config);
	signal_add("print text stripped", (SIGNAL_FUNC) sig_printtext_stripped);
}

void log_deinit(void)
{
	g_source_remove(rotate_tag);

	while (logs != NULL)
		log_close(logs->data);

	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
        signal_remove("setup reread", (SIGNAL_FUNC) log_read_config);
	signal_remove("print text stripped", (SIGNAL_FUNC) sig_printtext_stripped);
}
