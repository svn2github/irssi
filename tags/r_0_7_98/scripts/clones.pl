# /CLONES - display real name of nick

use Irssi;

sub cmd_clones {
	my ($data, $server, $channel) = @_;
	my %hostnames, $host, @nicks, $nick;

	@nicks = $channel->nicks();

	foreach $nick (@nicks) {
		$hostnames{$nick->{host}}++;
	}

	$channel->print("Clones:");
	foreach $host (keys %hostnames) {
		my $clones = $hostnames{$host};
		if ($clones >= 2) {
			$channel->print("$host: $clones");
		}
	}
	return 1;
}

Irssi::command_bind('clones', 'cmd_clones');
