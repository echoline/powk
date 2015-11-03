#
# Copyright (C) 2003 Ryan McCabe <ryan@numb.org>
#
# Works with the xmms-infopipe plugin (search google for it)
#
# Appends a line displaying what's playing in xmms to your profile
# every 30 seconds, and whenever the profile is changed.
#
# I'm a C programmer, not a Perl programmer, and it probably shows.
#

my $xmms_info = "/tmp/xmms-info";
my $timer_refnum;
my $sp_refnum;
my $unload_refnum;
my $last_stored;

#
# Update the profile every 30 seconds.
#

my $interval = 30;

sub np_init {
	$sp_refnum = PORK::event_add("SEND_PROFILE", "np_send_profile_handler");
	if (!defined $sp_refnum) {
		return (-1);
	}

	$unload_refnum = PORK::event_add("UNLOAD", "np_unload");
	if (!defined $unload_refnum) {
		PORK::event_del_refnum($sp_refnum);
		return (-1);
	}

	$timer_refnum = PORK::timer_add($interval, 0, "perl np_run");
	if (!defined $timer_refnum) {
		PORK::event_del_refnum($sp_refnum);
		PORK::event_del_refnum($unload_refnum);
	}

	$last_stored = PORK::get_profile();
	np_run();
}

sub np_get_info {
	my $info_hash = shift;
	local *FILE;

	open(FILE, "<$xmms_info") || return (0);

	while (<FILE>) {
		my $left;
		my $right;

		($left, $right) = split(/: /, $_, 2);
		if ($left && $right) {
			chop($right);
			$info_hash->{$left} = $right;
		}
	}

	close(FILE);
	return (1);
}

#
# Pork events always pass the reference number of the account
# that triggered the event as the last parameter of event
# handlers.
#

sub np_send_profile_handler {
	my $cur_profile = shift;
	my $user = shift;
	my %info_hash = ();
	my $np_str = "nothing";

	$last_stored = $cur_profile;

	if (np_get_info(\%info_hash)) {
		if (defined $info_hash{'Title'} &&
			defined $info_hash{'Status'} &&
			$info_hash{'Status'} eq "Playing")
		{
			$np_str = $info_hash{'Title'};
		}
	}

	#
	# Prepend "<HTML>" so that the string won't be converted to HTML
	# when it's sent -- it's already HTML, make sure the program leaves
	# it alone.
	#

	$np_str =~ s/&/&amp;/g;
	$np_str =~ s/</&lt;/g;
	$np_str =~ s/>/&gt;/g;

	$cur_profile = "$cur_profile<BR><BR>np: $np_str";
	if ($cur_profile !~ /^<HTML>/i) {
		$cur_profile = "<HTML>" . $cur_profile;
	}

	PORK::send_profile($cur_profile);
	return (1);
}

sub np_run {
	#
	# Do it for the current user.
	#

	np_send_profile_handler($last_stored, undef);
}

sub np_unload {
	PORK::timer_del_refnum($timer_refnum);
	PORK::event_del_refnum($sp_refnum);
	PORK::event_del_refnum($unload_refnum);

	PORK::set_profile($last_stored);
	return (0);
}

# This will run when the script is loaded.

np_init();
