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
#include "log.h"

#include "lib-config/iconfig.h"
#include "settings.h"

#define LOG_FILE_CREATE_MODE 644

#ifdef HAVE_FCNTL
static struct flock lock;
#endif

GSList *logs;

const char *log_timestamp;
static int log_file_create_mode;
static int rotate_tag;

static void log_write_timestamp(int handle, const char *format, const char *suffix, time_t t)
{
	struct tm *tm;
	char str[256];

	g_return_if_fail(format != NULL);
	if (*format == '\0') return;

	tm = localtime(&t);

	str[sizeof(str)-1] = '\0';
	strftime(str, sizeof(str)-1, format, tm);

	write(handle, str, strlen(str));
	if (suffix != NULL) write(handle, suffix, strlen(suffix));
}

int log_start_logging(LOG_REC *log)
{
	char *str, fname[1024];
	struct tm *tm;
	time_t t;

	g_return_val_if_fail(log != NULL, FALSE);

	if (log->handle != -1)
		return TRUE;

	t = time(NULL);
	tm = localtime(&t);

	/* Append/create log file */
	str = convert_home(log->fname);
	strftime(fname, sizeof(fname), str, tm);
	log->handle = open(fname, O_WRONLY | O_APPEND | O_CREAT, log_file_create_mode);
	g_free(str);

	if (log->handle == -1) {
		signal_emit("log create failed", 1, log);
		return FALSE;
	}
#ifdef HAVE_FCNTL
        memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	if (fcntl(log->handle, F_SETLK, &lock) == -1) {
		close(log->handle);
		log->handle = -1;
		signal_emit("log locked", 1, log);
		return FALSE;
	}
#endif
	lseek(log->handle, 0, SEEK_END);

	log->opened = log->last = time(NULL);
	log_write_timestamp(log->handle, settings_get_str("log_open_string"), "\n", log->last);

	signal_emit("log started", 1, log);
	return TRUE;
}

void log_stop_logging(LOG_REC *log)
{
	g_return_if_fail(log != NULL);

	if (log->handle == -1)
		return;

	signal_emit("log stopped", 1, log);

	log_write_timestamp(log->handle, settings_get_str("log_close_string"), "\n", time(NULL));

#ifdef HAVE_FCNTL
        memset(&lock, 0, sizeof(lock));
	lock.l_type = F_UNLCK;
	fcntl(log->handle, F_SETLK, &lock);
#endif

	close(log->handle);
	log->handle = -1;
}

void log_write_rec(LOG_REC *log, const char *str)
{
	struct tm *tm;
	time_t t;
	int day;

	g_return_if_fail(log != NULL);
	g_return_if_fail(str != NULL);

	if (log->handle == -1)
		return;

	t = time(NULL);
	tm = localtime(&t);
	day = tm->tm_mday;

	tm = localtime(&log->last);
	if (tm->tm_mday != day) {
		/* day changed */
		log_write_timestamp(log->handle, settings_get_str("log_day_changed"), "\n", t);
	}
	log->last = t;

	log_write_timestamp(log->handle, log_timestamp, str, t);
	write(log->handle, "\n", 1);

	signal_emit("log written", 2, log, str);
}

void log_file_write(const char *item, int level, const char *str, int no_fallbacks)
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
		else if (item != NULL && strarray_find(rec->items, item) != -1)
			log_write_rec(rec, str);
	}

	if (!found && !no_fallbacks && fallbacks != NULL) {
		/* not found from any items, so write it to all main logs */
		tmpstr = NULL;
		if (level & MSGLEVEL_PUBLIC)
			tmpstr = g_strconcat(item, ": ", str, NULL);

		g_slist_foreach(fallbacks, (GFunc) log_write_rec, tmpstr ? tmpstr : (char *)str);
		g_free_not_null(tmpstr);
	}
        g_slist_free(fallbacks);
}

