/* SERVER_REC definition, used for inheritance */

int type; /* module_get_uniq_id("SERVER", 0) */
int chat_type; /* chat_protocol_lookup(xx) */

STRUCT_SERVER_CONNECT_REC *connrec;
time_t connect_time; /* connection time */
time_t real_connect_time; /* time when server replied that we really are connected */

char *tag; /* tag name for addressing server */
char *nick; /* current nick */

int connected:1; /* connected to server */
int connection_lost:1; /* Connection lost unintentionally */

void *handle; /* NET_SENDBUF_REC socket */
int readtag; /* input tag */

/* for net_connect_nonblock() */
int connect_pipe[2];
int connect_tag;
int connect_pid;

/* For deciding if event should be handled internally */
GHashTable *eventtable; /* "event xxx" : GSList* of REDIRECT_RECs */
GHashTable *eventgrouptable; /* event group : GSList* of REDIRECT_RECs */
GHashTable *cmdtable; /* "command xxx" : REDIRECT_CMD_REC* */

void *rawlog;
void *buffer; /* receive buffer */
GHashTable *module_data;

char *version; /* server version */
char *away_reason;
char *last_invite; /* channel where you were last invited */
int server_operator:1;
int usermode_away:1;
int banned:1; /* not allowed to connect to this server */

time_t lag_sent; /* 0 or time when last lag query was sent to server */
time_t lag_last_check; /* last time we checked lag */
int lag; /* server lag in milliseconds */

GSList *channels;
GSList *queries;

/* -- support for multiple server types -- */

/* -- must not be NULL: -- */
/* join to a number of channels, channels are specified in `data' separated
   with commas. there can exist other information after first space like
   channel keys etc. */
void (*channels_join)(void *server, const char *data, int automatic);
/* returns true if `flag' indicates a nick flag (op/voice/halfop) */
int (*isnickflag)(char flag);
/* returns true if `flag' indicates a channel */
int (*ischannel)(char flag);
/* returns all nick flag characters in order op, voice, halfop. If some
   of them aren't supported '\0' can be used. */
const char *(*get_nick_flags)(void);
/* send public or private message to server */
void (*send_message)(void *server, const char *target, const char *msg);

/* -- Default implementations are used if NULL -- */
void *(*channel_find_func)(void *server, const char *name);
void *(*query_find_func)(void *server, const char *nick);
int (*mask_match_func)(const char *mask, const char *data);
/* returns true if `msg' was meant for `nick' */
int (*nick_match_msg)(const char *nick, const char *msg);

#undef STRUCT_SERVER_CONNECT_REC
