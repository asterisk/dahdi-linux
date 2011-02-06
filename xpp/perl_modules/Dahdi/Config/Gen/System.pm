package Dahdi::Config::Gen::System;
use strict;

use Dahdi::Config::Gen qw(is_true);

sub new($$$) {
	my $pack = shift || die;
	my $gconfig = shift || die;
	my $genopts = shift || die;
	my $file = $ENV{DAHDI_CONF_FILE} || "/etc/dahdi/system.conf";
	my $self = {
			FILE	=> $file,
			GCONFIG	=> $gconfig,
			GENOPTS	=> $genopts,
		};
	bless $self, $pack;
	return $self;
}

my $bri_te_last_timing = 1;

sub print_echo_can($$) {
	my $gconfig = shift || die;
	my $chans = shift || die; # channel or range of channels.
	my $echo_can = $gconfig->{'echo_can'};
	return if !defined($echo_can) || $echo_can eq 'none';

	print "echocanceller=$echo_can,$chans\n";
}

sub gen_t1_cas($$) {
	my $self = shift || die;
	my $gconfig = shift || die;
	my $parameters = $gconfig->{PARAMETERS} || die;
	my $genconf_file = $parameters->{GENCONF_FILE} || die;
	my $span = shift || die;
	my $num = $span->num() || die;
	my $proto = $span->proto || die;
	die "Generate configuration for '$proto' is not possible. Maybe you meant R2?"
		unless $proto eq 'T1';
	my $pri_connection_type = $gconfig->{pri_connection_type} || die;
	die "Span #$num is analog" unless $span->is_digital();
	die "Span #$num is not CAS" unless $span->is_pri && $pri_connection_type eq 'CAS';
	my $termtype = $span->termtype() || die "$0: Span #$num -- unkown termtype [NT/TE]\n";
	my $timing;
	my $lbo = 0;
	my $framing = $gconfig->{tdm_framing};
	if(!defined $framing) {
		$framing = 'esf';
	} elsif($framing ne 'esf' && $framing ne 'd4') {
		die "T1-CAS valid framing is only 'esf' or 'd4'. Not '$framing'. Check '$genconf_file'\n";
	}
	my $coding =  $span->coding() || die "$0: No coding information for span #$num\n";
	my $span_crc4 = $span->crc4();
	$span_crc4 = (defined $span_crc4) ? ",$span_crc4" : '';
	my $span_yellow = $span->yellow();
	$span_yellow = (defined $span_yellow) ? ",$span_yellow" : '';
	$timing = ($termtype eq 'NT') ? 0 : $bri_te_last_timing++;
	printf "span=%d,%d,%d,%s,%s%s%s\n",
			$num,
			$timing,
			$lbo,
			$framing,
			$coding,
			$span_crc4,
			$span_yellow;
	printf "# termtype: %s\n", lc($termtype);
	my $dchan_type;
	my $chan_range;
	if($span->is_pri()) {
		if ($pri_connection_type eq 'PRI') {
			$chan_range = Dahdi::Config::Gen::bchan_range($span);
			printf "bchan=%s\n", $chan_range;
			my $dchan = $span->dchan();
			printf "dchan=%d\n", $dchan->num();
		} elsif ($pri_connection_type eq 'R2' ) {
			my $idle_bits = $gconfig->{'r2_idle_bits'};
			$chan_range = Dahdi::Config::Gen::bchan_range($span);
			printf "cas=%s:$idle_bits\n", $chan_range;
		} elsif ($pri_connection_type eq 'CAS' ) {
			my $type = ($termtype eq 'TE') ? 'FXO' : 'FXS';
			my $sig = $gconfig->{'dahdi_signalling'}{$type};
			my $em_signalling = $gconfig->{'em_signalling'};
			if ($em_signalling ne 'none') {
				$sig = 'e&m';
				# FIXME: but we don't handle E1 yet
				$sig = 'e&me1' if $proto eq 'E1';
			}
			die "unknown default dahdi signalling for chan $num type $type" unless defined $sig;
			$chan_range = Dahdi::Config::Gen::chan_range($span->chans());
			printf "%s=%s\n", $sig, $chan_range;
		}
	} else {
		die "Digital span $num is not PRI";
	}
	print_echo_can($gconfig, $chan_range);
}

