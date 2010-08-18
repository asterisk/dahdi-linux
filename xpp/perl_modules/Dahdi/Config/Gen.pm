package Dahdi::Config::Gen;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2009, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#

=head1 NAME

Dahdi::Config::Gen -- Wrapper class for configuration generators.

=head1 SYNOPSIS

 use Dahdi::Config::Gen qw(is_true);
 my $params = Dahdi::Config::Params->new('the-config-file');
 my $gconfig = Dahdi::Config::Gen->new($params);
 my $num = $gconfig->{'base_exten'};
 my $overlap = is_true($gconfig->{'brint_overlap'});
 $gconfig->dump;	# For debugging
 $gconfig->run_generator('system', {}, @spans);

=head1 DESCRIPTION

The constructor must be given an C<Dahdi::Config::Params> object.
The returned object contains all data required for generation in the
form of a hash.

The constructor maps the C<item()>s from the parameter object into semantic
configuration keys.  E.g: the C<lc_country> item is mapped to C<loadzone> and
C<defaultzone> keys.

The actual generation is done by delegation to one of the generators.
This is done via the C<run_generator()> method which receive the
generator name, a generator specific options hash and a list of
span objects (from C<Dahdi::Span>) for which to generate configuration.

This module contains few helper functions. E.g: C<is_true()>, C<bchan_range()>.

=cut

require Exporter;
@ISA = qw(Exporter);

@EXPORT_OK = qw(is_true);

use strict;

# Parse values as true/false 
sub is_true($) {
	my $val = shift;
	return undef unless defined $val;
	return $val =~ /^(1|y|yes)$/i;
}

sub range_string($$) {
	my ($start, $end) = @_;

	if($start == $end) {
		sprintf "%d", $start;
	} else {
		sprintf "%d-%d", $start, $end;
	}
}

# Generate channel range strings from arrays of chan numbers
# E.g: "63-77,79-93"
sub channo_range(@) {
	my @channos = sort { $a <=> $b } @_;
	my $first_num = $channos[0];
	my $range_start = $first_num;
	my @range;
	my $prev = undef;

	foreach my $c (@channos) {
		my $curr = $c;
		if(!defined($prev)) {
			# First iteration
			$prev = $curr;
		} elsif($curr != $prev + 1) {
			# New range
			push(@range, range_string($range_start, $prev));
			$range_start = $curr;
		}
		$prev = $curr;
	}
	if($prev >= $first_num) {
		# Last range
		push(@range, range_string($range_start, $prev));
	}
	return join(',', @range);
}

# Generate channel range strings from chan objects
# E.g: "63-77,79-93"
sub chan_range(@) {
	my @chans = sort { $a->num <=> $b->num } @_;
	my @channos = map { $_->num } @chans;
	channo_range(@channos);
}

# Generate channel range strings from digital span objects
# E.g: "63-77,79-93"
sub bchan_range($) {
	my $span = shift || die;
	die unless $span->is_digital();
	my $first_chan = ($span->chans())[0];
	my $first_num = $first_chan->num();
	my $bchan_ref = $span->bchan_list();
	my @channos = map { $_ + $first_num } @{$bchan_ref};
	channo_range(@channos);
}

# Returns a channel numbers array from a channel range string
sub parse_chan_range($) {
	my $rangestr = shift;
	$rangestr =~ s/\s*//g;	# Squeeze
	die "Bad characters in '$rangestr'" if $rangestr =~ /[^\d\s,-]/;
	my @ranges = split(/,/, $rangestr);
	my @channos;
	my $last_end;

	foreach my $range (@ranges) {
		my ($start, $end) = split(/-/, $range, 2);
		$end = $start unless defined $end;
		die "Bad characters in '$start'" if $start =~ /\D/;
		die "Bad characters in '$end'" if $end =~ /\D/;
		die "Reversed range $end < $start" if $end < $start;
		die "Channel number < 1" if $start < 1;
		die "New range begins below previous $start <= $last_end" if defined($last_end) && $last_end >= $start;
		for(my $i = $start + 0; $i <= $end; $i++) {
			push(@channos, $i);
		}
		$last_end = $end;
	}
	return sort { $a <=> $b } @channos;
}

