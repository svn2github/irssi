/*
 term.c : irssi

    Copyright (C) 2001 Timo Sirainen

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
#include "settings.h"

#include "term.h"
#include "mainwindows.h"

#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif
#include <signal.h>
#include <termios.h>

#define MIN_SCREEN_WIDTH 20

int term_use_colors;
int term_type;

static int force_colors;
static int resize_dirty;

/* Resize the terminal if needed */
void term_resize_dirty(void)
{
#ifdef TIOCGWINSZ
	struct winsize ws;
#endif
        int width, height;

	if (!resize_dirty)
		return;

        resize_dirty = FALSE;

#ifdef TIOCGWINSZ
	/* Get new window size */
	if (ioctl(0, TIOCGWINSZ, &ws) < 0)
		return;

	if (ws.ws_row == term_height && ws.ws_col == term_width) {
		/* Same size, abort. */
		return;
	}

	if (ws.ws_col < MIN_SCREEN_WIDTH)
		ws.ws_col = MIN_SCREEN_WIDTH;

	width = ws.ws_col;
        height = ws.ws_row;
#else
        width = height = -1;
#endif
        term_resize(width, height);
	mainwindows_resize(term_width, term_height);
        term_resize_final(width, height);
}

#ifdef SIGWINCH
static void sig_winch(int p)
{
        irssi_set_dirty();
        resize_dirty = TRUE;
}
#endif

static void cmd_resize(void)
{
	resize_dirty = TRUE;
        term_resize_dirty();
}

static void read_settings(void)
{
        const char *str;
	int old_colors = term_use_colors;
        int old_type = term_type;

        term_auto_detach(settings_get_bool("term_auto_detach"));

        /* set terminal type */
	str = settings_get_str("term_type");
	if (g_strcasecmp(str, "utf8") == 0)
		term_type = TERM_TYPE_UTF8;
	else if (g_strcasecmp(str, "big5") == 0)
		term_type = TERM_TYPE_BIG5;
	else
		term_type = TERM_TYPE_8BIT;

	if (old_type != term_type)
                term_set_input_type(term_type);

        /* change color stuff */
	if (force_colors != settings_get_bool("term_force_colors")) {
		force_colors = settings_get_bool("term_force_colors");
		term_force_colors(force_colors);
	}

	term_use_colors = settings_get_bool("colors") &&
		(force_colors || term_has_colors());

	if (term_use_colors != old_colors)
		irssi_redraw();
}

void term_common_init(void)
{
#ifdef SIGWINCH
	struct sigaction act;
#endif
	settings_add_bool("lookandfeel", "colors", TRUE);
	settings_add_bool("lookandfeel", "term_force_colors", FALSE);
        settings_add_bool("lookandfeel", "term_auto_detach", FALSE);
        settings_add_str("lookandfeel", "term_type", "8bit");

	force_colors = FALSE;
	term_use_colors = term_has_colors() && settings_get_bool("colors");
        read_settings();

	signal_add("beep", (SIGNAL_FUNC) term_beep);
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);
	command_bind("resize", NULL, (SIGNAL_FUNC) cmd_resize);

#ifdef SIGWINCH
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sig_winch;
	sigaction(SIGWINCH, &act, NULL);
#endif
}

void term_common_deinit(void)
{
	command_unbind("resize", (SIGNAL_FUNC) cmd_resize);
	signal_remove("beep", (SIGNAL_FUNC) term_beep);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
}
