/*
 gui-entry.c : irssi

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
#include "misc.h"
#include "utf8.h"
#include "formats.h"

#include "gui-entry.h"
#include "gui-printtext.h"
#include "term.h"

const unichar empty_str[] = { 0 };

GUI_ENTRY_REC *active_entry;

static void entry_text_grow(GUI_ENTRY_REC *entry, int grow_size)
{
	if (entry->text_len+grow_size < entry->text_alloc)
		return;

	entry->text_alloc = nearest_power(entry->text_alloc+grow_size);
	entry->text = g_realloc(entry->text, entry->text_alloc);
}

GUI_ENTRY_REC *gui_entry_create(int xpos, int ypos, int width, int utf8)
{
	GUI_ENTRY_REC *rec;

	rec = g_new0(GUI_ENTRY_REC, 1);
	rec->xpos = xpos;
	rec->ypos = ypos;
        rec->width = width;
        rec->text_alloc = 1024;
	rec->text = g_new(unichar, rec->text_alloc);
        rec->text[0] = '\0';
        rec->utf8 = utf8;
	return rec;
}

void gui_entry_destroy(GUI_ENTRY_REC *entry)
{
        g_return_if_fail(entry != NULL);

	if (active_entry == entry)
		gui_entry_set_active(NULL);

        g_free(entry->text);
	g_free(entry->prompt);
        g_free(entry);
}

/* Fixes the cursor position in screen */
static void gui_entry_fix_cursor(GUI_ENTRY_REC *entry)
{
	int old_scrstart;

        old_scrstart = entry->scrstart;
	if (entry->pos - entry->scrstart < entry->width-2 - entry->promptlen &&
	    entry->pos - entry->scrstart > 0) {
		entry->scrpos = entry->pos - entry->scrstart;
	} else if (entry->pos < entry->width-1 - entry->promptlen) {
		entry->scrstart = 0;
		entry->scrpos = entry->pos;
	} else {
		entry->scrpos = (entry->width - entry->promptlen)*2/3;
		entry->scrstart = entry->pos - entry->scrpos;
	}

	if (old_scrstart != entry->scrstart)
                entry->redraw_needed_from = 0;
}

static void gui_entry_draw_from(GUI_ENTRY_REC *entry, int pos)
{
	const unichar *p;
	int xpos, end_xpos;

        xpos = entry->xpos + entry->promptlen + pos;
        end_xpos = entry->xpos + entry->width;
	if (xpos > end_xpos)
                return;

	term_set_color(root_window, ATTR_RESET);
	term_move(root_window, xpos, entry->ypos);

	p = entry->scrstart + pos < entry->text_len ?
		entry->text + entry->scrstart + pos : empty_str;
	for (; *p != '\0' && xpos < end_xpos; p++, xpos++) {
		if (entry->hidden)
                        term_addch(root_window, ' ');
		else if (*p >= 32 && (entry->utf8 || (*p & 127) >= 32))
			term_add_unichar(root_window, *p);
		else {
			term_set_color(root_window, ATTR_RESET|ATTR_REVERSE);
			term_addch(root_window, *p+'A'-1);
			term_set_color(root_window, ATTR_RESET);
		}
	}

        /* clear the rest of the input line */
        if (end_xpos == term_width)
		term_clrtoeol(root_window);
	else {
		while (xpos < end_xpos) {
                        term_addch(root_window, ' ');
                        xpos++;
		}
	}
}

static void gui_entry_draw(GUI_ENTRY_REC *entry)
{
	if (entry->redraw_needed_from >= 0) {
		gui_entry_draw_from(entry, entry->redraw_needed_from);
                entry->redraw_needed_from = -1;
	}

	term_move_cursor(entry->xpos + entry->scrpos + entry->promptlen,
			 entry->ypos);
	term_refresh(NULL);
}

static void gui_entry_redraw_from(GUI_ENTRY_REC *entry, int pos)
{
	pos -= entry->scrstart;
	if (pos < 0) pos = 0;

	if (entry->redraw_needed_from == -1 ||
	    entry->redraw_needed_from > pos)
		entry->redraw_needed_from = pos;
}

void gui_entry_move(GUI_ENTRY_REC *entry, int xpos, int ypos, int width)
{
	int old_width;

        g_return_if_fail(entry != NULL);

	if (entry->xpos != xpos || entry->ypos != ypos) {
                /* position in screen changed - needs a full redraw */
		entry->xpos = xpos;
		entry->ypos = ypos;
		entry->width = width;
		gui_entry_redraw(entry);
                return;
	}

	if (entry->width == width)
                return; /* no changes */

	if (width > entry->width) {
                /* input line grew - need to draw text at the end */
                old_width = width;
		entry->width = width;
		gui_entry_redraw_from(entry, old_width);
	} else {
		/* input line shrinked - make sure the cursor
		   is inside the input line */
		entry->width = width;
		if (entry->pos - entry->scrstart >
		    entry->width-2 - entry->promptlen) {
			gui_entry_fix_cursor(entry);
		}
	}

	gui_entry_draw(entry);
}

