#ifndef __IGNORE_H
#define __IGNORE_H

#ifdef HAVE_REGEX_H
#  include <regex.h>
#endif

typedef struct {
	char *mask; /* nick mask */
        char *servertag; /* this is for autoignoring */
	char **channels; /* ignore only in these channels */
	char *pattern; /* text body must match this pattern */

	int level; /* ignore these levels */
	int except_level; /* don't ignore these levels */

	int time; /* time in sec for temp ignores */
	int time_tag;

	unsigned int regexp:1;
	unsigned int fullword:1;
	unsigned int replies:1; /* ignore replies to nick in channel */
#ifdef HAVE_REGEX_H
	unsigned int regexp_compiled:1; /* should always be TRUE, unless regexp is invalid */
	regex_t preg;
#endif
} IGNORE_REC;

extern GSList *ignores;

int ignore_check(SERVER_REC *server, const char *nick, const char *host,
		 const char *channel, const char *text, int level);

IGNORE_REC *ignore_find(const char *servertag, const char *mask, char **channels);

void ignore_add_rec(IGNORE_REC *rec);
void ignore_update_rec(IGNORE_REC *rec);

void ignore_init(void);
void ignore_deinit(void);

#endif
