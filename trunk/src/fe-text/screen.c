/*
 screen.c : irssi

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
#include "misc.h"
#include "settings.h"
#include "commands.h"

#include "screen.h"
#include "gui-readline.h"
#include "mainwindows.h"

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <signal.h>

#if defined(USE_NCURSES) && !defined(RENAMED_NCURSES)
#  include <ncurses.h>
#else
#  include <curses.h>
#endif

#ifndef COLOR_PAIRS
#  define COLOR_PAIRS 64
#endif

#if defined (TIOCGWINSZ) && defined (HAVE_CURSES_RESIZETERM)
#  define USE_RESIZE_TERM
#endif


#define RESIZE_TIMEOUT 500 /* how often to check if the terminal has been resized */
#define MIN_SCREEN_WIDTH 20

struct _SCREEN_WINDOW {
	int x, y;
        int width, height;
	WINDOW *win;
};

SCREEN_WINDOW *screen_root;
int screen_width, screen_height;

static int scrx, scry;
static int use_colors;
static int freeze_refresh;

static int init_screen_int(void);
static void deinit_screen_int(void);

#ifdef SIGWINCH
static int resize_timeout_tag;
#endif

static int resize_needed;

static int resize_timeout(void)
{
#ifdef USE_RESIZE_TERM
	struct winsize ws;
#endif

	if (!resize_needed)
		return TRUE;

        resize_needed = FALSE;

#ifdef USE_RESIZE_TERM

	/* Get new window size */
	if (ioctl(0, TIOCGWINSZ, &ws) < 0)
		return TRUE;

	if (ws.ws_row == LINES && ws.ws_col == COLS) {
		/* Same size, abort. */
		return TRUE;
	}

	if (ws.ws_col < MIN_SCREEN_WIDTH)
		ws.ws_col = MIN_SCREEN_WIDTH;

	/* Resize curses terminal */
	resizeterm(ws.ws_row, ws.ws_col);
#else
	deinit_screen_int();
	init_screen_int();
	mainwindows_recreate();
#endif

	screen_width = COLS;
	screen_height = LINES;
	mainwindows_resize(COLS, LINES);

	return TRUE;
}

#ifdef SIGWINCH
static void sig_winch(int p)
{
        resize_needed = TRUE;
}
#endif

static void cmd_resize(void)
{
	resize_needed = TRUE;
	resize_timeout();
}

static void read_settings(void)
{
	int old_colors = use_colors;

	use_colors = settings_get_bool("colors");
	if (use_colors && !has_colors())
		use_colors = FALSE;

	if (use_colors != old_colors)
		irssi_redraw();
}

static int init_curses(void)
{
	char ansi_tab[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
	int num;
#if !defined (WIN32) && defined(SIGWINCH)
	struct sigaction act;
#endif

	if (!initscr())
		return FALSE;

	if (COLS < MIN_SCREEN_WIDTH)
		COLS = MIN_SCREEN_WIDTH;

#ifdef SIGWINCH
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sig_winch;
	sigaction(SIGWINCH, &act, NULL);
#endif
	raw(); noecho(); idlok(stdscr, 1);
#ifdef HAVE_CURSES_IDCOK
	/*idcok(stdscr, 1); - disabled currently, causes redrawing problems with NetBSD */
#endif
	intrflush(stdscr, FALSE); nodelay(stdscr, TRUE);

	if (has_colors())
		start_color();
	else if (use_colors)
                use_colors = FALSE;

#ifdef HAVE_NCURSES_USE_DEFAULT_COLORS
	/* this lets us to use the "default" background color for colors <= 7 so
	   background pixmaps etc. show up right */
	use_default_colors();

	for (num = 1; num < COLOR_PAIRS; num++)
		init_pair(num, ansi_tab[num & 7], num <= 7 ? -1 : ansi_tab[num >> 3]);

	init_pair(63, 0, -1); /* hm.. not THAT good idea, but probably more
	                         people want dark grey than white on white.. */
#else
	for (num = 1; num < COLOR_PAIRS; num++)
		init_pair(num, ansi_tab[num & 7], ansi_tab[num >> 3]);
	init_pair(63, 0, 0);
#endif

	clear();
	return TRUE;
}

static int init_screen_int(void)
{
	int ret;

	ret = init_curses();
	if (!ret) return 0;

	use_colors = settings_get_bool("colors");

	scrx = scry = 0;
	freeze_refresh = 0;

	screen_root = g_new0(SCREEN_WINDOW, 1);
        screen_root->win = stdscr;

	screen_width = COLS;
	screen_height = LINES;
        return ret;
}

static void deinit_screen_int(void)
{
	endwin();
	g_free_and_null(screen_root);
}

/* Initialize screen, detect screen length */
int init_screen(void)
{
	settings_add_bool("lookandfeel", "colors", TRUE);

	signal_add("beep", (SIGNAL_FUNC) beep);
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);
	command_bind("resize", NULL, (SIGNAL_FUNC) cmd_resize);

#ifdef SIGWINCH
	resize_timeout_tag = g_timeout_add(RESIZE_TIMEOUT,
					   (GSourceFunc) resize_timeout, NULL);
#endif
	return init_screen_int();
}

