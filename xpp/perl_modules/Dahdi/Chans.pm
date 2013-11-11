package Dahdi::Chans;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2007, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#
use strict;
use Dahdi::Utils;

=head1 NAME

Dahdi::Chans - Perl interface to a Dahdi channel information

This package allows access from perl to information about a Dahdi
channel. It is part of the Dahdi Perl package.

=head1 alarms()

In an array context returns a list of alarm strings (RED, BLUE, etc.)
for this channel (an empty list == false if there are no alarms).
In scalar context returns the number of alarms for a specific channel.

=head1 battery()

Returns 1 if channel reports to have battery (A remote PBX connected to
an FXO port), 0 if channel reports to not have battery and C<undef>
otherwise.

Currently only wcfxo and Astribank FXO modules report battery. For the
rest of the channels 

=head1 fqn()

(Fully Qualified Name) Returns the full "name" of the channel.

=head1 index()

Returns the number of this channel (in the span).

=head1 num()

Returns the number of this channel as a Dahdi channel.

=head signalling()

Returns the signalling set for this channel through /etc/dahdi/system.conf .
This is always empty before dahdi_cfg was run. And shows the "other" type
for FXS and for FXO.

=head1 span()

Returns a reference to the span to which this channel belongs.

=head1 type()

Returns the type of the channel: 'FXS', 'FXO', 'EMPTY', etc.

=cut

my @alarm_types = qw(BLUE YELLOW RED LOOP RECOVERING NOTOPEN);

# Taken from dahdi-base.c
my @sigtypes = (
	"FXSLS",
	"FXSKS",
	"FXSGS",
	"FXOLS",
	"FXOKS",
	"FXOGS",
	"E&M-E1",
	"E&M",
	"Clear",
	"HDLCRAW",
	"HDLCFCS",
	"HDLCNET",
	"Hardware-assisted HDLC",
	"MTP2",
	"Slave",
	"CAS",
	"DACS",
	"DACS+RBS",
	"SF (ToneOnly)",
	"Unconfigured",
	"Reserved"
	);

sub new($$$$$$) {
	my $pack = shift or die "Wasn't called as a class method\n";
	my $span = shift or die "Missing a span parameter\n";
	my $index = shift;
	my $line = shift or die "Missing an input line\n";
	defined $index or die "Missing an index parameter\n";
	my $self = {
			'SPAN' => $span,
			'INDEX' => $index,
		};
	bless $self, $pack;
	my ($num, $fqn, $rest) = split(/\s+/, $line, 3);
	$num or die "Missing a channel number parameter\n";
	$fqn or die "Missing a channel fqn parameter\n";
	my $signalling = '';
	my @alarms = ();
	my $info = '';
	if(defined $rest) {
		# remarks in parenthesis (In use), (no pcm)
		while($rest =~ s/\s*(\([^)]+\))\s*/ /) {
			$info .= " $1";
		}
		# Alarms
		foreach my $alarm (@alarm_types) {
			if($rest =~ s/\s*(\b${alarm}\b)\s*/ /) {
				push(@alarms, $1);
			}
		}
		foreach my $sig (@sigtypes) {
			if($rest =~ s/^\Q$sig\E/ /) {
				$signalling = $sig;
				last;
			}
		}
		warn "Unrecognized garbage '$rest' in $fqn\n"
			if $rest =~ /\S/;
	}
	$self->{NUM} = $num;
	$self->{FQN} = $fqn;
	$self->{SIGNALLING} = $signalling;
	$self->{ALARMS} = \@alarms;
	$self->{INFO} = $info;
	my $type;
	if($fqn =~ m|\bXPP_(\w+)/.*$|) {
		$type = $1;		# An Astribank
	} elsif ($fqn =~ m{\bWCFXO/.*}) {
		$type = "FXO"; # wcfxo - x100p and relatives.
		# A single port card. The driver issue RED alarm when
		# There's no better
		$self->{BATTERY} = !($span->description =~ /\bRED\b/);
	} elsif ($fqn =~ m{\bFXS/.*}) {
		$type = "FXS"; # likely Rhino
	} elsif ($fqn =~ m{\bFXO/.*}) {
		$type = "FXO"; # likely Rhino
	} elsif ($fqn =~ m{---/.*}) {
		$type = "EMPTY"; # likely Rhino, empty slot.
	} elsif ($fqn =~ m{\b(WCTE|TE[24]|WCT1|WCT13x|Tor2|TorISA|WP[TE]1|cwain[12]|R[124]T1|AP40[124]|APE40[124])/.*}) {
		# TE[24]: Digium wct4xxp
		# WCT1: Digium single span card drivers?
		# Tor2: Tor PCI cards
		# TorISA: ISA ones (still used?) 
		# WP[TE]1: Sangoma. TODO: this one tells us if it is TE or NT.
		# cwain: Junghanns E1 card.
		# R[124]: Rhino r1t1/rxt1 cards
		# AP40[124]: Aligera AP40X cards
		# APE40[124]: Aligera APE40X cards
		$type = "PRI";
	} elsif ($fqn =~ m{\b(WCBRI|B4|ZTHFC\d*|ztqoz\d*)/.*}) {
		# WCBRI: The Digium Hx8 series cards with BRI module.
		# B4: The Digium wcb4xxp DAHDI driver
		# ZTHFC: HFC-s single-port card (zaphfc/vzaphfc)
		# ztqoz: qozap (Junghanns) multi-port HFC card
		$type = "BRI";
        } elsif ($fqn =~ m{\bDYN/.*}) {
                # DYN : Dynamic span (TDMOE)
                $type = "DYN"
	} elsif ($fqn =~ m{\bztgsm/.*}) {
		# Junghanns GSM card
		$type = "GSM";
	} elsif($signalling ne '') {
		$type = 'FXO' if $signalling =~ /^FXS/;
		$type = 'FXS' if $signalling =~ /^FXO/;
	} else {
		$type = $self->probe_type();
	}
	$self->type($type);
	$self->span()->type($type)
		if ! defined($self->span()->type()) ||
			$self->span()->type() eq 'UNKNOWN';
	return $self;
}

