#include "common.h"

#define MODULE_NAME "proxy"

#include "network.h"
#include "line-split.h"
#include "irc-servers.h"

typedef struct {
	int port;
	char *ircnet;

	int tag;
	GIOChannel *handle;
} LISTEN_REC;

typedef struct {
	LINEBUF_REC *buffer;

	char *nick;
	GIOChannel *handle;
	int tag;

	char *proxy_address;
	LISTEN_REC *listen;
	IRC_SERVER_REC *server;
	unsigned int pass_sent:1;
	unsigned int connected:1;
} CLIENT_REC;

extern GSList *proxy_listens;
extern GSList *proxy_clients;

void plugin_proxy_setup_init(void);
void plugin_proxy_setup_deinit(void);

void plugin_proxy_listen_init(void);
void plugin_proxy_listen_deinit(void);

void proxy_settings_init(void);

void plugin_proxy_dump_data(CLIENT_REC *client);

void proxy_outdata(CLIENT_REC *client, const char *data, ...);
void proxy_outdata_all(IRC_SERVER_REC *server, const char *data, ...);
void proxy_outserver(CLIENT_REC *client, const char *data, ...);
/*void proxy_outserver_all(const char *data, ...);*/
void proxy_outserver_all_except(CLIENT_REC *client, const char *data, ...);
