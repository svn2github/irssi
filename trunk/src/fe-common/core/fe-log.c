/*
 fe-log.c : irssi

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
#include "servers.h"
#include "levels.h"
#include "misc.h"
#include "log.h"
#include "special-vars.h"
#include "settings.h"

#include "windows.h"
#include "window-items.h"
#include "themes.h"

/* close autologs after 5 minutes of inactivity */
#define AUTOLOG_INACTIVITY_CLOSE (60*5)

#define LOG_DIR_CREATE_MODE 0770

static int autolog_level;
static int autoremove_tag;
static const char *autolog_path;

static THEME_REC *log_theme;
static int skip_next_printtext;
static const char *log_theme_name;

static void log_add_targets(LOG_REC *log, const char *targets)
{
	char **tmp, **items;

        g_return_if_fail(log != NULL);
        g_return_if_fail(targets != NULL);

	items = g_strsplit(targets, " ", -1);

	for (tmp = items; *tmp != NULL; tmp++)
		log_item_add(log, LOG_ITEM_TARGET, *tmp, NULL);

	g_strfreev(items);
}

/* SYNTAX: LOG OPEN [-noopen] [-autoopen] [-targets <targets>]
                    [-window] <fname> [<levels>] */
static void cmd_log_open(const char *data)
{
        GHashTable *optlist;
	char *targetarg, *fname, *levels;
	void *free_arg;
	char window[MAX_INT_STRLEN];
	LOG_REC *log;
	int level;

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTIONS | PARAM_FLAG_GETREST,
			    "log open", &optlist, &fname, &levels))
		return;
	if (*fname == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	level = level2bits(levels);
	log = log_create_rec(fname, level != 0 ? level : MSGLEVEL_ALL);

	if (g_hash_table_lookup(optlist, "window")) {
		/* log by window ref# */
		ltoa(window, active_win->refnum);
		log_item_add(log, LOG_ITEM_WINDOW_REFNUM, window, NULL);
	} else {
		targetarg = g_hash_table_lookup(optlist, "targets");
		if (targetarg != NULL && *targetarg != '\0')
			log_add_targets(log, targetarg);
	}

	if (g_hash_table_lookup(optlist, "autoopen"))
		log->autoopen = TRUE;

	log_update(log);

	if (log->handle == -1 && g_hash_table_lookup(optlist, "noopen") == NULL) {
		/* start logging */
		if (log_start_logging(log)) {
			printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
				    IRCTXT_LOG_OPENED, fname);
		} else {
			log_close(log);
		}
	}

        cmd_params_free(free_arg);
}

static LOG_REC *log_find_from_data(const char *data)
{
	GSList *tmp;

	if (!is_numeric(data, ' '))
		return log_find(data);

	/* with index number */
	tmp = g_slist_nth(logs, atoi(data)-1);
	return tmp == NULL ? NULL : tmp->data;
}

/* SYNTAX: LOG CLOSE <id>|<file> */
static void cmd_log_close(const char *data)
{
	LOG_REC *log;

	log = log_find_from_data(data);
	if (log == NULL)
		printformat(NULL, NULL, MSGLEVEL_CLIENTERROR, IRCTXT_LOG_NOT_OPEN, data);
	else {
		log_close(log);
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_LOG_CLOSED, data);
	}
}

/* SYNTAX: LOG START <id>|<file> */
static void cmd_log_start(const char *data)
{
	LOG_REC *log;

	log = log_find_from_data(data);
	if (log != NULL) {
		log_start_logging(log);
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_LOG_OPENED, data);
	}
}

/* SYNTAX: LOG STOP <id>|<file> */
static void cmd_log_stop(const char *data)
{
	LOG_REC *log;

	log = log_find_from_data(data);
	if (log == NULL || log->handle == -1)
		printformat(NULL, NULL, MSGLEVEL_CLIENTERROR, IRCTXT_LOG_NOT_OPEN, data);
	else {
		log_stop_logging(log);
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_LOG_CLOSED, data);
	}
}

