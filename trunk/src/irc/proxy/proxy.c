/*
 sample.c : sample plugin for irssi

    Copyright (C) 1999 Timo Sirainen

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
#include "settings.h"
#include "levels.h"

void irc_proxy_deinit(void)
{
	plugin_proxy_listen_deinit();
}

void irc_proxy_init(void)
{
	settings_add_str("irssiproxy", "irssiproxy_ports", "");
	settings_add_str("irssiproxy", "irssiproxy_password", "");

	if (*settings_get_str("irssiproxy_password") == '\0') {
		/* no password - bad idea! */
		signal_emit("gui dialog", 2, "warning",
			    "Warning!! Password not specified, everyone can "
			    "use this proxy! Use /set irssiproxy_password "
			    "<password> to set it");
	}
	if (*settings_get_str("irssiproxy_ports") == '\0') {
		signal_emit("gui dialog", 2, "warning",
			    "No proxy ports specified. Use /SET "
			    "irssiproxy_ports <ircnet>=<port> <ircnet2>=<port2> "
			    "... to set them.");
	}

	plugin_proxy_listen_init();
}
