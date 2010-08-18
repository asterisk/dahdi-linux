package Dahdi::Xpp::Mpp;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2009, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#
use strict;
use File::Basename;
use Getopt::Std;
BEGIN { my $dir = dirname($0); unshift(@INC, "$dir", "$dir/perl_modules"); }

use Dahdi::Utils;

=head1 NAME

Dahdi::Xpp::Mpp - Perl interface to C<astribank_tool(8)>

=head1 DESCRIPTION

This package uses C<astribank_tool(8)> to collect information
about Astribanks via MPP (Management Processor Protocol).

The binary default location is F</usr/sbin/astribank_tool>. It may be
overridden via module parameter C<astribank_tool=> and the
C<ASTRIBANK_TOOL> environment variable (higher priority).

It may also be set/unset from code via the set_astribank_tool() method.

=head1 METHODS

=head2 mpp_addinfo()

Called with a list of C<Dahdi::Hardware> objects and augment their
data with C<Dahdi::Xpp::Mpp> objects.

This method is the normal external interface of this class.

=head2 new()

Constructor. Receive as parameter an instance of C<Dahdi::Hardware> class
and return a C<Dahdi::Xpp:Mpp> object.

Normally, used indirectly via the mpp_addinfo() method.

=head2 set_astribank_tool()

Override default location of astribank_tool(8). It is legal
to set it to C<undef>.

=head2 showinfo()

Dump an C<Dahdi::Xpp::Mpp> object for debugging.

=cut

my $astribank_tool = '/usr/sbin/astribank_tool';

sub set_astribank_tool($$) {
	my $pack = shift || die;
	$pack eq 'Dahdi::Xpp::Mpp' or die "$0: Called from wrong package? ($pack)";
	my $arg = shift;
	$astribank_tool = $arg;
	#print STDERR "Setting astribank_tool='$astribank_tool'\n";
}

sub import {
	my ($param) = grep(/^astribank_tool=/, @_);
	if(defined $param) {
		$param =~ s/^astribank_tool=//;
		$astribank_tool = $param;
	}
	if(defined $ENV{ASTRIBANK_TOOL}) {
		$astribank_tool = $ENV{ASTRIBANK_TOOL};
	}
}

sub showinfo($$) {
	my $self = shift || die;
	my $prefix = shift || die;

	return unless defined $self;
	foreach my $k (sort keys %{$self}) {
		my $v = $self->{$k};
		if(ref($v) eq 'ARRAY') {
			my @a = @{$v};
			my $i;
			my $ki;
			for($i = 0; $i < @a; $i++) {
				$ki = sprintf "%s[%d]", $k, $i;
				printf "$prefix%-20s %s\n", $ki, $a[$i];
			}
		} else {
			if($k eq 'DEV') {
				printf "$prefix%-20s -> %s\n", $k, $v->hardware_name;
			} else {
				printf "$prefix%-20s %s\n", $k, $v;
			}
		}
	}
}

sub astribank_tool_cmd($@) {
	my $dev = shift || die;
	my @args = @_;
	my $usb_top;

	# Find USB bus toplevel
	$usb_top = '/dev/bus/usb';
	$usb_top = '/proc/bus/usb' unless -d $usb_top;
	die "No USB toplevel found\n" unless -d $usb_top;
	my $name = $dev->priv_device_name();
	die "$0: Unkown private device name" unless defined $name;
	my $path = "$usb_top/$name";
	return ($astribank_tool, '-D', "$path", @args);
}

