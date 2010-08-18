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

my %file_warned;	# Prevent duplicate warnings about same file.

sub xbus_attr_path($$) {
	my ($busnum, @attr) = @_;
	foreach my $attr (@attr) {
		my $file = sprintf "$Dahdi::Xpp::sysfs_astribanks/xbus-%02d/$attr", $busnum;
		unless(-f $file) {
			my $procfile = sprintf "$Dahdi::proc_xpp_base/XBUS-%02d/$attr", $busnum;
			warn "$0: warning - OLD DRIVER: missing '$file'. Fall back to '$procfile'\n"
				unless $file_warned{$attr}++;
			$file = $procfile;
		}
		next unless -f $file;
		return $file;
	}
	return undef;
}

sub xbus_getattr($$) {
	my $xbus = shift || die;
	my $attr = shift || die;
	$attr = lc($attr);
	my $file = xbus_attr_path($xbus->num, lc($attr));

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

sub read_xpdnames_old($) {
	my $xbus_num = shift || die;
	my $pat = sprintf "$Dahdi::proc_xpp_base/XBUS-%02d/XPD-[0-9][0-9]", $xbus_num;
	my @xpdnames;

	#print STDERR "read_xpdnames_old($xbus_num): $pat\n";
	foreach (glob $pat) {
		die "Bad /proc entry: '$_'" unless /^.*XPD-([0-9])([0-9])$/;
		my $name = sprintf("%02d:%1d:%1d", $xbus_num, $1, $2);
		#print STDERR "\t> $_ ($name)\n";
		push(@xpdnames, $name);
	}
	return @xpdnames;
}

sub read_xpdnames($) {
	my $xbus_num = shift || die;
	my $xbus_dir = "$Dahdi::Xpp::sysfs_astribanks/xbus-$xbus_num";
	my $pat = sprintf "%s/xbus-%02d/[0-9][0-9]:[0-9]:[0-9]", $Dahdi::Xpp::sysfs_astribanks, $xbus_num;
	my @xpdnames;

	#print STDERR "read_xpdnames($xbus_num): $pat\n";
	foreach (glob $pat) {
		die "Bad /sys entry: '$_'" unless m/^.*\/([0-9][0-9]):([0-9]):([0-9])$/;
		my ($busnum, $unit, $subunit) = ($1, $2, $3);
		my $name = sprintf("%02d:%1d:%1d", $1, $2, $3);
		#print STDERR "\t> $_ ($name)\n";
		push(@xpdnames, $name);
	}
	return @xpdnames;
}

my $warned_notransport = 0;

sub new($$) {
	my $pack = shift or die "Wasn't called as a class method\n";
	my $num = shift;
	my $xbus_dir = "$Dahdi::Xpp::sysfs_astribanks/xbus-$num";
	my $self = {
		NUM		=> $num,
		NAME		=> "XBUS-$num",
		SYSFS_DIR	=> $xbus_dir,
		};
	bless $self, $pack;
	$self->read_attrs;
	# Get transport related info
	my $transport = "$xbus_dir/transport";
	my $transport_type = $self->transport_type($xbus_dir);
	if(defined $transport_type) {
		my $tt = "Dahdi::Hardware::$transport_type";
		my $hw = $tt->set_transport($self, $xbus_dir);
		#printf STDERR "Xbus::new transport($transport_type): %s\n", $hw->{HARDWARE_NAME};
	}
	my @xpdnames;
	my @xpds;
	if(-e $transport) {
		@xpdnames = read_xpdnames($num);
	} else {
		@xpdnames = read_xpdnames_old($num);
		warn "$0: warning - OLD DRIVER: missing '$transport'. Fall back to /proc\n"
			unless $warned_notransport++;
	}
	foreach my $xpdstr (@xpdnames) {
		my ($busnum, $unit, $subunit) = split(/:/, $xpdstr);
		my $procdir = "$Dahdi::proc_xpp_base/XBUS-$busnum/XPD-$unit$subunit";
		my $xpd = Dahdi::Xpp::Xpd->new($self, $unit, $subunit, $procdir, "$xbus_dir/$xpdstr");
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
