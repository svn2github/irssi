#include "printtext.h"

enum {
	IRCTXT_MODULE_NAME,

	IRCTXT_FILL_1,

	IRCTXT_NETSPLIT,
	IRCTXT_NETSPLIT_MORE,
	IRCTXT_NETSPLIT_JOIN,
	IRCTXT_NETSPLIT_JOIN_MORE,
	IRCTXT_NO_NETSPLITS,
	IRCTXT_NETSPLITS_HEADER,
	IRCTXT_NETSPLITS_LINE,
	IRCTXT_NETSPLITS_FOOTER,
	IRCTXT_IRCNET_ADDED,
	IRCTXT_IRCNET_REMOVED,
	IRCTXT_IRCNET_NOT_FOUND,
	IRCTXT_IRCNET_HEADER,
	IRCTXT_IRCNET_LINE,
	IRCTXT_IRCNET_FOOTER,
	IRCTXT_SETUPSERVER_HEADER,
	IRCTXT_SETUPSERVER_LINE,
	IRCTXT_SETUPSERVER_FOOTER,

	IRCTXT_FILL_2,

	IRCTXT_JOINERROR_TOOMANY,
	IRCTXT_JOINERROR_FULL,
	IRCTXT_JOINERROR_INVITE,
	IRCTXT_JOINERROR_BANNED,
	IRCTXT_JOINERROR_BAD_KEY,
	IRCTXT_JOINERROR_BAD_MASK,
	IRCTXT_JOINERROR_UNAVAIL,
	IRCTXT_JOINERROR_DUPLICATE,
	IRCTXT_CHANNEL_REJOIN,
	IRCTXT_INVITING,
	IRCTXT_NOT_INVITED,
	IRCTXT_NAMES,
	IRCTXT_NAMES_NICK,
	IRCTXT_ENDOFNAMES,
	IRCTXT_CHANNEL_CREATED,
	IRCTXT_TOPIC,
	IRCTXT_NO_TOPIC,
	IRCTXT_TOPIC_INFO,
	IRCTXT_CHANMODE_CHANGE,
	IRCTXT_SERVER_CHANMODE_CHANGE,
	IRCTXT_CHANNEL_MODE,
	IRCTXT_BANTYPE,
	IRCTXT_NO_BANS,
	IRCTXT_BANLIST,
	IRCTXT_BANLIST_LONG,
	IRCTXT_EBANLIST,
	IRCTXT_EBANLIST_LONG,
	IRCTXT_INVITELIST,
	IRCTXT_NO_SUCH_CHANNEL,
	IRCTXT_CHANNEL_SYNCED,

	IRCTXT_FILL_4,

	IRCTXT_USERMODE_CHANGE,
	IRCTXT_USER_MODE,
	IRCTXT_AWAY,
	IRCTXT_UNAWAY,
	IRCTXT_NICK_AWAY,
	IRCTXT_NO_SUCH_NICK,
	IRCTXT_YOUR_NICK,
	IRCTXT_NICK_IN_USE,
	IRCTXT_NICK_UNAVAILABLE,
	IRCTXT_YOUR_NICK_OWNED,

	IRCTXT_FILL_5,

	IRCTXT_WHOIS,
	IRCTXT_WHOWAS,
	IRCTXT_WHOIS_IDLE,
	IRCTXT_WHOIS_IDLE_SIGNON,
	IRCTXT_WHOIS_SERVER,
	IRCTXT_WHOIS_OPER,
	IRCTXT_WHOIS_REGISTERED,
	IRCTXT_WHOIS_CHANNELS,
	IRCTXT_WHOIS_AWAY,
	IRCTXT_END_OF_WHOIS,
	IRCTXT_END_OF_WHOWAS,
	IRCTXT_WHOIS_NOT_FOUND,
	IRCTXT_WHO,
	IRCTXT_END_OF_WHO,

	IRCTXT_FILL_6,

	IRCTXT_OWN_NOTICE,
	IRCTXT_OWN_ME,
	IRCTXT_OWN_CTCP,
	IRCTXT_OWN_WALL,
	
	IRCTXT_FILL_7,

	IRCTXT_NOTICE_SERVER,
	IRCTXT_NOTICE_PUBLIC,
	IRCTXT_NOTICE_PUBLIC_OPS,
	IRCTXT_NOTICE_PRIVATE,
	IRCTXT_ACTION_PRIVATE,
	IRCTXT_ACTION_PRIVATE_QUERY,
	IRCTXT_ACTION_PUBLIC,
	IRCTXT_ACTION_PUBLIC_CHANNEL,

	IRCTXT_FILL_8,

	IRCTXT_CTCP_REPLY,
	IRCTXT_CTCP_REPLY_CHANNEL,
	IRCTXT_CTCP_PING_REPLY,
	IRCTXT_CTCP_REQUESTED,

	IRCTXT_FILL_10,

	IRCTXT_ONLINE,
	IRCTXT_PONG,
	IRCTXT_WALLOPS,
	IRCTXT_ACTION_WALLOPS,
	IRCTXT_ERROR,
	IRCTXT_UNKNOWN_MODE,
	IRCTXT_NOT_CHANOP,

	IRCTXT_FILL_11,

	IRCTXT_SILENCED,
	IRCTXT_UNSILENCED,
	IRCTXT_SILENCE_LINE
};

extern FORMAT_REC fecommon_irc_formats[];
