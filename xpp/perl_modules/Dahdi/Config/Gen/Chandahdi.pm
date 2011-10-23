package Dahdi::Config::Gen::Chandahdi;
use strict;

use Dahdi::Config::Gen qw(is_true);

sub new($$$) {
	my $pack = shift || die;
	my $gconfig = shift || die;
	my $genopts = shift || die;
	my $file = $ENV{CHAN_DAHDI_CHANNELS_FILE} || "/etc/asterisk/dahdi-channels.conf";
	my $self = {
			FILE	=> $file,
			GCONFIG	=> $gconfig,
			GENOPTS	=> $genopts,
		};
	bless $self, $pack;
	return $self;
}

# Since chan_dahdi definitions "leak" to the next ones, we try
# To reset some important definitions to their chan_dahdi defaults.
my %chan_dahdi_defaults = (
	context => 'default',
	group => '63', # FIXME: should not be needed. 
	overlapdial => 'no',
	busydetect => 'no',
	rxgain => 0,
	txgain => 0,
);

sub reset_chandahdi_values {
	foreach my $arg (@_) {
		if (exists $chan_dahdi_defaults{$arg}) {
			print "$arg = $chan_dahdi_defaults{$arg}\n";
		} else {
			print "$arg =\n";
		}
	}
}

sub gen_openr2($$$) {
	my $self = shift || die;
	my $gconfig = shift || die;
	my $span = shift || die;
	my $num = $span->num() || die;
	my $termtype = $span->termtype() || die "$0: Span #$num -- unkown termtype [NT/TE]\n";
	my $type = $span->type;
	# Fake type for signalling
	my $faketype = ($termtype eq 'TE') ? 'E1_TE' : 'E1_TE';
	my $group = $gconfig->{'group'}{"$type"};
	die "$0: missing default group (termtype=$termtype)\n" unless defined($group);
	my $context = $gconfig->{'context'}{"$faketype"};
	die "$0: missing default context\n" unless $context;
	my @to_reset = qw/context group/;
	my $chans = Dahdi::Config::Gen::bchan_range($span);
	$group .= "," . (10 + $num);	# Invent unique group per span
	my $country = $gconfig->{'loadzone'};
	my @valid_countries = qw( ar br cn cz co ec itu mx ph ve );
	die "Country '$country' is invalid for R2. Use one of: @valid_countries\n"
		unless grep { $_ eq $country } @valid_countries;
	printf "group=$group\n";
	printf "context=$context\n";
	printf "switchtype = %s\n", $span->switchtype;
	printf "signalling = %s\n", 'mfcr2';
	printf "caller = %s\n", ($termtype eq 'TE') ? 'no' : 'yes';
	printf "mfcr2_logdir = span%d\n", $span->num;
	print <<"EOF";
mfcr2_variant=$country
mfcr2_get_ani_first=no
mfcr2_max_ani=10
mfcr2_max_dnis=4
mfcr2_category=national_subscriber
mfcr2_call_files=yes
mfcr2_logging=all
mfcr2_mfback_timeout=-1
mfcr2_metering_pulse_timeout=-1
EOF
	printf "channel => %s\n", $chans;

	reset_chandahdi_values(@to_reset);
}

sub gen_cas($$$) {
	my $self = shift || die;
	my $gconfig = shift || die;
	my $span = shift || die;
	my $num = $span->num() || die;
	my $termtype = $span->termtype() || die "$0: Span #$num -- unkown termtype [NT/TE]\n";
	my $type = $span->type;
	my $group = $gconfig->{'group'}{"$type"};
	die "$0: missing default group (termtype=$termtype)\n" unless defined($group);
	my $context = $gconfig->{'context'}{"$type"};
	die "$0: missing default context\n" unless $context;
	# Fake type for signalling
	my $faketype = ($termtype eq 'TE') ? 'FXO' : 'FXS';
	my $sig = $gconfig->{'chan_dahdi_signalling'}{$faketype};
	my $em_signalling = $gconfig->{'em_signalling'};
	if ($em_signalling ne 'none') {
		$sig = $em_signalling;
		# FIXME: but we don't handle E1 yet
		$sig = 'em_e1' if $span->proto eq 'E1';
	}
	my @to_reset = qw/context group/;
	my $chans = Dahdi::Config::Gen::chan_range($span->chans());
	$group .= "," . (10 + $num);	# Invent unique group per span
	printf "group=$group\n";
	printf "context=$context\n";
	printf "switchtype = %s\n", $span->switchtype;
	printf "signalling = %s\n", $sig;
	printf "channel => %s\n", $chans;
	reset_chandahdi_values(@to_reset);
}

