/*
 printtext.c : irssi

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
#include "modules.h"
#include "signals.h"
#include "commands.h"
#include "settings.h"

#include "levels.h"
#include "servers.h"

#include "themes.h"
#include "fe-windows.h"
#include "printtext.h"

static int beep_msg_level, beep_when_away;

static int signal_gui_print_text;
static int signal_print_text_stripped;
static int signal_print_text;
static int signal_print_text_finished;
static int signal_print_format;
static int signal_print_starting;

static int sending_print_starting;

static void print_line(TEXT_DEST_REC *dest, const char *text);

void printbeep(void)
{
	signal_emit_id(signal_gui_print_text, 6, active_win, NULL, NULL,
		       GINT_TO_POINTER(PRINTFLAG_BEEP), "", MSGLEVEL_NEVER);
}

static void printformat_module_dest(const char *module, TEXT_DEST_REC *dest,
				    int formatnum, va_list va)
{
	char *arglist[MAX_FORMAT_PARAMS];
	char buffer[DEFAULT_FORMAT_ARGLIST_SIZE];
	FORMAT_REC *formats;
	THEME_REC *theme;
	char *str;

	theme = dest->window->theme == NULL ? current_theme :
		dest->window->theme;

	formats = g_hash_table_lookup(default_formats, module);
	format_read_arglist(va, &formats[formatnum],
			    arglist, sizeof(arglist)/sizeof(char *),
			    buffer, sizeof(buffer));

	if (!sending_print_starting) {
		sending_print_starting = TRUE;
		signal_emit_id(signal_print_starting, 1, dest);
                sending_print_starting = FALSE;
	}

	signal_emit_id(signal_print_format, 5, theme, module,
		       dest, GINT_TO_POINTER(formatnum), arglist);

        str = format_get_text_theme_charargs(theme, module, dest, formatnum, arglist);
	if (*str != '\0') print_line(dest, str);
	g_free(str);
}

void printformat_module_args(const char *module, void *server,
			     const char *target, int level,
			     int formatnum, va_list va)
{
	TEXT_DEST_REC dest;

	format_create_dest(&dest, server, target, level, NULL);
	printformat_module_dest(module, &dest, formatnum, va);
}

void printformat_module(const char *module, void *server, const char *target, int level, int formatnum, ...)
{
	va_list va;

	va_start(va, formatnum);
	printformat_module_args(module, server, target, level, formatnum, va);
	va_end(va);
}

void printformat_module_window_args(const char *module, WINDOW_REC *window,
				    int level, int formatnum, va_list va)
{
	TEXT_DEST_REC dest;

	format_create_dest(&dest, NULL, NULL, level, window);
	printformat_module_dest(module, &dest, formatnum, va);
}

void printformat_module_window(const char *module, WINDOW_REC *window,
			       int level, int formatnum, ...)
{
	va_list va;

	va_start(va, formatnum);
	printformat_module_window_args(module, window, level, formatnum, va);
	va_end(va);
}

static void print_line(TEXT_DEST_REC *dest, const char *text)
{
	char *str, *tmp;

	g_return_if_fail(dest != NULL);
	g_return_if_fail(text != NULL);

	tmp = format_get_level_tag(current_theme, dest);
	str = format_add_linestart(text, tmp);
	g_free_not_null(tmp);

	/* send the plain text version for logging etc.. */
	tmp = strip_codes(str);
	signal_emit_id(signal_print_text_stripped, 2, dest, tmp);
	g_free(tmp);

	signal_emit_id(signal_print_text, 2, dest, str);
	g_free(str);
}

/* append string to `out', expand newlines. */
static void printtext_append_str(TEXT_DEST_REC *dest, GString *out, const char *str)
{
	while (*str != '\0') {
		if (*str != '\n')
			g_string_append_c(out, *str);
		else {
			print_line(dest, out->str);
			g_string_truncate(out, 0);
		}
		str++;
	}
}

