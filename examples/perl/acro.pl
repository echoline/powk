#
# Copyright (C) 2003 Ryan McCabe <ryan@numb.org>
#
# Load this to play acro in acro/4. The rules are easy to figure out.
#

my $rounds = 10;
my $chatroom = "acro/4";

my $letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
my @weights = (68, 59, 87, 56, 38, 37, 25, 26, 41, 9, 10, 24, 40, 15, 19, 67, 6, 56, 100, 37, 17, 12, 19, 3, 7, 5);
my $max_len = 7;
my $min_len = 4;

my $voting_time = 60;
my $pause_time = 30;

#
# 0 - no game
# 1 - acro round
# 2 - voting round
#

my $game_state = 0;

my $cur_round_tl = 0;
my $cur_round_num = 0;
my $cur_round_acro = "";

my $num_players;
my $num_voted;
my @players;
my %players_round = ();
my %players_game = ();

my $not_enough_players = 0;
my $unload_refnum;
my $chat_refnum;
my $im_refnum;
my $warn_refnum;
my $timer_refnum = -1;

sub acro_init {
	if (PORK::chat_join($chatroom) != 0) {
		PORK::err_msg("Error joining $chatroom");
		return;
	}

	$unload_refnum = PORK::event_add("UNLOAD", "acro_unload");
	if (!defined $unload_refnum) {
		PORK::err_msg("Error setting things up (unload).");
		return;
	}

	$warn_refnum = PORK::event_add("RECV_WARN", "warn_handler");
	if (!defined $warn_refnum) {
		PORK::event_del_refnum($unload_refnum);
		PORK::err_msg("Error setting things up (warn listener).");
		return;
	}

	$chat_refnum = PORK::event_add("RECV_CHAT_MSG", "acro_chat_handler");
	if (!defined $chat_refnum) {
		PORK::event_del_refnum($unload_refnum);
		PORK::event_del_refnum($warn_refnum);
		PORK::err_msg("Error setting things up (chat listener).");
		return;
	}

	$im_refnum = PORK::event_add("RECV_IM", "acro_im_handler");
	if (!defined $im_refnum) {
		PORK::event_del_refnum($unload_refnum);
		PORK::event_del_refnum($warn_refnum);
		PORK::event_del_refnum($chat_refnum);
		PORK::err_msg("Error setting things up (IM listener)");
		return;
	}

	# So blocking people works.
	PORK::privacy_mode(4);
}

sub warn_handler {
	my ($idiot, $new_level, $sn) = @_;

	PORK::buddy_add_block($idiot, $sn);
	return (0);
}

sub acro_unload {
	PORK::event_del_refnum($chat_refnum);
	PORK::event_del_refnum($im_refnum);
	PORK::event_del_refnum($unload_refnum);
	PORK::event_del_refnum($warn_refnum);

	if ($timer_refnum >= 0) {
		PORK::timer_del_refnum($timer_refnum);
	}
}

sub acro_chat_handler {
	my ($name, $user, $msg, $acct) = @_;

	if ($game_state == 0 &&
		$name eq $chatroom &&
		$msg eq "acro2 start")
	{
		PORK::chat_send($name, "[acro] New game started by $user");
		acro_new_round();
	}

	return (0);
}

