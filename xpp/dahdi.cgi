#! /usr/bin/perl -wT

# Written by Tzafrir Cohen <tzafrir.cohen@xorcom.com>
# Copyright (C) 2008, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.

use strict;
use File::Basename;
BEGIN { my $dir = dirname($0); unshift(@INC, "$dir", "$dir/perl_modules"); }

use CGI::Pretty qw/:standard start_ul start_li start_div start_pre/;
use Dahdi;
use Dahdi::Xpp;
use Dahdi::Hardware;

$ENV{'PATH'} = '/bin:/usr/bin';

my $DEF_TOK = '<Default>';

my $style=<<END;
<!--
body {
  margin-left: 0em;
  margin-right: 5em;
  //color: navy;
  background-color: #white;
}

dfn {
  font-style: italic;
  text-decoration: underline;
}

#content {
  margin-left: 10em;
}

h1, h2, h3 {
  color: #d03;
  margin-top: 2ex;
}

h1 { 
  text-align: center;
  color: #d03;
  background-color: #ccc;
  margin-left:5em;
}
/*
li:hover {
  background-color: #44c;
}

li li:hover {
  background-color: #448;
}
*/
/*li.status-ok  */
.status-noconf  {background-color: red; }
.status-notused {background-color: pink; } 

#toc {
  position: fixed;
  width: 9em;
  top: 3ex;
  bottom: 0pt;
  height: 100%;
  margins-left: 1em;
  color: #448;

}

#toc p {
  display: block;
  //text-align: center;
  height: 3ex;
}

#toc a {
  text-decoration: none;
  /*
  background-color: #F0FFF0;
  */
  font-weight: bold; 
  display: block; 
  padding: 0.2em; 
  width: 80%; 
  margin-bottom: 0.2ex; 
  border-top: 1px solid #8bd;
  color: #8bd;
  text-align: right;
}

-->
END

my @Toc = ();

sub header_line($$) {
	my ($text, $anchor) = @_;
	print a({-name=>$anchor},h2($text));
	push(@Toc, [$text, $anchor] );
}

print header,
	start_html(
		-title=>"DAHDI Information",
		-style=>{-code=>$style}),
	h1("DAHDI Information");

print start_div({-id=>'content'});

sub dahdi_spans() {
	my %ChansStat = (num=>0, configured=>0, inuse=>0);

	header_line("DAHDI Spans", 'spans');
	
	print p('Here we list the ',
		dfn({-title=> 'A span is a logical unit of dahdi
			channels. e.g.: all the channels that come from 
			a specific port, or all the analog channels from 
			a certain PCI card'},
			'spans'),
		' that DAHDI devices registered
		with DAHDI. For each span we list all of its channels.'
		),
		p('A channel that appears in ',
		span({-class=>'status-noconf'},'red text'),' ',
		'is one that has not been configured at all. Either not
		listed in system.conf, or dahdi_cfg was not run.'
		),
		p('A channel that appears in ',
		span({-class=>'status-notused'},'pink text'),' ',
		'is one that has been configured but is not used by any
		application. This usually means that either Asterisk is
		not running or Asterisk is not configured to use this
		channel'
		),
		p('If a port is disconnected it will have a "RED" alarm.
		For a FXO port this will only be on the specific port.
		For a BRI, E1 or T1 port it will be an alarm on the apn
		and all of the channels.'),
		;



	foreach my $span (Dahdi::spans()) {
		my $spanno = $span->num;
		my $index = 0;
		
		print h3(a({-name=>"zap_span_$spanno"}, "Span $spanno: ", 
			$span->name, " ", $span->description)),
			start_ul;
		foreach my $chan ($span->chans()) {
			my $batt = '';
			$batt = "(battery)" if $chan->battery;
			my $type = $chan->type;
			my $sig = $chan->signalling;
			my $info = $chan->info;
			my $chan_stat = 'ok';
			$ChansStat{num}++;
			if (!$sig) {
				$chan_stat = 'noconf';
			} else {
				$ChansStat{configured}++;
				if ($info =~ /\(In use\)/) {
					$ChansStat{inuse}++; 
				} else {
					$chan_stat = 'notused';
				}
			}
			# TODO: color differently if no signalling and
			# if not in-use and in alarm.
			print li({-class=>"status-$chan_stat"}, 
				$chan->num, " $type, $sig $info $batt");
		}
		print end_ul;
	}
}

sub dahdi_hardware() {
	header_line("DAHDI Hardware", 'zap_hard');

	print p('Here we list all the DAHDI hardware devices on your 
		system. If a device is not currently handled by a
		driver, it will	appear as ',
		span({-class=>'status-noconf'},'red text'),'.');

	my $hardware = Dahdi::Hardware->scan;

	print start_ul;
	foreach my $device ($hardware->device_list) {
		my $driver = $device->driver || "";
		my $status = 'ok';

		if (! $device->loaded) {
			$status = 'noconf';
		}

		print li({-class=>"status-$status"}, 
			$device->hardware_name, ": ", $driver, 
			" [".$device->vendor,"/". $device->product. "]	",
			$device->description);
	}
	print end_ul;
}

sub astribanks() {
	header_line("Astribanks", 'astribanks');

	print p('Here we list all the Astribank devices (That are
		handled by the drivers). For each Astribank we list
		its XPDs. A ',
		dfn({-title=>
			'a logical unit of the Astribank. It will '.
			'be registered in DAHDI as a single span. This	'.
			'can be either an analog (FXS or FXO) module or	'.
			'a single port in case of a BRI and PRI modules.'
			},
			'XPD'),'. ',
		' that is registered will have a link to the
		information about the span below. One that is not
		registered will appear as ',
		span({-class=>'status-noconf'},'red text'),'.');

	print start_ul;

	foreach my $xbus (Dahdi::Xpp::xbuses()) {
		print start_li, 
		      $xbus->name." (".$xbus->label .", ".$xbus->connector .")",
		      start_ul;
		foreach my $xpd ($xbus->xpds) {
			my $chan_stat = 'ok';
			my $span_str = 'UNREGISTERED';
			if ($xpd->spanno) {
				my $spanno = $xpd->spanno;
				$span_str =
				a({-href=>"#zap_span_$spanno"},
					"Span $spanno");
			} else {
				$chan_stat = 'noconf';
			}
			print li({-class=>"status-$chan_stat"}, 
				'[', $xpd->type, '] ', $span_str, $xpd->fqn
				);
		}
		print end_ul, end_li;
	}
	print end_ul;
}


dahdi_hardware();

astribanks();

dahdi_spans();

print end_div(); # content

print div({-id=>'toc'},
	p( a{-href=>'/'},'[Homepage]' ),
	( map {p( a({-href=> '#'.$_->[1]},$_->[0] ) )}  @Toc ),
);
