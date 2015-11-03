#
# Copyright (C) 2004-2005 Ryan McCabe <ryan@numb.org> (GPLv2)
#

use strict;

my $chat_status_op = 0x01;
my $chat_status_halfop = 0x02;
my $chat_status_voice = 0x04;

sub op {
	my $target = PORK::chat_target();
	my $i = 0;
	my @users = ();

	if (!$target) {
		return (-1);
	}

	@users = split(/ +/, shift);
	if (!@users) {
		return (-1);
	}

	while ($i < scalar(@users)) {
		PORK::quote("MODE $target +oooo @users[$i .. $i + 3]");
		$i += 4;
	}

	return (0);
}

sub cmdswitch {
	my $newc = "/";

	if (PORK::get_opt("CMDCHARS") eq "/") {
		$newc = "";
	}
	PORK::set_opt("CMDCHARS", $newc);
	PORK::err_msg("CMDCHARS is now $newc");
}

sub chanmode {
	my $target = PORK::chat_target();
	my $modestr = shift;


	if (!$target) {
		return (-1);
	}

	PORK::quote("MODE $target $modestr");
	return (0);
}

sub mop {
	my $target = PORK::chat_target();
	my @users = ();
	my $i = 0;

	if (!$target) {
		return (-1);
	}

	@users = PORK::chat_get_users($target, $chat_status_op, 1);
	while ($i < scalar(@users)) {
		PORK::quote("MODE $target +oooo @users[$i .. $i + 3]");
		$i += 4;
	}

	return (0);
}

sub dop {
	my $target = PORK::chat_target();
	my $i = 0;
	my @users = ();

	if (!$target) {
		return (-1);
	}

	@users = split(/ +/, shift);
	if (!@users) {
		return (-1);
	}

	while ($i < scalar(@users)) {
		PORK::quote("MODE $target -oooo @users[$i .. $i + 3]");
		$i += 4;
	}

	return (0);
}

sub mdop {
	my $target = PORK::chat_target();
	my @users = ();
	my $i = 0;
	my $me = PORK::cur_user();

	if (!$target) {
		return (-1);
	}

	@users = PORK::chat_get_users($target, $chat_status_op, 0);
	@users = grep(!/^$me$/, @users);
	while ($i < scalar(@users)) {
		PORK::quote("MODE $target -oooo @users[$i .. $i + 3]");
		$i += 4;
	}

	return (0);
}

sub chops {
	my $target = PORK::chat_target();

	if (!$target) {
		return (-1);
	}

	my @users = PORK::chat_get_users($target, $chat_status_op, 0);
	if (@users) {
		my $str = "";

		PORK::echo("%D--%m--%M--%C c%chanops%x on %G$target");

		foreach my $i (@users) {
			$str .= "%D[%g$i" . "%D] ";		
		}

		PORK::echo($str);
		my @a = PORK::chat_get_users($target, 0, 0);
		PORK::echo("%D--%m--%M--%C c%chanops%W: %g" . scalar(@users) . "%W/%G" . scalar(@a));
	}

	return (0);
}

sub nops {
	my $target = PORK::chat_target();

	if (!$target) {
		return (-1);
	}

	my @users = PORK::chat_get_users($target, $chat_status_op, 1);
	if (@users) {
		my $str = "";

		PORK::echo("%D--%m--%M--%C n%conops%x on %G$target");

		foreach my $i (@users) {
			$str .= "%D[%b$i" . "%D] ";		
		}

		PORK::echo($str);
		my @a = PORK::chat_get_users($target, 0, 0);
		PORK::echo("%D--%m--%M--%C n%conops%W: %b" . scalar(@users) . "%W/%B" . scalar(@a));
	}

	return (0);
}

sub wii {
	my @users = split(/ +/, shift);
	if (!@users) {
		return (-1);
	}

	foreach my $i (@users) {
		PORK::quote("WHOIS $i $i");
	}

	return (0);
}

sub kb {
	my $target = PORK::chat_target();
	my $i = 0;
	my @args = ();

	if (!$target) {
		return (-1);
	}

	@args = split(/ +/, shift, 2);
	if (!@args) {
		return (-1);
	}

	PORK::chat_ban($target, $args[0]);
	PORK::chat_kick($target, $args[0], $args[1]);

	return (0);
}

sub oper {
	my $user;
	my $passwd;

	my @args = split(/ /, shift, 2);
	if (!@args || scalar(@args) < 2) {
		$user = PORK::get_cur_user() if (!$args[0]);
		$passwd = PORK::prompt_user("Password: ");
		if (!$passwd) {
			PORK::err_msg("Unable to read password.");
			return (-1);
		}
	}
	$user = $args[0] if (scalar(@args) >= 1);
	$passwd = $args[1] if (scalar(@args) == 2);

	PORK::quote("OPER $user $passwd");
}

sub umode {
	my $me = PORK::cur_user();
	my $mode = shift;

	PORK::quote("MODE $me $mode");
}

sub irc_cmd {
	my ($cmd, $args) = @_;
	my @toks;
	my $dest;

	@toks = split(/ +/, $args, 2);
	if (!@toks) {
		return (-1);
	}

	return (PORK::quote("$cmd $toks[0] :$toks[1]"));
}

sub kill {
	return (irc_cmd("KILL", shift));
}

sub kline {
	return (irc_cmd("KLINE", shift));
}

PORK::bind("^\\", "perl cmdswitch");

PORK::alias("kb", "perl kb");
PORK::alias("bk", "perl kb");
PORK::alias("op", "perl op");
PORK::alias("mop", "perl mop");
PORK::alias("dop", "perl dop");
PORK::alias("mdop", "perl mdop");
PORK::alias("c", "perl chanmode");
PORK::alias("chops", "perl chops");
PORK::alias("nops", "perl nops");
PORK::alias("wii", "perl wii");
PORK::alias("oper", "perl oper");
PORK::alias("umode", "perl umode");
PORK::alias("kill", "perl kill");
PORK::alias("kline", "perl kline");
PORK::alias("list", "quote list");
PORK::alias("unkline", "quote unkline");
PORK::alias("stats", "quote stats");
PORK::alias("knock", "quote knock");
PORK::alias("links", "quote links");
PORK::alias("trace", "quote trace");
PORK::alias("lusers", "quote lusers");
PORK::alias("topic", "chat topic");
