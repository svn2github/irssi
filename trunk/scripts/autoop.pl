# /AUTOOP <*|#channel> [<nickmasks>]
# For Irssi 0.7.96 and above (older versions had a few bugs)

use Irssi;

my %opnicks, %temp_opped;

sub cmd_autoop {
	my ($data) = @_;
	my ($channel, $masks) = split(" ", $data, 2);

	if ($channel eq "") {
		if (!%opnicks) {
			Irssi::print("Usage: /AUTOOP <*|#channel> [<nickmasks>]");
			Irssi::print("No-one's being auto-opped currently.");
			return 1;
		}

		Irssi::print("Currently auto-opping in channels:");
		foreach $channel (keys %opnicks) {
			$masks = $opnicks{$channel};

			if ($channel eq "*") {
				Irssi::print("All channels: $masks");
			} else {
				Irssi::print("$channel: $masks");
			}
		}
		return 1;
	}

	if ($masks eq "") {
		$masks = "<no-one>" if (!$masks);
		delete $opnicks{$channel};
	} else {
		$opnicks{$channel} = $masks;
	}
	if ($channel eq "*") {
		Irssi::print("Now auto-opping in all channels: $masks");
	} else {
		Irssi::print("$channel: Now auto-opping: $masks");
	}
	return 1;
}

sub autoop {
	my ($channel, $masks, @nicks) = @_;
	my $nickrec;

	foreach $nickrec (@nicks) {
		$nick = $nickrec->values()->{'nick'};
		$host = $nickrec->values()->{'host'};

                if (!$temp_opped{$nick} &&
		    Irssi::irc_masks_match($masks, $nick, $host)) {
			$channel->command("/op $nick");
			$temp_opped{$nick} = 1;
		}
	}
}

sub event_massjoin {
	my ($channel, @nicks) = @_;

	return if (!$channel->values()->{'chanop'});

	undef %temp_opped;

	# channel specific
	my $masks = $opnicks{$channel->values()->{'name'}};
	autoop($channel, $masks, @nicks) if ($masks);

	# for all channels
	$masks = $opnicks{"*"};
	autoop($channel, $masks, @nicks) if ($masks);
}

Irssi::command_bind('autoop', '', 'cmd_autoop');
Irssi::signal_add_last('massjoin', 'event_massjoin');
