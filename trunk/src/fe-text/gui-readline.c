/*
 gui-readline.c : irssi

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
#include "misc.h"
#include "settings.h"
#include "special-vars.h"
#include "levels.h"
#include "servers.h"

#include "completion.h"
#include "command-history.h"
#include "keyboard.h"
#include "translation.h"
#include "printtext.h"

#include "term.h"
#include "gui-entry.h"
#include "gui-windows.h"
#include "utf8.h"

#include <signal.h>

#define PASTE_MAX_KEYCOUNT 100

typedef void (*ENTRY_REDIRECT_KEY_FUNC) (int key, void *data, SERVER_REC *server, WI_ITEM_REC *item);
typedef void (*ENTRY_REDIRECT_ENTRY_FUNC) (const char *line, void *data, SERVER_REC *server, WI_ITEM_REC *item);

typedef struct {
	SIGNAL_FUNC func;
        int flags;
	void *data;
} ENTRY_REDIRECT_REC;

static KEYBOARD_REC *keyboard;
static ENTRY_REDIRECT_REC *redir;
static int escape_next_key;

static int readtag;
static unichar prev_key;
static GTimeVal last_keypress;

static int paste_detect_time, paste_detect_keycount, paste_verify_line_count;
static int paste_state;
static char *paste_entry, *prev_entry;
static int paste_entry_pos, prev_entry_pos;
static GArray *paste_buffer;

static char *paste_old_prompt;
static int paste_prompt, paste_line_count;

static void sig_input(void);

void input_listen_init(int handle)
{
        GIOChannel *stdin_channel;

	stdin_channel = g_io_channel_unix_new(handle);
	readtag = g_input_add_full(stdin_channel,
				   G_PRIORITY_HIGH, G_INPUT_READ,
				   (GInputFunction) sig_input, NULL);
        g_io_channel_unref(stdin_channel);
}

void input_listen_deinit(void)
{
	g_source_remove(readtag);
        readtag = -1;
}

static void handle_key_redirect(int key)
{
	ENTRY_REDIRECT_KEY_FUNC func;
	void *data;

	func = (ENTRY_REDIRECT_KEY_FUNC) redir->func;
	data = redir->data;
	g_free_and_null(redir);

	gui_entry_set_prompt(active_entry, "");

	if (func != NULL)
		func(key, data, active_win->active_server, active_win->active);
}

static void handle_entry_redirect(const char *line)
{
	ENTRY_REDIRECT_ENTRY_FUNC func;
	void *data;

        gui_entry_set_hidden(active_entry, FALSE);

	func = (ENTRY_REDIRECT_ENTRY_FUNC) redir->func;
	data = redir->data;
	g_free_and_null(redir);

	gui_entry_set_prompt(active_entry, "");

	if (func != NULL) {
		func(line, data, active_win->active_server,
		     active_win->active);
	}
}

static int get_scroll_count(void)
{
	const char *str;
	double count;

	str = settings_get_str("scroll_page_count");
	count = atof(str + (*str == '/'));
	if (count <= 0)
		count = 1;
	else if (count < 1)
                count = 1.0/count;

	if (*str == '/') {
		count = (active_mainwin->height-active_mainwin->statusbar_lines)/count;
	}
	return (int)count;
}

static void window_prev_page(void)
{
	gui_window_scroll(active_win, -get_scroll_count());
}

static void window_next_page(void)
{
	gui_window_scroll(active_win, get_scroll_count());
}

static void paste_send(const char *utf8_start, GArray *buf)
{
	unichar *arr;
	GString *str;
	char out[10];
	unsigned int i;

	str = g_string_new(utf8_start);

	arr = (unichar *) buf->data;
	for (i = 0; i < buf->len; i++) {
		if (arr[i] == '\r' || arr[i] == '\n') {
			signal_emit("send text", 3, str->str,
				    active_win->active_server,
				    active_win->active);
			g_string_truncate(str, 0);
		} else {
			out[utf16_char_to_utf8(arr[i], out)] = '\0';
			g_string_append(str, out);
		}
	}

	gui_entry_set_text(active_entry, str->str);
	g_string_free(str, TRUE);
}

static void paste_flush(int send)
{
	if (send)
		paste_send(paste_entry, paste_buffer);
	else {
		/* revert back to pre-paste state */
		gui_entry_set_text(active_entry, paste_entry);
		gui_entry_set_pos(active_entry, paste_entry_pos);
	}
	g_array_set_size(paste_buffer, 0);

	gui_entry_set_prompt(active_entry,
			     paste_old_prompt == NULL ? "" : paste_old_prompt);
	g_free(paste_old_prompt); paste_old_prompt = NULL;
	paste_prompt = FALSE;

	paste_line_count = 0;
	paste_state = 0;

	gui_entry_redraw(active_entry);
}

