#ifndef __HILIGHT_TEXT_H
#define __HILIGHT_TEXT_H

#ifdef HAVE_REGEX_H
#  include <regex.h>
#endif

typedef struct {
	char *text;

	char **channels; /* if non-NULL, check the text only from these channels */
	int level; /* match only messages with this level, 0=default */
	char *color; /* if starts with number, \003 is automatically
	                inserted before it. */
        char *act_color; /* color for window activity */
	int priority;

	unsigned int nick:1; /* hilight only nick if possible */
	unsigned int word:1; /* hilight only word, not full line */

	unsigned int nickmask:1; /* `text' is a nick mask */
	unsigned int fullword:1; /* match `text' only for full words */
	unsigned int regexp:1; /* `text' is a regular expression */
#ifdef HAVE_REGEX_H
	unsigned int regexp_compiled:1; /* should always be TRUE, unless regexp is invalid */
	regex_t preg;
#endif
} HILIGHT_REC;

extern GSList *hilights;

HILIGHT_REC *hilight_match(SERVER_REC *server, const char *channel,
			   const char *nick, const char *address,
			   int level, const char *str,
			   int *match_beg, int *match_end);

char *hilight_match_nick(SERVER_REC *server, const char *channel,
			 const char *nick, const char *address,
			 int level, const char *msg);

void hilight_text_init(void);
void hilight_text_deinit(void);

#endif
