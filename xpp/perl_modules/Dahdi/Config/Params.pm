package Dahdi::Config::Params;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2009, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#
use strict;

=head1 NAME

Dahdi::Config::Params -- Object oriented representation of F<genconf_parameters> file.

=head1 SYNOPSIS

 use Dahdi::Config::Params;
 my $params = Dahdi::Config::Params->new('the-config-file');
 print $params->item{'some-key'};
 print $params->item{'some-key', NODEFAULTS => 1};
 $params->dump;	# For debugging

=head1 DESCRIPTION

The constructor must be given a configuration file name:

=over 4

=item * Missing file is B<not> an error.

=item * Other opening errors cause a C<die> to be thrown.

=item * The file name is saved as the value of C<GENCONF_FILE> key.

=back

The access to config keys should only be done via the C<item()> method:

=over 4

=item * It contains all hard-coded defaults.

=item * All these values are overriden by directives in the config file.

=item * Calling it with C<NODEFAULTS =E<gt> 1> option, returns C<undef> for keys that
do not appear in the configuration file.

=back

=cut

sub new($$) {
	my $pack = shift || die;
	my $cfg_file = shift || die;
	my $self = {
			GENCONF_FILE	=> $cfg_file,
		};
	bless $self, $pack;
	if(!open(F, $cfg_file)) {
		if(defined($!{ENOENT})) {
			#print STDERR "No $cfg_file. Assume empty config\n";
			return $self; # Empty configuration
		}
		die "$pack: Failed to open '$cfg_file': $!\n";
	}
	#print STDERR "$pack: $cfg_file\n";
	my $array_key;
	while(<F>) {
		my ($key, $val);
		chomp;
		s/#.*$//;
		s/\s+$//;	# trim tail whitespace
		next unless /\S/;
		if(defined $array_key && /^\s+/) {
			s/^\s+//;	# trim beginning whitespace
			push(@{$self->{$array_key}}, $_);
			next; 
		}
		undef $array_key;
		($key, $val) = split(/\s+/, $_, 2);
		$key = lc($key);
		if(! defined $val) {
			$array_key = $key;
			next;
		}
		die "$cfg_file:$.: Duplicate key '$key'\n", if exists $self->{$key};
		$self->{$key} = $val;
	}
	close F;
	return $self;
}

sub item($$@) {
	my $self = shift || die;
	my $key = shift || die;
	my %options = @_;
	my %defaults = (
			base_exten		=> '4000',
			freepbx			=> 'no',	# Better via -F command line
			fxs_immediate		=> 'no',
			fxs_default_start	=> 'ks',
			fxo_default_start	=> 'ks',
			em_signalling		=> 'none',
			lc_country		=> 'us',
			context_lines		=> 'from-pstn',
			context_phones		=> 'from-internal',
			context_input		=> 'astbank-input',
			context_output		=> 'astbank-output',
			group_phones		=> '5',
			group_lines		=> '0',
			brint_overlap		=> 'no',
			bri_sig_style		=> 'bri_ptmp',
			echo_can		=> 'mg2',
			bri_hardhdlc		=> 'auto',
			pri_connection_type	=> 'PRI',
			r2_idle_bits		=> '1101',
			tdm_framing		=> 'esf',
			'pri_termtype'		=> [ 'SPAN/* TE' ],
		);
	return $self->{$key} if exists($self->{$key}) or $options{NODEFAULTS};
	return $defaults{$key};
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

