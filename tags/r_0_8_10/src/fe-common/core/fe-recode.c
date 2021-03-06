/*
 fe-recode.c : irssi

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
#include "modules.h"
#include "module-formats.h"
#include "commands.h"
#include "levels.h"
#include "lib-config/iconfig.h"
#include "settings.h"
#include "printtext.h"
#include "formats.h"
#include "recode.h"

#ifdef HAVE_NL_LANGINFO
#  include <langinfo.h>
#endif

#define SLIST_FOREACH(var, head)		\
for ((var) = (head);				\
		 (var);					\
		 (var) = g_slist_next((var)))

#ifdef HAVE_GLIB2
const char *recode_fallback = NULL;
const char *recode_out_default = NULL;
const char *term_charset = NULL;

static const char *fe_recode_get_target (WI_ITEM_REC *witem)
{
	if (witem && (witem->type == module_get_uniq_id_str("WINDOW ITEM TYPE", "QUERY")
	    || witem->type == module_get_uniq_id_str("WINDOW ITEM TYPE", "CHANNEL")))
		return window_item_get_target(witem);

	printformat(NULL, NULL, MSGLEVEL_CLIENTERROR, TXT_NOT_CHANNEL_OR_QUERY);
	return NULL;
}

static int fe_recode_compare_func (CONFIG_NODE *node1, CONFIG_NODE *node2)
{
	return strcmp(node1->key, node2->key);
}

/* SYNTAX: RECODE */
static void fe_recode_cmd (const char *data, SERVER_REC *server, WI_ITEM_REC *witem)
{
	if (*data)
		command_runsub("recode", data, server, witem);
	else {
		CONFIG_NODE *conversions;
		GSList *tmp;
		GSList *sorted = NULL;

		conversions = iconfig_node_traverse("conversions", FALSE);

		for (tmp = conversions ? config_node_first(conversions->value) : NULL;
		     tmp != NULL;
		     tmp = config_node_next(tmp)) {
			CONFIG_NODE *node = tmp->data;

			if (node->type == NODE_TYPE_KEY)
				sorted = g_slist_insert_sorted(sorted, node, (GCompareFunc) fe_recode_compare_func);
		}

	 	printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_RECODE_HEADER);
		SLIST_FOREACH(tmp, sorted)
		{
			CONFIG_NODE *node = tmp->data;
			printformat(NULL, NULL, MSGLEVEL_CLIENTCRAP, TXT_RECODE_LINE, node->key, node->value);
		}

		g_slist_free(sorted);
	}
}

/* SYNTAX: RECODE ADD [<target>] <charset> */
static void fe_recode_add_cmd (const char *data, SERVER_REC *server, WI_ITEM_REC *witem)
{
	const char *first;
	const char *second;
	const char *target;
	const char *charset;
	void *free_arg;

	if (! cmd_get_params(data, &free_arg, 2, &first, &second))
		return;

	if (! *first)
		cmd_return_error(CMDERR_NOT_ENOUGH_PARAMS);

	if (*second) {
		target = first;
		charset = second;
	} else {
		target = fe_recode_get_target(witem);
		charset = first;
		if (! target)
			goto end;
	}
	if (is_valid_charset(charset)) {
		iconfig_set_str("conversions", target, charset);
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_CONVERSION_ADDED, target, charset);
	} else
		signal_emit("error command", 2, GINT_TO_POINTER(CMDERR_INVALID_CHARSET), charset);
 end:
	cmd_params_free(free_arg);
}

/* SYNTAX: RECODE REMOVE [<target>] */
static void fe_recode_remove_cmd (const char *data, SERVER_REC *server, WI_ITEM_REC *witem)
{
	const char *target;
	void *free_arg;

	if (! cmd_get_params(data, &free_arg, 1, &target))
		return;

	if (! *target) {
		target = fe_recode_get_target(witem);
		if (! target)
			goto end;
	}

	if (iconfig_get_str("conversions", target, NULL) == NULL)
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_CONVERSION_NOT_FOUND, target);
	else	{
		iconfig_set_str("conversions", target, NULL);
		printformat(NULL, NULL, MSGLEVEL_CLIENTNOTICE, TXT_CONVERSION_REMOVED, target);
	}

 end:
	cmd_params_free(free_arg);
}

