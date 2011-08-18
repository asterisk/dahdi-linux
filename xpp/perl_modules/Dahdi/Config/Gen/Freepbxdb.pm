package Dahdi::Config::Gen::Freepbxdb;

# Written by Tzafrir Cohen <tzafrir.cohen@xorcom.com>
# Copyright (C) 2011, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.

use strict;
use Socket;
use Dahdi::Config::Gen qw(is_true);

sub new($$$) {
	my $pack = shift || die;
	my $gconfig = shift || die;
	my $genopts = shift || die;
	my $self = {
			GCONFIG	=> $gconfig,
			GENOPTS	=> $genopts,
		};
	bless $self, $pack;
	return $self;
}

sub gen_channel($$) {
	my $self = shift || die;
	my $chan = shift || die;
	my $gconfig = $self->{GCONFIG};
	my $type = $chan->type;
	my $num = $chan->num;
	die "channel $num type $type is not an analog channel\n" if $chan->span->is_digital();
	my $exten = $gconfig->{'base_exten'} + $num;
	my $callerid = sprintf "\"Channel %d\" <%04d>", $num, $exten;
	my @cmds = ();
	#push @cmds, "database put DEVICE/$exten default_user $exten";
	#push @cmds, "database put DEVICE/$exten dial ZAP/$num";
	push @cmds, "database put DEVICE/$exten dial DAHDI/$num";
	#push @cmds, "database put DEVICE/$exten type fixed";
	push @cmds, "database put DEVICE/$exten user $exten";
	push @cmds, "database put AMPUSER/$exten device $exten";
	push @cmds, "database put AMPUSER/$exten cidname $callerid";
	return @cmds;
}

sub generate($) {
	my $self = shift || die;
	my $gconfig = $self->{GCONFIG};
	my $genopts = $self->{GENOPTS};
	#$gconfig->dump;
	my $ast_sock = '/var/run/asterisk/asterisk.ctl';
	my @spans = @_;
	my @cmds = ();
	warn "Empty configuration -- no spans\n" unless @spans;
	print "Configuring FXSs for FreePBX\n" if $genopts->{verbose};
	foreach my $span (@spans) {
		next if $span->is_digital;
		foreach my $chan ($span->chans()) {
			next unless ($chan->type eq 'FXS');
			push @cmds, $self->gen_channel($chan);
		}
	}
	#open(CMDS,"|$command >/dev/null") or
	socket(SOCK, PF_UNIX, SOCK_STREAM, 0) || die "socket: $!";
	connect(SOCK, sockaddr_un($ast_sock)) ||
		die "$0: Freepbxdb: Failed connecting to $ast_sock\n: $!";
	foreach (@cmds) {
		# Note: commands are NULL-terminated:
		print SOCK "$_\0";
		sleep 0.001;
	}
	close(SOCK) or
		die "$0: Freepbxdb: Failed sending commands ($ast_sock): $!\n";
}

1;

__END__

=head1 NAME

freepbxdb - Generate astdb configuration required by FreePBX

=head1 SYNOPSIS

 use Dahdi::Config::Gen::Freepbxdb;

 my $cfg = new Dahdi::Config::Gen::Freepbxdb(\%global_config, \%genopts);
 $cfg->generate(@span_list);

=head1 DESCRIPTION

Updates the Asterisk DB entries for FXS channels detected. Requires
Asterisk running.

The configuration generated here bypasses FreePBX's standard configuration
and allows using a simple dialplan snippet such as:

  [from-internal-custom](+)
  exten => _4XXX,1,Dial(DAHDI/${EXTEN:1})

This may come in handy in testing. At least until FreePBX will provide a
simple automated interface to do the same.

=head1 OPTIONS

None, so far.

=head1 FILES

=over

=item C</var/run/asterisk/asterisk.sock>

The socket to which commands are sent. FIXME: make this a parameter.

=back

