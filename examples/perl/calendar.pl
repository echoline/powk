# -----------------------------------------------------------------------------
# PorkCal -- a calendar for pork.
# 
# This script sets up an alias that prints out a calendar to your pork
# window. The calendar can start weeks with either Sunday or Monday,
# and can be printed with, or without color. For more info, call
#       /cal help
#
# Copyright (C) 2005 Ammon Riley <ammon@rhythm.com>
# -----------------------------------------------------------------------------

package Calendar;
use Date::Calc qw( Calendar );

# -----------------------------------------------------------------------------
# User variables
# -----------------------------------------------------------------------------
my $alias = 'cal';		# What to call the user alias

my %color_defaults = (
    title         => '%G',	# Month and year
    weekday_title => '%g',	# Mon - Fri
    weekend_title => '%C',	# Sat and Sun
    weekday       => undef,	# Weekday dates
    weekend       => '%c',	# Weekend dates
    today         => '%r',	# Today (if it's showing)
    error         => '%r',	# Error message header
    help          => '%g',	# Help
);

my $sunday_first = 1;		# Format of calander
my $colorized = 1;		# Use color or not

# -----------------------------------------------------------------------------
# Main Function. No need to edit below this line.
# -----------------------------------------------------------------------------
sub print_calendar {
    # Be forgiving if the user forgets to use a comma (or is too lazy).
    my ($m, $y);
    if (@_ == 1) { ($m, $y) = split /\s+/, shift; }
    else	 { ($m, $y) = @_		  }

    # Make sure that all colors are defined, in case user decided to
    # use 'undef' to mean 'no color'.
    my %color = ();
    for $color (keys %color_defaults) {
	if (!defined $color_defaults{$color} || $colorized == 0) {
	    $color{$color} = '';
	}
	else {
	    $color{$color} = $color_defaults{$color};
	}
    }

    # Get month/year default values.
    my ($cd, $cm, $cy) = (localtime)[3..5];
    $cm++; $cy += 1900;

    $m = $cm unless $m;
    $y = $cy unless $y;

    ($m, $y) = (lc $m, lc $y);		# In case it's a command.

    # Take care of extra functions -- help, setting color, and format.
    if ($m eq 'help') {
	PORK::echo("$color{help}Usage:\%x");
	PORK::echo("    $color{help}/$alias <month> [<year>]\%x");
	PORK::echo("        <month> is an integer from 1 through 12.");
	PORK::echo("        <year> is a 4 digit year.");
	PORK::echo("        <month> and <year> default to the current date.");
	PORK::echo("    $color{help}/$alias color <on|off|0|1|true|false|toggle>\%x");
	PORK::echo("        set use of color");
	PORK::echo("    $color{help}/$alias format <monday|sunday|toggle>\%x");
	PORK::echo("        which day starts week");
	return -1;
    }
    elsif ($m eq 'color') {
	# We want to accept all the values that pork does.
	my %valid_color = (
	    (map { $_ => 1 } qw( on 1 true )),
	    (map { $_ => 0 } qw( off 0 false )),
	);
	if    (exists $valid_color{$y}) { $colorized = $valid_color{$y}; }
	elsif ($y eq 'toggle')		{ $colorized = !$colorized;	 }
	else {
	    PORK::echo("$color{error}$alias error:\%x bad color value ($y)");
	    return -1;
	}
	return 0;
    }
    elsif ($m eq 'format') {
	if    ($y eq 'toggle') { $sunday_first = !$sunday_first; }
	elsif ($y eq 'monday') { $sunday_first = 0;		 }
	elsif ($y eq 'sunday') { $sunday_first = 1;		 }
	else {
	    PORK::echo("$color{error}$alias error:\%x bad format value ($y)");
	    return -1;
	}
	return 0;
    }

    # Make sure month and year are valid.
    if ($m =~ /\D/ || $m < 1 || $m > 12) {
	PORK::echo("$color{error}$alias error:\%x bad month ($m)");
	return -1;
    }
    if ($y =~ /\D/) {
	PORK::echo("$color{error}$alias error:\%x bad year ($m)");
	return -1;
    }

    # Okay, we're off to the races!
    my @calendar = grep /\S/, split /\n/, Calendar($y, $m, $sunday_first);
    chomp @calendar;

    # Colorize the lines!
    PORK::echo($color{title} . shift(@calendar) . "%x");

    # Next up is the header. Since there's only one space between
    # columns in the header, it's easy to split off the weekend from the
    # weekday.
    my @header = split / /, shift @calendar;
    if ($sunday_first) {
	$header[0] = "$color{weekend_title}$header[0]\%x$color{weekday_title}";
	$header[6] = "\%x$color{weekend_title}$header[6]\%x";
    }
    else {
	$header[0] = "$color{weekday_title}$header[0]";
	$header[5] = "\%x$color{weekend_title}$header[5]";
	$header[6] = "$header[6]\%x";
    }
    PORK::echo(join ' ', @header);

    # Next, we have the dates. Since the row of dates can change depending
    # on the length, we'll make sure that length is always the same, so
    # we don't have to guess where we're supposed to put color codes.
    for (@calendar) {
	@line = split //, sprintf "%-27s", $_;
	if ($sunday_first) {
	    splice @line, -3, 0, "\%x$color{weekend}";
	    splice @line, 3, 0, "\%x$color{weekday}";
	    unshift @line, $color{weekend};
	}
	else {
	    splice @line, -7, 0, "\%x$color{weekend}";
	    unshift @line, $color{weekday};
	}

	# If today's date is in here, then we need to mark that, too.
	# That'll be easier if we put the line back together.
	my $line = join '', @line, '%x';

	if ($m == $cm && $y == $cy) {
	    # We'll need to make sure that if we change the color,
	    # while marking today, that we put it back. Note that 
	    # if the user doesn't have any color for weekdays, then
	    # there may not be a color code, if Monday is the first
	    # day.
	    $line =~ s/(\%.)?([^%]*)\b$cd\b/$1$2$color{today}$cd\%x$1/;
	}

	PORK::echo($line);
    }

    return 0;
}

PORK::alias($alias, 'perl Calendar::print_calendar');

1;
