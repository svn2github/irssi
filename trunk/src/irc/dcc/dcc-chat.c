/*
 dcc-chat.c : irssi

    Copyright (C) 1999-2000 Timo Sirainen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "module.h"
#include "signals.h"
#include "commands.h"
#include "network.h"
#include "net-nonblock.h"
#include "net-sendbuffer.h"
#include "line-split.h"
#include "misc.h"
#include "settings.h"

#include "masks.h"
#include "irc.h"
#include "servers-setup.h"
#include "irc-queries.h"

#include "dcc.h"

DCC_REC *dcc_chat_find_id(const char *id)
{
	GSList *tmp;

	g_return_val_if_fail(id != NULL, NULL);

	for (tmp = dcc_conns; tmp != NULL; tmp = tmp->next) {
		DCC_REC *dcc = tmp->data;

		if (dcc->type == DCC_TYPE_CHAT && dcc->chat_id != NULL &&
		    g_strcasecmp(dcc->chat_id, id) == 0)
			return dcc;
	}

	return NULL;
}

static void dcc_chat_set_id(DCC_REC *dcc)
{
        char *id;
	int num;

	if (dcc_chat_find_id(dcc->nick) == NULL) {
                /* same as nick, good */
		dcc->chat_id = g_strdup(dcc->nick);
                return;
	}

	/* keep adding numbers after nick until some of them isn't found */
	for (num = 2;; num++) {
                id = g_strdup_printf("%s%d", dcc->nick, num);
		if (dcc_chat_find_id(id) == NULL) {
			dcc->chat_id = id;
                        break;
		}
                g_free(id);
	}
}

/* Send `data' to dcc chat. */
void dcc_chat_send(DCC_REC *dcc, const char *data)
{
        g_return_if_fail(dcc != NULL);
        g_return_if_fail(dcc->sendbuf != NULL);
	g_return_if_fail(data != NULL);

	net_sendbuffer_send(dcc->sendbuf, data, strlen(data));
	net_sendbuffer_send(dcc->sendbuf, "\n", 1);
}

/* If `item' is a query of a =nick, return DCC chat record of nick */
DCC_REC *item_get_dcc(WI_ITEM_REC *item)
{
	QUERY_REC *query;

	query = IRC_QUERY(item);
	if (query == NULL || *query->name != '=')
		return NULL;

	return dcc_chat_find_id(query->name+1);
}

/* Send text to DCC chat */
static void cmd_msg(const char *data)
{
	DCC_REC *dcc;
	char *text, *target;
	void *free_arg;

	g_return_if_fail(data != NULL);

	if (*data != '=') {
		/* handle only DCC messages */
		return;
	}

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &target, &text))
		return;

	dcc = dcc_chat_find_id(++target);
	if (dcc != NULL && dcc->sendbuf != NULL)
		dcc_chat_send(dcc, text);

	cmd_params_free(free_arg);
	signal_stop();
}

static void cmd_me(const char *data, IRC_SERVER_REC *server, QUERY_REC *item)
{
	DCC_REC *dcc;
	char *str;

	g_return_if_fail(data != NULL);

	dcc = item_get_dcc((WI_ITEM_REC *) item);
	if (dcc == NULL) return;

	str = g_strconcat("ACTION ", data, NULL);
	dcc_ctcp_message(NULL, dcc->nick, dcc, FALSE, str);
	g_free(str);

	signal_stop();
}

static void cmd_action(const char *data, IRC_SERVER_REC *server)
{
	DCC_REC *dcc;
	char *target, *text, *str;
	void *free_arg;

	g_return_if_fail(data != NULL);

	if (*data != '=') {
		/* handle only DCC actions */
		return;
	}

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &target, &text))
		return;
	if (*target == '\0' || *text == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	dcc = dcc_chat_find_id(target+1);
	if (dcc != NULL) {
		str = g_strconcat("ACTION ", text, NULL);
		dcc_ctcp_message(NULL, dcc->nick, dcc, FALSE, str);
		g_free(str);
	}

	cmd_params_free(free_arg);
	signal_stop();
}