static char *printtext_get_args(TEXT_DEST_REC *dest, const char *str, va_list va)
{
	GString *out;
	char *ret;

	out = g_string_new(NULL);
	for (; *str != '\0'; str++) {
		if (*str != '%') {
			g_string_append_c(out, *str);
			continue;
		}

		if (*++str == '\0')
			break;

		/* standard parameters */
		switch (*str) {
		case 's': {
			char *s = (char *) va_arg(va, char *);
			if (s && *s) printtext_append_str(dest, out, s);
			break;
		}
		case 'd': {
			int d = (int) va_arg(va, int);
			g_string_sprintfa(out, "%d", d);
			break;
		}
		case 'f': {
			double f = (double) va_arg(va, double);
			g_string_sprintfa(out, "%0.2f", f);
			break;
		}
		case 'u': {
			unsigned int d = (unsigned int) va_arg(va, unsigned int);
			g_string_sprintfa(out, "%u", d);
			break;
                }
		case 'l': {
			long d = (long) va_arg(va, long);

			if (*++str != 'd' && *str != 'u') {
				g_string_sprintfa(out, "%ld", d);
				str--;
			} else {
				if (*str == 'd')
					g_string_sprintfa(out, "%ld", d);
				else
					g_string_sprintfa(out, "%lu", d);
			}
			break;
		}
		default:
			if (!format_expand_styles(out, *str, dest)) {
				g_string_append_c(out, '%');
				g_string_append_c(out, *str);
			}
			break;
		}
	}

	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

static char *printtext_expand_formats(TEXT_DEST_REC *dest, const char *str)
{
	GString *out;
	char *ret;

	out = g_string_new(NULL);
	for (; *str != '\0'; str++) {
		if (*str != '%') {
			g_string_append_c(out, *str);
			continue;
		}

		if (*++str == '\0')
			break;

		if (!format_expand_styles(out, *str, dest)) {
			g_string_append_c(out, '%');
			g_string_append_c(out, *str);
		}
	}

	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

void printtext_dest(TEXT_DEST_REC *dest, const char *text, va_list va)
{
	char *str;

	if (!sending_print_starting) {
		sending_print_starting = TRUE;
		signal_emit_id(signal_print_starting, 1, dest);
                sending_print_starting = FALSE;
	}

	str = printtext_get_args(dest, text, va);
	print_line(dest, str);
	g_free(str);
}

/* Write text to target - convert color codes */
void printtext(void *server, const char *target, int level, const char *text, ...)
{
	TEXT_DEST_REC dest;
	va_list va;

	g_return_if_fail(text != NULL);

        format_create_dest(&dest, server, target, level, NULL);

	va_start(va, text);
	printtext_dest(&dest, text, va);
	va_end(va);
}

/* Like printtext(), but don't handle %s etc. */
void printtext_string(void *server, const char *target, int level, const char *text)
{
	TEXT_DEST_REC dest;
        char *str;

	g_return_if_fail(text != NULL);

        format_create_dest(&dest, server, target, level, NULL);

	if (!sending_print_starting) {
		sending_print_starting = TRUE;
		signal_emit_id(signal_print_starting, 1, dest);
                sending_print_starting = FALSE;
	}

        str = printtext_expand_formats(&dest, text);
	print_line(&dest, str);
        g_free(str);
}

void printtext_window(WINDOW_REC *window, int level, const char *text, ...)
{
	TEXT_DEST_REC dest;
	va_list va;

	g_return_if_fail(text != NULL);

	format_create_dest(&dest, NULL, NULL, level,
			   window != NULL ? window : active_win);

	va_start(va, text);
	printtext_dest(&dest, text, va);
	va_end(va);
}

static void msg_beep_check(SERVER_REC *server, int level)
{
	if (level != 0 && (level & MSGLEVEL_NOHILIGHT) == 0 &&
	    (beep_msg_level & level) &&
	    (beep_when_away || (server != NULL && !server->usermode_away))) {
		printbeep();
	}
}

static void sig_print_text(TEXT_DEST_REC *dest, const char *text)
{
	char *str, *tmp;

	g_return_if_fail(dest != NULL);
	g_return_if_fail(text != NULL);

	msg_beep_check(dest->server, dest->level);

	dest->window->last_line = time(NULL);
	format_newline(dest->window);

	/* add timestamp/server tag here - if it's done in print_line()
	   it would be written to log files too */
	tmp = format_get_line_start(current_theme, dest, time(NULL));
	str = format_add_linestart(text, tmp);
	g_free_not_null(tmp);

	format_send_to_gui(dest, str);
	g_free(str);

	signal_emit_id(signal_print_text_finished, 1, dest->window);
}

void printtext_multiline(void *server, const char *target, int level,
			 const char *format, const char *text)
{
	char **lines, **tmp;

	g_return_if_fail(format != NULL);
	g_return_if_fail(text != NULL);

	lines = g_strsplit(text, "\n", -1);
        for (tmp = lines; *tmp != NULL; tmp++)
		printtext(NULL, NULL, level, format, *tmp);
	g_strfreev(lines);
}

static void sig_gui_dialog(const char *type, const char *text)
{
	char *format;

	if (g_strcasecmp(type, "warning") == 0)
		format = _("%_Warning:%_ %s");
	else if (g_strcasecmp(type, "error") == 0)
		format = _("%_Error:%_ %s");
	else
		format = "%s";

        printtext_multiline(NULL, NULL, MSGLEVEL_NEVER, format, text);
}

static void read_settings(void)
{
	beep_msg_level = level2bits(settings_get_str("beep_msg_level"));
	beep_when_away = settings_get_bool("beep_when_away");
}

void printtext_init(void)
{
	settings_add_int("misc", "timestamp_timeout", 0);
	settings_add_str("lookandfeel", "beep_msg_level", "");

	sending_print_starting = FALSE;
	signal_gui_print_text = signal_get_uniq_id("gui print text");
	signal_print_text_stripped = signal_get_uniq_id("print text stripped");
	signal_print_text = signal_get_uniq_id("print text");
	signal_print_text_finished = signal_get_uniq_id("print text finished");
	signal_print_format = signal_get_uniq_id("print format");
	signal_print_starting = signal_get_uniq_id("print starting");

	read_settings();
	signal_add("print text", (SIGNAL_FUNC) sig_print_text);
	signal_add("gui dialog", (SIGNAL_FUNC) sig_gui_dialog);
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);
}

void printtext_deinit(void)
{
	signal_remove("print text", (SIGNAL_FUNC) sig_print_text);
	signal_remove("gui dialog", (SIGNAL_FUNC) sig_gui_dialog);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
}