static gboolean paste_timeout(gpointer data)
{
	GTimeVal now;
	int diff;

	if (paste_line_count == 0) {
		/* gone already */
		return FALSE;
	}

        g_get_current_time(&now);
	diff = (now.tv_sec - last_keypress.tv_sec) * 1000 +
		(now.tv_usec - last_keypress.tv_usec)/1000;

	if (diff < paste_detect_time) {
		/* still pasting */
		return TRUE;
	}

	if (paste_line_count < paste_verify_line_count) {
		/* paste without asking */
		paste_flush(TRUE);
	} else if (!paste_prompt) {
		paste_prompt = TRUE;
		paste_old_prompt = g_strdup(active_entry->prompt);
		printtext_window(active_win, MSGLEVEL_CLIENTNOTICE,
				 "Pasting %u lines to %s. Press Ctrl-O if you wish to do this or Ctrl-C to cancel.",
				 paste_line_count,
				 active_win->active == NULL ? "window" :
				 active_win->active->visible_name);
		gui_entry_set_prompt(active_entry, "Hit Ctrl-O to paste, Ctrl-C to abort?");
		gui_entry_set_text(active_entry, "");
	}
	return TRUE;
}

#define IS_PASTE_SKIP_KEY(c) \
	((c) == '\r' || (c) == '\n')

static int check_pasting(unichar key, int diff)
{
	unsigned int i;

	if (paste_state == 0) {
		/* two keys hit together quick. possibly pasting */
		if (diff > paste_detect_time)
			return FALSE;

		g_free(paste_entry);
		paste_entry = g_strdup(prev_entry);
		paste_entry_pos = prev_entry_pos;

		paste_state++;
		paste_line_count = 0;
		g_array_set_size(paste_buffer, 0);
		if (!IS_PASTE_SKIP_KEY(prev_key))
			g_array_append_val(paste_buffer, prev_key);
	} else if (paste_state > 0 && diff > paste_detect_time &&
		   paste_line_count == 0) {
		/* reset paste state */
		paste_state = 0;
		return FALSE;
	}

	/* continuing quick hits */
	if (paste_state == paste_detect_keycount) {
		/* pasting.. */
		if ((key == 15 || key == 3) && paste_prompt) {
			paste_flush(key == 15);
			return TRUE;
		}

		if (IS_PASTE_SKIP_KEY(key)) {
			if (paste_verify_line_count <= 0) {
				/* handle CR/LF the normal way */
				return FALSE;
			}

			if (paste_line_count++ == 0) {
				/* end of first line - see how many lines
				   we'll get */
				g_timeout_add(200, paste_timeout, NULL);
			}
		}
		if (paste_verify_line_count > 0)
			g_array_append_val(paste_buffer, key);
		else
			gui_entry_insert_char(active_entry, key);
		return TRUE;
	}

	if (IS_PASTE_SKIP_KEY(key)) {
		/* we might be pasting, but we can't go back from executed
		   commands.. */
		paste_state++;
		g_array_set_size(paste_buffer, 0);
		return FALSE;
	}

	paste_state++;
	g_array_append_val(paste_buffer, key);
	if (paste_state == paste_detect_keycount) {
		/* ok, we started pasting. */
		gui_entry_set_text(active_entry, paste_entry);
		gui_entry_set_pos(active_entry, paste_entry_pos);
		if (paste_verify_line_count <= 0) {
			for (i = 0; i < paste_buffer->len; i++) {
				gui_entry_insert_char(active_entry,
					g_array_index(paste_buffer, unichar, i));
			}
			g_array_set_size(paste_buffer, 0);
		}
		return TRUE;
	}

	return FALSE;
}

