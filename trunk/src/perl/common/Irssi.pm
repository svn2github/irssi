#
# Perl interface to irssi functions.
#

package Irssi;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

$VERSION = "0.20";

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw(INPUT_READ INPUT_WRITE
	MSGLEVEL_CRAP MSGLEVEL_MSGS MSGLEVEL_PUBLIC MSGLEVEL_NOTICES
	MSGLEVEL_SNOTES MSGLEVEL_CTCPS MSGLEVEL_ACTIONS MSGLEVEL_JOINS
	MSGLEVEL_PARTS MSGLEVEL_QUITS MSGLEVEL_KICKS MSGLEVEL_MODES
	MSGLEVEL_TOPICS MSGLEVEL_WALLOPS MSGLEVEL_INVITES MSGLEVEL_NICKS
	MSGLEVEL_DCC MSGLEVEL_DCCMSGS MSGLEVEL_CLIENTNOTICE MSGLEVEL_CLIENTCRAP
	MSGLEVEL_CLIENTERROR MSGLEVEL_HILIGHT MSGLEVEL_ALL MSGLEVEL_NOHILIGHT
	MSGLEVEL_NO_ACT MSGLEVEL_NEVER MSGLEVEL_LASTLOG
);
@EXPORT_OK = qw();

bootstrap Irssi $VERSION;

@Irssi::Channel::ISA = qw(Irssi::Windowitem);
@Irssi::Query::ISA = qw(Irssi::Windowitem);

1;

