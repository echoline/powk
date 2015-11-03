#
# Copyright (C) 2003 Ryan McCabe <ryan@numb.org>
#
# Demonstrates how to setup event handlers using Perl.
#
# This doesn't handle all the events that pork supports;
# more have been added since I wrote this, but you'll get the
# idea. 
#

sub setup {
	PORK::event_add("BUDDY_AWAY", "buddy_away_handler");
	PORK::event_add("BUDDY_BACK", "buddy_back_handler");
	PORK::event_add("BUDDY_IDLE", "buddy_idle_handler");
	PORK::event_add("BUDDY_SIGNOFF", "buddy_signoff_handler");
	PORK::event_add("BUDDY_SIGNON", "buddy_signon_handler");
	PORK::event_add("BUDDY_UNIDLE", "buddy_unidle_handler");
	PORK::event_add("QUIT", "quit_handler");
	PORK::event_add("RECV_AWAYMSG", "recv_awaymsg_handler");
	PORK::event_add("RECV_IM", "recv_im_handler");
	PORK::event_add("RECV_PROFILE", "recv_profile_handler");
	PORK::event_add("RECV_SEARCH_RESULT", "recv_search_result_handler");
	PORK::event_add("RECV_WARN", "recv_warn_handler");
	PORK::event_add("SEND_AWAY", "send_away_handler");
	PORK::event_add("SEND_IDLE", "send_idle_handler");
	PORK::event_add("SEND_IM", "send_im_handler");
	PORK::event_add("SEND_LINE", "send_line_handler");
	PORK::event_add("SEND_PROFILE", "send_profile_handler");
	PORK::event_add("SEND_WARN", "send_warn_handler");
	PORK::event_add("SIGNOFF", "signoff_handler");
	PORK::event_add("SIGNON", "signon_handler");

	return (0);
}

#
# All of the handler functions (except quit) receive the screen name
# the event happened on as their last parameter (i.e. 'account').
#

sub buddy_away_handler {
	my ($buddy, $account) = @_;

	PORK::echo("called buddy_away_handler args: @_");
	return(0);
}

sub buddy_back_handler {
	my ($buddy, $account) = @_;

	PORK::echo("called buddy_back_handler args: @_");
	return(0);
}

sub buddy_idle_handler {
	my ($buddy, $idle_time, $account) = @_;

	PORK::echo("called buddy_idle_handler args: @_");
	return(0);
}

sub buddy_signoff_handler {
	my ($buddy, $account) = @_;

	PORK::echo("called buddy_signoff_handler args: @_");
	return(0);
}

sub buddy_signon_handler {
	my ($buddy, $account) = @_;

	PORK::echo("called buddy_signon_handler args: @_");
	return(0);
}

sub buddy_unidle_handler {
	my ($buddy, $account) = @_;

	PORK::echo("called buddy_unidle_handler args: @_");
	return(0);
}

sub quit_handler {
	PORK::echo("called quit_handler");
	return(0);
}

sub recv_awaymsg_handler {
	my ($buddy, $is_part_of_whois, $member_since, $online_since, $idle_time,
		$warn_level, $awaymsg, $account) = @_;

	PORK::echo("called recv_awaymsg_handler args: @_");
	return(0);
}

sub recv_im_handler {
	my ($buddy, $msg, $account) = @_;

	PORK::echo("called recv_im_handler args: @_");
	return(0);
}

sub recv_profile_handler {
	my ($buddy, $member_since, $online_since, $idle_time, $warn_level, $profile, $account)
		= @_;

	PORK::echo("called recv_profile_handler args: @_");
	return(0);
}

sub recv_search_result_handler {
	my ($search_str, $results, $account) = @_;

	PORK::echo("called recv_search_result_handler args: @_");
	return(0);
}

sub recv_warn_handler {
	my ($buddy, $new_warn_level, $account) = @_;

	PORK::echo("called recv_warn_handler args: @_");
	return(0);
}

sub send_away_handler {
	my ($away_msg, $account) = @_;

	PORK::echo("called send_away_handler args: @_");
	return(0);
}

sub send_idle_handler {
	my ($idle_seconds, $account) = @_;

	PORK::echo("called send_idle_handler args: @_");
	return(0);
}

sub send_im_handler {
	my ($buddy, $msg, $account) = @_;

	PORK::echo("called send_im_handler args: @_");
	return(0);
}

sub send_line_handler {
	my ($line, $account) = @_;

	PORK::echo("called send_line_handler args: @_");
	return(0);
}

sub send_profile_handler {
	my ($profile, $account) = @_;

	PORK::echo("called send_profile_handler args: @_");
	return(0);
}

sub send_warn_handler {
	my ($buddy, $is_anonymous, $account) = @_;

	PORK::echo("called send_warn_handler args: @_");
	return(0);
}

sub signoff_handler {
	my $account = shift;

	PORK::echo("called signoff_handler args: @_");
	return(0);
}

sub signon_handler {
	my $account = shift;

	PORK::echo("called signon_handler args: @_");
	return(0);
}

setup();
