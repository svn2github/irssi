#ifndef __CHANNELS_H
#define __CHANNELS_H

#include "irc-server.h"

typedef struct {
	int type;
	GHashTable *module_data;

	IRC_SERVER_REC *server;
	char *name;

	int new_data;
	int last_color;

	time_t createtime;

	GHashTable *nicks; /* list of nicks */
	GSList *banlist; /* list of bans */
	GSList *ebanlist; /* list of ban exceptions */
	GSList *invitelist; /* invite list */

	char *topic;

	/* channel mode */
	int no_modes:1; /* channel doesn't support modes */
        char *mode;
	int limit; /* user limit */
	char *key; /* password key */

	int chanop:1; /* You're a channel operator */

	int names_got:1; /* Received /NAMES list */
	int wholist:1; /* WHO list got */
	int synced:1; /* Channel synced - all queries done */

	int joined:1; /* Have we even received JOIN event for this channel? */
	int left:1; /* You just left the channel */
	int kicked:1; /* You just got kicked */
	int destroying:1;

	time_t massjoin_start; /* Massjoin start time */
	int massjoins; /* Number of nicks waiting for massjoin signal.. */
	int last_massjoins; /* Massjoins when last checked in timeout function */
} CHANNEL_REC;

extern GSList *channels;

void channels_init(void);
void channels_deinit(void);

/* Create new channel record */
CHANNEL_REC *channel_create(IRC_SERVER_REC *server, const char *channel, int automatic);
void channel_destroy(CHANNEL_REC *channel);

/* find channel by name, if `server' is NULL, search from all servers */
CHANNEL_REC *channel_find(IRC_SERVER_REC *server, const char *channel);

/* Join to channels. `data' contains channels and channel keys */
void channels_join(IRC_SERVER_REC *server, const char *data, int automatic);

#endif