void gui_entry_set_active(GUI_ENTRY_REC *entry)
{
	active_entry = entry;

	if (entry != NULL) {
		term_move_cursor(entry->xpos + entry->scrpos +
				 entry->promptlen, entry->ypos);
		term_refresh(NULL);
	}
}

void gui_entry_set_prompt(GUI_ENTRY_REC *entry, const char *str)
{
	int oldlen;

        g_return_if_fail(entry != NULL);

        oldlen = entry->promptlen;
	if (str != NULL) {
		g_free_not_null(entry->prompt);
		entry->prompt = g_strdup(str);
		entry->promptlen = format_get_length(str);
	}

        if (entry->prompt != NULL)
		gui_printtext(entry->xpos, entry->ypos, entry->prompt);

	if (entry->promptlen != oldlen) {
		gui_entry_fix_cursor(entry);
		gui_entry_draw(entry);
	}
}

void gui_entry_set_hidden(GUI_ENTRY_REC *entry, int hidden)
{
        g_return_if_fail(entry != NULL);

        entry->hidden = hidden;
}

void gui_entry_set_utf8(GUI_ENTRY_REC *entry, int utf8)
{
        g_return_if_fail(entry != NULL);

        entry->utf8 = utf8;
}

void gui_entry_set_text(GUI_ENTRY_REC *entry, const char *str)
{
	g_return_if_fail(entry != NULL);
	g_return_if_fail(str != NULL);

	entry->text_len = 0;
	entry->pos = 0;
	entry->text[0] = '\0';

	gui_entry_insert_text(entry, str);
}

char *gui_entry_get_text(GUI_ENTRY_REC *entry)
{
	char *buf;
        int i;

	g_return_val_if_fail(entry != NULL, NULL);

	buf = g_malloc(entry->text_len*6 + 1);
	if (entry->utf8)
		utf16_to_utf8(entry->text, buf);
	else {
		for (i = 0; i <= entry->text_len; i++)
			buf[i] = entry->text[i];
	}
	return buf;
}

void gui_entry_insert_text(GUI_ENTRY_REC *entry, const char *str)
{
        unichar chr;
	int i, len;

        g_return_if_fail(entry != NULL);
	g_return_if_fail(str != NULL);

        gui_entry_redraw_from(entry, entry->pos);

	len = !entry->utf8 ? strlen(str) : strlen_utf8(str);
        entry_text_grow(entry, len);

        /* make space for the string */
	g_memmove(entry->text + entry->pos + len, entry->text + entry->pos,
		  (entry->text_len-entry->pos + 1) * sizeof(unichar));

	if (!entry->utf8) {
		for (i = 0; i < len; i++)
                        entry->text[entry->pos+i] = str[i];
	} else {
                chr = entry->text[entry->pos+len];
		utf8_to_utf16(str, entry->text+entry->pos);
                entry->text[entry->pos+len] = chr;
	}

	entry->text_len += len;
        entry->pos += len;

	gui_entry_fix_cursor(entry);
	gui_entry_draw(entry);
}

void gui_entry_insert_char(GUI_ENTRY_REC *entry, unichar chr)
{
        g_return_if_fail(entry != NULL);

	if (chr == 0 || chr == 13 || chr == 10)
		return; /* never insert NUL, CR or LF characters */

        gui_entry_redraw_from(entry, entry->pos);

	entry_text_grow(entry, 1);

	/* make space for the string */
	g_memmove(entry->text + entry->pos + 1, entry->text + entry->pos,
		  (entry->text_len-entry->pos + 1) * sizeof(unichar));

	entry->text[entry->pos] = chr;
	entry->text_len++;
        entry->pos++;

	gui_entry_fix_cursor(entry);
	gui_entry_draw(entry);
}

char *gui_entry_get_cutbuffer(GUI_ENTRY_REC *entry)
{
	char *buf;
        int i;

	g_return_val_if_fail(entry != NULL, NULL);

	buf = g_malloc(entry->cutbuffer_len*6 + 1);
	if (entry->utf8)
		utf16_to_utf8(entry->cutbuffer, buf);
	else {
		for (i = 0; i <= entry->cutbuffer_len; i++)
			buf[i] = entry->cutbuffer[i];
	}
	return buf;
}