static char *log_items_get_list(LOG_REC *log)
{
	GSList *tmp;
	GString *str;
	char *ret;

	g_return_val_if_fail(log != NULL, NULL);
	g_return_val_if_fail(log->items != NULL, NULL);

	str = g_string_new(NULL);
	for (tmp = log->items; tmp != NULL; tmp = tmp->next) {
		LOG_ITEM_REC *rec = tmp->data;

                g_string_sprintfa(str, "%s, ", rec->name);
	}
	g_string_truncate(str, str->len-2);

	ret = str->str;
	g_string_free(str, FALSE);
	return ret;
}

/* SYNTAX: LOG LIST */
static void cmd_log_list(void)
{
	GSList *tmp;
	char *levelstr, *items;
	int index;

	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, IRCTXT_LOG_LIST_HEADER);
	for (tmp = logs, index = 1; tmp != NULL; tmp = tmp->next, index++) {
		LOG_REC *rec = tmp->data;

		levelstr = bits2level(rec->level);
		items = rec->items == NULL ? NULL :
                        log_items_get_list(rec);

		printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, IRCTXT_LOG_LIST,
			    index, rec->fname, items != NULL ? items : "",
			    levelstr, rec->autoopen ? " -autoopen" : "");

		g_free_not_null(items);
		g_free(levelstr);
	}
	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, IRCTXT_LOG_LIST_FOOTER);
}

static void cmd_log(const char *data, SERVER_REC *server, void *item)
{
	if (*data == '\0')
		cmd_log_list();
	else
		command_runsub("log", data, server, item);
}

static LOG_REC *logs_find_item(int type, const char *item,
			       SERVER_REC *server, LOG_ITEM_REC **ret_item)
{
	LOG_ITEM_REC *logitem;
	GSList *tmp;

	for (tmp = logs; tmp != NULL; tmp = tmp->next) {
		LOG_REC *log = tmp->data;

		logitem = log_item_find(log, type, item, server);
		if (logitem != NULL) {
			if (ret_item != NULL) *ret_item = logitem;
			return log;
		}
	}

	return NULL;
}

/* SYNTAX: WINDOW LOG on|off|toggle [<filename>] */
static void cmd_window_log(const char *data)
{
	LOG_REC *log;
	char *set, *fname, window[MAX_INT_STRLEN];
	void *free_arg;
	int open_log, close_log;

	if (!cmd_get_params(data, &free_arg, 2, &set, &fname))
		return;

        ltoa(window, active_win->refnum);
	log = logs_find_item(LOG_ITEM_WINDOW_REFNUM, window, NULL, NULL);

        open_log = close_log = FALSE;
	if (g_strcasecmp(set, "ON") == 0)
		open_log = TRUE;
	else if (g_strcasecmp(set, "OFF") == 0) {
		close_log = TRUE;
	} else if (g_strcasecmp(set, "TOGGLE") == 0) {
                open_log = log == NULL;
                close_log = log != NULL;
	} else {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_NOT_TOGGLE);
		cmd_params_free(free_arg);
		return;
	}

	if (open_log && log == NULL) {
		/* irc.log.<windowname> or irc.log.Window<ref#> */
		fname = *fname != '\0' ? g_strdup(fname) :
			g_strdup_printf("~/irc.log.%s%s",
					active_win->name != NULL ? active_win->name : "Window",
					active_win->name != NULL ? "" : window);
		log = log_create_rec(fname, MSGLEVEL_ALL);
                log_item_add(log, LOG_ITEM_WINDOW_REFNUM, window, NULL);
		log_update(log);
		g_free(fname);
	}

	if (open_log && log != NULL) {
		log_start_logging(log);
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_LOG_OPENED, log->fname);
	} else if (close_log && log != NULL && log->handle != -1) {
		log_stop_logging(log);
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_LOG_CLOSED, log->fname);
	}

        cmd_params_free(free_arg);
}