sub new($$$) {
	my $pack = shift || die;
	my $dev = shift || die;
	my $product = $dev->product;

	return undef unless $dev->is_astribank;
	return undef unless $dev->bus_type eq 'USB';
	return undef unless $product =~ /116./;
	my $mppinfo = {
			DEV	=> $dev,
			HAS_MPP	=> 1,
		};
	bless $mppinfo, $pack;
	#print STDERR "$astribank_tool($path) -- '$product'\n";
	if(! -x $astribank_tool) {
		warn "Could not run '$astribank_tool'\n";
		return $mppinfo;
	}
	return $mppinfo unless $product =~ /116[12]/;
	$mppinfo->{'MPP_TALK'} = 1;
	my @cmd = astribank_tool_cmd($dev, '-Q');
	my $name = $dev->priv_device_name();
	my $dbg_file = "$name";
	$dbg_file =~ s/\W/_/g;
	#$dbg_file = "/tmp/twinstar-debug-$dbg_file";
	$dbg_file = "/dev/null";
	unless(open(F, "@cmd 2> '$dbg_file' |")) {
		warn "Failed running '$astribank_tool': $!";
		return undef;
	}
	local $/ = "\n";
	local $_;
	while(<F>) {
		chomp;
		#printf STDERR "'%s'\n", $_;
		if(s/^INFO:\s*//) {
			$mppinfo->{'PROTOCOL'} = $1 if /^protocol\s+version:\s*(\d+)/i;
		} elsif(s/^EEPROM:\s*//) {
			$mppinfo->{'EEPROM_RELEASE'} = $1 if /^release\s*:\s*([\d\.]+)/i;
			$mppinfo->{'EEPROM_LABEL'} = $1 if /^label\s*:\s*([\w._'-]+)/i;
		} elsif(s/^Extrainfo:\s+:\s*(.+?)$//) {
			$mppinfo->{'EEPROM_EXTRAINFO'} = $1;
		} elsif(s/^Capabilities:\s*TwinStar\s*:\s*(.+?)$//) {
			my $cap = $1;
			$mppinfo->{'TWINSTAR_CAPABLE'} = ($cap =~ /yes/i) ? 1 : 0;
		} elsif(s/^TwinStar:\s*//) {
			$mppinfo->{'TWINSTAR_PORT'} = $1 if /^connected\s+to\s*:\s*usb-(\d+)/i;
			if(s/^USB-(\d+)\s*POWER\s*:\s*//) {
				my $v = ($_ eq 'ON') ? 1 : 0;
				$mppinfo->{'TWINSTAR_POWER'}->[$1] = $v;
			}
			if(s/^Watchdog[^:]+:\s*//) {
				my $v = ($_ eq 'on-guard') ? 1 : 0;
				$mppinfo->{'TWINSTAR_WATCHDOG'} = $v;
			}
			#printf STDERR "\t%s\n", $_;
		} else {
			#printf STDERR "\t%s\n", $_;
		}
	}
	unless(close F) {
		warn "Failed running '$astribank_tool': $!";
		return undef;
	}
	#$mppinfo->showinfo;
	return $mppinfo;
}

sub mpp_setwatchdog($$) {
	my $mppinfo = shift || die;
	my $on = shift;
	die "$0: Bad value '$on'" unless defined($on) && $on =~ /^[0-1]$/;
	my $dev = $mppinfo->dev || die;
	return undef unless defined $mppinfo->mpp_talk;
	my $old = $mppinfo->tws_watchdog;
	my @cmd = astribank_tool_cmd($dev, '-w', $on);
	print STDERR "DEBUG($on): '@cmd'\n";
	system(@cmd);
	die "Running $astribank_tool failed: $?" if $?;
}

sub mpp_jump($) {
	my $mppinfo = shift || die;
	my $dev = $mppinfo->dev || die;
	return undef unless defined $mppinfo->mpp_talk;
	my $port = $mppinfo->twinstar_port;
	$port = ($port == 1) ? 0 : 1;
	die "Unknown TwinStar port" unless defined $port;
	my @cmd = astribank_tool_cmd($dev, '-p', $port);
	system(@cmd);
	die "Running $astribank_tool failed: $?" if $?;
}

sub mpp_addinfo($@) {
	my $pack = shift || die;
	my @devlist = @_;

	foreach my $dev (@devlist) {
		$dev->{MPPINFO} = $pack->new($dev);
	}
}

1;