static void read_settings(void)
{
	const char *old_term_charset = g_strdup (term_charset);
	const char *old_recode_fallback = g_strdup (recode_fallback);
	const char *old_recode_out_default = g_strdup (recode_out_default);

	recode_fallback = settings_get_str("recode_fallback");
	if (!is_valid_charset(recode_fallback)) {
		signal_emit("error command", 2, GINT_TO_POINTER(CMDERR_INVALID_CHARSET), recode_fallback);
		settings_set_str("recode_fallback", old_recode_fallback != NULL ? old_recode_fallback : "ISO8859-1");
	}
	recode_fallback = g_strdup(settings_get_str("recode_fallback"));
	
	term_charset = settings_get_str("term_charset");
	if (!is_valid_charset(term_charset)) {
#if defined (HAVE_NL_LANGINFO) && defined(CODESET)
		settings_set_str("term_charset", is_valid_charset(old_term_charset) ? 
				 old_term_charset : *nl_langinfo(CODESET) != '\0' ?
				 nl_langinfo(CODESET) : "ISO8859-1");
#else
		settings_set_str("term_charset", is_valid_charset(old_term_charset) ? old_term_charset : "ISO8859-1");
#endif		
		/* FIXME: move the check of term_charset into fe-text/term.c 
		          it breaks the proper term_input_type 
		          setup and reemitting of the signal is kludgy */
		if (g_strcasecmp(term_charset, old_term_charset) != 0);
			signal_emit("setup changed", 0);
	}
	term_charset = g_strdup(settings_get_str("term_charset"));

	recode_out_default = settings_get_str("recode_out_default_charset");
	if (recode_out_default != NULL && *recode_out_default != '\0')
		if( !is_valid_charset(recode_out_default)) {
			signal_emit("error command", 2, GINT_TO_POINTER(CMDERR_INVALID_CHARSET), recode_out_default);
			settings_set_str("recode_out_default_charset", old_recode_out_default);
		}
	recode_out_default = g_strdup(settings_get_str("recode_out_default_charset"));
}

static void message_own_public(const SERVER_REC *server, const char *msg,
			       const char *target)
{
	char *recoded;
	recoded = recode_in(msg, target);
	signal_continue(3, server, recoded, target);
	g_free(recoded);
}

static void message_own_private(const SERVER_REC *server, const char *msg,
			       const char *target, const char *orig_target)
{
	char *recoded;
	recoded = recode_in(msg, target);
	signal_continue(4, server, recoded, target, orig_target);
	g_free(recoded);
}

static void message_irc_own_action(const SERVER_REC *server, const char *msg,
				   const char *nick, const char *addr,
				   const char *target)
{
	char *recoded;
	recoded = recode_in(msg, target);
	signal_continue(5, server, recoded, nick, addr, target);
	g_free(recoded);
}

static void message_irc_own_notice(const SERVER_REC *server, const char *msg,
				   const char *channel)
{
	char *recoded;
	recoded = recode_in(msg, channel);
	signal_continue(3, server, recoded, channel);
	g_free(recoded);
}
#endif

void fe_recode_init (void)
{
/* FIXME: print this is not supported instead */
#ifdef HAVE_GLIB2
	command_bind("recode", NULL, (SIGNAL_FUNC) fe_recode_cmd);
	command_bind("recode add", NULL, (SIGNAL_FUNC) fe_recode_add_cmd);
	command_bind("recode remove", NULL, (SIGNAL_FUNC) fe_recode_remove_cmd);
	signal_add("setup changed", (SIGNAL_FUNC) read_settings);
	signal_add("message own_public",  (SIGNAL_FUNC) message_own_public);
	signal_add("message own_private", (SIGNAL_FUNC) message_own_private);
	signal_add("message irc own_action", (SIGNAL_FUNC) message_irc_own_action);
	signal_add("message irc own_notice", (SIGNAL_FUNC) message_irc_own_notice);
	read_settings();
#endif
}

void fe_recode_deinit (void)
{
/* FIXME: print this is not supported instead */
#ifdef HAVE_GLIB2
	command_unbind("recode", (SIGNAL_FUNC) fe_recode_cmd);
	command_unbind("recode add", (SIGNAL_FUNC) fe_recode_add_cmd);
	command_unbind("recode remove", (SIGNAL_FUNC) fe_recode_remove_cmd);
	signal_remove("setup changed", (SIGNAL_FUNC) read_settings);
	signal_remove("message own_public",  (SIGNAL_FUNC) message_own_public);
	signal_remove("message own_private", (SIGNAL_FUNC) message_own_private);
	signal_remove("message irc own_action", (SIGNAL_FUNC) message_irc_own_action);
	signal_remove("message irc own_notice", (SIGNAL_FUNC) message_irc_own_notice);
#endif
}

