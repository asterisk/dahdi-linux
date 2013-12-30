package Dahdi::Xpp::Xbus;
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
use Dahdi::Hardware;
use Dahdi::Xpp::Xpd;

sub xpds($) {
	my $xbus = shift;
	return @{$xbus->{XPDS}};
}

sub by_number($) {
	my $busnumber = shift;
	die "Missing xbus number parameter" unless defined $busnumber;
	my @xbuses = Dahdi::Xpp::xbuses();

	my ($xbus) = grep { $_->num == $busnumber } @xbuses;
	return $xbus;
}

sub by_label($) {
	my $label = shift;
	die "Missing xbus label parameter" unless defined $label;
	my @xbuses = Dahdi::Xpp::xbuses();

	my ($xbus) = grep { $_->label eq $label } @xbuses;
	return $xbus;
}

sub get_xpd_by_number($$) {
	my $xbus = shift;
	my $xpdid = shift;
	die "Missing XPD id parameter" unless defined $xpdid;
	$xpdid = sprintf("%02d", $xpdid);
	my @xpds = $xbus->xpds;
	my ($wanted) = grep { $_->id eq $xpdid } @xpds;
	return $wanted;
}

sub xbus_getattr($$) {
	my $xbus = shift || die;
	my $attr = shift || die;
	$attr = lc($attr);
	my $file = sprintf "%s/%s", $xbus->sysfs_dir, $attr;

	open(F, $file) || die "Failed opening '$file': $!";
	my $val = <F>;
	close F;
	chomp $val;
	return $val;
}

sub read_attrs() {
	my $xbus = shift || die;
	my @attrnames = qw(CONNECTOR LABEL STATUS);
	my @attrs;

	foreach my $attr (@attrnames) {
		my $val = xbus_getattr($xbus, $attr);
		if($attr eq 'STATUS') {
			# Some values are in all caps as well
			$val = uc($val);
		} elsif($attr eq 'CONNECTOR') {
			$val =~ s/^/@/;	# Add prefix
		} elsif($attr eq 'LABEL') {
			# Fix badly burned labels.
			$val =~ s/[[:^print:]]/_/g;
		}
		$xbus->{$attr} = $val;
	}
}

sub transport_type($$) {
	my $xbus = shift || die;
	my $xbus_dir = shift;
	my $transport = "$xbus_dir/transport";
	if(-e "$transport/ep_00") {	# It's USB
		$xbus->{TRANSPORT_TYPE} = 'USB';
	} else {
		warn "Unkown transport in $xbus_dir\n";
		undef $xbus->{TRANSPORT_TYPE};
	}
	return $xbus->{TRANSPORT_TYPE};
}

sub read_xpdnames($) {
	my $xbus_dir = shift or die;
	my $pat = sprintf "%s/[0-9][0-9]:[0-9]:[0-9]", $xbus_dir;
	my @xpdnames;

	#printf STDERR "read_xpdnames(%s): $pat\n", $xbus_dir;
	foreach (glob $pat) {
		die "Bad /sys entry: '$_'" unless m/^.*\/([0-9][0-9]):([0-9]):([0-9])$/;
		my ($busnum, $unit, $subunit) = ($1, $2, $3);
		my $name = sprintf("%02d:%1d:%1d", $1, $2, $3);
		#print STDERR "\t> $_ ($name)\n";
		push(@xpdnames, $name);
	}
	return @xpdnames;
}

sub read_num($) {
	my $self = shift or die;
	my $xbus_dir = $self->sysfs_dir;
	$xbus_dir =~ /.*-(\d\d)$/;
	return $1;
}

sub new($$) {
	my $pack = shift or die "Wasn't called as a class method\n";
	my $parent_dir = shift or die;
	my $entry_dir = shift or die;
	my $xbus_dir = "$parent_dir/$entry_dir";
	my $self = {};
	bless $self, $pack;
	$self->{SYSFS_DIR} = $xbus_dir;
	my $num = $self->read_num;
	$self->{NUM} = $num;
	$self->{NAME} = "XBUS-$num";
	$self->read_attrs;
	# Get transport related info
	my $transport = "$xbus_dir/transport";
	die "OLD DRIVER: missing '$transport'\n" unless -e $transport;
	my $transport_type = $self->transport_type($xbus_dir);
	if(defined $transport_type) {
		my $tt = "Dahdi::Hardware::$transport_type";
		my $hw = $tt->set_transport($self, $xbus_dir);
		#printf STDERR "Xbus::new transport($transport_type): %s\n", $hw->{HARDWARE_NAME};
	}
	my @xpdnames;
	my @xpds;
	@xpdnames = read_xpdnames($self->sysfs_dir);
	foreach my $xpdstr (@xpdnames) {
		my $xpd = Dahdi::Xpp::Xpd->new($self, $xpdstr);
		push(@xpds, $xpd);
	}
	@{$self->{XPDS}} = sort { $a->id <=> $b->id } @xpds;
	return $self;
}

sub pretty_xpds($) {
		my $xbus = shift;
		my @xpds = sort { $a->id <=> $b->id } $xbus->xpds();
		my @xpd_types = map { $_->type } @xpds;
		my $last_type = '';
		my $mult = 0;
		my $xpdstr = '';
		foreach my $curr (@xpd_types) {
			if(!$last_type || ($curr eq $last_type)) {
				$mult++;
			} else {
				if($mult == 1) {
					$xpdstr .= "$last_type ";
				} elsif($mult) {
					$xpdstr .= "$last_type*$mult ";
				}
				$mult = 1;
			}
			$last_type = $curr;
		}
		if($mult == 1) {
			$xpdstr .= "$last_type ";
		} elsif($mult) {
			$xpdstr .= "$last_type*$mult ";
		}
		$xpdstr =~ s/\s*$//;	# trim trailing space
		return $xpdstr;
}

1;