sub gen_digital($$$) {
	my $self = shift || die;
	my $gconfig = shift || die;
	my $span = shift || die;
	my $num = $span->num() || die;
	die "Span #$num is analog" unless $span->is_digital();
	if($span->is_pri && $gconfig->{'pri_connection_type'} eq 'R2') {
		printf "; Skipped: $gconfig->{'pri_connection_type'}\n\n";
		return;
	}
	my $type = $span->type() || die "$0: Span #$num -- unkown type\n";
	my $termtype = $span->termtype() || die "$0: Span #$num -- unkown termtype [NT/TE]\n";
	my $group = $gconfig->{'group'}{"$type"};
	my $context = $gconfig->{'context'}{"$type"};
	my @to_reset = qw/context group/;

	die "$0: missing default group (termtype=$termtype)\n" unless defined($group);
	die "$0: missing default context\n" unless $context;

	my $sig = $span->signalling || die "missing signalling info for span #$num type $type";
	grep($gconfig->{'bri_sig_style'} eq $_, 'bri', 'bri_ptmp', 'pri') or die "unknown signalling style for BRI";
	if($span->is_bri() and $gconfig->{'bri_sig_style'} eq 'bri_ptmp') {
		$sig .= '_ptmp';
	}
	if ($span->is_bri() && $termtype eq 'NT' && is_true($gconfig->{'brint_overlap'})) {
		print "overlapdial = yes\n";
		push(@to_reset, qw/overlapdial/);
	}

	$group .= "," . (10 + $num);	# Invent unique group per span
	printf "group=$group\n";
	printf "context=$context\n";
	printf "switchtype = %s\n", $span->switchtype;
	printf "signalling = %s\n", $sig;
	printf "channel => %s\n", Dahdi::Config::Gen::bchan_range($span);
	reset_chandahdi_values(@to_reset);
}

sub gen_channel($$) {
	my $self = shift || die;
	my $chan = shift || die;
	my $gconfig = $self->{GCONFIG};
	my $type = $chan->type;
	my $num = $chan->num;
	die "channel $num type $type is not an analog channel\n" if $chan->span->is_digital();
	my $exten = $gconfig->{'base_exten'} + $num;
	my $sig = $gconfig->{'chan_dahdi_signalling'}{$type};
	my $context = $gconfig->{'context'}{$type};
	my $group = $gconfig->{'group'}{$type};
	my $callerid;
	my $immediate;

	return if $type eq 'EMPTY';
	die "missing default_chan_dahdi_signalling for chan #$num type $type" unless $sig;
	die "missing context for chan #$num type $type" unless $context;
	$callerid = ($type eq 'FXO')
			? 'asreceived'
			: sprintf "\"Channel %d\" <%d>", $num, $exten;
	if($type eq 'IN') {
		$immediate = 'yes';
	}
	# FIXME: $immediage should not be set for 'OUT' channels, but meanwhile
	#        it's better to be compatible with genzaptelconf
	$immediate = 'yes' if $gconfig->{'fxs_immediate'} eq 'yes' and $sig =~ /^fxo_/;
	my $signalling = $chan->signalling;
	$signalling = " " . $signalling if $signalling;
	my $info = $chan->info;
	$info = " " . $info if $info;
	printf ";;; line=\"%d %s%s%s\"\n", $num, $chan->fqn, $signalling, $info;
	printf "signalling=$sig\n";
	printf "callerid=$callerid\n";
	printf "mailbox=%d\n", $exten unless $type eq 'FXO';
	if(defined $group) {
		printf "group=$group\n";
	}
	printf "context=$context\n";
	printf "immediate=$immediate\n" if defined $immediate;
	printf "channel => %d\n", $num;
	# Reset following values to default
	printf "callerid=\n";
	printf "mailbox=\n" unless $type eq 'FXO';
	if(defined $group) {
		printf "group=\n";
	}
	printf "context=default\n";
	printf "immediate=no\n" if defined $immediate;
	print "\n";
}