sub new($) {
	my $pack = shift || die "$0: Missing package argument";
	my $p = shift || die "$0: Missing parameters argument";

	# Set defaults
	my $fxs_default_start = $p->item('fxs_default_start');
	my $fxo_default_start = $p->item('fxo_default_start');

	my %default_context = (
		FXO	=> $p->item('context_lines'),
		FXS	=> $p->item('context_phones'),
		IN	=> $p->item('context_input'),
		OUT	=> $p->item('context_output'),
		DYN	=> $p->item('context_lines'),
		BRI_TE	=> $p->item('context_lines'),
		BRI_NT	=> $p->item('context_lines'),
		E1_TE	=> $p->item('context_lines'),
		T1_TE	=> $p->item('context_lines'),
		J1_TE	=> $p->item('context_lines'),
		E1_NT	=> $p->item('context_lines'),
		T1_NT	=> $p->item('context_lines'),
		J1_NT	=> $p->item('context_lines'),
		);
	my %default_group = (
		FXO	=> $p->item('group_lines'),
		FXS	=> $p->item('group_phones'),
		IN	=> '',
		OUT	=> '',
		DYN	=> '',
		BRI_TE	=> $p->item('group_lines'),
		BRI_NT	=> $p->item('group_lines'),
		E1_TE	=> $p->item('group_lines'),
		T1_TE	=> $p->item('group_lines'),
		J1_TE	=> $p->item('group_lines'),
		E1_NT	=> $p->item('group_lines'),
		T1_NT	=> $p->item('group_lines'),
		J1_NT	=> $p->item('group_lines'),
		);
	my %default_dahdi_signalling = (
		FXO	=> "fxs$fxo_default_start",
		FXS	=> "fxo$fxs_default_start",
		IN	=> "fxo$fxs_default_start",
		OUT	=> "fxo$fxs_default_start",
		DYN	=> "clear",
		);
	my %default_chan_dahdi_signalling = (
		FXO	=> "fxs_$fxo_default_start",
		FXS	=> "fxo_$fxs_default_start",
		IN	=> "fxo_$fxs_default_start",
		OUT	=> "fxo_$fxs_default_start",
		DYN	=> "auto", # Cheating. Won't really work
		);

	# First complex mapping
	my $gconfig = {
			PARAMETERS	=> $p,
			'loadzone'	=> $p->item('lc_country'),
			'defaultzone'	=> $p->item('lc_country'),
			'context'	=> \%default_context,
			'group'		=> \%default_group,
			'dahdi_signalling'	=> \%default_dahdi_signalling,
			'chan_dahdi_signalling'	=> \%default_chan_dahdi_signalling,
		};
	# Now add trivial mappings
	my @trivial = qw(
		base_exten
		freepbx
		fxs_immediate
		bri_hardhdlc
		bri_sig_style
		r2_idle_bits
		tdm_framing
		echo_can
		brint_overlap
		pri_termtype
		pri_connection_type
		em_signalling
		);
	foreach my $k (@trivial) {
		$gconfig->{$k} = $p->item($k);
	}
	bless $gconfig,$pack;

	return $gconfig;
}

sub run_generator($$@) {
	my $gconfig = shift || die;
	my $name = shift || die "$0: Missing generator name argument";
	my $genopts = shift || die "$0: Missing genopts argument";
	ref($genopts) eq 'HASH' or die "$0: Bad genopts argument";
	my @spans = @_;

	my $module = "Dahdi::Config::Gen::$name";
	#print STDERR "DEBUG: $module\n";
	eval "use $module";
	if($@) {
		die "Failed to load configuration generator for '$name': $@\n";
	}
	my $cfg = $module->new($gconfig, $genopts);
	$cfg->generate(@spans);
}

sub dump($) {
	my $self = shift || die;
	printf STDERR "%s dump:\n", ref $self;
	my $width = 30;
	foreach my $k (sort keys %$self) {
		my $val = $self->{$k};
		my $ref = ref $val;
		#print STDERR "DEBUG: '$k', '$ref', '$val'\n";
		if($ref eq '') {
			printf STDERR "%-${width}s %s\n", $k, $val;
		} elsif($ref eq 'SCALAR') {
			printf STDERR "%-${width}s %s\n", $k, ${$val};
		} elsif($ref eq 'ARRAY') {
			#printf STDERR "%s:\n", $k;
			my $i = 0;
			foreach my $v (@{$val}) {
				printf STDERR "%-${width}s %s\n", "$k\->[$i]", $v;
				$i++;
			}
		} elsif($ref eq 'HASH') {
			#printf STDERR "%s:\n", $k;
			foreach my $k1 (keys %{$val}) {
				printf STDERR "%-${width}s %s\n", "$k\->\{$k1\}", ${$val}{$k1};
			}
		} else {
			printf STDERR "%-${width}s (-> %s)\n", $k, $ref;
		}
	}
}


1;