/* Create log file entry to window, but don't start logging */
/* SYNTAX: WINDOW LOGFILE <file> */
static void cmd_window_logfile(const char *data)
{
	LOG_REC *log;
	char window[MAX_INT_STRLEN];

        ltoa(window, active_win->refnum);
	log = logs_find_item(LOG_ITEM_WINDOW_REFNUM, window, NULL, NULL);

	if (log != NULL) {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_WINDOWLOG_FILE_LOGGING);
		return;
	}

	log = log_create_rec(data, MSGLEVEL_ALL);
	log_item_add(log, LOG_ITEM_WINDOW_REFNUM, window, NULL);
	log_update(log);

	printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_WINDOWLOG_FILE, data);
}

/* window's refnum changed - update the logs to log the new window refnum */
static void sig_window_refnum_changed(WINDOW_REC *window, gpointer old_refnum)
{
	char winnum[MAX_INT_STRLEN];
	LOG_REC *log;
	LOG_ITEM_REC *item;

        ltoa(winnum, GPOINTER_TO_INT(old_refnum));
	log = logs_find_item(LOG_ITEM_WINDOW_REFNUM, winnum, NULL, &item);

	if (log != NULL) {
		ltoa(winnum, window->refnum);

		g_free(item->name);
		item->name = g_strdup(winnum);
	}
}

static void autologs_close_all(void)
{
	GSList *tmp, *next;

	for (tmp = logs; tmp != NULL; tmp = next) {
		LOG_REC *rec = tmp->data;

		next = tmp->next;
		if (rec->temp) log_close(rec);
	}
}

static void autolog_log(void *server, const char *target)
{
	LOG_REC *log;
	char *fname, *dir, *str;

	log = logs_find_item(LOG_ITEM_TARGET, target, server, NULL);
	if (log != NULL && !log->failed) {
		log_start_logging(log);
		return;
	}

	fname = parse_special_string(autolog_path, server, NULL, target, NULL);
	if (log_find(fname) == NULL) {
		str = convert_home(fname);
		dir = g_dirname(str);
		g_free(str);

		mkpath(dir, LOG_DIR_CREATE_MODE);
		g_free(dir);

		log = log_create_rec(fname, autolog_level);
		log_item_add(log, LOG_ITEM_TARGET, target, server);

		log->temp = TRUE;
		log_update(log);
		log_start_logging(log);
	}
	g_free(fname);
}

static void log_line(WINDOW_REC *window, void *server, const char *target,
		     int level, const char *text)
{
	char windownum[MAX_INT_STRLEN];
	char **targets, **tmp;
	LOG_REC *log;

	if (level == MSGLEVEL_NEVER) return;

	/* let autolog create the log records */
	if ((autolog_level & level) && target != NULL && *target != '\0') {
                /* there can be multiple targets separated with comma */
		targets = g_strsplit(target, ",", -1);
		for (tmp = targets; *tmp != NULL; tmp++) {
			autolog_log(server, *tmp);
		}
		g_strfreev(targets);
	}

        /* save to log created with /WINDOW LOG */
	ltoa(windownum, window->refnum);
	log = logs_find_item(LOG_ITEM_WINDOW_REFNUM, windownum, NULL, NULL);
	if (log != NULL) log_write_rec(log, text);

	/* save line to log files */
	if (logs == NULL)
		return;

        if (target == NULL)
		log_file_write(server, NULL, level, text, FALSE);
	else {
		/* there can be multiple items separated with comma */
		targets = g_strsplit(target, ",", -1);
		for (tmp = targets; *tmp != NULL; tmp++)
			log_file_write(server, *tmp, level, text, FALSE);
		g_strfreev(targets);
	}
}

static void sig_printtext_stripped(WINDOW_REC *window, void *server,
				   const char *target, gpointer levelp,
				   const char *text)
{
	if (skip_next_printtext) {
		skip_next_printtext = FALSE;
		return;
	}

	log_line(window, server, target, GPOINTER_TO_INT(levelp), text);
}