static void sig_gui_key_pressed(gpointer keyp)
{
	GTimeVal now;
        unichar key;
	char str[20];
	int diff;

	key = GPOINTER_TO_INT(keyp);

	if (redir != NULL && redir->flags & ENTRY_REDIRECT_FLAG_HOTKEY) {
		handle_key_redirect(key);
		return;
	}

        g_get_current_time(&now);
	diff = (now.tv_sec - last_keypress.tv_sec) * 1000 +
		(now.tv_usec - last_keypress.tv_usec)/1000;
	last_keypress = now;

	if (check_pasting(key, diff))
		return;

	if (key < 32) {
		/* control key */
                str[0] = '^';
		str[1] = (char)key+'@';
                str[2] = '\0';
	} else if (key == 127) {
                str[0] = '^';
		str[1] = '?';
                str[2] = '\0';
	} else if (!active_entry->utf8) {
		if (key <= 0xff) {
			str[0] = (char)key;
			str[1] = '\0';
		} else {
			str[0] = (char) (key >> 8);
			str[1] = (char) (key & 0xff);
			str[2] = '\0';
		}
	} else {
                /* need to convert to utf8 */
		str[utf16_char_to_utf8(key, str)] = '\0';
	}

	if (strcmp(str, "^") == 0) {
		/* change it as ^^ */
		str[1] = '^';
		str[2] = '\0';
	}

	g_free(prev_entry);
	prev_entry = gui_entry_get_text(active_entry);
	prev_entry_pos = gui_entry_get_pos(active_entry);
	prev_key = key;

	if (escape_next_key || !key_pressed(keyboard, str)) {
		/* key wasn't used for anything, print it */
                escape_next_key = FALSE;
		gui_entry_insert_char(active_entry, key);
	}
}

static void key_send_line(void)
{
	HISTORY_REC *history;
        char *str, *add_history;

	str = gui_entry_get_text(active_entry);

	/* we can't use gui_entry_get_text() later, since the entry might
	   have been destroyed after we get back */
	add_history = *str == '\0' ? NULL : g_strdup(str);
	history = command_history_current(active_win);

	translate_output(str);

	if (redir == NULL) {
		signal_emit("send command", 3, str,
			    active_win->active_server,
			    active_win->active);
	} else {
		if (redir->flags & ENTRY_REDIRECT_FLAG_HIDDEN)
                        g_free_and_null(add_history);
		handle_entry_redirect(str);
	}

	if (add_history != NULL) {
		history = command_history_find(history);
		if (history != NULL)
			command_history_add(history, add_history);
                g_free(add_history);
	}

	if (active_entry != NULL)
		gui_entry_set_text(active_entry, "");
	command_history_clear_pos(active_win);

        g_free(str);
}

static void key_combo(void)
{
}

static void key_backward_history(void)
{
	const char *text;
        char *line;

	line = gui_entry_get_text(active_entry);
	text = command_history_prev(active_win, line);
	gui_entry_set_text(active_entry, text);
        g_free(line);
}

static void key_forward_history(void)
{
	const char *text;
	char *line;

	line = gui_entry_get_text(active_entry);
	text = command_history_next(active_win, line);
	gui_entry_set_text(active_entry, text);
        g_free(line);
}

static void key_beginning_of_line(void)
{
        gui_entry_set_pos(active_entry, 0);
}

static void key_end_of_line(void)
{
	gui_entry_set_pos(active_entry, active_entry->text_len);
}

static void key_backward_character(void)
{
	gui_entry_move_pos(active_entry, -1);
}

static void key_forward_character(void)
{
	gui_entry_move_pos(active_entry, 1);
}

static void key_backward_word(void)
{
	gui_entry_move_words(active_entry, -1, FALSE);
}

static void key_forward_word(void)
{
	gui_entry_move_words(active_entry, 1, FALSE);
}

static void key_backward_to_space(void)
{
	gui_entry_move_words(active_entry, -1, TRUE);
}

static void key_forward_to_space(void)
{
	gui_entry_move_words(active_entry, 1, TRUE);
}

static void key_erase_line(void)
{
	gui_entry_set_pos(active_entry, active_entry->text_len);
	gui_entry_erase(active_entry, active_entry->text_len, TRUE);
}

static void key_erase_to_beg_of_line(void)
{
	int pos;

	pos = gui_entry_get_pos(active_entry);
	gui_entry_erase(active_entry, pos, TRUE);
}

static void key_erase_to_end_of_line(void)
{
	int pos;

	pos = gui_entry_get_pos(active_entry);
	gui_entry_set_pos(active_entry, active_entry->text_len);
	gui_entry_erase(active_entry, active_entry->text_len - pos, TRUE);
}

static void key_yank_from_cutbuffer(void)
{
	char *cutbuffer;

        cutbuffer = gui_entry_get_cutbuffer(active_entry);
	if (cutbuffer != NULL) {
		gui_entry_insert_text(active_entry, cutbuffer);
                g_free(cutbuffer);
	}
}