void log_write(const char *item, int level, const char *str)
{
	log_file_write(item, level, str, TRUE);
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

const char *log_rotate2str(int rotate)
{
	switch (rotate) {
	case LOG_ROTATE_HOUR:
		return "hour";
	case LOG_ROTATE_DAY:
		return "day";
	case LOG_ROTATE_WEEK:
		return "week";
	case LOG_ROTATE_MONTH:
		return "month";
	}

	return NULL;
}

int log_str2rotate(const char *str)
{
	if (str == NULL)
		return -1;

	if (g_strncasecmp(str, "hour", 4) == 0)
		return LOG_ROTATE_HOUR;
	if (g_strncasecmp(str, "day", 3) == 0)
		return LOG_ROTATE_DAY;
	if (g_strncasecmp(str, "week", 4) == 0)
		return LOG_ROTATE_WEEK;
	if (g_strncasecmp(str, "month", 5) == 0)
		return LOG_ROTATE_MONTH;
	if (g_strncasecmp(str, "never", 5) == 0)
		return LOG_ROTATE_NEVER;

	return -1;
}

static void log_set_config(LOG_REC *log)
{
	CONFIG_NODE *node;
	char *levelstr;

	node = iconfig_node_traverse("logs", TRUE);
	node = config_node_section(node, log->fname, NODE_TYPE_BLOCK);

	if (log->autoopen)
		config_node_set_bool(node, "auto_open", TRUE);
	else
		iconfig_node_set_str(node, "auto_open", NULL);

	iconfig_node_set_str(node, "rotate", log_rotate2str(log->rotate));

	levelstr = bits2level(log->level);
	iconfig_node_set_str(node, "level", levelstr);
	g_free(levelstr);

	iconfig_node_set_str(node, "items", NULL);

	if (log->items != NULL && *log->items != NULL) {
		node = config_node_section(node, "items", NODE_TYPE_LIST);
		config_node_add_list(node, log->items);
	}
}

static void log_remove_config(LOG_REC *log)
{
	iconfig_set_str("logs", log->fname, NULL);
}

LOG_REC *log_create_rec(const char *fname, int level, const char *items)
{
	LOG_REC *rec;

	g_return_val_if_fail(fname != NULL, NULL);

	rec = log_find(fname);
	if (rec == NULL) {
		rec = g_new0(LOG_REC, 1);
		rec->fname = g_strdup(fname);
		rec->handle = -1;
	} else {
		g_strfreev(rec->items);
	}

	rec->items = items == NULL || *items == '\0' ? NULL :
		g_strsplit(items, " ", -1);
	rec->level = level;
	return rec;
}

void log_update(LOG_REC *log)
{
	g_return_if_fail(log != NULL);

	if (log_find(log->fname) == NULL) {
		logs = g_slist_append(logs, log);
		log->handle = -1;
	}

	log_set_config(log);
	signal_emit("log new", 1, log);
}

void log_close(LOG_REC *log)
{
	g_return_if_fail(log != NULL);

	log_remove_config(log);
	if (log->handle != -1)
		log_stop_logging(log);

	logs = g_slist_remove(logs, log);
	signal_emit("log remove", 1, log);

	if (log->items != NULL) g_strfreev(log->items);
	g_free(log->fname);
	g_free(log);
}

static void sig_printtext_stripped(void *window, void *server, const char *item, gpointer levelp, const char *str)
{
	int level;

	g_return_if_fail(str != NULL);

	level = GPOINTER_TO_INT(levelp);
	if (logs != NULL && level != MSGLEVEL_NEVER)
		log_file_write(item, level, str, FALSE);
}

static int sig_rotate_check(void)
{
	static int last_hour = -1;
	struct tm tm_now, *tm;
	GSList *tmp;
	time_t now;

	/* don't do anything until hour is changed */
	now = time(NULL);
	memcpy(&tm_now, localtime(&now), sizeof(tm_now));
	if (tm_now.tm_hour == last_hour) return 1;
	last_hour = tm_now.tm_hour;

	for (tmp = logs; tmp != NULL; tmp = tmp->next) {
		LOG_REC *rec = tmp->data;

		if (rec->handle == -1 || rec->rotate == LOG_ROTATE_NEVER)
			continue;

		tm = localtime(&rec->opened);
		if (rec->rotate == LOG_ROTATE_MONTH) {
			if (tm->tm_mon == tm_now.tm_mon)
				continue;
		} else if (rec->rotate == LOG_ROTATE_WEEK) {
			if (tm->tm_wday != 1 || tm->tm_mday == tm_now.tm_mday)
				continue;
		} else if (rec->rotate == LOG_ROTATE_DAY) {
			if (tm->tm_mday == tm_now.tm_mday)
				continue;
		}

                log_stop_logging(rec);
                log_start_logging(rec);
	}

	return 1;
}

static void log_read_config(void)
{
	CONFIG_NODE *node;
	LOG_REC *log;
	GSList *tmp;

	while (logs != NULL)
		log_close(logs->data);

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
		log->rotate = log_str2rotate(config_node_get_str(node, "rotate", NULL));
                if (log->rotate < 0) log->rotate = LOG_ROTATE_NEVER;

		node = config_node_section(node, "items", -1);
		if (node != NULL) log->items = config_node_get_list(node);

		if (log->autoopen) log_start_logging(log);
	}
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

	settings_add_int("log", "log_create_mode", LOG_FILE_CREATE_MODE);
	settings_add_str("log", "log_timestamp", "%H:%M ");
	settings_add_str("log", "log_open_string", "--- Log opened %a %b %d %H:%M:%S %Y");
	settings_add_str("log", "log_close_string", "--- Log closed %a %b %d %H:%M:%S %Y");
	settings_add_str("log", "log_day_changed", "--- Day changed %a %b %d %Y");

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
