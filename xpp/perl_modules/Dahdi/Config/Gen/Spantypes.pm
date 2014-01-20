package Dahdi::Config::Gen::Spantypes;
use strict;

use Dahdi::Config::Gen qw(is_true);

sub new($$$) {
	my $pack = shift || die;
	my $gconfig = shift || die;
	my $genopts = shift || die;
	my $file = $ENV{SPAN_TYPES_CONF_FILE} || "/etc/dahdi/span-types.conf";
	my $self = {
			FILE	=> $file,
			GCONFIG	=> $gconfig,
			GENOPTS	=> $genopts,
		};
	bless $self, $pack;
	return $self;
}

sub generate($$$) {
	my $self = shift || die;
	my $file = $self->{FILE};
	my $gconfig = $self->{GCONFIG};
	my $genopts = $self->{GENOPTS};
	my @spans = @_;

	# If the dahdi_span_types utilities were not installed we do not want to run
	# this generator or report any errors.
	system "which dahdi_span_types > /dev/null 2>&1";
	return if $?;

	my $line_mode = $genopts->{'line-mode'};
	$line_mode = 'E1' unless defined $line_mode;
	$line_mode =~ /^[ETJ]1$/ or die "Bad line-mode='$line_mode'\n";
	warn "Empty configuration -- no spans\n" unless @spans;
	rename "$file", "$file.bak"
		or $! == 2	# ENOENT (No dependency on Errno.pm)
		or die "Failed to backup old config: $!\n";
	#$gconfig->dump;
	printf("Generating $file (with default line-mode %s)\n", $line_mode)
		if $genopts->{verbose};
	my $cmd = "dahdi_span_types --line-mode=$line_mode dumpconfig > $file";
	system $cmd;
	die "Command failed (status=$?): '$cmd'" if $?;
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

Generate the F</etc/dahdi/span-types.conf>.
This is the configuration for dahdi_span_types.

Its location may be overriden via the environment variable F<SPAN_TYPES_CONF_FILE>.

You would normally run:

  dahdi_genconf --line-mode=<line_mode>

which is a short for:

  dahdi_genconf spantypes=line-mode=<line_mode>

This is done by running:
  dahdi_span_types dumpconfig --line-mode=line_mode>

where I<line_mode> is the module parameter, and defaults to B<E1> if not
given (running C<dahdi_genconf spantypes>).
