package Dahdi::Config::Gen::Users;
use strict;

use File::Basename;
use Dahdi::Config::Gen qw(is_true);

# Generate a complete users.conf for the asterisk-gui
# As the asterisk-gui provides no command-line interface of its own and
# no decent support of #include, we have no choice but to nuke users.conf
# if we're to provide a working system

sub new($$$) {
	my $pack = shift || die;
	my $gconfig = shift || die;
	my $genopts = shift || die;
	my $file = $ENV{USERS_FILE} || "/etc/asterisk/users.conf";
	my $self = {
			FILE	=> $file,
			GCONFIG	=> $gconfig,
			GENOPTS	=> $genopts,
		};
	bless $self, $pack;
	return $self;
}

# A single analog trunk for all the FXO channels
sub gen_analog_trunk {
	my @fxo_ports = @_;
	return unless (@fxo_ports); # no ports

	my $ports = join(',', @fxo_ports);

	print << "EOF"
[trunk_1]
trunkname = analog
hasexten = no
hasiax = no
hassip = no
hasregisteriax = no
hasregistersip = no
trunkstyle = analog
dahdichan = $ports

EOF
}

# A digital trunk for a single span.
# FIXME: how do I create the DID context?
sub gen_digital_trunk($) {
	my $span = shift;
	my $num = $span->num;
	my $sig = $span->signalling;
	my $type = $span->type;
	my $bchan_range = Dahdi::Config::Gen::bchan_range($span);

	print << "EOF";
[span_$num]
group = $num
hasexten = no
signalling = $sig
trunkname = Span $num $type
trunkstyle = digital  ; GUI metadata
hassip = no
hasiax = no
context = DID_span_$num
dahdichan = $bchan_range

EOF
}

my $ExtenNum;

# A single user for a FXS channel
sub gen_channel($$) {
	my $self = shift || die;
	my $chan = shift || die;
	my $gconfig = $self->{GCONFIG};
	my $type = $chan->type;
	my $num = $chan->num;
	die "channel $num type $type is not an analog channel\n" if $chan->span->is_digital();
	my $exten = $ExtenNum++;
	my $sig = $gconfig->{'chan_dahdi_signalling'}{$type};
	my $full_name = "$type $num";

	die "missing default_chan_dahdi_signalling for chan #$num type $type" unless $sig;
	print << "EOF";
[$exten]
context = DLPN_DialPlan1
callwaiting = yes
fullname = $full_name
cid_number = $exten
hasagent = no
hasdirectory = no
hasiax = no
hasmanager = no
hassip = no
hasvoicemail = yes
mailbox = $exten
threewaycalling = yes
vmsecret = $exten
signalling = $sig
dahdichan = $num
registeriax = no
registersip = no
canreinvite = no

EOF
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
	print <<"HEAD";
;!
;! Automatically generated configuration file
;! Filename: @{[basename($file)]} ($file)
;! Generator: $0
;! Creation Date: @{[scalar(localtime)]}
;! If you edit this file and execute $0 again,
;! your manual changes will be LOST.
;!
[general]
;
; Starting point of allocation of extensions
;
userbase = @{[$gconfig->{'base_exten'}+1]}
;
; Create voicemail mailbox and use use macro-stdexten
;
hasvoicemail = yes
;
; Set voicemail mailbox @{[$gconfig->{'base_exten'}+1]} password to 1234
;
vmsecret = 1234
;
; Create SIP Peer
;
hassip = no
;
; Create IAX friend
;
hasiax = no
;
; Create Agent friend
;
hasagent = no
;
; Create H.323 friend
;
;hash323 = yes
;
; Create manager entry
;
hasmanager = no
;
; Remaining options are not specific to users.conf entries but are general.
;
callwaiting = yes
threewaycalling = yes
callwaitingcallerid = yes
transfer = yes
canpark = yes
cancallforward = yes
callreturn = yes
callgroup = 1
pickupgroup = 1
localextenlength = @{[length($gconfig->{'base_exten'})]}


HEAD
	my @fxo_ports = ();
	$ExtenNum = $self->{GCONFIG}->{'base_exten'};
	foreach my $span (@spans) {
		printf "; Span %d: %s %s\n", $span->num, $span->name, $span->description;
		if ($span->type =~ /^(BRI_(NT|TE)|E1|T1)$/) {
			gen_digital_trunk($span);
			next;
		}
		foreach my $chan ($span->chans()) {
			if (grep { $_ eq $span->type} ( 'FXS', 'IN', 'OUT' )) {
				$self->gen_channel($chan);
			} elsif ($chan->type eq 'FXO') {
				# TODO: "$first_chan-$last_chan"
				push @fxo_ports,($chan->num);
			}
		}
		print "\n";
	}
	gen_analog_trunk(@fxo_ports);
	close F;
	select $old;
}

1;

__END__

=head1 NAME

users - Generate configuration for users.conf.

=head1 SYNOPSIS

 use Dahdi::Config::Gen::Users;

 my $cfg = new Dahdi::Config::Gen::Users(\%global_config, \%genopts);
 $cfg->generate(@span_list);

=head1 DESCRIPTION

Generate the F</etc/asterisk/users.conf> which is used by asterisk(1) 
and AsteriskGUI. This will replace your entire configuration including
any SIP/IAX users and trunks you may have set. Thus it's probably only
appropriate for an initial setup.

Its location may be overriden via the environment variable F<USERS_FILE>.
