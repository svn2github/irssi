/*
 pidwait.c :

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
#include "modules.h"

#include <sys/wait.h>

static GSList *pids;

static unsigned int childcheck_tag;
static int signal_pidwait;

/* add a pid to wait list */
void pidwait_add(int pid)
{
	pids = g_slist_append(pids, GINT_TO_POINTER(pid));
}

/* remove pid from wait list */
void pidwait_remove(int pid)
{
	pids = g_slist_remove(pids, GINT_TO_POINTER(pid));
}

static int child_check(void)
{
	GSList *tmp, *next;
	int status;

	/* wait for each pid.. */
	for (tmp = pids; tmp != NULL; tmp = next) {
                next = tmp->next;
		if (waitpid(GPOINTER_TO_INT(tmp->data), &status, WNOHANG) > 0) {
			/* process terminated, remove from list */
			pids = g_slist_remove(pids, tmp->data);
			signal_emit_id(signal_pidwait, 1, GPOINTER_TO_INT(tmp->data));
		}
	}
	return 1;
}

void pidwait_init(void)
{
	pids = NULL;
	childcheck_tag = g_timeout_add(1000, (GSourceFunc) child_check, NULL);

	signal_pidwait = module_get_uniq_id_str("signals", "pidwait");
}

void pidwait_deinit(void)
{
	g_slist_free(pids);

	g_source_remove(childcheck_tag);
}
