#ifndef __CHANNELS_SETUP_H
#define __CHANNELS_SETUP_H

#include "modules.h"

struct _CHANNEL_SETUP_REC {
	char *name;
	char *chatnet;
	char *password;

	char *botmasks;
	char *autosendcmd;

	unsigned int autojoin:1;
	GHashTable *module_data;
};

extern GSList *setupchannels;

void channels_setup_init(void);
void channels_setup_deinit(void);

void channels_setup_create(CHANNEL_SETUP_REC *channel);
void channels_setup_destroy(CHANNEL_SETUP_REC *channel);

CHANNEL_SETUP_REC *channels_setup_find(const char *channel,
				       const char *chatnet);

#define channel_chatnet_match(rec, chatnet) \
	((rec) == NULL || (rec)[0] == '\0' || \
	 ((chatnet) != NULL && g_strcasecmp(rec, chatnet) == 0))

#endif
