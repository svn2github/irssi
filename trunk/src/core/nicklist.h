#ifndef __NICKLIST_H
#define __NICKLIST_H

#include "servers.h"
#include "channels.h"

typedef struct {
	time_t last_check; /* last time gone was checked */

	char *nick;
	char *host;
	char *realname;
	int hops;

	/* status in server */
	unsigned int gone:1;
	unsigned int serverop:1;

	/* status in channel */
	unsigned int send_massjoin:1; /* Waiting to be sent in massjoin signal */
	unsigned int op:1;
	unsigned int halfop:1;
	unsigned int voice:1;
} NICK_REC;

/* Add new nick to list */
NICK_REC *nicklist_insert(CHANNEL_REC *channel, const char *nick,
			  int op, int voice, int send_massjoin);
/* remove nick from list */
void nicklist_remove(CHANNEL_REC *channel, NICK_REC *nick);
/* Find nick record from list */
NICK_REC *nicklist_find(CHANNEL_REC *channel, const char *mask);
/* Get list of nicks that match the mask */
GSList *nicklist_find_multiple(CHANNEL_REC *channel, const char *mask);
/* Get list of nicks */
GSList *nicklist_getnicks(CHANNEL_REC *channel);
/* Get all the nick records of `nick'. Returns channel, nick, channel, ... */
GSList *nicklist_get_same(SERVER_REC *server, const char *nick);

/* Update specified nick's status in server. */
void nicklist_update_flags(SERVER_REC *server, const char *nick,
			   int gone, int ircop);

/* Nick record comparision for sort functions */
int nicklist_compare(NICK_REC *p1, NICK_REC *p2);

/* Check is `msg' is meant for `nick'. */
int nick_match_msg(SERVER_REC *server, const char *msg, const char *nick);

void nicklist_init(void);
void nicklist_deinit(void);

#endif