sub acro_im_handler {
	my ($ouser, $auto, $msg, $acct) = @_;
	my $user;

	$user = acro_normalize($ouser);
	if (substr($msg, 0, 5) eq "acro ") {
		if ($game_state == 1) {
			my $acro = substr($msg, 5);

			if (acro_ok($acro)) {
				if (defined $players_round{$user}) {
					$players_round{$user}{'acro'} = $acro;
					PORK::send_msg($user, "Your acro was changed to: $acro");
				} else {
					$players_round{$user} = {
						"name" => $ouser,
						"acro" => $acro,
						"num" => $num_players + 1,
						"voted" => 0,
						"votes" => 0
					};

					$players[$num_players++] = \$players_round{$user};
					PORK::send_msg($user,
						"Your acro is set to: $acro.  You are player number $num_players.");
				}
			} else {
				PORK::send_msg($user, "Invalid acro: $acro.");
			}
		} elsif ($game_state == 2) {
			if (defined $players_round{$user}) {
				my $vote = int(substr($msg, 5));

				if ($vote > 0 && $vote <= $num_players) {
					if ($vote == $players_round{$user}{'num'}) {
						PORK::send_msg($user,
							"Don't try it, bin Laden, you can't vote for yourself.");
					} else {
						if ($players_round{$user}{'voted'} > 0) {
							my $old_vote = $players_round{$user}{'voted'};

							${$players[$old_vote - 1]}->{'votes'}--;
							${$players[$vote - 1]}->{'votes'}++;
							$players_round{$user}{'voted'} = $vote;

							PORK::send_msg($user,
								"Your vote was changed to $vote");
						} else {
							${$players[$vote - 1]}->{'votes'}++;
							$players_round{$user}{'voted'} = $vote;

							PORK::send_msg($user,
								"Your vote is set to $vote");

							$num_voted++;
							if ($num_voted == $num_players) {
								PORK::timer_del_refnum($timer_refnum);
								acro_timeup_vote();
							}
						}
					}
				} else {
					PORK::send_msg($user, "Invalid vote: $vote");
				}
			}
		}
	}

	return (0);
}

sub acro_normalize {
	my $name = lc(shift);

	$name =~ s/ //g;
	return ($name);
}

sub acro_generate {
	my $acro_len = 0;
	my $i = 0;
	my $acro = "";

	while ($acro_len < $min_len) {
		$acro_len = rand($max_len);
	}

	while ($i < $acro_len) {
		my $pos = rand(length($letters));
		my $letter = substr($letters, $pos, 1);

		if ($weights[$pos] >= int(rand(100))) {
			$acro .= $letter;
			$i++;
		}
	}

	return ($acro);
}

sub acro_ok {
	my $acro = shift;
	my @tokens = ();
	my $i;
	my $acrolen = length($cur_round_acro);

	$acro =~ s/^\s+//;

	@tokens = split /[ \t]+/,$acro;
	if (!@tokens || scalar(@tokens) != $acrolen) {
		return (0);
	}

	for ($i = 0 ; $i < $acrolen ; $i++) {
		my $token_letter = uc(substr($tokens[$i], 0, 1));
		my $acro_letter = substr($cur_round_acro, $i, 1);

		if ($token_letter ne $acro_letter) {
			if ($token_letter eq "\"" ||
				$token_letter eq "'" ||
				$token_letter eq "(")
			{
				$token_letter = uc(substr($tokens[$i], 1, 1));

				if ($token_letter eq $acro_letter) {
					next;
				}
			}

			return (0);
		}
	}

	return (1);
}

sub acro_halftime_vote {
	my $time_left = $voting_time / 2;

	PORK::chat_send($chatroom,
		"[acro] $time_left seconds left to vote.");

	$timer_refnum = PORK::timer_add($time_left, 1, "perl acro_timeup_vote");
}

sub acro_timeup_vote {
	my $i;
	my $msg;
	my @standings;

	PORK::chat_send($chatroom,
		"[acro] TIME UP!  Results for $cur_round_num");

	for ($i = 1 ; $i <= $num_players ; $i++) {
		my $num_votes = ${$players[$i - 1]}->{'votes'};
		my $cur_name = ${$players[$i - 1]}->{'name'};
		my $nname = acro_normalize($cur_name);

		select(undef, undef, undef, 0.70);

		$msg = "[acro] #$i by $cur_name got $num_votes vote";
		if ($num_votes != 1) {
			$msg .= "s";
		}

		if (${$players[$i - 1]}->{'voted'} < 1) {
			$msg .= ", but $cur_name didn't vote, and thus, gets ZERO points.";
			PORK::chat_send($chatroom, $msg);
			next;
		}

		$msg .= ".";
		PORK::chat_send($chatroom, $msg);

		if ($num_votes < 1) {
			next;
		}

		if (defined $players_game{$nname}) {
			$players_game{$nname}{'votes'} += $num_votes;
		} else {
			$players_game{$nname} = {
				"name" => $cur_name,
				"votes" => $num_votes
			};
		}
	}

	@standings = reverse sort { $players_game{$a}{'votes'} <=> $players_game{$b}{'votes'} } keys %players_game;

	if (@standings) {
		$msg = "[acro] Current scores:";
		foreach $i (@standings) {
			$msg .= " $players_game{$i}{'name'}: $players_game{$i}{'votes'},";
		}
		chop($msg);

		select(undef, undef, undef, 0.80);
		PORK::chat_send($chatroom, $msg);
	}

	select(undef, undef, undef, 0.80);

	if ($cur_round_num >= $rounds) {
		acro_game_over();
	} else {
		PORK::chat_send($chatroom,
			"[acro] $pause_time seconds until the next round begins.");

		$timer_refnum = PORK::timer_add($pause_time, 1, "perl acro_new_round");
	}
}