static void cmd_ctcp(const char *data, IRC_SERVER_REC *server)
{
	DCC_REC *dcc;
	char *target, *ctcpcmd, *ctcpdata, *str;
	void *free_arg;

	g_return_if_fail(data != NULL);
	if (server == NULL || !server->connected) cmd_return_error(CMDERR_NOT_CONNECTED);

	if (!cmd_get_params(data, &free_arg, 3 | PARAM_FLAG_GETREST, &target, &ctcpcmd, &ctcpdata))
		return;
	if (*target == '\0' || *ctcpcmd == '\0') cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);

	if (*target != '=') {
		/* handle only DCC CTCPs */
		cmd_params_free(free_arg);
		return;
	}

	dcc = dcc_chat_find_id(target+1);
	if (dcc != NULL) {
		g_strup(ctcpcmd);

		str = g_strconcat(ctcpcmd, " ", ctcpdata, NULL);
		dcc_ctcp_message(NULL, dcc->nick, dcc, FALSE, str);
		g_free(str);
	}

	cmd_params_free(free_arg);
	signal_stop();
}

/* input function: DCC CHAT received some data.. */
static void dcc_chat_input(DCC_REC *dcc)
{
        char tmpbuf[512], *str;
	int recvlen, ret;

	g_return_if_fail(dcc != NULL);

	do {
		recvlen = net_receive(dcc->handle, tmpbuf, sizeof(tmpbuf));

		ret = line_split(tmpbuf, recvlen, &str, (LINEBUF_REC **) &dcc->databuf);
		if (ret == -1) {
			/* connection lost */
                        dcc->connection_lost = TRUE;
			signal_emit("dcc closed", 1, dcc);
			dcc_destroy(dcc);
			break;
		}

		if (ret > 0) {
			dcc->transfd += ret;
			signal_emit("dcc chat message", 2, dcc, str);
		}
	} while (ret > 0);
}

/* input function: DCC CHAT - someone tried to connect to our socket */
static void dcc_chat_listen(DCC_REC *dcc)
{
	IPADDR ip;
        GIOChannel *handle;
	int port;

	g_return_if_fail(dcc != NULL);

	/* accept connection */
	handle = net_accept(dcc->handle, &ip, &port);
	if (handle == NULL)
		return;

	/* TODO: add paranoia check - see dcc-files.c */

	g_source_remove(dcc->tagconn);
	net_disconnect(dcc->handle);

	dcc->starttime = time(NULL);
	dcc->handle = handle;
	dcc->sendbuf = net_sendbuffer_create(handle, 0);
	memcpy(&dcc->addr, &ip, sizeof(IPADDR));
	net_ip2host(&dcc->addr, dcc->addrstr);
	dcc->port = port;
	dcc->tagread = g_input_add(handle, G_INPUT_READ,
				   (GInputFunction) dcc_chat_input, dcc);

	signal_emit("dcc connected", 1, dcc);
}

/* callback: DCC CHAT - net_connect_nonblock() finished */
static void sig_chat_connected(DCC_REC *dcc)
{
	g_return_if_fail(dcc != NULL);

	if (net_geterror(dcc->handle) != 0) {
		/* error connecting */
		signal_emit("dcc error connect", 1, dcc);
		dcc_destroy(dcc);
		return;
	}

	/* connect ok. */
	g_source_remove(dcc->tagconn);
	dcc->starttime = time(NULL);
	dcc->sendbuf = net_sendbuffer_create(dcc->handle, 0);
	dcc->tagread = g_input_add(dcc->handle, G_INPUT_READ,
				   (GInputFunction) dcc_chat_input, dcc);

	signal_emit("dcc connected", 1, dcc);
}

static void dcc_chat_connect(DCC_REC *dcc)
{
	g_return_if_fail(dcc != NULL);

	if (dcc->addrstr[0] == '\0' ||
	    dcc->starttime != 0 || dcc->handle != NULL) {
		/* already sent a chat request / already chatting */
		return;
	}

	dcc->handle = net_connect_ip(&dcc->addr, dcc->port,
				     source_host_ok ? source_host_ip : NULL);
	if (dcc->handle != NULL) {
		dcc->tagconn = g_input_add(dcc->handle,
					   G_INPUT_WRITE | G_INPUT_READ,
					   (GInputFunction) sig_chat_connected, dcc);
	} else {
		/* error connecting */
		signal_emit("dcc error connect", 1, dcc);
		dcc_destroy(dcc);
	}
}

