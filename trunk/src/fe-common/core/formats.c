/*
 formats.c : irssi

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
#include "special-vars.h"
#include "settings.h"

#include "levels.h"

#include "fe-windows.h"
#include "formats.h"
#include "themes.h"
#include "translation.h"

static int signal_gui_print_text;
static int hide_text_style;

static int timestamps, msgs_timestamps;
static int timestamp_timeout;

int format_find_tag(const char *module, const char *tag)
{
	FORMAT_REC *formats;
	int n;

	formats = g_hash_table_lookup(default_formats, module);
	if (formats == NULL)
		return -1;

	for (n = 0; formats[n].def != NULL; n++) {
		if (formats[n].tag != NULL &&
		    g_strcasecmp(formats[n].tag, tag) == 0)
			return n;
	}

	return -1;
}

int format_expand_styles(GString *out, char format, TEXT_DEST_REC *dest)
{
	static const char *backs = "04261537";
	static const char *fores = "kbgcrmyw";
	static const char *boldfores = "KBGCRMYW";
	char *p;

	switch (format) {
	case 'U':
		/* Underline on/off */
		g_string_append_c(out, 4);
		g_string_append_c(out, FORMAT_STYLE_UNDERLINE);
		break;
	case '9':
	case '_':
		/* bold on/off */
		g_string_append_c(out, 4);
		g_string_append_c(out, FORMAT_STYLE_BOLD);
		break;
	case '8':
		/* reverse */
		g_string_append_c(out, 4);
		g_string_append_c(out, FORMAT_STYLE_REVERSE);
		break;
	case '%':
		g_string_append_c(out, '%');
		break;
	case ':':
		/* Newline */
		g_string_append_c(out, '\n');
		break;
	case '|':
		/* Indent here */
		g_string_append_c(out, 4);
		g_string_append_c(out, FORMAT_STYLE_INDENT);
		break;
	case 'F':
		/* flashing - ignore */
		break;
	case 'N':
		/* don't put clear-color tag at the end of the output - ignore */
		break;
	case 'n':
		/* default color */
		g_string_append_c(out, 4);
		g_string_append_c(out, FORMAT_STYLE_DEFAULTS);
		break;
	default:
		/* check if it's a background color */
		p = strchr(backs, format);
		if (p != NULL) {
			g_string_append_c(out, 4);
			g_string_append_c(out, FORMAT_COLOR_NOCHANGE);
			g_string_append_c(out, (char) ((int) (p-backs)+'0'));
			break;
		}

		/* check if it's a foreground color */
		if (format == 'p') format = 'm';
		p = strchr(fores, format);
		if (p != NULL) {
			g_string_append_c(out, 4);
			g_string_append_c(out, (char) ((int) (p-fores)+'0'));
			g_string_append_c(out, FORMAT_COLOR_NOCHANGE);
			break;
		}

		/* check if it's a bold foreground color */
		if (format == 'P') format = 'M';
		p = strchr(boldfores, format);
		if (p != NULL) {
			g_string_append_c(out, 4);
			g_string_append_c(out, (char) (8+(int) (p-boldfores)+'0'));
			g_string_append_c(out, FORMAT_COLOR_NOCHANGE);
			break;
		}

		return FALSE;
	}

	return TRUE;
}

void format_read_arglist(va_list va, FORMAT_REC *format,
			 char **arglist, int arglist_size,
			 char *buffer, int buffer_size)
{
	int num, len, bufpos;

        g_return_if_fail(format->params < arglist_size);

	bufpos = 0;
        arglist[format->params] = NULL;
	for (num = 0; num < format->params; num++) {
		switch (format->paramtypes[num]) {
		case FORMAT_STRING:
			arglist[num] = (char *) va_arg(va, char *);
			if (arglist[num] == NULL) {
				g_warning("format_read_arglist() : parameter %d is NULL", num);
				arglist[num] = "";
			}
			break;
		case FORMAT_INT: {
			int d = (int) va_arg(va, int);

			if (bufpos >= buffer_size) {
				arglist[num] = "";
				break;
			}

			arglist[num] = buffer+bufpos;
			len = g_snprintf(buffer+bufpos, buffer_size-bufpos,
					 "%d", d);
			bufpos += len+1;
			break;
		}
		case FORMAT_LONG: {
			long l = (long) va_arg(va, long);

			if (bufpos >= buffer_size) {
				arglist[num] = "";
				break;
			}

			arglist[num] = buffer+bufpos;
			len = g_snprintf(buffer+bufpos, buffer_size-bufpos,
					 "%ld", l);
                        bufpos += len+1;
			break;
		}
		case FORMAT_FLOAT: {
			double f = (double) va_arg(va, double);

			if (bufpos >= buffer_size) {
				arglist[num] = "";
				break;
			}

			arglist[num] = buffer+bufpos;
			len = g_snprintf(buffer+bufpos, buffer_size-bufpos,
					 "%0.2f", f);
			bufpos += len+1;
			break;
		}
		}
	}
}

