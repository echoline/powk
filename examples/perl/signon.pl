# Copyright (C) 2004 Ryan McCabe <ryan@numb.org>
#
# Demonstrates signon handler

# You will send $signon_msg to $msg_user when $signon_user signs on.
# Edit these next three variables

my $signon_user = "signon_user";
my $msg_user = "msg_user";
my $signon_msg = "This is a test message";


my $signon_refnum;
my $unload_refnum;

sub normalize {
	my $name = lc(shift);

	$name =~ s/ //g; 
	return ($name);
}

sub setup {
	$signon_refnum = PORK::event_add("BUDDY_SIGNON", "buddy_signon_handler");
	if (!defined $signon_refnum) {
		PORK::err_msg("Error setting up signon notifier.");
		return (-1);
	}

	$unload_refnum = PORK::event_add("UNLOAD", "unload_handler");
	if (!defined $unload_refnum) {
		PORK::err_msg("Error setting up unload handler."); 
		return (-1);
	}

	return (0);
}

sub unload_handler {
	PORK::event_del_refnum($signon_refnum);
	PORK::event_del_refnum($unload_refnum);
}

sub buddy_signon_handler {
	my ($buddy, $account) = @_;

	if (normalize($buddy) eq $signon_user) {
		PORK::send_msg($msg_user, $signon_msg, $account);
	}

	return(0);
}

$signon_user = normalize($signon_user);
setup();