/* SYNTAX: DCC CHAT [<nick>] */
static void cmd_dcc_chat(const char *data, IRC_SERVER_REC *server)
{
	void *free_arg;
	DCC_REC *dcc;
	IPADDR own_ip;
        GIOChannel *handle;
	char *nick, host[MAX_IP_LEN];
	int port;

	g_return_if_fail(data != NULL);

	if (!cmd_get_params(data, &free_arg, 1, &nick))
		return;

	if (*nick == '\0') {
		dcc = dcc_find_request_latest(DCC_TYPE_CHAT);
		if (dcc != NULL)
			dcc_chat_connect(dcc);
		cmd_params_free(free_arg);
		return;
	}

	dcc = dcc_chat_find_id(nick);
	if (dcc != NULL && dcc_is_waiting_user(dcc)) {
		/* found from dcc chat requests,
		   we're the connecting side */
		dcc_chat_connect(dcc);
		cmd_params_free(free_arg);
		return;
	}

	if (dcc != NULL && dcc_is_listening(dcc) &&
	    dcc->server == server) {
		/* sending request again even while old request is
		   still waiting, remove it. */
                dcc_destroy(dcc);
	}

	/* start listening  */
	if (server == NULL || !server->connected)
		cmd_param_error(CMDERR_NOT_CONNECTED);

	handle = dcc_listen(net_sendbuffer_handle(server->handle),
			    &own_ip, &port);
	if (handle == NULL)
		cmd_param_error(CMDERR_ERRNO);

	dcc = dcc_create(DCC_TYPE_CHAT, nick, "chat", server, NULL);
        dcc_chat_set_id(dcc);
        dcc->handle = handle;
	dcc->tagconn = g_input_add(dcc->handle, G_INPUT_READ,
				   (GInputFunction) dcc_chat_listen, dcc);

	/* send the chat request */
	dcc_make_address(&own_ip, host);
	irc_send_cmdv(server, "PRIVMSG %s :\001DCC CHAT CHAT %s %d\001",
		      nick, host, port);

	cmd_params_free(free_arg);
}

/* SYNTAX: MIRCDCC ON|OFF */
static void cmd_mircdcc(const char *data, IRC_SERVER_REC *server,
			QUERY_REC *item)
{
	DCC_REC *dcc;

	g_return_if_fail(data != NULL);

	dcc = item_get_dcc((WI_ITEM_REC *) item);
	if (dcc == NULL) return;

	dcc->mirc_ctcp = toupper(*data) != 'N' &&
		g_strncasecmp(data, "OF", 3) != 0;
}

#define DCC_AUTOACCEPT_PORT(dcc) \
	((dcc)->port >= 1024 || settings_get_bool("dcc_autoaccept_lowport"))

#define DCC_CHAT_AUTOACCEPT(dcc, server, nick, addr) \
	(DCC_AUTOACCEPT_PORT(dcc) && \
	masks_match(SERVER(server), \
		settings_get_str("dcc_autochat_masks"), (nick), (addr)))


/* CTCP: DCC CHAT */
static void ctcp_msg_dcc_chat(IRC_SERVER_REC *server, const char *data,
			      const char *nick, const char *addr,
			      const char *target, DCC_REC *chat)
{
        DCC_REC *dcc;
	char **params;
	int paramcount;
        int autoallow = FALSE;

        /* CHAT <unused> <address> <port> */
	params = g_strsplit(data, " ", -1);
	paramcount = strarray_length(params);

	if (paramcount < 3) {
		g_strfreev(params);
                return;
	}

	dcc = dcc_find_request(DCC_TYPE_CHAT, nick, NULL);
	if (dcc != NULL) {
		if (dcc_is_listening(dcc)) {
			/* we requested dcc chat, they requested
			   dcc chat from us .. allow it. */
			dcc_destroy(dcc);
			autoallow = TRUE;
		} else {
			/* we already have one dcc chat request
			   from this nick, remove it. */
                        dcc_destroy(dcc);
		}
	}

	dcc = dcc_create(DCC_TYPE_CHAT, nick, params[0], server, chat);
        dcc_chat_set_id(dcc);
	dcc->target = g_strdup(target);
	dcc->port = atoi(params[2]);
	dcc_get_address(params[1], &dcc->addr);
	net_ip2host(&dcc->addr, dcc->addrstr);

	signal_emit("dcc request", 2, dcc, addr);

	if (autoallow || DCC_CHAT_AUTOACCEPT(dcc, server, nick, addr))
		dcc_chat_connect(dcc);

	g_strfreev(params);
}