void format_create_dest(TEXT_DEST_REC *dest,
			void *server, const char *target,
			int level, WINDOW_REC *window)
{
	dest->server = server;
	dest->target = target;
	dest->level = level;
	dest->window = window != NULL ? window :
		window_find_closest(server, target, level);
}

static char *format_get_text_args(TEXT_DEST_REC *dest,
				  const char *text, char **arglist)
{
	GString *out;
	char code, *ret;
	int need_free;

	out = g_string_new(NULL);

	code = 0;
	while (*text != '\0') {
		if (code == '%') {
			/* color code */
			if (!format_expand_styles(out, *text, dest)) {
				g_string_append_c(out, '%');
				g_string_append_c(out, '%');
				g_string_append_c(out, *text);
			}
			code = 0;
		} else if (code == '$') {
			/* argument */
			char *ret;

			ret = parse_special((char **) &text, active_win->active_server,
					    active_win->active, arglist, &need_free, NULL);

			if (ret != NULL) {
				/* string shouldn't end with \003 or it could
				   mess up the next one or two characters */
                                int diff;
				int len = strlen(ret);
				while (len > 0 && ret[len-1] == 3) len--;
				diff = strlen(ret)-len;

				g_string_append(out, ret);
				if (diff > 0)
					g_string_truncate(out, out->len-diff);
				if (need_free) g_free(ret);
			}
			code = 0;
		} else {
			if (*text == '%' || *text == '$')
				code = *text;
			else
				g_string_append_c(out, *text);
		}

		text++;
	}

	ret = out->str;
	g_string_free(out, FALSE);
	return ret;
}

char *format_get_text_theme(THEME_REC *theme, const char *module,
			    TEXT_DEST_REC *dest, int formatnum, ...)
{
	va_list va;
	char *str;

	if (theme == NULL) {
		theme = dest->window->theme == NULL ? current_theme :
			dest->window->theme;
	}

	va_start(va, formatnum);
	str = format_get_text_theme_args(theme, module, dest, formatnum, va);
	va_end(va);

	return str;
}

char *format_get_text_theme_args(THEME_REC *theme, const char *module,
				 TEXT_DEST_REC *dest, int formatnum,
				 va_list va)
{
	char *arglist[MAX_FORMAT_PARAMS];
	char buffer[DEFAULT_FORMAT_ARGLIST_SIZE];
	FORMAT_REC *formats;

	formats = g_hash_table_lookup(default_formats, module);
	format_read_arglist(va, &formats[formatnum],
			    arglist, sizeof(arglist)/sizeof(char *),
			    buffer, sizeof(buffer));

	return format_get_text_theme_charargs(theme, module, dest,
					      formatnum, arglist);
}

char *format_get_text_theme_charargs(THEME_REC *theme, const char *module,
				     TEXT_DEST_REC *dest, int formatnum,
				     char **args)
{
	MODULE_THEME_REC *module_theme;
	char *text;

	module_theme = g_hash_table_lookup(theme->modules, module);
	if (module_theme == NULL)
		return NULL;

        text = module_theme->expanded_formats[formatnum];
	return format_get_text_args(dest, text, args);
}

char *format_get_text(const char *module, WINDOW_REC *window,
		      void *server, const char *target,
		      int formatnum, ...)
{
	TEXT_DEST_REC dest;
	THEME_REC *theme;
	va_list va;
	char *str;

	format_create_dest(&dest, server, target, 0, window);
	theme = dest.window->theme == NULL ? current_theme :
		dest.window->theme;

	va_start(va, formatnum);
	str = format_get_text_theme_args(theme, module, &dest, formatnum, va);
	va_end(va);

	return str;
}