=head1 probe_type()

In the case of some cards, the information in /proc/dahdi is not good
enough to tell the type of each channel. In this case an extra explicit
probe is needed.

Currently this is implemented by using some invocations of dahdi_cfg(8).

It may later be replaced by dahdi_scan(8).

=cut

my $dahdi_cfg = $ENV{DAHDI_CFG} || '/usr/sbin/dahdi_cfg';
sub probe_type($) {
	my $self = shift;
	my $fqn = $self->fqn;
	my $num = $self->num;
	my $type;

	if($fqn =~ m:WCTDM/|WRTDM/|OPVXA1200/:) {
		my %maybe;

		undef %maybe;
		foreach my $sig (qw(fxo fxs)) {
			my $cmd = "echo ${sig}ks=$num | $dahdi_cfg -c /dev/fd/0";

			$maybe{$sig} = system("$cmd >/dev/null 2>&1") == 0;
		}
		if($maybe{fxo} and $maybe{fxs}) {
			$type = 'EMPTY';
		} elsif($maybe{fxo}) {
			$type = 'FXS';
		} elsif($maybe{fxs}) {
			$type = 'FXO';
		} else {
			$type = 'EMPTY';
		}
	} else {
		$type = $self->type;
	}
	return $type;
}

sub battery($) {
	my $self = shift or die;
	my $span = $self->span or die;

	return undef unless defined $self->type && $self->type eq 'FXO';
	return $self->{BATTERY} if defined $self->{BATTERY};

	my $xpd = Dahdi::Xpp::xpd_of_span($span);
	my $index = $self->index;
	return undef if !$xpd;

	# It's an XPD (FXO)
	my @lines = @{$xpd->lines};
	my $line = $lines[$index];
	return $line->battery;
}

sub alarms($) {
	my $self = shift or die;
	my @alarms = @{$self->{ALARMS}};

	return @alarms;
}

sub blink($$) {
	my $self = shift or die;
	my $on = shift;
	my $span = $self->span or die;

	my $xpd = Dahdi::Xpp::xpd_of_span($span);
	my $index = $self->index;
	return undef if !$xpd;

	my @lines = @{$xpd->lines};
	my $line = $lines[$index];
	return $line->blink($on);
}


1;