static void key_transpose_characters(void)
{
	gui_entry_transpose_chars(active_entry);
}

static void key_delete_character(void)
{
	if (gui_entry_get_pos(active_entry) < active_entry->text_len) {
		gui_entry_move_pos(active_entry, 1);
		gui_entry_erase(active_entry, 1, FALSE);
	}
}

static void key_backspace(void)
{
	gui_entry_erase(active_entry, 1, FALSE);
}

static void key_delete_previous_word(void)
{
	gui_entry_erase_word(active_entry, FALSE);
}

static void key_delete_next_word(void)
{
	gui_entry_erase_next_word(active_entry, FALSE);
}

static void key_delete_to_previous_space(void)
{
	gui_entry_erase_word(active_entry, TRUE);
}

static void key_delete_to_next_space(void)
{
	gui_entry_erase_next_word(active_entry, TRUE);
}

static void sig_input(void)
{
        unichar buffer[128];
	int ret, i;

	if (!active_entry) {
                /* no active entry yet - wait until we have it */
		return;
	}

	ret = term_gets(buffer, sizeof(buffer)/sizeof(buffer[0]));
	if (ret == -1) {
		/* lost terminal */
		if (!term_detached)
			signal_emit("command quit", 1, "Lost terminal");
	} else {
		for (i = 0; i < ret; i++) {
			signal_emit("gui key pressed", 1,
				    GINT_TO_POINTER(buffer[i]));
		}
	}
}

time_t get_idle_time(void)
{
	return last_keypress.tv_sec;
}

static void key_scroll_backward(void)
{
	window_prev_page();
}

static void key_scroll_forward(void)
{
	window_next_page();
}

static void key_scroll_start(void)
{
	signal_emit("command scrollback home", 3, NULL, active_win->active_server, active_win->active);
}

static void key_scroll_end(void)
{
	signal_emit("command scrollback end", 3, NULL, active_win->active_server, active_win->active);
}

static void key_change_window(const char *data)
{
	signal_emit("command window goto", 3, data, active_win->active_server, active_win->active);
}

static void key_completion(int erase)
{
	char *text, *line;
	int pos;

	pos = gui_entry_get_pos(active_entry);

        text = gui_entry_get_text(active_entry);
	line = word_complete(active_win, text, &pos, erase);
	g_free(text);

	if (line != NULL) {
		gui_entry_set_text(active_entry, line);
		gui_entry_set_pos(active_entry, pos);
		g_free(line);
	}
}

static void key_word_completion(void)
{
        key_completion(FALSE);
}

static void key_erase_completion(void)
{
        key_completion(TRUE);
}

static void key_check_replaces(void)
{
	char *text, *line;
	int pos;

	pos = gui_entry_get_pos(active_entry);

        text = gui_entry_get_text(active_entry);
	line = auto_word_complete(text, &pos);
	g_free(text);

	if (line != NULL) {
		gui_entry_set_text(active_entry, line);
		gui_entry_set_pos(active_entry, pos);
		g_free(line);
	}
}

static void key_previous_window(void)
{
	signal_emit("command window previous", 3, "", active_win->active_server, active_win->active);
}

static void key_next_window(void)
{
	signal_emit("command window next", 3, "", active_win->active_server, active_win->active);
}

static void key_left_window(void)
{
	signal_emit("command window left", 3, "", active_win->active_server, active_win->active);
}

static void key_right_window(void)
{
	signal_emit("command window right", 3, "", active_win->active_server, active_win->active);
}

static void key_upper_window(void)
{
	signal_emit("command window up", 3, "", active_win->active_server, active_win->active);
}

static void key_lower_window(void)
{
	signal_emit("command window down", 3, "", active_win->active_server, active_win->active);
}

static void key_active_window(void)
{
	signal_emit("command window goto", 3, "active", active_win->active_server, active_win->active);
}

