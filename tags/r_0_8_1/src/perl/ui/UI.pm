#
# Perl interface to irssi functions.
#

package Irssi::UI;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

$VERSION = "0.9";

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw();
@EXPORT_OK = qw();

bootstrap Irssi::UI $VERSION if (!Irssi::Core::is_static());

Irssi::UI::init();

Irssi::EXPORT_ALL();

1;