/* Deinitialize screen */
void deinit_screen(void)
{
	deinit_screen_int();

#ifdef SIGWINCH
	g_source_remove(resize_timeout_tag);
#endif

	command_unbind("resize", (SIGNAL_FUNC) cmd_resize);
	signal_remove("beep", (SIGNAL_FUNC) beep);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
}

int screen_has_colors(void)
{
        return has_colors();
}

void screen_clear(void)
{
        clear();
}

SCREEN_WINDOW *screen_window_create(int x, int y, int width, int height)
{
        SCREEN_WINDOW *window;

	window = g_new0(SCREEN_WINDOW, 1);
	window->x = x; window->y = y;
        window->width = width; window->height = height;
	window->win = newwin(height, width, y, x);
	idlok(window->win, 1);

        return window;
}

void screen_window_destroy(SCREEN_WINDOW *window)
{
	delwin(window->win);
        g_free(window);
}

void screen_window_clear(SCREEN_WINDOW *window)
{
        werase(window->win);
}

void screen_window_move(SCREEN_WINDOW *window, int x, int y,
			int width, int height)
{
	/* some checks to make sure the window is visible in screen,
	   otherwise curses could get nasty and not show our window anymore. */
        if (width < 1) width = 1;
	if (height < 1) height = 1;
	if (x+width > screen_width) x = screen_width-width;
	if (y+height > screen_height) y = screen_height-height;

#ifdef HAVE_CURSES_WRESIZE
	if (window->width != width || window->height != height)
		wresize(window->win, height, width);
        if (window->x != x || window->y != y)
		mvwin(window->win, y, x);
#else
	if (window->width != width || window->height != height ||
	    window->x != x || window->y != y) {
		delwin(window->win);
		window->win = newwin(height, width, y, x);
		idlok(window->win, 1);
	}
#endif
        window->x = x; window->y = y;
        window->width = width; window->height = height;
}

void screen_window_scroll(SCREEN_WINDOW *window, int count)
{
	scrollok(window->win, TRUE);
	wscrl(window->win, count);
	scrollok(window->win, FALSE);
}

static int get_attr(int color)
{
	int attr;

	if (!use_colors)
		attr = (color & 0x70) ? A_REVERSE : 0;
	else if (color & ATTR_COLOR8)
                attr = (A_DIM | COLOR_PAIR(63));
	else if ((color & 0x77) == 0)
		attr = A_NORMAL;
	else
		attr = (COLOR_PAIR((color&7) + (color&0x70)/2));

	if (color & 0x08) attr |= A_BOLD;
	if (color & 0x80) attr |= A_BLINK;

	if (color & ATTR_UNDERLINE) attr |= A_UNDERLINE;
	if (color & ATTR_REVERSE) attr |= A_REVERSE;
        return attr;
}

void screen_set_color(SCREEN_WINDOW *window, int color)
{
	wattrset(window->win, get_attr(color));
}

void screen_set_bg(SCREEN_WINDOW *window, int color)
{
	wbkgdset(window->win, ' ' | get_attr(color));
}

void screen_move(SCREEN_WINDOW *window, int x, int y)
{
        wmove(window->win, y, x);
}

void screen_addch(SCREEN_WINDOW *window, int chr)
{
        waddch(window->win, chr);
}

void screen_addstr(SCREEN_WINDOW *window, char *str)
{
        waddstr(window->win, str);
}

void screen_clrtoeol(SCREEN_WINDOW *window)
{
        wclrtoeol(window->win);
}

void screen_move_cursor(int x, int y)
{
	scry = y;
	scrx = x;
}

void screen_refresh_freeze(void)
{
	freeze_refresh++;
}

void screen_refresh_thaw(void)
{
	if (freeze_refresh > 0) {
		freeze_refresh--;
		if (freeze_refresh == 0) screen_refresh(NULL);
	}
}

void screen_refresh(SCREEN_WINDOW *window)
{
	if (window != NULL)
		wnoutrefresh(window->win);

	if (freeze_refresh == 0) {
		move(scry, scrx);
		wnoutrefresh(stdscr);
		doupdate();
	}
}

int screen_getch(void)
{
	int key;

	key = getch();
	if (key == ERR)
		return -1;

#ifdef KEY_RESIZE
	if (key == KEY_RESIZE)
                return -1;
#endif

	return key;
}