static void sig_print_format(THEME_REC *theme, const char *module,
			     TEXT_DEST_REC *dest, gpointer formatnump,
			     va_list va)
{
	MODULE_THEME_REC *module_theme;
	FORMAT_REC *formats;
	int formatnum;
	char *str, *str2, *stripped, *tmp;

	if (log_theme == NULL) {
		/* theme isn't loaded for some reason (/reload destroys it),
		   reload it. */
		log_theme = theme_load(log_theme_name);
		if (log_theme == NULL) return;
	}

	if (theme == log_theme)
		return;

	/* log uses a different theme .. very ugly kludge follows.. : */
	formatnum = GPOINTER_TO_INT(formatnump);
	module_theme = g_hash_table_lookup(log_theme->modules, module);
	formats = g_hash_table_lookup(default_formats, module);

	str = output_format_text_args(dest, &formats[formatnum],
				      module_theme->expanded_formats[formatnum], va);
	if (*str != '\0') {
		/* get_line_start_text() gets the line start with
		   current theme. */
		THEME_REC *old_theme = current_theme;
		current_theme = log_theme;
		tmp = get_line_start_text(dest);
		current_theme = old_theme;

		/* line start + text */
		str2 = tmp == NULL ? str :
			g_strconcat(tmp, str, NULL);
		if (str2 != str) g_free(str);
		str = str2;
		g_free_not_null(tmp);

		/* strip colors from text, log it. */
		stripped = strip_codes(str);
		skip_next_printtext = TRUE;
		log_line(dest->window, dest->server, dest->target,
			 dest->level, stripped);
		g_free(stripped);
	}
	g_free(str);

}

static int sig_autoremove(void)
{
	SERVER_REC *server;
	LOG_ITEM_REC *logitem;
	GSList *tmp, *next;
	time_t removetime;

        removetime = time(NULL)-AUTOLOG_INACTIVITY_CLOSE;
	for (tmp = logs; tmp != NULL; tmp = next) {
		LOG_REC *log = tmp->data;

		next = tmp->next;

		if (!log->temp || log->last > removetime || log->items == NULL)
                        continue;

		/* Close only logs with private messages */
		logitem = log->items->data;
		if (logitem->servertag == NULL)
			continue;

		server = server_find_tag(logitem->servertag);
		if (logitem->type == LOG_ITEM_TARGET &&
		    server != NULL && !server->ischannel(*logitem->name))
			log_close(log);
	}
	return 1;
}

static void sig_window_item_remove(WINDOW_REC *window, WI_ITEM_REC *item)
{
	LOG_REC *log;

	log = logs_find_item(LOG_ITEM_TARGET, item->name, item->server, NULL);
        if (log != NULL) log_close(log);
}

static void sig_log_locked(LOG_REC *log)
{
	printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		    IRCTXT_LOG_LOCKED, log->fname);
}

static void sig_log_create_failed(LOG_REC *log)
{
	printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE,
		    IRCTXT_LOG_CREATE_FAILED, log->fname, g_strerror(errno));
}

static void sig_awaylog_show(LOG_REC *log, gpointer pmsgs, gpointer pfilepos)
{
	char *str;
	int msgs, filepos;

	msgs = GPOINTER_TO_INT(pmsgs);
	filepos = GPOINTER_TO_INT(pfilepos);

	if (msgs == 0)
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_LOG_NO_AWAY_MSGS, log->fname);
	else {
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, IRCTXT_LOG_AWAY_MSGS, log->fname, msgs);

                str = g_strdup_printf("\"%s\" %d", log->fname, filepos);
		signal_emit("command cat", 1, str);
		g_free(str);
	}
}

static void sig_theme_destroyed(THEME_REC *theme)
{
	if (theme == log_theme)
		log_theme = NULL;
}