/* DCC CHAT: text received */
static void dcc_chat_msg(DCC_REC *dcc, const char *msg)
{
	char *cmd, *ptr;
	int reply;

	g_return_if_fail(dcc != NULL);
	g_return_if_fail(msg != NULL);

	reply = FALSE;
	if (g_strncasecmp(msg, "CTCP_MESSAGE ", 13) == 0) {
		/* bitchx (and ircii?) sends this */
		msg += 13;
		dcc->mirc_ctcp = FALSE;
	} else if (g_strncasecmp(msg, "CTCP_REPLY ", 11) == 0) {
		/* bitchx (and ircii?) sends this */
		msg += 11;
		reply = TRUE;
		dcc->mirc_ctcp = FALSE;
	} else if (*msg == 1) {
		/* Use the mirc style CTCPs from now on.. */
		dcc->mirc_ctcp = TRUE;
	}

	/* Handle only DCC CTCPs */
	if (*msg != 1)
		return;

	/* get ctcp command, remove \001 chars */
	cmd = g_strconcat(reply ? "dcc reply " : "dcc ctcp ", msg+1, NULL);
	if (cmd[strlen(cmd)-1] == 1) cmd[strlen(cmd)-1] = '\0';

	ptr = strchr(cmd+9, ' ');
	if (ptr != NULL) *ptr++ = '\0'; else ptr = "";

	g_strdown(cmd+9);
	if (!signal_emit(cmd, 2, ptr, dcc)) {
		signal_emit(reply ? "default dcc reply" :
			    "default dcc ctcp", 2, msg, dcc);
	}
	g_free(cmd);

	signal_stop();
}

static void dcc_ctcp_redirect(const char *msg, DCC_REC *dcc)
{
	g_return_if_fail(msg != NULL);
	g_return_if_fail(dcc != NULL);

	signal_emit("ctcp msg dcc", 6, dcc->server, msg,
		    dcc->nick, "dcc", dcc->mynick, dcc);
}

static void dcc_ctcp_reply_redirect(const char *msg, DCC_REC *dcc)
{
	g_return_if_fail(msg != NULL);
	g_return_if_fail(dcc != NULL);

	signal_emit("ctcp reply dcc", 6, dcc->server, msg,
		    dcc->nick, "dcc", dcc->mynick, dcc);
}

void dcc_chat_init(void)
{
	settings_add_str("dcc", "dcc_autochat_masks", "");

	command_bind("msg", NULL, (SIGNAL_FUNC) cmd_msg);
	command_bind("me", NULL, (SIGNAL_FUNC) cmd_me);
	command_bind("action", NULL, (SIGNAL_FUNC) cmd_action);
	command_bind("ctcp", NULL, (SIGNAL_FUNC) cmd_ctcp);
	command_bind("dcc chat", NULL, (SIGNAL_FUNC) cmd_dcc_chat);
	command_bind("mircdcc", NULL, (SIGNAL_FUNC) cmd_mircdcc);
	signal_add("ctcp msg dcc chat", (SIGNAL_FUNC) ctcp_msg_dcc_chat);
	signal_add_first("dcc chat message", (SIGNAL_FUNC) dcc_chat_msg);
	signal_add("dcc ctcp dcc", (SIGNAL_FUNC) dcc_ctcp_redirect);
	signal_add("dcc reply dcc", (SIGNAL_FUNC) dcc_ctcp_reply_redirect);
}

void dcc_chat_deinit(void)
{
	command_unbind("msg", (SIGNAL_FUNC) cmd_msg);
	command_unbind("me", (SIGNAL_FUNC) cmd_me);
	command_unbind("action", (SIGNAL_FUNC) cmd_action);
	command_unbind("ctcp", (SIGNAL_FUNC) cmd_ctcp);
	command_unbind("dcc chat", (SIGNAL_FUNC) cmd_dcc_chat);
	command_unbind("mircdcc", (SIGNAL_FUNC) cmd_mircdcc);
	signal_remove("ctcp msg dcc chat", (SIGNAL_FUNC) ctcp_msg_dcc_chat);
	signal_remove("dcc chat message", (SIGNAL_FUNC) dcc_chat_msg);
	signal_remove("dcc ctcp dcc", (SIGNAL_FUNC) dcc_ctcp_redirect);
	signal_remove("dcc reply dcc", (SIGNAL_FUNC) dcc_ctcp_reply_redirect);
}
