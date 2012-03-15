package Dahdi::Xpp::Xpd;
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
use Dahdi::Xpp;
use Dahdi::Xpp::Line;

=head1 NAME

Dahdi::Xpp::Xpd - Perl interface to the Xorcom Astribank XPDs (spans)

=head1 SYNOPSIS

  # Listing all Astribanks:
  use Dahdi::Xpp;
  # scans hardware:
  my @xbuses = Dahdi::Xpp::xbuses("SORT_CONNECTOR");
  for my $xbus (@xbuses) {
    print $xbus->name." (".$xbus->label .", ". $xbus->connector .")\n";
    for my $xpd ($xbus->xpds) {
      print " - ".$xpd->fqn,"\n";
    }
  }

=head1 xbus

The parent L<Dahdi::Xpp::Xbus>

=head1 id

The two-digit ID in the Xbus. Normally 0I<x> for digital spans and 
I<x>0 for analog ones (for some digit, I<x>).

=head1 unit

First digit of the ID. Zero-based number of the module inside the
Astribank,

=head1 subunit

Second digit of the ID. Zero-based sub-part inside the module.
Applicable only to digital (BRI/PRI) modules and always 0 for others.

=head1 FQN

Textual name: E.g. C<XPD-10>.

=head1 sysfs_dir

The SysFS directory with information about the module. E.g.
C</sys/bus/astribanks/devices/xbus-00/00:1:0>.

=head1 channels

A list of L<Dahdi::Xpp:Chan> channels of this span. In a scalar context
this will be the number of channels in the span.

=head1 spanno

0 if not registered with Dahdi. Otherwise, the number of the span it is
registered as.

=head1 type

The type of the XPD. One of: C<FXS>, C<FXO>, C<BRI_TE>, C<BRI_NT>,
C<E1>, C<T1>.

=head1 is_bri

True if this XPD is BRI.

=head1 is_pri

True if this XPD is PRI (E1/T1).

=head1 is_digital

True if this XPD is a digital port (BRI / PRI).

=head1 termtype

For a digital span: C<TE> or C<NT>.

=head1 dchan_hardhdlc

For a BRI port: true if the driver with hardhdlc support (rather than
bri_dchan).

=cut

my %file_warned;	# Prevent duplicate warnings about same file.

sub xpd_attr_path($@) {
	my $self = shift || die;
	my $xbus = $self->xbus;
	my ($busnum, $unitnum, $subunitnum, @attr) = (
		$xbus->num,
		$self->unit,
		$self->subunit,
		@_);
	foreach my $attr (@attr) {
		my $file = sprintf "%s/%02d:%1d:%1d/$attr",
		   $xbus->sysfs_dir, $busnum, $unitnum, $subunitnum;
		next unless -f $file;
		return $file;
	}
	return undef;
}

my %attr_missing_warned;	# Prevent duplicate warnings

sub xpd_driver_getattr($$) {
	my $xpd = shift || die;
	my $attr = shift || die;
	$attr = lc($attr);
	my ($busnum, $unitnum, $subunitnum) = ($xpd->xbus->num, $xpd->unit, $xpd->subunit);
	my $file = sprintf "$Dahdi::Xpp::sysfs_xpds/%02d:%1d:%1d/driver/$attr",
			$busnum, $unitnum, $subunitnum;
	if(!defined($file)) {
		warn "$0: xpd_driver_getattr($attr) -- Missing attribute.\n" if
			$attr_missing_warned{$attr};
		return undef;
	}
	open(F, $file) || return undef;
	my $val = <F>;
	close F;
	chomp $val;
	return $val;
}

sub xpd_getattr($$) {
	my $xpd = shift || die;
	my $attr = shift || die;
	$attr = lc($attr);
	my $file = $xpd->xpd_attr_path(lc($attr));

	if(!defined($file)) {
		warn "$0: xpd_getattr($attr) -- Missing attribute.\n" if
			$attr_missing_warned{$attr};
		return undef;
	}
	open(F, $file) || return undef;
	my $val = <F>;
	close F;
	chomp $val;
	return $val;
}

sub xpd_setattr($$$) {
	my $xpd = shift || die;
	my $attr = shift || die;
	my $val = shift;
	$attr = lc($attr);
	my $file = xpd_attr_path($xpd, $attr);
	my $oldval = $xpd->xpd_getattr($attr);
	open(F, ">$file") or die "Failed to open $file for writing: $!";
	print F "$val";
	if(!close(F)) {
		if($! == 17) {	# EEXISTS
			# good
		} else {
			return undef;
		}
	}
	return $oldval;
}

sub blink($$) {
	my $self = shift;
	my $on = shift;
	my $result = $self->xpd_getattr("blink");
	if(defined($on)) {		# Now change
		$self->xpd_setattr("blink", ($on)?"0xFFFF":"0");
	}
	return $result;
}