static void read_settings(void)
{
	const char *old_log_theme = log_theme_name;
	int old_autolog = autolog_level;

	autolog_path = settings_get_str("autolog_path");
	autolog_level = !settings_get_bool("autolog") ? 0 :
		level2bits(settings_get_str("autolog_level"));

	if (old_autolog && !autolog_level)
		autologs_close_all();

	/* write to log files with different theme? */
	log_theme_name = settings_get_str("log_theme");
	if (old_log_theme == NULL && *log_theme_name != '\0') {
                /* theme set */
		signal_add("print format", (SIGNAL_FUNC) sig_print_format);
	} else if (old_log_theme != NULL && *log_theme_name == '\0') {
		/* theme unset */
		signal_remove("print format", (SIGNAL_FUNC) sig_print_format);
	}

	log_theme = *log_theme_name == '\0' ? NULL :
		theme_load(log_theme_name);
}

void fe_log_init(void)
{
	autoremove_tag = g_timeout_add(60000, (GSourceFunc) sig_autoremove, NULL);
	skip_next_printtext = FALSE;

        settings_add_str("log", "autolog_path", "~/irclogs/$tag/$0.log");
	settings_add_str("log", "autolog_level", "all -crap -clientcrap");
        settings_add_bool("log", "autolog", FALSE);
        settings_add_str("log", "log_theme", "");

	autolog_level = 0;
	read_settings();

	command_bind("log", NULL, (SIGNAL_FUNC) cmd_log);
	command_bind("log open", NULL, (SIGNAL_FUNC) cmd_log_open);
	command_bind("log close", NULL, (SIGNAL_FUNC) cmd_log_close);
	command_bind("log start", NULL, (SIGNAL_FUNC) cmd_log_start);
	command_bind("log stop", NULL, (SIGNAL_FUNC) cmd_log_stop);
	command_bind("window log", NULL, (SIGNAL_FUNC) cmd_window_log);
	command_bind("window logfile", NULL, (SIGNAL_FUNC) cmd_window_logfile);
	signal_add_first("print text stripped", (SIGNAL_FUNC) sig_printtext_stripped);
	signal_add("window item remove", (SIGNAL_FUNC) sig_window_item_remove);
	signal_add("window refnum changed", (SIGNAL_FUNC) sig_window_refnum_changed);
	signal_add("log locked", (SIGNAL_FUNC) sig_log_locked);
	signal_add("log create failed", (SIGNAL_FUNC) sig_log_create_failed);
	signal_add("awaylog show", (SIGNAL_FUNC) sig_awaylog_show);
	signal_add("theme destroyed", (SIGNAL_FUNC) sig_theme_destroyed);
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);

	command_set_options("log open", "noopen autoopen -targets window");
}

void fe_log_deinit(void)
{
	g_source_remove(autoremove_tag);
	if (log_theme_name != NULL && *log_theme_name != '\0')
                signal_remove("print format", (SIGNAL_FUNC) sig_print_format);

	command_unbind("log", (SIGNAL_FUNC) cmd_log);
	command_unbind("log open", (SIGNAL_FUNC) cmd_log_open);
	command_unbind("log close", (SIGNAL_FUNC) cmd_log_close);
	command_unbind("log start", (SIGNAL_FUNC) cmd_log_start);
	command_unbind("log stop", (SIGNAL_FUNC) cmd_log_stop);
	command_unbind("window log", (SIGNAL_FUNC) cmd_window_log);
	command_unbind("window logfile", (SIGNAL_FUNC) cmd_window_logfile);
	signal_remove("print text stripped", (SIGNAL_FUNC) sig_printtext_stripped);
	signal_remove("window item remove", (SIGNAL_FUNC) sig_window_item_remove);
	signal_remove("window refnum changed", (SIGNAL_FUNC) sig_window_refnum_changed);
	signal_remove("log locked", (SIGNAL_FUNC) sig_log_locked);
	signal_remove("log create failed", (SIGNAL_FUNC) sig_log_create_failed);
	signal_remove("awaylog show", (SIGNAL_FUNC) sig_awaylog_show);
	signal_remove("theme destroyed", (SIGNAL_FUNC) sig_theme_destroyed);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
}
