/*
 perl-sources.c : irssi

    Copyright (C) 1999-2001 Timo Sirainen

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

#define NEED_PERL_H
#include "module.h"
#include "signals.h"

#include "perl-core.h"
#include "perl-common.h"

typedef struct {
	int tag;
        int refcount;
	char *func;
	SV *data;
} PERL_SOURCE_REC;

static GSList *perl_sources;

static void perl_source_ref(PERL_SOURCE_REC *rec)
{
        rec->refcount++;
}

static void perl_source_unref(PERL_SOURCE_REC *rec)
{
	if (--rec->refcount != 0)
		return;

        SvREFCNT_dec(rec->data);
	g_free(rec->func);
	g_free(rec);
}

static void perl_source_destroy(PERL_SOURCE_REC *rec)
{
	perl_sources = g_slist_remove(perl_sources, rec);

	g_source_remove(rec->tag);
	rec->tag = -1;

	perl_source_unref(rec);
}

static int perl_source_event(PERL_SOURCE_REC *rec)
{
	dSP;
	int retcount;

	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	XPUSHs(sv_mortalcopy(rec->data));
	PUTBACK;

        perl_source_ref(rec);
	retcount = perl_call_pv(rec->func, G_EVAL|G_DISCARD);
	SPAGAIN;

	if (SvTRUE(ERRSV)) {
		STRLEN n_a;
                char *package;

                package = perl_function_get_package(rec->func);
		signal_emit("script error", 2,
			    perl_script_find_package(package),
			    SvPV(ERRSV, n_a));
                g_free(package);
	}
        perl_source_unref(rec);

	PUTBACK;
	FREETMPS;
	LEAVE;

	return 1;
}

int perl_timeout_add(int msecs, const char *func, SV *data)
{
	PERL_SOURCE_REC *rec;

	rec = g_new0(PERL_SOURCE_REC, 1);
	perl_source_ref(rec);

        SvREFCNT_inc(data);
	rec->data = data;

	rec->func = g_strdup_printf("%s::%s", perl_get_package(), func);
	rec->tag = g_timeout_add(msecs, (GSourceFunc) perl_source_event, rec);

	perl_sources = g_slist_append(perl_sources, rec);
	return rec->tag;
}

int perl_input_add(int source, int condition, const char *func, SV *data)
{
	PERL_SOURCE_REC *rec;
        GIOChannel *channel;

	rec = g_new0(PERL_SOURCE_REC, 1);
	perl_source_ref(rec);

        SvREFCNT_inc(data);
	rec->data = data;

	rec->func = g_strdup_printf("%s::%s", perl_get_package(), func);
        channel = g_io_channel_unix_new(source);
	rec->tag = g_input_add(channel, condition,
			       (GInputFunction) perl_source_event, rec);
	g_io_channel_unref(channel);

	perl_sources = g_slist_append(perl_sources, rec);
	return rec->tag;
}

void perl_source_remove(int tag)
{
	GSList *tmp;

	for (tmp = perl_sources; tmp != NULL; tmp = tmp->next) {
		PERL_SOURCE_REC *rec = tmp->data;

		if (rec->tag == tag) {
			perl_source_destroy(rec);
			break;
		}
	}
}

void perl_source_remove_package(const char *package)
{
	GSList *tmp, *next;
        int len;

        len = strlen(package);
	for (tmp = perl_sources; tmp != NULL; tmp = next) {
		PERL_SOURCE_REC *rec = tmp->data;

		next = tmp->next;
		if (strncmp(rec->func, package, len) == 0)
			perl_source_destroy(rec);
	}
}

void perl_sources_start(void)
{
	perl_sources = NULL;
}

void perl_sources_stop(void)
{
	/* timeouts and input waits */
	while (perl_sources != NULL)
		perl_source_destroy(perl_sources->data);
}
