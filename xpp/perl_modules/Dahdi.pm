package Dahdi;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2007, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#
use strict;
use Dahdi::Span;

=head1 NAME

Dahdi - Perl interface to Dahdi information

This package allows access from Perl to information about Dahdi
hardware and loaded Dahdi devices.

=head1 SYNOPSIS

  # Listing channels in analog spans:
  use Dahdi;
  # scans system:
  my @spans = Dahdi::spans();
  for my $span (@spans) {
    next if ($span->is_digital);
     $span->num. " - [". $span->type ."] ". $span->name. "\n";
    for my $chan ($span->chans) {
      print " - ".$chan->num . " - [". $chan->type. "] ". $chan->fqn". \n";
    }
  }
=cut

our $virt_base;
our $proc_dahdi_base;
our $proc_usb_base;
our $sys_base;

=head1 spans()

Returns a list of span objects, ordered by span number.

=cut

sub spans() {
	my @spans;

	-d $proc_dahdi_base or return ();
	foreach my $zfile (glob "$proc_dahdi_base/*") {
		next unless ($zfile =~ m{^$proc_dahdi_base/\d+$});
		my $span = Dahdi::Span->new($zfile);
		push(@spans, $span);
	}
	@spans = sort { $a->num <=> $b->num } @spans;
	return @spans;
}

=head1 ENVIRONMENT

If C<DAHDI_VIRT_TOP> is set in the environment, it will be considered
as a path to a directory that holds a dump (copy) of all the required
files from /proc and /sys . You can generate that directory using the
script C<build_tools/dump_sys_state> .

=head1 SEE ALSO

Span objects: L<Dahdi::Span>.

Dahdi channels objects: L<Dahdi::Chan>.

Dahdi hardware devices information: L<Dahdi::Hardware>.

Xorcom Astribank -specific information: L<Dahdi::Xpp>.

=cut

1;
