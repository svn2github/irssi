#include "printtext.h"

enum {
	IRCTXT_MODULE_NAME,

	IRCTXT_FILL_1,

	IRCTXT_OWN_DCC,
	IRCTXT_DCC_MSG,
	IRCTXT_ACTION_DCC,
	IRCTXT_DCC_CTCP,
	IRCTXT_DCC_CHAT,
	IRCTXT_DCC_CHAT_NOT_FOUND,
	IRCTXT_DCC_CHAT_CONNECTED,
	IRCTXT_DCC_CHAT_DISCONNECTED,
	IRCTXT_DCC_SEND,
	IRCTXT_DCC_SEND_EXISTS,
	IRCTXT_DCC_SEND_NOT_FOUND,
	IRCTXT_DCC_SEND_FILE_NOT_FOUND,
	IRCTXT_DCC_SEND_CONNECTED,
	IRCTXT_DCC_SEND_COMPLETE,
	IRCTXT_DCC_SEND_ABORTED,
	IRCTXT_DCC_GET_NOT_FOUND,
	IRCTXT_DCC_GET_CONNECTED,
	IRCTXT_DCC_GET_COMPLETE,
	IRCTXT_DCC_GET_ABORTED,
	IRCTXT_DCC_UNKNOWN_CTCP,
	IRCTXT_DCC_UNKNOWN_REPLY,
	IRCTXT_DCC_UNKNOWN_TYPE,
	IRCTXT_DCC_CONNECT_ERROR,
	IRCTXT_DCC_CANT_CREATE,
	IRCTXT_DCC_REJECTED,
	IRCTXT_DCC_CLOSE,
};

extern FORMAT_REC fecommon_irc_dcc_formats[];
#define MODULE_FORMATS fecommon_irc_dcc_formats