sub acro_timeup {
	my $i;

	if ($num_players < 2) {
		if ($not_enough_players > 0) {
			PORK::chat_send($chatroom,
				"[acro] Nobody wants to play.  GAME OVER.");
			acro_game_over();
			return;
		}

		PORK::chat_send($chatroom,
			"[acro] Not enough players.  Extending $cur_round_tl seconds.");

		$not_enough_players++;
		$timer_refnum = PORK::timer_add($cur_round_tl, 1, "perl acro_halftime");

		return;
	}

	PORK::chat_send($chatroom,
		"[acro] TIME UP!  Submissions for Round $cur_round_num");

	for ($i = 1 ; $i <= $num_players ; $i++) {
		select(undef, undef, undef, 0.70);

		PORK::chat_send($chatroom,
			" #$i: ${$players[$i - 1]}->{'acro'}");
	}

	select(undef, undef, undef, 0.60);

	PORK::chat_send($chatroom,
		"[acro] You have $voting_time seconds to vote.  Send me an IM of the form 'acro < 1 - $num_players >' to vote.");

	$game_state++;
	$timer_refnum = PORK::timer_add($voting_time / 2, 1,
		"perl acro_halftime_vote");
}

sub acro_halftime {
	my $time_left = $cur_round_tl / 2;

	PORK::chat_send($chatroom, "[acro] $time_left seconds left!");
	$timer_refnum = PORK::timer_add($time_left, 1, "perl acro_timeup");
}

sub acro_game_over {
	my @standings = reverse sort { $players_game{$a}{'votes'} <=> $players_game{$b}{'votes'} } keys %players_game;

	if (@standings) {
		my $msg = "[acro] Final scores: ";
		my $winning_votes;
		my $winners = "";
		my $num_winners = 0;
		my $s = "";
		my $i;

		foreach $i (@standings) {
			$msg .= " $players_game{$i}{'name'}: $players_game{$i}{'votes'},";
		}
		chop($msg);

		PORK::chat_send($chatroom, $msg);

		$winning_votes = $players_game{$standings[0]}{'votes'};
		for ($i = 0 ; $i < $num_players ; $i++) {
			if ($players_game{$standings[$i]}{'votes'} == $winning_votes) {
				$num_winners++;
				$winners .= " $players_game{$standings[$i]}{'name'},";
			}
		}
		chop($winners);

		if ($num_winners > 1) {
			$s = "s";
			$winners .= " (tie)"
		}

		select(undef, undef, undef, 0.60);

		PORK::chat_send($chatroom,
			"[acro] Winner$s:$winners with $winning_votes votes.");
	}

	$timer_refnum = -1;
	$not_enough_players = 0;
	$game_state = 0;

	$cur_round_acro = "";
	$cur_round_num = 0;
	$cur_round_tl = 0;

	$num_voted = 0;
	$num_players = 0;

	@players = ();
	%players_game = ();
	%players_round = ();
}

sub acro_new_round {
	my $time_limit;

	$not_enough_players = 0;
	$num_voted = 0;
	$num_players = 0;
	@players = ();
	%players_round = ();

	$game_state = 1;
	$cur_round_acro = acro_generate();
	$cur_round_tl = 60 + 10 * (length($cur_round_acro) - 4);

	$cur_round_num++;

	PORK::chat_send($chatroom,
		"[$cur_round_num/$rounds] The acro for this round is $cur_round_acro.  You have $cur_round_tl seconds.  Send me an IM of the form 'acro < YOUR ACRO >' to play.");

	$timer_refnum = PORK::timer_add($cur_round_tl / 2, 1, "perl acro_halftime");
}

acro_init();
