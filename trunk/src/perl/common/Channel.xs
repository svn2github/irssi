MODULE = Irssi  PACKAGE = Irssi

void
channels()
PREINIT:
	GSList *tmp;
PPCODE:
	for (tmp = channels; tmp != NULL; tmp = tmp->next) {
		XPUSHs(sv_2mortal(irssi_bless((CHANNEL_REC *) tmp->data)));
	}

Irssi::Channel
channel_find(channel)
	char *channel
CODE:
	RETVAL = channel_find(NULL, channel);
OUTPUT:
	RETVAL

Irssi::Channel
channel_create(chat_type, name, automatic)
	int chat_type
	char *name
	int automatic
CODE:
	channel_create(chat_type, NULL, name, automatic);

#*******************************
MODULE = Irssi  PACKAGE = Irssi::Server
#*******************************

void
channels(server)
	Irssi::Server server
PREINIT:
	GSList *tmp;
PPCODE:
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		XPUSHs(sv_2mortal(irssi_bless((CHANNEL_REC *) tmp->data)));
	}

void
channels_join(server, channels, automatic)
	Irssi::Server server
	char *channels
	int automatic
CODE:
	server->channels_join(server, channels, automatic);

Irssi::Channel
channel_create(server, name, automatic)
	Irssi::Server server
	char *name
	int automatic
CODE:
	channel_create(server->chat_type, server, name, automatic);

Irssi::Channel
channel_find(server, name)
	Irssi::Server server
	char *name

void
nicks_get_same(server, nick)
	Irssi::Server server
        char *nick
PREINIT:
	GSList *list, *tmp;
PPCODE:
	list = nicklist_get_same(server, nick);

	for (tmp = list; tmp != NULL; tmp = tmp->next->next) {
		XPUSHs(sv_2mortal(irssi_bless((CHANNEL_REC *) tmp->data)));
		XPUSHs(sv_2mortal(irssi_bless((NICK_REC *) tmp->next->data)));
	}
	g_slist_free(list);

#*******************************
MODULE = Irssi  PACKAGE = Irssi::Channel  PREFIX = channel_
#*******************************

void
channel_destroy(channel)
	Irssi::Channel channel

Irssi::Nick
nick_insert(channel, nick, op, voice, send_massjoin)
	Irssi::Channel channel
	char *nick
	int op
	int voice
	int send_massjoin
CODE:
	RETVAL = nicklist_insert(channel, nick, op, voice, send_massjoin);
OUTPUT:
	RETVAL

void
nick_remove(channel, nick)
	Irssi::Channel channel
	Irssi::Nick nick
CODE:
	nicklist_remove(channel, nick);

Irssi::Nick
nick_find(channel, mask)
	Irssi::Channel channel
	char *mask
CODE:
	RETVAL = nicklist_find(channel, mask);
OUTPUT:
	RETVAL

void
nicks(channel)
	Irssi::Channel channel
PREINIT:
	GSList *list, *tmp;
PPCODE:
	list = nicklist_getnicks(channel);

	for (tmp = list; tmp != NULL; tmp = tmp->next) {
		XPUSHs(sv_2mortal(irssi_bless((NICK_REC *) tmp->data)));
	}
	g_slist_free(list);