sub dahdi_registration($$) {
	my $self = shift;
	my $on = shift;
	my $result;
	my $file = $self->xpd_attr_path("span", "dahdi_registration");
	die "$file is missing" unless -f $file;
	# First query
	open(F, "$file") or die "Failed to open $file for reading: $!";
	$result = <F>;
	chomp $result;
	close F;
	if(defined($on) and $on ne $result) {		# Now change
		open(F, ">$file") or die "Failed to open $file for writing: $!";
		print F ($on)?"1":"0";
		if(!close(F)) {
			if($! == 17) {	# EEXISTS
				# good
			} else {
				undef $result;
			}
		}
	}
	return $result;
}

sub xpds_by_spanno() {
	my @xbuses = Dahdi::Xpp::xbuses();
	my @xpds = map { $_->xpds } @xbuses;
	@xpds = grep { $_->spanno } @xpds;
	@xpds = sort { $a->spanno <=> $b->spanno } @xpds;
	my @spanno = map { $_->spanno } @xpds;
	my @idx;
	@idx[@spanno] = @xpds;	# The spanno is the index now
	return @idx;
}

sub new($$$) {
	my $pack = shift or die "Wasn't called as a class method\n";
	my $xbus = shift or die;
	my $xpdstr = shift or die;
	my $sysfsdir = sprintf "%s/%s", $xbus->sysfs_dir, $xpdstr;
	my ($busnum, $unit, $subunit) = split(/:/, $xpdstr);
	my $self = {
		XBUS		=> $xbus,
		ID		=> sprintf("%1d%1d", $unit, $subunit),
		FQN		=> $xbus->name . "/" . "XPD-$unit$subunit",
		UNIT		=> $unit,
		SUBUNIT		=> $subunit,
		SYSFS_DIR	=> $sysfsdir,
		};
	bless $self, $pack;
	my @offhook = split / /, ($self->xpd_getattr('offhook'));
	$self->{CHANNELS} = @offhook;
	my $type = $self->xpd_getattr('type');
	my $span = $self->xpd_getattr('span');
	my $timing_priority = $self->xpd_getattr('timing_priority');
	$self->{SPANNO} = $span;
	$self->{TYPE} = $type;
	$self->{TIMING_PRIORITY} = $timing_priority;
	if($type =~ /BRI_(NT|TE)/) {
		$self->{IS_BRI} = 1;
		$self->{TERMTYPE} = $1;
		$self->{DCHAN_HARDHDLC} = $self->xpd_driver_getattr('dchan_hardhdlc');
	} elsif($type =~ /[ETJ]1/) {
		$self->{IS_PRI} = 1;
		# older drivers may not have 'timing_priority'
		# attribute. Preserve original behaviour:
		if(defined($timing_priority) && ($timing_priority == 0)) {
			$self->{TERMTYPE} = 'NT';
		} else {
			$self->{TERMTYPE} = 'TE';
		}
	}
	$self->{IS_DIGITAL} = ( $self->{IS_BRI} || $self->{IS_PRI} );
	Dahdi::Xpp::Line->create_all($self);
	return $self;
}

#------------------------------------
# static xpd related helper functions
#------------------------------------

# Returns only the telephony XPD's from a list
# of one or more XPD's.
# I.e: Filters-out ECHO cancelers
sub telephony_devs {
	my @devs = grep { $_->channels } @_;
	return @devs;
}

sub format_rank($$) {
	my ($rank, $prio) = @_;
	my $width = 2;
	# 0 is replaced with a character that is sorted *AFTER* numbers.
	$prio = '_' x $width unless defined $prio && $prio;
	return sprintf "%${width}s-%s", $prio, $rank;
}

sub sync_priority_rank($) {
	my $xpd = shift || die;
	my $prio = $xpd->timing_priority;
	# The @rank array is ordered by priority of sync (good to bad)
	# It is used when timing_priority is not defined (analog) or
	# is 0 (NT).
	my @rank = (
		($xpd->is_pri and defined($xpd->termtype) and $xpd->termtype eq 'TE'),
		($xpd->is_bri and defined($xpd->termtype) and $xpd->termtype eq 'TE'),
		($xpd->type eq 'FXO'),
		($xpd->is_pri),
		($xpd->is_bri),
		($xpd->type eq 'FXS'),
		);
	my $i;
	for($i = 0; $i < @rank; $i++) {
		last if $rank[$i];
	}
	return format_rank($i, $prio);
}

# An XPD sync priority comparator for sort()
sub sync_priority_compare() {
	my $rank_a = sync_priority_rank($a);
	my $rank_b = sync_priority_rank($b);
	#print STDERR "DEBUG(rank): $rank_a (", $a->fqn, ") $rank_b (", $b->fqn, ")\n";
	return $rank_a cmp $rank_b;	# The easy case
}

# For debugging: show a list of XPD's with relevant sync info.
sub show_xpd_rank(@) {
	print STDERR "XPD's by rank\n";
	foreach my $xpd (@_) {
		my $type = $xpd->type;
		my $extra = "";
		my $rank = sync_priority_rank($xpd);
		if($xpd->is_digital) {
			$extra .= " termtype " . ($xpd->termtype || "UNKNOWN");
		}
		printf STDERR "%3s %-15s %s\n", $rank, $xpd->fqn, $extra;
	}
}

sub xpds_by_rank(@) {
	my @xpd_prio = sort sync_priority_compare @_;
	#show_xpd_rank(@xpd_prio);
	return @xpd_prio;
}

1;
