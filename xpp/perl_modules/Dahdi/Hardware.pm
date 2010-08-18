package Dahdi::Hardware;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2007, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#
use strict;

=head1 NAME

Dahdi::Hardware - Perl interface to a Dahdi devices listing


  use Dahdi::Hardware;
  
  my $hardware = Dahdi::Hardware->scan; 
  
  # mini dahdi_hardware:
  foreach my $device ($hardware->device_list) {
    print "Vendor: device->{VENDOR}, Product: $device->{PRODUCT}\n"
  }

  # let's see if there are devices without loaded drivers, and sugggest
  # drivers to load:
  my @to_load = ();
  foreach my $device ($hardware->device_list) {
    if (! $device->{LOADED} ) {
      push @to_load, ($device->${DRIVER});
    }
  }
  if (@to_load) {
    print "To support the extra devices you probably need to run:\n"
    print "  modprobe ". (join ' ', @to_load). "\n";
  }


This module provides information about available Dahdi devices on the
system. It identifies devices by (USB/PCI) bus IDs.


=head1 Device Attributes

As usual, object attributes can be used in either upp-case or
lower-case, or lower-case functions.

=head2 bus_type

'PCI' or 'USB'.


=head2 description

A one-line description of the device.


=head2 driver

Name of a Dahdi device driver that should handle this device. This is
based on a pre-made list.


=head2 vendor, product, subvendor, subproduct

The PCI and USB vendor ID, product ID, sub-vendor ID and sub-product ID.
(The standard short lspci and lsusb listings show only vendor and
product IDs).


=head2 loaded

If the device is handled by a module - the name of the module. Else -
undef.


=head2 priv_device_name

A string that shows the "location" of that device on the bus.


=head2 is_astribank

True if the device is a Xorcom Astribank (which may provide some extra
attributes).

=head2 serial

(Astribank-specific attrribute) - the serial number string of the
Astribank.

=cut
#
# A global hardware handle
#

my %hardware_list = (
			'PCI'	=> [],
			'USB'	=> [],
		);


sub new($$) {
	my $pack = shift || die "Wasn't called as a class method\n";
	my $name =  shift || die "$0: Missing device name";
	my $type =  shift || die "$0: Missing device type";
	my $dev = {};
	$dev->{'BUS_TYPE'} = $type;
	$dev->{IS_ASTRIBANK} = 0 unless defined $dev->{'IS_ASTRIBANK'};
	$dev->{'HARDWARE_NAME'} = $name;
	return $dev;
}

=head1 device_list()

Returns a list of the hardware devices on the system.

You must run scan() first for this function to run meaningful output.

=cut

sub device_list($) {
	my $pack = shift || die;
	my @types = @_;
	my @list;

	@types = qw(USB PCI) unless @types;
	foreach my $t (@types) {
		my $lst = $hardware_list{$t};
		@list = ( @list, @{$lst} );
	}
	return @list;
}

sub device_by_hwname($$) {
	my $pack = shift || die;
	my $name = shift || die;
	my @list = device_list('localcall');

	my @good = grep { $_->hardware_name eq $name } @list;
	return undef unless @good;
	@good > 1 && die "$pack: Multiple matches for '$name': @good";
	return $good[0];
}

=head1 drivers()

Returns a list of drivers (currently sorted by name) that are used by
the devices in the current system (regardless to whether or not they are
loaded.

=cut

sub drivers($) {
	my $self = shift || die;
	my @devs = device_list('localcall');
	my @drvs = map { $_->{DRIVER} } @devs;
	# Make unique
	my %drivers;
	@drivers{@drvs} = 1;
	return sort keys %drivers;
}


=head1 scan()

Scan the system for Dahdi devices (PCI and USB). Returns nothing but
must be run to initialize the module.

=cut

my $hardware_scanned;

sub scan($) {
	my $pack = shift || die;

	return if $hardware_scanned++;
	foreach my $type (qw(PCI USB)) {
		eval "use Dahdi::Hardware::$type";
		die $@ if $@;
		$hardware_list{$type} = [ "Dahdi::Hardware::$type"->scan_devices ];
	}
}

=head1 rescan

Rescan for devices. In case new devices became available since the script
has started.

=cut

sub rescan($) {
	my $pack = shift || die;

	$hardware_scanned = 0;
	$pack->scan();
}

sub import {
	Dahdi::Hardware->scan unless grep(/\bnoscan\b/i, @_);
}

sub showall {
	my $pack = shift || die;
	my @devs;

	my $printer = sub {
			my $title = shift;
			my @devs = @_;

			return unless @devs;
			printf "%s:\n", $title;
			foreach my $dev (@devs) {
				printf "\t%s\n", $dev->hardware_name;
				foreach my $k (sort keys %{$dev}) {
					my $v = $dev->{$k};
					if($k eq 'MPPINFO') {
						printf "\t\tMPPINFO:\n";
						eval "use Dahdi::Xpp::Mpp";
						die $@ if $@;
						$v->showinfo("\t\t  ");
					} else {
						printf "\t\t%-20s %s\n", $k, $v;
					}
				}
			}
		};
	foreach my $type (qw(USB PCI)) {
		my $lst = $hardware_list{$type};
		&$printer("$type devices", @{$lst});
	}
}

1;
