#ifndef __QUERY_H
#define __QUERY_H

#include "server.h"

typedef struct {
	int type;
	GHashTable *module_data;

	IRC_SERVER_REC *server;
	char *nick;

	int new_data;

	char *address;
	char *server_tag;
	int destroying:1;
} QUERY_REC;

extern GSList *queries;

QUERY_REC *query_create(IRC_SERVER_REC *server, const char *nick, int automatic);
void query_destroy(QUERY_REC *query);

/* Find query by name, if `server' is NULL, search from all servers */
QUERY_REC *query_find(IRC_SERVER_REC *server, const char *nick);

void query_change_server(QUERY_REC *query, IRC_SERVER_REC *server);

#endif
