#ifndef __NICKLIST_H
#define __NICKLIST_H

#include "irc-server.h"
#include "channels.h"

typedef struct {
	time_t last_check; /* last time gone was checked */
	int send_massjoin; /* Waiting to be sent in massjoin signal */

	char *nick;
	char *host;
	char *realname;

	int hops;

	int op:1;
	int voice:1;
	int gone:1;
	int ircop:1;
} NICK_REC;

/* Add new nick to list */
NICK_REC *nicklist_insert(CHANNEL_REC *channel, const char *nick, int op, int voice, int send_massjoin);
/* remove nick from list */
void nicklist_remove(CHANNEL_REC *channel, NICK_REC *nick);
/* Find nick record from list */
NICK_REC *nicklist_find(CHANNEL_REC *channel, const char *mask);
/* Get list of nicks that match the mask */
GSList *nicklist_find_multiple(CHANNEL_REC *channel, const char *mask);
/* Get list of nicks */
GSList *nicklist_getnicks(CHANNEL_REC *channel);
/* Get all the nick records of `nick'. Returns channel, nick, channel, ... */
GSList *nicklist_get_same(IRC_SERVER_REC *server, const char *nick);

/* Nick record comparision for sort functions */
int nicklist_compare(NICK_REC *p1, NICK_REC *p2);

/* Remove all "extra" characters from `nick'. Like _nick_ -> nick */
char *nick_strip(const char *nick);
/* Check is `msg' is meant for `nick'. */
int irc_nick_match(const char *nick, const char *msg);

void nicklist_init(void);
void nicklist_deinit(void);

#endif