/* add `linestart' to start of each line in `text'. `text' may contain
   multiple lines separated with \n. */
char *format_add_linestart(const char *text, const char *linestart)
{
	GString *str;
	char *ret;

	if (linestart == NULL)
		return g_strdup(text);

	if (strchr(text, '\n') == NULL)
		return g_strconcat(linestart, text, NULL);

	str = g_string_new(linestart);
	while (*text != '\0') {
		g_string_append_c(str, *text);
		if (*text == '\n')
			g_string_append(str, linestart);
		text++;
	}

	ret = str->str;
	g_string_free(str, FALSE);
        return ret;
}

#define LINE_START_IRSSI_LEVEL \
	(MSGLEVEL_CLIENTERROR | MSGLEVEL_CLIENTNOTICE)

#define NOT_LINE_START_LEVEL \
	(MSGLEVEL_NEVER | MSGLEVEL_LASTLOG | MSGLEVEL_CLIENTCRAP | \
	MSGLEVEL_MSGS | MSGLEVEL_PUBLIC | MSGLEVEL_DCC | MSGLEVEL_DCCMSGS | \
	MSGLEVEL_ACTIONS | MSGLEVEL_NOTICES | MSGLEVEL_SNOTES | MSGLEVEL_CTCPS)

/* return the "-!- " text at the start of the line */
char *format_get_level_tag(THEME_REC *theme, TEXT_DEST_REC *dest)
{
	int format;

	if (dest->level & LINE_START_IRSSI_LEVEL)
		format = IRCTXT_LINE_START_IRSSI;
	else if ((dest->level & NOT_LINE_START_LEVEL) == 0)
		format = IRCTXT_LINE_START;
	else
		return NULL;

	return format_get_text_theme(theme, MODULE_NAME, dest, format);
}

#define show_timestamp(level) \
	((level & (MSGLEVEL_NEVER|MSGLEVEL_LASTLOG)) == 0 && \
	(timestamps || (msgs_timestamps && ((level) & MSGLEVEL_MSGS))))

