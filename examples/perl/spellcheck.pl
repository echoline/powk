#
# spellcheck.pl - spell checking module for pork (http://ojnk.sourceforge.net/)
#
# Author: Justin Chouinard <jvchouinard@yahoo.com>
# AIM: jvcEFNet
#

use Text::Aspell;

my $binding = 'meta-s';

my $unload_refnum;

sub setup {
  PORK::bind($binding, 'perl spell_check');

  $unload_refnum = PORK::event_add("UNLOAD", "unload_handler");
  if (!defined $unload_refnum) {
    PORK::err_msg("Error setting up unload handler.");
    return (-1);
  }

  return(0);
}

sub unload_handler {
  PORK::unbind($binding);
}

sub spell_check {
  local $_ = PORK::input_get_data();

  # Create a new instance of ASPell
  my $speller = Text::Aspell->new;

  # Set each mispelled word to red
  @results = map { $speller->check($_) ? $_ : "%R$_%x"; } split(/\s+/);

  PORK::echo("spell-check: @results");

  return(0);
}

setup();