void gui_entry_erase(GUI_ENTRY_REC *entry, int size)
{
        g_return_if_fail(entry != NULL);

	if (entry->pos < size)
		return;

        /* put erased text to cutbuffer */
	if (entry->cutbuffer_len < size) {
		g_free(entry->cutbuffer);
		entry->cutbuffer = g_new(unichar, size+1);
	}

	entry->cutbuffer_len = size;
        entry->cutbuffer[size] = '\0';
	memcpy(entry->cutbuffer, entry->text + entry->pos - size,
	       size * sizeof(unichar));

	g_memmove(entry->text + entry->pos - size, entry->text + entry->pos,
		  (entry->text_len-entry->pos+1) * sizeof(unichar));

	entry->pos -= size;
        entry->text_len -= size;

	gui_entry_redraw_from(entry, entry->pos);
	gui_entry_fix_cursor(entry);
	gui_entry_draw(entry);
}

void gui_entry_erase_word(GUI_ENTRY_REC *entry, int to_space)
{
	int to;

        g_return_if_fail(entry != NULL);
	if (entry->pos == 0)
		return;

	to = entry->pos - 1;

	if (to_space) {
		while (entry->text[to] == ' ' && to > 0)
			to--;
		while (entry->text[to] != ' ' && to > 0)
			to--;
	} else {
		while (!i_isalnum(entry->text[to]) && to > 0)
			to--;
		while (i_isalnum(entry->text[to]) && to > 0)
			to--;
	}
	if (to > 0) to++;

        gui_entry_erase(entry, entry->pos-to);
}

void gui_entry_erase_next_word(GUI_ENTRY_REC *entry, int to_space)
{
	int to, size;

        g_return_if_fail(entry != NULL);
	if (entry->pos == entry->text_len)
		return;

        to = entry->pos;
	if (to_space) {
		while (entry->text[to] == ' ' && to < entry->text_len)
			to++;
		while (entry->text[to] != ' ' && to < entry->text_len)
			to++;
	} else {
		while (!i_isalnum(entry->text[to]) && to < entry->text_len)
			to++;
		while (i_isalnum(entry->text[to]) && to < entry->text_len)
			to++;
	}

        size = to-entry->pos;
	entry->pos = to;
        gui_entry_erase(entry, size);
}

int gui_entry_get_pos(GUI_ENTRY_REC *entry)
{
        g_return_val_if_fail(entry != NULL, 0);

	return entry->pos;
}

void gui_entry_set_pos(GUI_ENTRY_REC *entry, int pos)
{
        g_return_if_fail(entry != NULL);

	if (pos >= 0 && pos <= entry->text_len)
		entry->pos = pos;

	gui_entry_fix_cursor(entry);
	gui_entry_draw(entry);
}

void gui_entry_move_pos(GUI_ENTRY_REC *entry, int pos)
{
        g_return_if_fail(entry != NULL);

	if (entry->pos+pos >= 0 && entry->pos+pos <= entry->text_len)
		entry->pos += pos;

	gui_entry_fix_cursor(entry);
	gui_entry_draw(entry);
}

static void gui_entry_move_words_left(GUI_ENTRY_REC *entry, int count, int to_space)
{
	int pos;

	pos = entry->pos;
	while (count > 0 && pos > 0) {
		if (to_space) {
			while (pos > 0 && entry->text[pos-1] == ' ')
				pos--;
			while (pos > 0 && entry->text[pos-1] != ' ')
				pos--;
		} else {
			while (pos > 0 && !i_isalnum(entry->text[pos-1]))
				pos--;
			while (pos > 0 &&  i_isalnum(entry->text[pos-1]))
				pos--;
		}
		count--;
	}

        entry->pos = pos;
}

static void gui_entry_move_words_right(GUI_ENTRY_REC *entry, int count, int to_space)
{
	int pos;

	pos = entry->pos;
	while (count > 0 && pos < entry->text_len) {
		if (to_space) {
			while (pos < entry->text_len && entry->text[pos] == ' ')
				pos++;
			while (pos < entry->text_len && entry->text[pos] != ' ')
				pos++;
		} else {
			while (pos < entry->text_len && !i_isalnum(entry->text[pos]))
				pos++;
			while (pos < entry->text_len &&  i_isalnum(entry->text[pos]))
				pos++;
		}
		count--;
	}

        entry->pos = pos;
}

void gui_entry_move_words(GUI_ENTRY_REC *entry, int count, int to_space)
{
        g_return_if_fail(entry != NULL);

	if (count < 0)
		gui_entry_move_words_left(entry, -count, to_space);
	else if (count > 0)
		gui_entry_move_words_right(entry, count, to_space);

	gui_entry_fix_cursor(entry);
	gui_entry_draw(entry);
}

void gui_entry_redraw(GUI_ENTRY_REC *entry)
{
        g_return_if_fail(entry != NULL);

	gui_entry_set_prompt(entry, NULL);
        gui_entry_redraw_from(entry, 0);
	gui_entry_fix_cursor(entry);
	gui_entry_draw(entry);
}
