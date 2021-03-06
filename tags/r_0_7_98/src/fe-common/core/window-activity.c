/*
 window-activity.c : irssi

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
#include "levels.h"
#include "servers.h"
#include "channels.h"
#include "misc.h"
#include "settings.h"

#include "fe-windows.h"
#include "window-items.h"
#include "nicklist.h"
#include "hilight-text.h"
#include "formats.h"

static char **hide_targets;
static int hide_level, msg_level, hilight_level;

static void window_activity(WINDOW_REC *window, int data_level,
			    int hilight_color, int hilight_bg_color)
{
	int old_data_level;

	old_data_level = window->data_level;
	if (data_level == 0 || window->data_level < data_level) {
		window->data_level = data_level;
		window->hilight_color = hilight_color;
		window->hilight_bg_color = hilight_bg_color;
		signal_emit("window hilight", 1, window);
	}

	signal_emit("window activity", 2, window,
		    GINT_TO_POINTER(old_data_level));
}

static void window_item_activity(WI_ITEM_REC *item, int data_level,
				 int hilight_color, int hilight_bg_color)
{
	int old_data_level;

	old_data_level = item->data_level;
	if (data_level == 0 || item->data_level < data_level) {
		item->data_level = data_level;
		item->hilight_color = hilight_color;
		item->hilight_bg_color = hilight_bg_color;
		signal_emit("window item hilight", 1, item);
	}

	signal_emit("window item activity", 2, item,
		    GINT_TO_POINTER(old_data_level));
}

#define hide_target_activity(data_level, target) \
	((target) != NULL && hide_targets != NULL && \
	strarray_find(hide_targets, target) != -1)

static void sig_hilight_text(TEXT_DEST_REC *dest, const char *msg)
{
	WI_ITEM_REC *item;
	int data_level;

	if (dest->window == active_win || (dest->level & hide_level))
		return;

	if (dest->level & hilight_level) {
		data_level = DATA_LEVEL_HILIGHT+dest->hilight_priority;
	} else {
		data_level = (dest->level & msg_level) ?
			DATA_LEVEL_MSG : DATA_LEVEL_TEXT;
	}

	if ((dest->level & MSGLEVEL_HILIGHT) == 0 &&
	    hide_target_activity(data_level, dest->target))
		return;

	if (dest->target != NULL) {
		item = window_item_find(dest->server, dest->target);
		if (item != NULL) {
			window_item_activity(item, data_level,
					     dest->hilight_color,
					     dest->hilight_bg_color);
		}
	}
	window_activity(dest->window, data_level,
			dest->hilight_color, dest->hilight_bg_color);
}

static void sig_dehilight_window(WINDOW_REC *window)
{
        GSList *tmp;

	g_return_if_fail(window != NULL);

	if (window->data_level != 0) {
		window_activity(window, 0, 0, 0);
		for (tmp = window->items; tmp != NULL; tmp = tmp->next)
			window_item_activity(tmp->data, 0, 0, 0);
	}
}

static void read_settings(void)
{
	const char *targets;

	if (hide_targets != NULL)
		g_strfreev(hide_targets);

        targets = settings_get_str("activity_hide_targets");
	hide_targets = *targets == '\0' ? NULL :
		g_strsplit(targets, " ", -1);

	hide_level = MSGLEVEL_NEVER | MSGLEVEL_NO_ACT |
		level2bits(settings_get_str("activity_hide_level"));
	msg_level = level2bits(settings_get_str("activity_msg_level"));
	hilight_level = MSGLEVEL_HILIGHT |
		level2bits(settings_get_str("activity_hilight_level"));
}

void window_activity_init(void)
{
	settings_add_str("lookandfeel", "activity_hide_targets", "");
	settings_add_str("lookandfeel", "activity_hide_level", "");
	settings_add_str("lookandfeel", "activity_msg_level", "PUBLIC");
	settings_add_str("lookandfeel", "activity_hilight_level", "MSGS DCCMSGS");

	read_settings();
	signal_add("print text", (SIGNAL_FUNC) sig_hilight_text);
	signal_add("window changed", (SIGNAL_FUNC) sig_dehilight_window);
	signal_add("window dehilight", (SIGNAL_FUNC) sig_dehilight_window);
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);
}

void window_activity_deinit(void)
{
	if (hide_targets != NULL)
		g_strfreev(hide_targets);

	signal_remove("print text", (SIGNAL_FUNC) sig_hilight_text);
	signal_remove("window changed", (SIGNAL_FUNC) sig_dehilight_window);
	signal_remove("window dehilight", (SIGNAL_FUNC) sig_dehilight_window);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
}
