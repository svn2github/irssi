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
#include "printtext.h"

FORMAT_REC fecommon_irc_notifylist_formats[] =
{
    { MODULE_NAME, "Notifylist", 0 },

    /* ---- */
    { NULL, "Notifylist", 0 },

    { "notify_join", "%_$0%_ %K[%n$1@$2%K] [%n%_$3%_%K]%n has joined to $4", 5, { 0, 0, 0, 0, 0 } },
    { "notify_part", "%_$0%_ has left $4", 5, { 0, 0, 0, 0, 0 } },
    { "notify_away", "%_$0%_ %K[%n$5%K]%n %K[%n$1@$2%K] [%n%_$3%_%K]%n is now away: $4", 6, { 0, 0, 0, 0, 0, 0 } },
    { "notify_unaway", "%_$0%_ %K[%n$4%K]%n %K[%n$1@$2%K] [%n%_$3%_%K]%n is now unaway", 5, { 0, 0, 0, 0, 0 } },
    { "notify_unidle", "%_$0%_ %K[%n$5%K]%n %K[%n$1@$2%K] [%n%_$3%_%K]%n just stopped idling", 6, { 0, 0, 0, 0, 0, 0 } },
    { "notify_online", "On $0: %_$1%_", 2, { 0, 0 } },
    { "notify_offline", "Offline: $0", 1, { 0 } },
    { "notify_list", "$0: $1 $2 $3", 4, { 0, 0, 0, 0 } }
};
