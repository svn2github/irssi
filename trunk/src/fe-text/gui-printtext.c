/*
 gui-printtext.c : irssi

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
#include "settings.h"

#include "formats.h"
#include "printtext.h"

#include "screen.h"
#include "gui-printtext.h"
#include "gui-windows.h"

int mirc_colors[] = { 15, 0, 1, 2, 12, 6, 5, 4, 14, 10, 3, 11, 9, 13, 8, 7 };
static int scrollback_lines, scrollback_hours, scrollback_burst_remove;

static int last_color, last_flags;
static int next_xpos, next_ypos;

static GHashTable *indent_functions;
static INDENT_FUNC default_indent_func;

void gui_register_indent_func(const char *name, INDENT_FUNC func)
{
	gpointer key, value;
        GSList *list;

	if (g_hash_table_lookup_extended(indent_functions, name, &key, &value)) {
                list = value;
		g_hash_table_remove(indent_functions, key);
	} else {
		key = g_strdup(name);
                list = NULL;
	}

	list = g_slist_append(list, func);
	g_hash_table_insert(indent_functions, key, list);
}

void gui_unregister_indent_func(const char *name, INDENT_FUNC func)
{
	gpointer key, value;
        GSList *list;

	if (g_hash_table_lookup_extended(indent_functions, name, &key, &value)) {
		list = value;

		list = g_slist_remove(list, func);
		g_hash_table_remove(indent_functions, key);
		if (list == NULL)
			g_free(key);
                else
			g_hash_table_insert(indent_functions, key, list);
	}

	if (default_indent_func == func)
		gui_set_default_indent(NULL);

	textbuffer_views_unregister_indent_func(func);
}

void gui_set_default_indent(const char *name)
{
	GSList *list;

	list = name == NULL ? NULL :
		g_hash_table_lookup(indent_functions, name);
	default_indent_func = list == NULL ? NULL : list->data;
        gui_windows_reset_settings();
}

INDENT_FUNC get_default_indent_func(void)
{
        return default_indent_func;
}

void gui_printtext(int xpos, int ypos, const char *str)
{
	next_xpos = xpos;
	next_ypos = ypos;

	printtext_gui(str);

	next_xpos = next_ypos = -1;
}

void gui_printtext_after(TEXT_DEST_REC *dest, LINE_REC *prev, const char *str)
{
	GUI_WINDOW_REC *gui;

	gui = WINDOW_GUI(dest->window);

	gui->use_insert_after = TRUE;
	gui->insert_after = prev;
	format_send_to_gui(dest, str);
	gui->use_insert_after = FALSE;
}

static void remove_old_lines(TEXT_BUFFER_VIEW_REC *view)
{
	LINE_REC *line;
	time_t old_time;

	old_time = time(NULL)-(scrollback_hours*3600)+1;
	if (view->buffer->lines_count >=
	    scrollback_lines+scrollback_burst_remove) {
                /* remove lines by line count */
		while (view->buffer->lines_count > scrollback_lines) {
			line = view->buffer->first_line;
			if (line->info.time >= old_time ||
			    scrollback_lines == 0) {
				/* too new line, don't remove yet - also
				   if scrollback_lines is 0, we want to check
				   only scrollback_hours setting. */
				break;
			}
			textbuffer_view_remove_line(view, line);
		}
	}
}

static void get_colors(int flags, int *fg, int *bg)
{
	if (flags & GUI_PRINT_FLAG_MIRC_COLOR) {
		/* mirc colors - real range is 0..15, but after 16
		   colors wrap to 0, 1, ... */
		*bg = *bg < 0 ? 0 : mirc_colors[*bg % 16];
		if (*fg > 0) *fg = mirc_colors[*fg % 16];
	} else {
		/* default colors */
		*bg = *bg < 0 || *bg > 15 ? 0 : *bg;
                if (*fg > 8) *fg &= ~8;
	}

	if (*fg < 0 || *fg > 15) {
		*fg = *bg == 0 ? current_theme->default_color :
			current_theme->default_real_color;
	}

	if (flags & GUI_PRINT_FLAG_REVERSE)
                *fg |= ATTR_REVERSE;

	if (*fg == 8) *fg |= ATTR_COLOR8;
	if (flags & GUI_PRINT_FLAG_BOLD) {
		if (*fg == 0) *fg = current_theme->default_real_color;
		*fg |= 8;
	}
	if (flags & GUI_PRINT_FLAG_UNDERLINE) *fg |= ATTR_UNDERLINE;
	if (flags & GUI_PRINT_FLAG_BLINK) *bg |= 0x08;
}