sub gen_digital($$$) {
	my $self = shift || die;
	my $gconfig = shift || die;
	my $span = shift || die;
	my $num = $span->num() || die;
	die "Span #$num is analog" unless $span->is_digital();
	my $termtype = $span->termtype() || die "$0: Span #$num -- unkown termtype [NT/TE]\n";
	my $timing;
	my $lbo = 0;
	my $framing = $span->framing() || die "$0: No framing information for span #$num\n";
	my $coding =  $span->coding() || die "$0: No coding information for span #$num\n";
	my $span_crc4 = $span->crc4();
	$span_crc4 = (defined $span_crc4) ? ",$span_crc4" : '';
	my $span_yellow = $span->yellow();
	$span_yellow = (defined $span_yellow) ? ",$span_yellow" : '';
	my $span_termination = $span->termination();
	$span_termination = (defined $span_termination) ? ",$span_termination" : '';
	my $span_softntte = $span->softntte();
	$span_softntte = (defined $span_softntte) ? ",$span_softntte" : '';
	# "MFC/R2 does not normally use CRC4"
	# FIXME: a finer way to override:
	if ($gconfig->{'pri_connection_type'} eq 'R2') { 
		$span_crc4 = '';
		$framing = 'cas';
	}
	$timing = ($termtype eq 'NT') ? 0 : $bri_te_last_timing++;
	printf "span=%d,%d,%d,%s,%s%s%s%s%s\n",
			$num,
			$timing,
			$lbo,
			$framing,
			$coding,
			$span_crc4,
			$span_yellow,
			$span_termination,
			$span_softntte;
	printf "# termtype: %s\n", lc($termtype);
	my $dchan_type;
	if ($span->is_bri()) {
		my $use_bristuff = 0;
		my $cfg_hardhdlc = $gconfig->{'bri_hardhdlc'};
		my $xpd = Dahdi::Xpp::xpd_of_span($span);
		if(!defined($cfg_hardhdlc) || $cfg_hardhdlc =~ /AUTO/i) {
			# Autodetect
			if(defined($xpd)) {
				# Bristuff?
				if(defined($xpd->dchan_hardhdlc) && !is_true($xpd->dchan_hardhdlc)) {
					$use_bristuff = 1;
				}
			}
		} elsif(!is_true($cfg_hardhdlc)) {
			$use_bristuff = 1;
		}
		if($use_bristuff) {
			$dchan_type = 'dchan';
		} else {
			$dchan_type = 'hardhdlc';
		}
		printf "bchan=%s\n", Dahdi::Config::Gen::bchan_range($span);
		my $dchan = $span->dchan();
		printf "$dchan_type=%d\n", $dchan->num();
	} elsif($span->is_pri()) {
		if ($gconfig->{'pri_connection_type'} eq 'PRI') {
			printf "bchan=%s\n", Dahdi::Config::Gen::bchan_range($span);
			my $dchan = $span->dchan();
			printf "dchan=%d\n", $dchan->num();
		} elsif ($gconfig->{'pri_connection_type'} eq 'R2' ) {
			my $idle_bits = $gconfig->{'r2_idle_bits'};
			printf "cas=%s:$idle_bits\n", Dahdi::Config::Gen::bchan_range($span);
			printf "dchan=%d\n", $span->dchan()->num();
		}
	} else {
		die "Digital span $num is not BRI, nor PRI?";
	}
	print_echo_can($gconfig, Dahdi::Config::Gen::bchan_range($span));
}

sub gen_signalling($$) {
	my $gconfig = shift || die;
	my $chan = shift || die;
	my $type = $chan->type;
	my $num = $chan->num;

	die "channel $num type $type is not an analog channel\n" if $chan->span->is_digital();
	if($type eq 'EMPTY') {
		printf "# channel %d, %s, no module.\n", $num, $chan->fqn;
		return;
	}
	my $signalling = $gconfig->{'dahdi_signalling'};
	my $sig = $signalling->{$type} || die "unknown default dahdi signalling for chan $num type $type";
	if ($type eq 'IN') {
		printf "# astbanktype: input\n";
	} elsif ($type eq 'OUT') {
		printf "# astbanktype: output\n";
	}
	printf "$sig=$num\n";
	print_echo_can($gconfig, $num);
}

sub generate($$$) {
	my $self = shift || die;
	my $file = $self->{FILE};
	my $gconfig = $self->{GCONFIG};
	my $genopts = $self->{GENOPTS};
	my @spans = @_;
	warn "Empty configuration -- no spans\n" unless @spans;
	rename "$file", "$file.bak"
		or $! == 2	# ENOENT (No dependency on Errno.pm)
		or die "Failed to backup old config: $!\n";
	#$gconfig->dump;
	print "Generating $file\n" if $genopts->{verbose};
	open(F, ">$file") || die "$0: Failed to open $file: $!\n";
	my $old = select F;
	printf "# Autogenerated by $0 on %s\n", scalar(localtime);
	print  "# If you edit this file and execute $0 again,\n";
	print  "# your manual changes will be LOST.\n";
	print <<"HEAD";
# Dahdi Configuration File
#
# This file is parsed by the Dahdi Configurator, dahdi_cfg
#
HEAD
	foreach my $span (@spans) {
		printf "# Span %d: %s %s\n", $span->num, $span->name, $span->description;
		if($span->is_digital) {
			if($span->is_pri) {
				if($gconfig->{'pri_connection_type'} eq 'CAS') {
					$self->gen_t1_cas($gconfig, $span);
				} else {
					$self->gen_digital($gconfig, $span);
				}
			} elsif($span->is_bri) {
				$self->gen_digital($gconfig, $span);
			}
		} else {
			foreach my $chan ($span->chans()) {
				if(1 || !defined $chan->type) {
					my $type = $chan->probe_type;
					my $num = $chan->num;
					die "Failed probing type for channel $num"
						unless defined $type;
					$chan->type($type);
				}
				gen_signalling($gconfig, $chan);
			}
		}
		print "\n";
	}
	print <<"TAIL";
# Global data

loadzone	= $gconfig->{'loadzone'}
defaultzone	= $gconfig->{'defaultzone'}
TAIL
	close F;
	select $old;
}

1;

__END__

=head1 NAME

dahdi - Generate configuration for dahdi drivers.

=head1 SYNOPSIS

 use Dahdi::Config::Gen::Dahdi;

 my $cfg = new Dahdi::Config::Gen::Dahdi(\%global_config, \%genopts);
 $cfg->generate(@span_list);

=head1 DESCRIPTION

Generate the F</etc/dahdi/system.conf>.
This is the configuration for dahdi_cfg(1).

Its location may be overriden via the environment variable F<DAHDI_CONF_FILE>.