sub generate($) {
	my $self = shift || die;
	my $file = $self->{FILE};
	my $gconfig = $self->{GCONFIG};
	my $genopts = $self->{GENOPTS};
	#$gconfig->dump;
	my @spans = @_;
	warn "Empty configuration -- no spans\n" unless @spans;
	rename "$file", "$file.bak"
		or $! == 2	# ENOENT (No dependency on Errno.pm)
		or die "Failed to backup old config: $!\n";
	print "Generating $file\n" if $genopts->{verbose};
	open(F, ">$file") || die "$0: Failed to open $file: $!\n";
	my $old = select F;
	printf "; Autogenerated by $0 on %s\n", scalar(localtime);
	print  "; If you edit this file and execute $0 again,\n";
	print  "; your manual changes will be LOST.\n";
	print <<"HEAD";
; Dahdi Channels Configurations (chan_dahdi.conf)
;
; This is not intended to be a complete chan_dahdi.conf. Rather, it is intended
; to be #include-d by /etc/chan_dahdi.conf that will include the global settings
;

HEAD
	foreach my $span (@spans) {
		printf "; Span %d: %s %s\n", $span->num, $span->name, $span->description;
		if($span->is_digital) {
			if($span->is_pri) {
				if($gconfig->{'pri_connection_type'} eq 'R2') {
					$self->gen_openr2($gconfig, $span);
				} elsif($gconfig->{'pri_connection_type'} eq 'CAS') {
					$self->gen_cas($gconfig, $span);
				} else {
					$self->gen_digital($gconfig, $span);
				}
			} elsif($span->is_bri) {
				$self->gen_digital($gconfig, $span);
			}
		} else {
			foreach my $chan ($span->chans()) {
				if(is_true($genopts->{'freepbx'}) || is_true($gconfig->{'freepbx'})) {
					# Freepbx has its own idea about channels
					my $type = $chan->type;
					if($type eq 'FXS' || $type eq 'OUT' || $type eq 'IN') {
						printf "; Skip channel=%s($type) -- freepbx option.\n",
							$chan->num;
						next;
					}
				}
				$self->gen_channel($chan);
			}
		}
		print "\n";
	}
	close F;
	select $old;
}

1;

__END__

=head1 NAME

chandahdi - Generate configuration for chan_dahdi channels.

=head1 SYNOPSIS

 use Dahdi::Config::Gen::Chandahdi;

 my $cfg = new Dahdi::Config::Gen::Chandahdi(\%global_config, \%genopts);
 $cfg->generate(@span_list);

=head1 DESCRIPTION

Generate the F</etc/asterisk/dahdi-channels.conf>
This is used as a configuration for asterisk(1).
It should be included in the main F</etc/asterisk/chan_dahdi.conf>.

Its location may be overriden via the environment variable 
C<CHAN_DAHDI_CHANNELS_FILE>.

=head1 OPTIONS

=over 4

=item freepbx

With this option we do not generate channel definitions for FXS, Input and
Output ports. This is done because these channel definitions need to be
generated and inserted into I<freepbx> database anyway.

=back

The I<freepbx> option may be activated also by adding a C<freepbx yes> line
to the C<genconf_parameters> file.
