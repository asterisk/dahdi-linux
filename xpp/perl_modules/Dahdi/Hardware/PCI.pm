package Dahdi::Hardware::PCI;
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

our @ISA = qw(Dahdi::Hardware);

# Lookup algorithm:
# 	First match 'vendor:product/subvendor:subproduct' key
#	Else match 'vendor:product/subvendor' key
#	Else match 'vendor:product' key
#	Else not a dahdi hardware.
my %pci_ids = (
	# from wct4xxp
	'10ee:0314'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE410P/TE405P (1st Gen)' },
	'd161:1420'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE420 (5th Gen)' },
	'd161:1410'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE410P (5th Gen)' },
	'd161:1405'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE405P (5th Gen)' },
	'd161:0420/0004'	=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE420 (4th Gen)' },
	'd161:0410/0004'	=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE410P (4th Gen)' },
	'd161:0405/0004'	=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE405P (4th Gen)' },
	'd161:0410/0003'	=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE410P (3rd Gen)' },
	'd161:0405/0003'	=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE405P (3rd Gen)' },
	'd161:0410'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE410P (2nd Gen)' },
	'd161:0405'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE405P (2nd Gen)' },
	'd161:1220'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE220 (5th Gen)' },
	'd161:1205'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE205P (5th Gen)' },
	'd161:1210'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE210P (5th Gen)' },
	'd161:0220/0004'	=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE220 (4th Gen)' },
	'd161:0205/0004'	=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE205P (4th Gen)' },
	'd161:0210/0004'	=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE210P (4th Gen)' },
	'd161:0205/0003'	=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE205P (3rd Gen)' },
	'd161:0210/0003'	=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE210P (3rd Gen)' },
	'd161:0205'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE205P ' },
	'd161:0210'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE210P ' },
	'd161:1820'		=> { DRIVER => 'wct4xxp', DESCRIPTION => 'Wildcard TE820 (5th Gen)' },

	# from wctdm24xxp
	'd161:2400'		=> { DRIVER => 'wctdm24xxp', DESCRIPTION => 'Wildcard TDM2400P' },
	'd161:0800'		=> { DRIVER => 'wctdm24xxp', DESCRIPTION => 'Wildcard TDM800P' },
	'd161:8002'		=> { DRIVER => 'wctdm24xxp', DESCRIPTION => 'Wildcard AEX800' },
	'd161:8003'		=> { DRIVER => 'wctdm24xxp', DESCRIPTION => 'Wildcard AEX2400' },
	'd161:8005'		=> { DRIVER => 'wctdm24xxp', DESCRIPTION => 'Wildcard TDM410P' },
	'd161:8006'		=> { DRIVER => 'wctdm24xxp', DESCRIPTION => 'Wildcard AEX410P' },
	'd161:8007'		=> { DRIVER => 'wctdm24xxp', DESCRIPTION => 'HA8-0000' },
	'd161:8008'		=> { DRIVER => 'wctdm24xxp', DESCRIPTION => 'HB8-0000' },

	# from pciradio
	'e159:0001/e16b'	=> { DRIVER => 'pciradio', DESCRIPTION => 'PCIRADIO' },

	# from wcfxo
	'e159:0001/8084'	=> { DRIVER => 'wcfxo', DESCRIPTION => 'Wildcard X101P clone' },
	'e159:0001/8085'	=> { DRIVER => 'wcfxo', DESCRIPTION => 'Wildcard X101P' },
	'e159:0001/8086'	=> { DRIVER => 'wcfxo', DESCRIPTION => 'Wildcard X101P clone' },
	'e159:0001/8087'	=> { DRIVER => 'wcfxo', DESCRIPTION => 'Wildcard X101P clone' },
	'1057:5608'		=> { DRIVER => 'wcfxo', DESCRIPTION => 'Wildcard X100P' },

	# from wct1xxp
	'e159:0001/6159'	=> { DRIVER => 'wct1xxp', DESCRIPTION => 'Digium Wildcard T100P T1/PRI or E100P E1/PRA Board' },

	# from wctdm
	'e159:0001/a159'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard S400P Prototype' },
	'e159:0001/e159'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard S400P Prototype' },
	'e159:0001/b100'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV E/F' },
	'e159:0001/b1d9'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV I' },
	'e159:0001/b118'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV I' },
	'e159:0001/b119'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV I' },
	'e159:0001/a9fd'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV H' },
	'e159:0001/a8fd'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV H' },
	'e159:0001/a800'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV H' },
	'e159:0001/a801'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV H' },
	'e159:0001/a908'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV H' },
	'e159:0001/a901'	=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV H' },
	#'e159:0001'		=> { DRIVER => 'wctdm', DESCRIPTION => 'Wildcard TDM400P REV H' },

	# from wcte11xp
	'e159:0001/71fe'	=> { DRIVER => 'wcte11xp', DESCRIPTION => 'Digium Wildcard TE110P T1/E1 Board' },
	'e159:0001/79fe'	=> { DRIVER => 'wcte11xp', DESCRIPTION => 'Digium Wildcard TE110P T1/E1 Board' },
	'e159:0001/795e'	=> { DRIVER => 'wcte11xp', DESCRIPTION => 'Digium Wildcard TE110P T1/E1 Board' },
	'e159:0001/79de'	=> { DRIVER => 'wcte11xp', DESCRIPTION => 'Digium Wildcard TE110P T1/E1 Board' },
	'e159:0001/797e'	=> { DRIVER => 'wcte11xp', DESCRIPTION => 'Digium Wildcard TE110P T1/E1 Board' },

	# from wcte12xp
	'd161:0120'		=> { DRIVER => 'wcte12xp', DESCRIPTION => 'Wildcard TE12xP' },
	'd161:8000'		=> { DRIVER => 'wcte12xp', DESCRIPTION => 'Wildcard TE121' },
	'd161:8001'		=> { DRIVER => 'wcte12xp', DESCRIPTION => 'Wildcard TE122' },

	# from wcb4xxp
	'd161:b410'		=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'Digium Wildcard B410P' },

	# from tor2
	'10b5:9030'		=> { DRIVER => 'tor2', DESCRIPTION => 'PLX 9030' },
	'10b5:3001'		=> { DRIVER => 'tor2', DESCRIPTION => 'PLX Development Board' },
	'10b5:d00d'		=> { DRIVER => 'tor2', DESCRIPTION => 'Tormenta 2 Quad T1/PRI or E1/PRA' },
	'10b5:4000'		=> { DRIVER => 'tor2', DESCRIPTION => 'Tormenta 2 Quad T1/E1 (non-Digium clone)' },

	# # from wctc4xxp
	'd161:3400'		=> { DRIVER => 'wctc4xxp', DESCRIPTION => 'Wildcard TC400P' },
	'd161:8004'		=> { DRIVER => 'wctc4xxp', DESCRIPTION => 'Wildcard TCE400P' },

	# Cologne Chips:
	# (Still a partial list)
	'1397:08b4/1397:b540'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'Swyx 4xS0 SX2 QuadBri' },
	'1397:08b4/1397:b556'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'Junghanns DuoBRI ISDN card' },
	'1397:08b4/1397:b520'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'Junghanns QuadBRI ISDN card' },
	'1397:08b4/1397:b550'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'Junghanns QuadBRI ISDN card' },
	'1397:08b4/1397:b752'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'Junghanns QuadBRI ISDN PCI-E card' },
	'1397:16b8/1397:b552'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'Junghanns OctoBRI ISDN card' },
	'1397:16b8/1397:b55b'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'Junghanns OctoBRI ISDN card' },
	'1397:08b4/1397:e884'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'OpenVox B200P' },
	'1397:08b4/1397:e888'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'OpenVox B400P' },
	'1397:16b8/1397:e998'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'OpenVox B800P' },
	'1397:08b4/1397:b566'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'BeroNet BN2S0' },
	'1397:08b4/1397:b560'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'BeroNet BN4S0' },
	'1397:08b4/1397:b762'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'BeroNet BN4S0 PCI-E card' },
	'1397:16b8/1397:b562'	=> { DRIVER => 'wcb4xxp', DESCRIPTION => 'BeroNet BN8S0' },
	'1397:08b4'		=> { DRIVER => 'qozap', DESCRIPTION => 'Generic Cologne ISDN card' },
	'1397:16b8'		=> { DRIVER => 'qozap', DESCRIPTION => 'Generic OctoBRI ISDN card' },
	'1397:30b1'		=> { DRIVER => 'cwain', DESCRIPTION => 'HFC-E1 ISDN E1 card' },
	'1397:2bd0'		=> { DRIVER => 'zaphfc', DESCRIPTION => 'HFC-S ISDN BRI card' },
	# Has three submodels. Tested with 0675:1704:
	'1043:0675'		=> { DRIVER => 'zaphfc', DESCRIPTION => 'ASUSTeK Computer Inc. ISDNLink P-IN100-ST-D' },
	'1397:f001'		=> { DRIVER => 'ztgsm', DESCRIPTION => 'HFC-GSM Cologne Chips GSM' },

	# Rhino cards (based on pci.ids)
	'0b0b:0105'	=> { DRIVER => 'r1t1', DESCRIPTION => 'Rhino R1T1' },
	'0b0b:0205'	=> { DRIVER => 'r4fxo', DESCRIPTION => 'Rhino R14FXO' },
	'0b0b:0206'	=> { DRIVER => 'rcbfx', DESCRIPTION => 'Rhino RCB4FXO 4-channel FXO analog telphony card' },
	'0b0b:0305'	=> { DRIVER => 'rxt1', DESCRIPTION => 'Rhino R4T1' },
	'0b0b:0405'	=> { DRIVER => 'rcbfx', DESCRIPTION => 'Rhino R8FXX' },
	'0b0b:0406'	=> { DRIVER => 'rcbfx', DESCRIPTION => 'Rhino RCB8FXX 8-channel modular analog telphony card' },
	'0b0b:0505'	=> { DRIVER => 'rcbfx', DESCRIPTION => 'Rhino R24FXX' },
	'0b0b:0506'	=> { DRIVER => 'rcbfx', DESCRIPTION => 'Rhino RCB24FXS 24-Channel FXS analog telphony card' },
	'0b0b:0605'	=> { DRIVER => 'rxt1', DESCRIPTION => 'Rhino R2T1' },
	'0b0b:0705'	=> { DRIVER => 'rcbfx', DESCRIPTION => 'Rhino R24FXS' },
	'0b0b:0706'	=> { DRIVER => 'rcbfx', DESCRIPTION => 'Rhino RCB24FXO 24-Channel FXO analog telphony card' },
	'0b0b:0906'	=> { DRIVER => 'rcbfx', DESCRIPTION => 'Rhino RCB24FXX 24-channel modular analog telphony card' },

	# Sangoma cards (based on pci.ids)
	'1923:0040'	=> { DRIVER => 'wanpipe', DESCRIPTION => 'Sangoma Technologies Corp. A200/Remora FXO/FXS Analog AFT card' },
	'1923:0100'	=> { DRIVER => 'wanpipe', DESCRIPTION => 'Sangoma Technologies Corp. A104d QUAD T1/E1 AFT card' },
	'1923:0300'	=> { DRIVER => 'wanpipe', DESCRIPTION => 'Sangoma Technologies Corp. A101 single-port T1/E1' },
	'1923:0400'	=> { DRIVER => 'wanpipe', DESCRIPTION => 'Sangoma Technologies Corp. A104u Quad T1/E1 AFT' },

	# Yeastar (from output of modinfo):
	'e159:0001/2151' => { DRIVER => 'ystdm8xx', DESCRIPTION => 'Yeastar YSTDM8xx'},

	'e159:0001/9500:0003' => { DRIVER => 'opvxa1200', DESCRIPTION => 'OpenVox A800P' },

	# Aligera
 	'10ee:1004'		=> { DRIVER => 'ap400', DESCRIPTION => 'Aligera AP40X/APE40X 1E1/2E1/4E1 card' },
	);

