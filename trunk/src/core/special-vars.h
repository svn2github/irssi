#ifndef __SPECIAL_VARS_H
#define __SPECIAL_VARS_H

#include "signals.h"
#include "servers.h"

#define PARSE_FLAG_GETNAME	0x01 /* return argument name instead of it's value */
#define PARSE_FLAG_ISSET_ANY	0x02 /* arg_used field specifies that at least one of the $variables was non-empty */

typedef char* (*SPECIAL_HISTORY_FUNC)
	(const char *text, void *item, int *free_ret);

/* Parse and expand text after '$' character. return value has to be
   g_free()'d if `free_ret' is TRUE. */
char *parse_special(char **cmd, SERVER_REC *server, void *item,
		    char **arglist, int *free_ret, int *arg_used, int flags);

/* parse the whole string. $ and \ chars are replaced */
char *parse_special_string(const char *cmd, SERVER_REC *server, void *item,
			   const char *data, int *arg_used, int flags);

/* execute the commands in string - commands can be split with ';' */
void eval_special_string(const char *cmd, const char *data,
			 SERVER_REC *server, void *item);

void special_history_func_set(SPECIAL_HISTORY_FUNC func);

void special_vars_add_signals(const char *text,
			      int funccount, SIGNAL_FUNC *funcs);
void special_vars_remove_signals(const char *text,
				 int funccount, SIGNAL_FUNC *funcs);

#endif