static void line_add_colors(TEXT_BUFFER_REC *buffer, LINE_REC **line,
			    int fg, int bg, int flags)
{
	unsigned char data[20];
	int color, pos;

	/* color should never have last bit on or it would be treated as a
	   command! */
	color = (fg & 0x0f) | ((bg & 0x07) << 4);
	pos = 0;

	if (((fg & ATTR_COLOR8) == 0 && (fg|(bg << 4)) != last_color) ||
	    ((fg & ATTR_COLOR8) && (fg & 0xf0) != (last_color & 0xf0))) {
		data[pos++] = 0;
		data[pos++] = color == 0 ? LINE_CMD_COLOR0 : color;
	}

	if ((flags & GUI_PRINT_FLAG_UNDERLINE) != (last_flags & GUI_PRINT_FLAG_UNDERLINE)) {
		data[pos++] = 0;
		data[pos++] = LINE_CMD_UNDERLINE;
	}
	if ((flags & GUI_PRINT_FLAG_REVERSE) != (last_flags & GUI_PRINT_FLAG_REVERSE)) {
		data[pos++] = 0;
		data[pos++] = LINE_CMD_REVERSE;
	}
	if (fg & ATTR_COLOR8) {
		data[pos++] = 0;
		data[pos++] = LINE_CMD_COLOR8;
	}
	if (bg & 0x08) {
		data[pos++] = 0;
		data[pos++] = LINE_CMD_BLINK;
	}
	if (flags & GUI_PRINT_FLAG_INDENT) {
		data[pos++] = 0;
		data[pos++] = LINE_CMD_INDENT;
	}

        if (pos > 0)
		*line = textbuffer_insert(buffer, *line, data, pos, NULL);

	last_flags = flags;
	last_color = fg | (bg << 4);
}

static void line_add_indent_func(TEXT_BUFFER_REC *buffer, LINE_REC **line,
				 const char *function)
{
        GSList *list;
        unsigned char data[1+sizeof(INDENT_FUNC)];

        list = g_hash_table_lookup(indent_functions, function);
	if (list != NULL) {
		data[0] = LINE_CMD_INDENT_FUNC;
		memcpy(data+1, list->data, sizeof(INDENT_FUNC));
		*line = textbuffer_insert(buffer, *line,
					  data, sizeof(data), NULL);
	}
}

static void view_add_eol(TEXT_BUFFER_VIEW_REC *view, LINE_REC **line)
{
	static const unsigned char eol[] = { 0, LINE_CMD_EOL };

	*line = textbuffer_insert(view->buffer, *line, eol, 2, NULL);
	textbuffer_view_insert_line(view, *line);
}

static void sig_gui_print_text(WINDOW_REC *window, void *fgcolor,
			       void *bgcolor, void *pflags,
			       char *str, void *level)
{
        GUI_WINDOW_REC *gui;
        TEXT_BUFFER_VIEW_REC *view;
	LINE_REC *insert_after;
        LINE_INFO_REC lineinfo;
	int fg, bg, flags;

	flags = GPOINTER_TO_INT(pflags);
	fg = GPOINTER_TO_INT(fgcolor);
	bg = GPOINTER_TO_INT(bgcolor);
	get_colors(flags, &fg, &bg);

	if (window == NULL) {
                g_return_if_fail(next_xpos != -1);

		screen_move(screen_root, next_xpos, next_ypos);
		if (flags & GUI_PRINT_FLAG_CLRTOEOL) {
			screen_set_bg(screen_root, fg | (bg << 4));
			screen_clrtoeol(screen_root);
			screen_set_bg(screen_root, 0);
		}
		screen_set_color(screen_root, fg | (bg << 4));
		screen_addstr(screen_root, str);
		next_xpos += strlen(str);
                return;
	}

	lineinfo.level = GPOINTER_TO_INT(level);
        lineinfo.time = time(NULL);

        gui = WINDOW_GUI(window);
	view = gui->view;
	insert_after = gui->use_insert_after ?
		gui->insert_after : view->buffer->cur_line;

	if (flags & GUI_PRINT_FLAG_NEWLINE)
                view_add_eol(view, &insert_after);
	line_add_colors(view->buffer, &insert_after, fg, bg, flags);

	if (flags & GUI_PRINT_FLAG_INDENT_FUNC) {
		/* specify the indentation function */
                line_add_indent_func(view->buffer, &insert_after, str);
	} else {
		insert_after = textbuffer_insert(view->buffer, insert_after,
						 str, strlen(str), &lineinfo);
	}
	if (gui->use_insert_after)
                gui->insert_after = insert_after;
}

static void sig_gui_printtext_finished(WINDOW_REC *window)
{
	TEXT_BUFFER_VIEW_REC *view;
	LINE_REC *insert_after;

        last_color = 0;
	last_flags = 0;

	view = WINDOW_GUI(window)->view;
	insert_after = WINDOW_GUI(window)->use_insert_after ?
		WINDOW_GUI(window)->insert_after : view->buffer->cur_line;

        view_add_eol(view, &insert_after);
	remove_old_lines(view);
}

static void read_settings(void)
{
	scrollback_lines = settings_get_int("scrollback_lines");
	scrollback_hours = settings_get_int("scrollback_hours");
        scrollback_burst_remove = settings_get_int("scrollback_burst_remove");
}

void gui_printtext_init(void)
{
	next_xpos = next_ypos = -1;
	default_indent_func = NULL;
	indent_functions = g_hash_table_new((GHashFunc) g_str_hash,
					    (GCompareFunc) g_str_equal);

	settings_add_int("history", "scrollback_lines", 500);
	settings_add_int("history", "scrollback_hours", 24);
	settings_add_int("history", "scrollback_burst_remove", 10);

	signal_add("gui print text", (SIGNAL_FUNC) sig_gui_print_text);
	signal_add("gui print text finished", (SIGNAL_FUNC) sig_gui_printtext_finished);
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);

	read_settings();
}

void gui_printtext_deinit(void)
{
	g_hash_table_destroy(indent_functions);

	signal_remove("gui print text", (SIGNAL_FUNC) sig_gui_print_text);
	signal_remove("gui print text finished", (SIGNAL_FUNC) sig_gui_printtext_finished);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
}