$ENV{PATH} .= ":/usr/sbin:/sbin:/usr/bin:/bin";

sub pci_sorter {
	return $a->priv_device_name() cmp $b->priv_device_name();
}

sub new($@) {
	my $pack = shift || die "Wasn't called as a class method\n";
	my %attr = @_;
	my $name = sprintf("pci:%s", $attr{PRIV_DEVICE_NAME});
	my $self = Dahdi::Hardware->new($name, 'PCI');
	%{$self} = (%{$self}, %attr);
	bless $self, $pack;
	return $self;
}

my %pci_devs;

sub readfile($) {
	my $name = shift || die;
	open(F, $name) || die "Failed to open '$name': $!";
	my $str = <F>;
	close F;
	chomp($str);
	return $str;
}

sub scan_devices($) {
	my @devices;

	while(<$Dahdi::sys_base/bus/pci/devices/*>) {
		m,([^/]+)$,,;
		my $name = $1;
		my $l = readlink $_ || die;
		$pci_devs{$name}{PRIV_DEVICE_NAME} = $name;
		$pci_devs{$name}{DEVICE} = $l;
		$pci_devs{$name}{VENDOR} = readfile "$_/vendor";
		$pci_devs{$name}{PRODUCT} = readfile "$_/device";
		$pci_devs{$name}{SUBVENDOR} = readfile "$_/subsystem_vendor";
		$pci_devs{$name}{SUBPRODUCT} = readfile "$_/subsystem_device";
		my $dev = $pci_devs{$name};
		grep(s/0x//, $dev->{VENDOR}, $dev->{PRODUCT}, $dev->{SUBVENDOR}, $dev->{SUBPRODUCT});
		$pci_devs{$name}{DRIVER} = '';
	}

	while(<$Dahdi::sys_base/bus/pci/drivers/*/[0-9]*>) {
		m,^(.*?)/([^/]+)/([^/]+)$,;
		my $prefix = $1;
		my $drvname = $2;
		my $id = $3;
		my $l = readlink "$prefix/$drvname/module";
		# Find the real module name (if we can).
		if(defined $l) {
			my $moduledir = "$prefix/$drvname/$l";
			my $modname = $moduledir;
			$modname =~ s:^.*/::;
			$drvname = $modname;
		}
		$pci_devs{$id}{LOADED} = $drvname;
	}
	foreach (sort keys %pci_devs) {
		my $dev = $pci_devs{$_};
		my $key;
		# Try to match
		$key = "$dev->{VENDOR}:$dev->{PRODUCT}/$dev->{SUBVENDOR}:$dev->{SUBPRODUCT}";
		$key = "$dev->{VENDOR}:$dev->{PRODUCT}/$dev->{SUBVENDOR}" if !defined($pci_ids{$key});
		$key = "$dev->{VENDOR}:$dev->{PRODUCT}" if !defined($pci_ids{$key});
		next unless defined $pci_ids{$key};

		my $d = Dahdi::Hardware::PCI->new(
			PRIV_DEVICE_NAME	=> $dev->{PRIV_DEVICE_NAME},
			VENDOR			=> $dev->{VENDOR},
			PRODUCT			=> $dev->{PRODUCT},
			SUBVENDOR		=> $dev->{SUBVENDOR},
			SUBPRODUCT		=> $dev->{SUBPRODUCT},
			LOADED			=> $dev->{LOADED},
			DRIVER			=> $pci_ids{$key}{DRIVER},
			DESCRIPTION		=> $pci_ids{$key}{DESCRIPTION},
			);
		push(@devices, $d);
	}
	@devices = sort pci_sorter @devices;
	return @devices;
}

1;
