/*
 gui-special-vars.c : irssi

    Copyright (C) 2000 Timo Sirainen

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
#include "special-vars.h"

#include "gui-entry.h"
#include "gui-readline.h"

/* idle time */
static char *expando_idletime(void *server, void *item, int *free_ret)
{
	int diff;

        *free_ret = TRUE;
	diff = (int) (time(NULL) - get_idle_time());
	return g_strdup_printf("%d", diff);
}

/* current contents of the input line */
static char *expando_inputline(void *server, void *item, int *free_ret)
{
	return gui_entry_get_text();
}

/* FIXME: value of cutbuffer */
static char *expando_cutbuffer(void *server, void *item, int *free_ret)
{
	return NULL;
}

void gui_special_vars_init(void)
{
	expando_create("E", expando_idletime);
	expando_create("L", expando_inputline);
	expando_create("U", expando_cutbuffer);
}

void gui_special_vars_deinit(void)
{
	expando_destroy("E", expando_idletime);
	expando_destroy("L", expando_inputline);
	expando_destroy("U", expando_cutbuffer);
}