static SERVER_REC *get_prev_server(SERVER_REC *current)
{
	int pos;

	if (current == NULL) {
		return servers != NULL ? g_slist_last(servers)->data :
			lookup_servers != NULL ?
			g_slist_last(lookup_servers)->data : NULL;
	}

	/* connect2 -> connect1 -> server2 -> server1 -> connect2 -> .. */

	pos = g_slist_index(servers, current);
	if (pos != -1) {
		if (pos > 0)
			return g_slist_nth(servers, pos-1)->data;
		if (lookup_servers != NULL)
			return g_slist_last(lookup_servers)->data;
		return g_slist_last(servers)->data;
	}

	pos = g_slist_index(lookup_servers, current);
	g_assert(pos >= 0);

	if (pos > 0)
		return g_slist_nth(lookup_servers, pos-1)->data;
	if (servers != NULL)
		return g_slist_last(servers)->data;
	return g_slist_last(lookup_servers)->data;
}

static SERVER_REC *get_next_server(SERVER_REC *current)
{
	GSList *pos;

	if (current == NULL) {
		return servers != NULL ? servers->data :
			lookup_servers != NULL ? lookup_servers->data : NULL;
	}

	/* server1 -> server2 -> connect1 -> connect2 -> server1 -> .. */

	pos = g_slist_find(servers, current);
	if (pos != NULL) {
		if (pos->next != NULL)
			return pos->next->data;
		if (lookup_servers != NULL)
			return lookup_servers->data;
		return servers->data;
	}

	pos = g_slist_find(lookup_servers, current);
	g_assert(pos != NULL);

	if (pos->next != NULL)
		return pos->next->data;
	if (servers != NULL)
		return servers->data;
	return lookup_servers->data;
}

static void key_previous_window_item(void)
{
	SERVER_REC *server;

	if (active_win->items != NULL) {
		signal_emit("command window item prev", 3, "",
			    active_win->active_server, active_win->active);
	} else if (servers != NULL || lookup_servers != NULL) {
		/* change server */
		server = active_win->active_server;
		if (server == NULL)
			server = active_win->connect_server;
		server = get_prev_server(server);
		signal_emit("command window server", 3, server->tag,
			    active_win->active_server, active_win->active);
	}
}

static void key_next_window_item(void)
{
	SERVER_REC *server;

	if (active_win->items != NULL) {
		signal_emit("command window item next", 3, "",
			    active_win->active_server, active_win->active);
	} else if (servers != NULL || lookup_servers != NULL) {
		/* change server */
		server = active_win->active_server;
		if (server == NULL)
			server = active_win->connect_server;
		server = get_next_server(server);
		signal_emit("command window server", 3, server->tag,
			    active_win->active_server, active_win->active);
	}
}

static void key_escape(void)
{
        escape_next_key = TRUE;
}

static void key_insert_text(const char *data)
{
	char *str;

	str = parse_special_string(data, active_win->active_server,
				   active_win->active, "", NULL, 0);
	gui_entry_insert_text(active_entry, str);
        g_free(str);
}

static void key_sig_stop(void)
{
        term_stop();
}

static void sig_window_auto_changed(void)
{
	char *text;

	if (active_entry == NULL)
		return;

        text = gui_entry_get_text(active_entry);
	command_history_next(active_win, text);
	gui_entry_set_text(active_entry, "");
        g_free(text);
}

static void sig_gui_entry_redirect(SIGNAL_FUNC func, const char *entry,
				   void *flags, void *data)
{
	redir = g_new0(ENTRY_REDIRECT_REC, 1);
	redir->func = func;
	redir->flags = GPOINTER_TO_INT(flags);
	redir->data = data;

	if (redir->flags & ENTRY_REDIRECT_FLAG_HIDDEN)
		gui_entry_set_hidden(active_entry, TRUE);
	gui_entry_set_prompt(active_entry, entry);
}

static void setup_changed(void)
{
	paste_detect_time = settings_get_time("paste_detect_time");
	if (paste_detect_time == 0)
		paste_state = -1;
	else if (paste_state == -1)
		paste_state = 0;

	paste_detect_keycount = settings_get_int("paste_detect_keycount");
	if (paste_detect_keycount < 2)
		paste_detect_keycount = 2;
	else if (paste_detect_keycount > PASTE_MAX_KEYCOUNT)
		paste_detect_keycount = PASTE_MAX_KEYCOUNT;

        paste_verify_line_count = settings_get_int("paste_verify_line_count");
}

