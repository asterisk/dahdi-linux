package XppConfig;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2008, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#
use strict;

my $conf_file = "/etc/dahdi/xpp.conf";

sub import {
	my $pack = shift || die "Import without package?";
	my $init_dir = shift || die "$pack::import -- missing init_dir parameter";
	my $local_conf = "$init_dir/xpp.conf";
	$conf_file = $local_conf if -r $local_conf;
}

sub read_config($) {
	my $opts = shift || die;

	open(F, $conf_file) || return ();
	while(<F>) {
		chomp;
		s/#.*//;	# strip comments
		next unless /\S/;
		s/\s*$//;	# Trim trailing whitespace
		my ($key, $value) = split(/\s+/, $_, 2);
		$opts->{$key} = $value;
	}
	close F;
	$opts->{'xppconf'} = $conf_file;
	return %{$opts};
}

1;
