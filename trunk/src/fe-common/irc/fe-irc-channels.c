/*
 fe-irc-channels.c : irssi

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
#include "servers.h"

#include "irc.h"

static void cmd_channel(const char *data, SERVER_REC *server)
{
	if (ischannel(*data)) {
		signal_emit("command join", 2, data, server);
		signal_stop();
	}
}

void fe_irc_channels_init(void)
{
	command_bind("channel", NULL, (SIGNAL_FUNC) cmd_channel);
}

void fe_irc_channels_deinit(void)
{
	command_unbind("channel", (SIGNAL_FUNC) cmd_channel);
}
