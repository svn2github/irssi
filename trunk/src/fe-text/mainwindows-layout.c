/*
 mainwindows-layout.c : irssi

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
#include "misc.h"
#include "lib-config/iconfig.h"
#include "settings.h"

#include "mainwindows.h"
#include "gui-windows.h"

static void sig_window_save(WINDOW_REC *window, CONFIG_NODE *node)
{
	WINDOW_REC *active;

	if (WINDOW_GUI(window)->sticky) {
		iconfig_node_set_bool(node, "sticky", TRUE);
		active = WINDOW_MAIN(window)->active;
		if (window != active)
			iconfig_node_set_int(node, "parent", active->refnum);
	}
}

static void sig_window_restore(WINDOW_REC *window, CONFIG_NODE *node)
{
	WINDOW_REC *parent;

	parent = window_find_refnum(config_node_get_int(node, "parent", -1));
	if (parent != NULL)
		gui_window_reparent(window, WINDOW_MAIN(parent));

	if (config_node_get_bool(node, "sticky", FALSE))
		gui_window_set_sticky(window);
}

static void main_window_save(MAIN_WINDOW_REC *window, CONFIG_NODE *node)
{
        char num[MAX_INT_STRLEN];

        ltoa(num, window->active->refnum);
	node = config_node_section(node, num, NODE_TYPE_BLOCK);

	iconfig_node_set_int(node, "first_line", window->first_line);
	iconfig_node_set_int(node, "lines", window->height);
}

static void sig_windows_saved(void)
{
	CONFIG_NODE *node;

	iconfig_set_str(NULL, "mainwindows", NULL);
	node = iconfig_node_traverse("mainwindows", TRUE);

	g_slist_foreach(mainwindows, (GFunc) main_window_save, node);
}

static int window_node_cmp(CONFIG_NODE *n1, CONFIG_NODE *n2)
{
	return config_node_get_int(n1, "first_line", 0) >
		config_node_get_int(n2, "first_line", 0) ? -1 : 1;
}

/* Returns list of mainwindow nodes sorted by first_line
   (lowest in screen first) */
static GSList *get_sorted_windows_config(CONFIG_NODE *node)
{
	GSList *tmp, *output;

        output = NULL;
	for (tmp = node->value; tmp != NULL; tmp = tmp->next) {
		output = g_slist_insert_sorted(output, tmp->data,
					       (GCompareFunc) window_node_cmp);
	}

        return output;
}

static void sig_windows_restored(void)
{
        MAIN_WINDOW_REC *lower_window;
        WINDOW_REC *window;
	CONFIG_NODE *node;
	GSList *tmp, *sorted_config;
        int lower_size;

	node = iconfig_node_traverse("mainwindows", FALSE);
	if (node == NULL) return;

	/* create all the visible windows with correct size */
	lower_window = NULL; lower_size = 0;

	sorted_config = get_sorted_windows_config(node);
	for (tmp = sorted_config; tmp != NULL; tmp = tmp->next) {
		CONFIG_NODE *node = tmp->data;

		/* create a new window + mainwindow */
		signal_emit("gui window create override", 1,
			    GINT_TO_POINTER(0));
		window = window_create(NULL, TRUE);
                window_set_refnum(window, atoi(node->key));

		if (lower_size > 0)
			mainwindow_set_size(lower_window, lower_size);

                lower_window = WINDOW_MAIN(window);
		lower_size = config_node_get_int(node, "lines", 0);
	}
	g_slist_free(sorted_config);

	if (lower_size > 0)
		mainwindow_set_size(lower_window, lower_size);
}

void mainwindows_layout_init(void)
{
	signal_add("window save", (SIGNAL_FUNC) sig_window_save);
	signal_add("window restore", (SIGNAL_FUNC) sig_window_restore);
	signal_add("windows saved", (SIGNAL_FUNC) sig_windows_saved);
	signal_add_first("windows restored", (SIGNAL_FUNC) sig_windows_restored);
}

void mainwindows_layout_deinit(void)
{
	signal_remove("window save", (SIGNAL_FUNC) sig_window_save);
	signal_remove("window restore", (SIGNAL_FUNC) sig_window_restore);
	signal_remove("windows saved", (SIGNAL_FUNC) sig_windows_saved);
	signal_remove("windows restored", (SIGNAL_FUNC) sig_windows_restored);
}
