int type; /* module_get_uniq_id("SERVER SETUP", 0) */
int chat_type; /* chat_protocol_lookup(xx) */

char *chatnet;

unsigned short family; /* 0 = default, AF_INET or AF_INET6 */
char *address;
int port;
char *password;

char *own_host; /* address to use when connecting this server */
IPADDR *own_ip; /* resolved own_address if not NULL */

time_t last_connect; /* to avoid reconnecting too fast.. */

unsigned int autoconnect:1;
unsigned int last_failed:1; /* if last connection attempt failed */
unsigned int banned:1; /* if we're banned from this server */

GHashTable *module_data;