void gui_readline_init(void)
{
	static char changekeys[] = "1234567890qwertyuio";
	char *key, data[MAX_INT_STRLEN];
	int n;

        escape_next_key = FALSE;
	redir = NULL;
	paste_state = 0;
	paste_entry = NULL;
	paste_entry_pos = 0;
        paste_buffer = g_array_new(FALSE, FALSE, sizeof(unichar));
	g_get_current_time(&last_keypress);
        input_listen_init(STDIN_FILENO);

	settings_add_str("history", "scroll_page_count", "/2");
	settings_add_time("misc", "paste_detect_time", "10msecs");
	/* NOTE: function keys can generate at least 5 characters long
	   keycodes. this must be larger to allow them to work. */
	settings_add_int("misc", "paste_detect_keycount", 6);
	settings_add_int("misc", "paste_verify_line_count", 5);
        setup_changed();

	keyboard = keyboard_create(NULL);
        key_configure_freeze();

	key_bind("key", NULL, " ", "space", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "^M", "return", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "^J", "return", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "^H", "backspace", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "^?", "backspace", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "^I", "tab", (SIGNAL_FUNC) key_combo);

        /* meta */
	key_bind("key", NULL, "^[", "meta", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta-[", "meta2", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta-O", "meta2", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta-[O", "meta2", (SIGNAL_FUNC) key_combo);

        /* arrow keys */
	key_bind("key", NULL, "meta2-A", "up", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-B", "down", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-C", "right", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-D", "left", (SIGNAL_FUNC) key_combo);

	key_bind("key", NULL, "meta2-1~", "home", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-7~", "home", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-H", "home", (SIGNAL_FUNC) key_combo);

	key_bind("key", NULL, "meta2-4~", "end", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-8~", "end", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-F", "end", (SIGNAL_FUNC) key_combo);

	key_bind("key", NULL, "meta2-5~", "prior", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-I", "prior", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-6~", "next", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-G", "next", (SIGNAL_FUNC) key_combo);

	key_bind("key", NULL, "meta2-2~", "insert", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-3~", "delete", (SIGNAL_FUNC) key_combo);

	key_bind("key", NULL, "meta2-d", "cleft", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-c", "cright", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-5D", "cleft", (SIGNAL_FUNC) key_combo);
	key_bind("key", NULL, "meta2-5C", "cright", (SIGNAL_FUNC) key_combo);

	/* cursor movement */
	key_bind("backward_character", "", "left", NULL, (SIGNAL_FUNC) key_backward_character);
	key_bind("forward_character", "", "right", NULL, (SIGNAL_FUNC) key_forward_character);
 	key_bind("backward_word", "", "cleft", NULL, (SIGNAL_FUNC) key_backward_word);
 	key_bind("backward_word", NULL, "meta-b", NULL, (SIGNAL_FUNC) key_backward_word);
	key_bind("forward_word", "", "cright", NULL, (SIGNAL_FUNC) key_forward_word);
	key_bind("forward_word", NULL, "meta-f", NULL, (SIGNAL_FUNC) key_forward_word);
 	key_bind("backward_to_space", "", NULL, NULL, (SIGNAL_FUNC) key_backward_to_space);
	key_bind("forward_to_space", "", NULL, NULL, (SIGNAL_FUNC) key_forward_to_space);
	key_bind("beginning_of_line", "", "home", NULL, (SIGNAL_FUNC) key_beginning_of_line);
	key_bind("beginning_of_line", NULL, "^A", NULL, (SIGNAL_FUNC) key_beginning_of_line);
	key_bind("end_of_line", "", "end", NULL, (SIGNAL_FUNC) key_end_of_line);
	key_bind("end_of_line", NULL, "^E", NULL, (SIGNAL_FUNC) key_end_of_line);

        /* history */
	key_bind("backward_history", "", "up", NULL, (SIGNAL_FUNC) key_backward_history);
	key_bind("forward_history", "", "down", NULL, (SIGNAL_FUNC) key_forward_history);

        /* line editing */
	key_bind("backspace", "", "backspace", NULL, (SIGNAL_FUNC) key_backspace);
	key_bind("delete_character", "", "delete", NULL, (SIGNAL_FUNC) key_delete_character);
	key_bind("delete_character", NULL, "^D", NULL, (SIGNAL_FUNC) key_delete_character);
	key_bind("delete_next_word", "meta-d", NULL, NULL, (SIGNAL_FUNC) key_delete_next_word);
	key_bind("delete_previous_word", "meta-backspace", NULL, NULL, (SIGNAL_FUNC) key_delete_previous_word);
	key_bind("delete_to_previous_space", "", "^W", NULL, (SIGNAL_FUNC) key_delete_to_previous_space);
	key_bind("delete_to_next_space", "", "", NULL, (SIGNAL_FUNC) key_delete_to_next_space);
	key_bind("erase_line", "", "^U", NULL, (SIGNAL_FUNC) key_erase_line);
	key_bind("erase_to_beg_of_line", "", NULL, NULL, (SIGNAL_FUNC) key_erase_to_beg_of_line);
	key_bind("erase_to_end_of_line", "", "^K", NULL, (SIGNAL_FUNC) key_erase_to_end_of_line);
	key_bind("yank_from_cutbuffer", "", "^Y", NULL, (SIGNAL_FUNC) key_yank_from_cutbuffer);
	key_bind("transpose_characters", "Swap current and previous character", "^T", NULL, (SIGNAL_FUNC) key_transpose_characters);

        /* line transmitting */
	key_bind("send_line", "Execute the input line", "return", NULL, (SIGNAL_FUNC) key_send_line);
	key_bind("word_completion", "", "tab", NULL, (SIGNAL_FUNC) key_word_completion);
	key_bind("erase_completion", "", "meta-k", NULL, (SIGNAL_FUNC) key_erase_completion);
	key_bind("check_replaces", "Check word replaces", NULL, NULL, (SIGNAL_FUNC) key_check_replaces);

        /* window managing */
	key_bind("previous_window", "Previous window", "^P", NULL, (SIGNAL_FUNC) key_previous_window);
	key_bind("next_window", "Next window", "^N", NULL, (SIGNAL_FUNC) key_next_window);
	key_bind("upper_window", "Upper window", "meta-up", NULL, (SIGNAL_FUNC) key_upper_window);
	key_bind("lower_window", "Lower window", "meta-down", NULL, (SIGNAL_FUNC) key_lower_window);
	key_bind("left_window", "Window in left", "meta-left", NULL, (SIGNAL_FUNC) key_left_window);
	key_bind("right_window", "Window in right", "meta-right", NULL, (SIGNAL_FUNC) key_right_window);
	key_bind("active_window", "Go to next window with the highest activity", "meta-a", NULL, (SIGNAL_FUNC) key_active_window);
	key_bind("next_window_item", "Next channel/query", "^X", NULL, (SIGNAL_FUNC) key_next_window_item);
	key_bind("previous_window_item", "Previous channel/query", NULL, NULL, (SIGNAL_FUNC) key_previous_window_item);

	key_bind("refresh_screen", "Redraw screen", "^L", NULL, (SIGNAL_FUNC) irssi_redraw);
	key_bind("scroll_backward", "Previous page", "prior", NULL, (SIGNAL_FUNC) key_scroll_backward);
	key_bind("scroll_backward", NULL, "meta-p", NULL, (SIGNAL_FUNC) key_scroll_backward);
	key_bind("scroll_forward", "Next page", "next", NULL, (SIGNAL_FUNC) key_scroll_forward);
	key_bind("scroll_forward", NULL, "meta-n", NULL, (SIGNAL_FUNC) key_scroll_forward);
	key_bind("scroll_start", "Beginning of the window", "", NULL, (SIGNAL_FUNC) key_scroll_start);
	key_bind("scroll_end", "End of the window", "", NULL, (SIGNAL_FUNC) key_scroll_end);

        /* inserting special input characters to line.. */
	key_bind("escape_char", "Escape the next keypress", NULL, NULL, (SIGNAL_FUNC) key_escape);
	key_bind("insert_text", "Append text to line", NULL, NULL, (SIGNAL_FUNC) key_insert_text);

        /* autoreplaces */
	key_bind("multi", NULL, "return", "check_replaces;send_line", NULL);
	key_bind("multi", NULL, "space", "check_replaces;insert_text  ", NULL);

        /* moving between windows */
	for (n = 0; changekeys[n] != '\0'; n++) {
		key = g_strdup_printf("meta-%c", changekeys[n]);
		ltoa(data, n+1);
		key_bind("change_window", "Change window", key, data, (SIGNAL_FUNC) key_change_window);
		g_free(key);
	}

        /* misc */
	key_bind("stop_irc", "Send SIGSTOP to client", "^Z", NULL, (SIGNAL_FUNC) key_sig_stop);

        key_configure_thaw();

	signal_add("window changed automatic", (SIGNAL_FUNC) sig_window_auto_changed);
	signal_add("gui entry redirect", (SIGNAL_FUNC) sig_gui_entry_redirect);
	signal_add("gui key pressed", (SIGNAL_FUNC) sig_gui_key_pressed);
	signal_add("setup changed", (SIGNAL_FUNC) setup_changed);
}

void gui_readline_deinit(void)
{
        input_listen_deinit();

        key_configure_freeze();

	key_unbind("backward_character", (SIGNAL_FUNC) key_backward_character);
	key_unbind("forward_character", (SIGNAL_FUNC) key_forward_character);
 	key_unbind("backward_word", (SIGNAL_FUNC) key_backward_word);
	key_unbind("forward_word", (SIGNAL_FUNC) key_forward_word);
 	key_unbind("backward_to_space", (SIGNAL_FUNC) key_backward_to_space);
	key_unbind("forward_to_space", (SIGNAL_FUNC) key_forward_to_space);
	key_unbind("beginning_of_line", (SIGNAL_FUNC) key_beginning_of_line);
	key_unbind("end_of_line", (SIGNAL_FUNC) key_end_of_line);

	key_unbind("backward_history", (SIGNAL_FUNC) key_backward_history);
	key_unbind("forward_history", (SIGNAL_FUNC) key_forward_history);

	key_unbind("backspace", (SIGNAL_FUNC) key_backspace);
	key_unbind("delete_character", (SIGNAL_FUNC) key_delete_character);
	key_unbind("delete_next_word", (SIGNAL_FUNC) key_delete_next_word);
	key_unbind("delete_previous_word", (SIGNAL_FUNC) key_delete_previous_word);
	key_unbind("delete_to_next_space", (SIGNAL_FUNC) key_delete_to_next_space);
	key_unbind("delete_to_previous_space", (SIGNAL_FUNC) key_delete_to_previous_space);
	key_unbind("erase_line", (SIGNAL_FUNC) key_erase_line);
	key_unbind("erase_to_beg_of_line", (SIGNAL_FUNC) key_erase_to_beg_of_line);
	key_unbind("erase_to_end_of_line", (SIGNAL_FUNC) key_erase_to_end_of_line);
	key_unbind("yank_from_cutbuffer", (SIGNAL_FUNC) key_yank_from_cutbuffer);
	key_unbind("transpose_characters", (SIGNAL_FUNC) key_transpose_characters);

	key_unbind("send_line", (SIGNAL_FUNC) key_send_line);
	key_unbind("word_completion", (SIGNAL_FUNC) key_word_completion);
	key_unbind("erase_completion", (SIGNAL_FUNC) key_erase_completion);
	key_unbind("check_replaces", (SIGNAL_FUNC) key_check_replaces);

	key_unbind("previous_window", (SIGNAL_FUNC) key_previous_window);
	key_unbind("next_window", (SIGNAL_FUNC) key_next_window);
	key_unbind("upper_window", (SIGNAL_FUNC) key_upper_window);
	key_unbind("lower_window", (SIGNAL_FUNC) key_lower_window);
	key_unbind("left_window", (SIGNAL_FUNC) key_left_window);
	key_unbind("right_window", (SIGNAL_FUNC) key_right_window);
	key_unbind("active_window", (SIGNAL_FUNC) key_active_window);
	key_unbind("next_window_item", (SIGNAL_FUNC) key_next_window_item);
	key_unbind("previous_window_item", (SIGNAL_FUNC) key_previous_window_item);

	key_unbind("refresh_screen", (SIGNAL_FUNC) irssi_redraw);
	key_unbind("scroll_backward", (SIGNAL_FUNC) key_scroll_backward);
	key_unbind("scroll_forward", (SIGNAL_FUNC) key_scroll_forward);
	key_unbind("scroll_start", (SIGNAL_FUNC) key_scroll_start);
	key_unbind("scroll_end", (SIGNAL_FUNC) key_scroll_end);

	key_unbind("escape_char", (SIGNAL_FUNC) key_escape);
	key_unbind("insert_text", (SIGNAL_FUNC) key_insert_text);
	key_unbind("change_window", (SIGNAL_FUNC) key_change_window);
	key_unbind("stop_irc", (SIGNAL_FUNC) key_sig_stop);
	keyboard_destroy(keyboard);
        g_array_free(paste_buffer, TRUE);

        key_configure_thaw();

	signal_remove("window changed automatic", (SIGNAL_FUNC) sig_window_auto_changed);
	signal_remove("gui entry redirect", (SIGNAL_FUNC) sig_gui_entry_redirect);
	signal_remove("gui key pressed", (SIGNAL_FUNC) sig_gui_key_pressed);
	signal_remove("setup changed", (SIGNAL_FUNC) setup_changed);
}
