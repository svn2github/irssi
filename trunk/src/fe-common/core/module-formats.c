/*
 module-formats.c : irssi

    Copyright (C) 2000 Timo Sirainen

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
#include "formats.h"

FORMAT_REC fecommon_core_formats[] = {
	{ MODULE_NAME, "Core", 0 },

	/* ---- */
	{ NULL, "Windows", 0 },

	{ "line_start", "{line_start}", 0 },
	{ "line_start_irssi", "{line_start}{hilight Irssi:} ", 0 },
        { "timestamp", "{timestamp $[-2.0]3:$[-2.0]4} ", 6, { 1, 1, 1, 1, 1, 1 } },
	{ "servertag", "[$0] ", 1, { 0 } },
	{ "daychange", "Day changed to $[-2.0]{0} $3 $2", 4, { 1, 1, 1, 0 } },
	{ "talking_with", "You are now talking with {nick $0}", 1, { 0 } },
	{ "refnum_too_low", "Window number must be greater than 1", 0 },
	{ "windowlist_header", "Ref Name                 Active item     Server          Level", 0 },
	{ "windowlist_line", "$[3]0 %|$[20]1 $[15]2 $[15]3 $4", 5, { 1, 0, 0, 0, 0 } },
	{ "windowlist_footer", "", 0 },

	/* ---- */
	{ NULL, "Server", 0 },

	{ "looking_up", "Looking up {server $0}", 1, { 0 } },
	{ "connecting", "Connecting to {server $0} [$1] port {hilight $2}", 3, { 0, 0, 1 } },
	{ "connection_established", "Connection to {server $0} established", 1, { 0 } },
	{ "cant_connect", "Unable to connect server {server $0} port {hilight $1} {reason $2}", 3, { 0, 1, 0 } },
	{ "connection_lost", "Connection lost to {server $0}", 1, { 0 } },
	{ "lag_disconnected", "No PONG reply from server {server $0} in $1 seconds, disconnecting", 2, { 0, 1 } },
	{ "disconnected", "Disconnected from {server $0} {reason $1}", 2, { 0, 0 } },
	{ "server_quit", "Disconnecting from server {server $0}: {reason $1}", 2, { 0, 0 } },
	{ "server_changed", "Changed to {hilight $2} server {server $1}", 3, { 0, 0, 0 } },
	{ "unknown_server_tag", "Unknown server tag {server $0}", 1, { 0 } },
	{ "server_list", "{server $0}: $1:$2 ($3)", 5, { 0, 0, 1, 0, 0 } },
	{ "server_lookup_list", "{server $0}: $1:$2 ($3) (connecting...)", 5, { 0, 0, 1, 0, 0 } },
	{ "server_reconnect_list", "{server $0}: $1:$2 ($3) ($5 left before reconnecting)", 6, { 0, 0, 1, 0, 0, 0 } },
	{ "server_reconnect_removed", "Removed reconnection to server {server $0} port {hilight $1}", 3, { 0, 1, 0 } },
	{ "server_reconnect_not_found", "Reconnection tag {server $0} not found", 1, { 0 } },
	{ "setupserver_added", "Server {server $0} saved", 2, { 0, 1 } },
	{ "setupserver_removed", "Server {server $0} removed", 2, { 0, 1 } },
	{ "setupserver_not_found", "Server {server $0} not found", 2, { 0, 1 } },

	/* ---- */
	{ NULL, "Channels", 0 },

	{ "join", "{channick_hilight $0} {chanhost_hilight $1} has joined {channel $2}", 3, { 0, 0, 0 } },
	{ "part", "{channick $0} {chanhost $1} has left {channel $2} {reason $3}", 4, { 0, 0, 0, 0 } },
	{ "kick", "{channick $0} was kicked from {channel $1} by {nick $2} {reason $3}", 4, { 0, 0, 0, 0 } },
	{ "quit", "{channick $0} {chanhost $1} has quit {reason $2}", 3, { 0, 0, 0 } },
	{ "quit_once", "{channel $3} {channick $0} {chanhost $1} has quit {reason $2}", 4, { 0, 0, 0, 0 } },
	{ "invite", "{nick $0} invites you to {channel $1}", 2, { 0, 0 } },
	{ "new_topic", "{nick $0} changed the topic of {channel $1} to: $2", 3, { 0, 0, 0 } },
	{ "topic_unset", "Topic unset by {nick $0} on {channel $1}", 2, { 0, 0 } },
	{ "your_nick_changed", "You're now known as {nick $1}", 2, { 0, 0 } },
	{ "nick_changed", "{channick $0} is now known as {channick_hilight $1}", 2, { 0, 0 } },
	{ "talking_in", "You are now talking in {channel $0}", 1, { 0 } },
	{ "not_in_channels", "You are not on any channels", 0 },
	{ "current_channel", "Current channel {channel $0}", 1, { 0 } },
	{ "chanlist_header", "You are on the following channels:", 0 },
	{ "chanlist_line", "{channel $[-10]0} %|+$1 ($2): $3", 4, { 0, 0, 0, 0 } },
	{ "chansetup_not_found", "Channel {channel $0} not found", 2, { 0, 0 } },
	{ "chansetup_added", "Channel {channel $0} saved", 2, { 0, 0 } },
	{ "chansetup_removed", "Channel {channel $0} removed", 2, { 0, 0 } },
	{ "chansetup_header", "Channel         IRC net    Password   Settings", 0 },
	{ "chansetup_line", "{channel $[15]0} %|$[10]1 $[10]2 $3", 4, { 0, 0, 0, 0 } },
	{ "chansetup_footer", "", 0 },

	/* ---- */
	{ NULL, "Messages", 0 },

	{ "own_msg", "{ownmsgnick $2 {ownnick $0}}$1", 3, { 0, 0, 0 } },
	{ "own_msg_channel", "{ownmsgnick $3 {ownnick $0}{msgchannel $1}}$2", 4, { 0, 0, 0, 0 } },
	{ "own_msg_private", "{ownprivmsg msg $0}$1", 2, { 0, 0 } },
	{ "own_msg_private_query", "{ownprivmsgnick {ownprivnick $2}}$1", 3, { 0, 0, 0 } },
	{ "pubmsg_me", "{pubmsgmenick $2 {menick $0}}$1", 3, { 0, 0, 0 } },
	{ "pubmsg_me_channel", "{pubmsgmenick $3 {menick $0}{msgchannel $1}}$2", 4, { 0, 0, 0, 0 } },
	{ "pubmsg_hilight", "{pubmsghinick $0 $3 $1}$2", 4, { 0, 0, 0, 0 } },
	{ "pubmsg_hilight_channel", "{pubmsghinick $0 $4 $1{msgchannel $2}}$3", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg", "{pubmsgnick $2 {pubnick $0}}$1", 3, { 0, 0, 0 } },
	{ "pubmsg_channel", "{pubmsgnick $3 {pubnick $0}{msgchannel $1}}$2", 4, { 0, 0, 0, 0 } },
	{ "msg_private", "{privmsg $0 $1}$2", 3, { 0, 0, 0 } },
	{ "msg_private_query", "{privmsgnick $0}$2", 3, { 0, 0, 0 } },
	{ "no_msgs_got", "You have not received a message from anyone yet", 0 },
	{ "no_msgs_sent", "You have not sent a message to anyone yet", 0 },

	/* ---- */
	{ NULL, "Queries", 0 },

	{ "query_start", "Starting query with {nick $0}", 1, { 0 } },
	{ "no_query", "No query with {nick $0}", 1, { 0 } },
	{ "query_server_changed", "Query with {nick $0} changed to server {server $1}", 2, { 0, 0 } },

	/* ---- */
	{ NULL, "Highlighting", 0 },

	{ "hilight_header", "Highlights:", 0 },
	{ "hilight_line", "$[-4]0 $1 $2 $3$4$5", 7, { 1, 0, 0, 0, 0, 0, 0 } },
	{ "hilight_footer", "", 0 },
	{ "hilight_not_found", "Highlight not found: $0", 1, { 0 } },
	{ "hilight_removed", "Highlight removed: $0", 1, { 0 } },

	/* ---- */
	{ NULL, "Aliases", 0 },

	{ "alias_added", "Alias $0 added", 1, { 0 } },
	{ "alias_removed", "Alias $0 removed", 1, { 0 } },
	{ "alias_not_found", "No such alias: $0", 1, { 0 } },
	{ "aliaslist_header", "Aliases:", 0 },
	{ "aliaslist_line", "$[10]0 $1", 2, { 0, 0 } },
	{ "aliaslist_footer", "", 0 },

	/* ---- */
	{ NULL, "Logging", 0 },

	{ "log_opened", "Log file {hilight $0} opened", 1, { 0 } },
	{ "log_closed", "Log file {hilight $0} closed", 1, { 0 } },
	{ "log_create_failed", "Couldn't create log file {hilight $0}: $1", 2, { 0, 0 } },
	{ "log_locked", "Log file {hilight $0} is locked, probably by another running Irssi", 1, { 0 } },
	{ "log_not_open", "Log file {hilight $0} not open", 1, { 0 } },
	{ "log_started", "Started logging to file {hilight $0}", 1, { 0 } },
	{ "log_stopped", "Stopped logging to file {hilight $0}", 1, { 0 } },
	{ "log_list_header", "Logs:", 0 },
	{ "log_list", "$0 $1: $2 $3$4", 5, { 1, 0, 0, 0, 0, 0 } },
	{ "log_list_footer", "", 0 },
	{ "windowlog_file", "Window LOGFILE set to $0", 1, { 0 } },
	{ "windowlog_file_logging", "Can't change window's logfile while log is on", 0 },
	{ "no_away_msgs", "No new messages in awaylog", 1, { 0 } },
	{ "away_msgs", "{hilight $1} new messages in awaylog:", 2, { 0, 1 } },

	/* ---- */
	{ NULL, "Modules", 0 },

	{ "module_already_loaded", "Module {hilight $0} already loaded", 1, { 0 } },
	{ "module_load_error", "Error loading module {hilight $0}: $1", 2, { 0, 0 } },
	{ "module_invalid", "{hilight $0} isn't Irssi module", 1, { 0 } },
	{ "module_loaded", "Loaded module {hilight $0}", 1, { 0 } },
	{ "module_unloaded", "Unloaded module {hilight $0}", 1, { 0 } },

	/* ---- */
	{ NULL, "Commands", 0 },

	{ "command_unknown", "Unknown command: $0", 1, { 0 } },
	{ "command_ambiguous", "Ambiguous command: $0", 1, { 0 } },
	{ "option_unknown", "Unknown option: $0", 1, { 0 } },
	{ "option_ambiguous", "Ambiguous option: $0", 1, { 0 } },
	{ "option_missing_arg", "Missing required argument for: $0", 1, { 0 } },
	{ "not_enough_params", "Not enough parameters given", 0 },
	{ "not_connected", "Not connected to IRC server yet", 0 },
	{ "not_joined", "Not joined to any channels yet", 0 },
	{ "chan_not_found", "Not joined to such channel", 0 },
	{ "chan_not_synced", "Channel not fully synchronized yet, try again after a while", 0 },
	{ "not_good_idea", "Doing this is not a good idea. Add -YES if you really mean it", 0 },

	/* ---- */
	{ NULL, "Themes", 0 },

	{ "theme_saved", "Theme saved to $0", 1, { 0 } },
	{ "theme_save_failed", "Error saving theme to $0: $1", 2, { 0, 0 } },
	{ "theme_not_found", "Theme {hilight $0} not found", 1, { 0 } },
	{ "window_theme_changed", "Using theme {hilight $0} in this window", 1, { 0 } },
	{ "format_title", "%:[{hilight $0}] - [{hilight $1}]%:", 2, { 0, 0 } },
	{ "format_subtitle", "[{hilight $0}]", 1, { 0 } },
	{ "format_item", "$0 = $1", 2, { 0, 0 } },

	/* ---- */
	{ NULL, "Ignores", 0 },

	{ "ignored", "Ignoring {hilight $1} from {nick $0}", 2, { 0, 0 } },
	{ "unignored", "Unignored {nick $0}", 1, { 0 } },
	{ "ignore_not_found", "{nick $0} is not being ignored", 1, { 0 } },
	{ "ignore_no_ignores", "There are no ignores", 0 },
	{ "ignore_header", "Ignorance List:", 0 },
	{ "ignore_line", "$[-4]0 $1: $2 $3 $4", 4, { 1, 0, 0, 0 } },
	{ "ignore_footer", "", 0 },

	/* ---- */
	{ NULL, "Misc", 0 },

	{ "not_toggle", "Value must be either ON, OFF or TOGGLE", 0 },
	{ "perl_error", "Perl error: $0", 1, { 0 } },
	{ "bind_key", "$[10]0 $1 $2", 3, { 0, 0, 0 } },
	{ "bind_unknown_id", "Unknown bind action: $0", 1, { 0 } },
	{ "config_saved", "Saved configuration to file $0", 1, { 0 } },
	{ "config_reloaded", "Reloaded configuration", 1, { 0 } },
	{ "config_modified", "Configuration file was modified since irssi was last started - do you want to overwrite the possible changes?", 1, { 0 } },

	{ NULL, NULL, 0 }
};
