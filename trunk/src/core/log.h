#ifndef __LOG_H
#define __LOG_H

#include "servers.h"

enum {
	LOG_ITEM_TARGET, /* channel, query, .. */
	LOG_ITEM_WINDOW_REFNUM
};

typedef struct {
	int type;
        char *name;
	char *servertag;
} LOG_ITEM_REC;

typedef struct {
	char *fname; /* file name, in strftime() format */
	char *real_fname; /* the current expanded file name */
	int handle; /* file handle */
	time_t opened;

	int level; /* log only these levels */
	GSList *items; /* log only on these items */

	time_t last; /* when last message was written */

	int autoopen:1; /* automatically start logging at startup */
	int failed:1; /* opening log failed last time */
	int temp:1; /* don't save this to config file */
} LOG_REC;

extern GSList *logs;

/* Create log record - you still need to call log_update() to actually add it
   into log list */
LOG_REC *log_create_rec(const char *fname, int level);
void log_update(LOG_REC *log);
void log_close(LOG_REC *log);
LOG_REC *log_find(const char *fname);

void log_item_add(LOG_REC *log, int type, const char *name,
		  SERVER_REC *server);
void log_item_destroy(LOG_REC *log, LOG_ITEM_REC *item);
LOG_ITEM_REC *log_item_find(LOG_REC *log, int type, const char *item,
			    SERVER_REC *server);

void log_write_rec(LOG_REC *log, const char *str);

int log_start_logging(LOG_REC *log);
void log_stop_logging(LOG_REC *log);

void log_init(void);
void log_deinit(void);

#endif
