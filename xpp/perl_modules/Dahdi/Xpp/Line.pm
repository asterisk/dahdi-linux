package Dahdi::Xpp::Line;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2008, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#
use strict;
use Dahdi::Utils;

sub new($$$) {
	my $pack = shift or die "Wasn't called as a class method\n";
	my $xpd = shift or die;
	my $index = shift;
	defined $index or die;
	my $self = {};
	bless $self, $pack;
	$self->{XPD} = $xpd;
	$self->{INDEX} = $index;
	return $self;
}

sub blink($$) {
	my $self = shift;
	my $on = shift;
	my $xpd = $self->xpd;
	my $result = $xpd->xpd_getattr("blink");
	$result = hex($result);
	if(defined($on)) {		# Now change
		my $onbitmask = 1 << $self->index;
		my $offbitmask = $result & ~$onbitmask;

		$result = $offbitmask;
		$result |= $onbitmask if $on;
		$result = $xpd->xpd_setattr("blink", $result);
	}
	return $result;
}

sub create_all($$) {
	my $pack = shift or die "Wasn't called as a class method\n";
	my $xpd = shift || die;
	local $/ = "\n";
	my @lines;
	for(my $i = 0; $i < $xpd->{CHANNELS}; $i++) {
		my $line = Dahdi::Xpp::Line->new($xpd, $i);
		push(@lines, $line);
	}
	$xpd->{LINES} = \@lines;
	if($xpd->type eq 'FXO') {
		my $battery = $xpd->xpd_getattr("fxo_battery");
		die "Missing '$battery' attribute\n" unless defined $battery;
		my @batt = split(/\s+/, $battery);
		foreach my $l (@lines) {
			die unless @batt;
			my $state = shift @batt;
			$l->{BATTERY} = ($state eq '+') ? 1 : 0;
		}
	}
}


1;