static char *get_timestamp(THEME_REC *theme, TEXT_DEST_REC *dest, time_t t)
{
	struct tm *tm;
	int diff;

	if (!show_timestamp(dest->level))
		return NULL;

	if (timestamp_timeout > 0) {
		diff = t - dest->window->last_timestamp;
		dest->window->last_timestamp = t;
		if (diff < timestamp_timeout)
			return NULL;
	}

	tm = localtime(&t);
	return format_get_text_theme(theme, MODULE_NAME, dest, IRCTXT_TIMESTAMP,
				     tm->tm_year+1900,
				     tm->tm_mon+1, tm->tm_mday,
				     tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static char *get_server_tag(THEME_REC *theme, TEXT_DEST_REC *dest)
{
	SERVER_REC *server;
	int count = 0;

	server = dest->server;

	if (server == NULL || (dest->window->active != NULL &&
			       dest->window->active->server == server))
		return NULL;

	if (servers != NULL) {
		count++;
		if (servers->next != NULL)
			count++;
	}
	if (count < 2 && lookup_servers != NULL) {
                count++;
		if (lookup_servers->next != NULL)
			count++;
	}

	return count < 2 ? NULL :
		format_get_text_theme(theme, MODULE_NAME, dest,
				      IRCTXT_SERVERTAG, server->tag);
}

char *format_get_line_start(THEME_REC *theme, TEXT_DEST_REC *dest, time_t t)
{
	char *timestamp, *servertag;
	char *linestart;

	timestamp = get_timestamp(theme, dest, t);
	servertag = get_server_tag(theme, dest);

	if (timestamp == NULL && servertag == NULL)
		return NULL;

	linestart = g_strconcat(timestamp != NULL ? timestamp : "",
				servertag, NULL);

	g_free_not_null(timestamp);
	g_free_not_null(servertag);
	return linestart;
}

void format_newline(WINDOW_REC *window)
{
	window->lines++;
	if (window->lines != 1) {
		signal_emit_id(signal_gui_print_text, 6, window,
			       GINT_TO_POINTER(-1), GINT_TO_POINTER(-1),
			       GINT_TO_POINTER(PRINTFLAG_NEWLINE),
			       "", GINT_TO_POINTER(-1));
	}
}

/* parse ANSI color string */
static char *get_ansi_color(THEME_REC *theme, char *str,
			    int *fg_ret, int *bg_ret, int *flags_ret)
{
	static char ansitab[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
	char *start;
	int fg, bg, flags, num;

	if (*str != '[')
		return str;
	start = str++;

	fg = fg_ret == NULL || *fg_ret < 0 ? theme->default_color : *fg_ret;
	bg = bg_ret == NULL || *bg_ret < 0 ? -1 : *bg_ret;
	flags = flags_ret == NULL ? 0 : *flags_ret;

	num = 0;
	for (;; str++) {
		if (*str == '\0') return start;

		if (isdigit((int) *str)) {
			num = num*10 + (*str-'0');
			continue;
		}

		if (*str != ';' && *str != 'm')
			return start;

		switch (num) {
		case 0:
			/* reset colors back to default */
			fg = theme->default_color;
			bg = -1;
			flags &= ~(PRINTFLAG_BEEP|PRINTFLAG_INDENT);
			break;
		case 1:
			/* hilight */
			flags |= PRINTFLAG_BOLD;
			break;
		case 5:
			/* blink */
			flags |= PRINTFLAG_BLINK;
			break;
		case 7:
			/* reverse */
			flags |= PRINTFLAG_REVERSE;
			break;
		default:
			if (num >= 30 && num <= 37)
				fg = (fg & 0xf8) + ansitab[num-30];
			if (num >= 40 && num <= 47) {
				if (bg == -1) bg = 0;
				bg = (bg & 0xf8) + ansitab[num-40];
			}
			break;
		}
		num = 0;

		if (*str == 'm') {
			if (fg_ret != NULL) *fg_ret = fg;
			if (bg_ret != NULL) *bg_ret = bg;
			if (flags_ret != NULL) *flags_ret = flags;

			str++;
			break;
		}
	}

	return str;
}

/* parse MIRC color string */
static void get_mirc_color(const char **str, int *fg_ret, int *bg_ret)
{
	int fg, bg;

	fg = fg_ret == NULL ? -1 : *fg_ret;
	bg = bg_ret == NULL ? -1 : *bg_ret;

	if (!isdigit((int) **str) && **str != ',') {
		fg = -1;
		bg = -1;
	} else {
		/* foreground color */
		if (**str != ',') {
			fg = **str-'0';
                        (*str)++;
			if (isdigit((int) **str)) {
				fg = fg*10 + (**str-'0');
				(*str)++;
			}
		}
		if (**str == ',') {
			/* background color */
			(*str)++;
			if (!isdigit((int) **str))
				bg = -1;
			else {
				bg = **str-'0';
				(*str)++;
				if (isdigit((int) **str)) {
					bg = bg*10 + (**str-'0');
					(*str)++;
				}
			}
		}
	}

	if (fg_ret) *fg_ret = fg;
	if (bg_ret) *bg_ret = bg;
}

#define IS_COLOR_CODE(c) \
	((c) == 2 || (c) == 3 || (c) == 4 || (c) == 6 || (c) == 7 || \
	(c) == 15 || (c) == 22 || (c) == 27 || (c) == 31)

char *strip_codes(const char *input)
{
	const char *p;
	char *str, *out;

	out = str = g_strdup(input);
	for (p = input; *p != '\0'; p++) {
		if (*p == 3) {
			p++;

			/* mirc color */
			get_mirc_color(&p, NULL, NULL);
			p--;
			continue;
		}

		if (*p == 4 && p[1] != '\0') {
			if (p[1] >= FORMAT_STYLE_SPECIAL) {
				p++;
				continue;
			}

			/* irssi color */
			if (p[2] != '\0') {
				p += 2;
				continue;
			}
		}

		if (!IS_COLOR_CODE(*p))
			*out++ = *p;
	}

	*out = '\0';
	return str;
}

/* send a fully parsed text string for GUI to print */
void format_send_to_gui(TEXT_DEST_REC *dest, const char *text)
{
	char *dup, *str, *ptr, type;
	int fgcolor, bgcolor;
	int flags;

	dup = str = g_strdup(text);

	flags = 0; fgcolor = -1; bgcolor = -1;
	while (*str != '\0') {
                type = '\0';
		for (ptr = str; *ptr != '\0'; ptr++) {
			if (IS_COLOR_CODE(*ptr) || *ptr == '\n') {
				type = *ptr;
				*ptr++ = '\0';
				break;
			}

			*ptr = (char) translation_in[(int) (unsigned char) *ptr];
		}

		if (type == 7) {
			/* bell */
			if (settings_get_bool("bell_beeps"))
				flags |= PRINTFLAG_BEEP;
		}

		if (*str != '\0' || (flags & PRINTFLAG_BEEP)) {
                        /* send the text to gui handler */
			signal_emit_id(signal_gui_print_text, 6, dest->window,
				       GINT_TO_POINTER(fgcolor),
				       GINT_TO_POINTER(bgcolor),
				       GINT_TO_POINTER(flags), str,
				       dest->level);
			flags &= ~(PRINTFLAG_BEEP | PRINTFLAG_INDENT);
		}

		if (type == '\n')
			format_newline(dest->window);

		if (*ptr == '\0')
			break;

		switch (type)
		{
		case 2:
			/* bold */
			if (!hide_text_style)
				flags ^= PRINTFLAG_BOLD;
			break;
		case 3:
			/* MIRC color */
			get_mirc_color((const char **) &ptr,
				       hide_text_style ? NULL : &fgcolor,
				       hide_text_style ? NULL : &bgcolor);
			if (!hide_text_style)
				flags |= PRINTFLAG_MIRC_COLOR;
			break;
		case 4:
			/* user specific colors */
			flags &= ~PRINTFLAG_MIRC_COLOR;
			switch (*ptr) {
			case FORMAT_STYLE_UNDERLINE:
				flags ^= PRINTFLAG_UNDERLINE;
				break;
			case FORMAT_STYLE_BOLD:
				flags ^= PRINTFLAG_BOLD;
				break;
			case FORMAT_STYLE_REVERSE:
				flags ^= PRINTFLAG_REVERSE;
				break;
			case FORMAT_STYLE_INDENT:
				flags |= PRINTFLAG_INDENT;
				break;
			case FORMAT_STYLE_DEFAULTS:
				fgcolor = bgcolor = -1;
				flags &= PRINTFLAG_INDENT;
				break;
			default:
				if (*ptr != FORMAT_COLOR_NOCHANGE) {
					fgcolor = (unsigned char) *ptr-'0';
					if (fgcolor <= 7)
						flags &= ~PRINTFLAG_BOLD;
					else {
						/* bold */
						if (fgcolor != 8) fgcolor -= 8;
						flags |= PRINTFLAG_BOLD;
					}
				}
				ptr++;
				if (*ptr != FORMAT_COLOR_NOCHANGE)
					bgcolor = *ptr-'0';
			}
			ptr++;
			break;
		case 6:
			/* blink */
			if (!hide_text_style)
				flags ^= PRINTFLAG_BLINK;
			break;
		case 15:
			/* remove all styling */
			flags &= PRINTFLAG_BEEP;
			fgcolor = bgcolor = -1;
			break;
		case 22:
			/* reverse */
			if (!hide_text_style)
				flags ^= PRINTFLAG_REVERSE;
			break;
		case 31:
			/* underline */
			if (!hide_text_style)
				flags ^= PRINTFLAG_UNDERLINE;
			break;
		case 27:
			/* ansi color code */
			ptr = get_ansi_color(dest->window->theme == NULL ?
					     current_theme : dest->window->theme,
					     ptr,
					     hide_text_style ? NULL : &fgcolor,
					     hide_text_style ? NULL : &bgcolor,
					     hide_text_style ? NULL : &flags);
			break;
		}

		str = ptr;
	}

	g_free(dup);
}

static void read_settings(void)
{
	hide_text_style = settings_get_bool("hide_text_style");
	timestamps = settings_get_bool("timestamps");
	timestamp_timeout = settings_get_int("timestamp_timeout");
	msgs_timestamps = settings_get_bool("msgs_timestamps");
}

void formats_init(void)
{
	signal_gui_print_text = signal_get_uniq_id("gui print text");

	read_settings();
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);
}

void formats_deinit(void)
{
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
}
