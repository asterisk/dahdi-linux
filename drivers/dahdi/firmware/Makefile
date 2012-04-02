#
# DAHDI
# 
# Makefile for firmware downloading/installation
#
# Copyright (C) 2007-2010, Digium, Inc.
#
# Joshua Colp <jcolp@digium.com>
#
#
# See http://www.asterisk.org for more information about
# the Asterisk project. Please do not directly contact
# any of the maintainers of this project for assistance;
# the project provides a web site, mailing lists and IRC
# channels for your use.
#
# This program is free software, distributed under the terms of
# the GNU General Public License Version 2 as published by the
# Free Software Foundation. See the LICENSE file included with
# this program for more details.
#

.PHONY: dist-clean clean all uninstall have_download install object-build hotplug-install hotplug-dirs hotplug-uninstall make_firmware_object firmware-loaders

OCT6114_064_VERSION:=1.05.01
OCT6114_128_VERSION:=1.05.01
OCT6114_256_VERSION:=1.05.01
TC400M_VERSION:=MR6.12
VPMADT032_VERSION:=1.25.0
HX8_VERSION:=2.06
VPMOCT032_VERSION:=1.12.0
WCT820_VERSION:=1.76

FIRMWARE_URL:=http://downloads.digium.com/pub/telephony/firmware/releases

ALL_FIRMWARE=FIRMWARE-OCT6114-064 FIRMWARE-OCT6114-128 FIRMWARE-OCT6114-256 FIRMWARE-TC400M FIRMWARE-HX8 FIRMWARE-VPMOCT032 FIRMWARE-TE820

# Firmware files should use the naming convention: dahdi-fw-<base name>-<sub name>-<version> or dahdi-fw-<base name>-<version>
# First example: dahdi-fw-oct6114-064-1.05.01
# This means this is version 1.05.01 of the oct6114-064 firmware
# Second example: dahdi-fw-tc400m-MR5.6
# This means this is version MR5.6 of the tc400m firmware

# Build a list of firmware package filenames we need
FIRMWARE:=$(ALL_FIRMWARE:FIRMWARE-OCT6114-064=dahdi-fw-oct6114-064-$(OCT6114_064_VERSION).tar.gz)
FIRMWARE:=$(FIRMWARE:FIRMWARE-OCT6114-128=dahdi-fw-oct6114-128-$(OCT6114_128_VERSION).tar.gz)
FIRMWARE:=$(FIRMWARE:FIRMWARE-OCT6114-256=dahdi-fw-oct6114-256-$(OCT6114_256_VERSION).tar.gz)
FIRMWARE:=$(FIRMWARE:FIRMWARE-TC400M=dahdi-fw-tc400m-$(TC400M_VERSION).tar.gz)
FIRMWARE:=$(FIRMWARE:FIRMWARE-HX8=dahdi-fw-hx8-$(HX8_VERSION).tar.gz)
FIRMWARE:=$(FIRMWARE:FIRMWARE-VPMOCT032=dahdi-fw-vpmoct032-$(VPMOCT032_VERSION).tar.gz)
FIRMWARE:=$(FIRMWARE:FIRMWARE-TE820=dahdi-fw-te820-$(WCT820_VERSION).tar.gz)

FWLOADERS:=dahdi-fwload-vpmadt032-$(VPMADT032_VERSION).tar.gz

# Build a list of object files if hotplug will not be used
OBJECT_FILES:=$(ALL_FIRMWARE:FIRMWARE-OCT6114-064=dahdi-fw-oct6114-064.o)
OBJECT_FILES:=$(OBJECT_FILES:FIRMWARE-OCT6114-128=dahdi-fw-oct6114-128.o)
OBJECT_FILES:=$(OBJECT_FILES:FIRMWARE-OCT6114-256=dahdi-fw-oct6114-256.o)
OBJECT_FILES:=$(OBJECT_FILES:FIRMWARE-TC400M=dahdi-fw-tc400m.o)
OBJECT_FILES:=$(OBJECT_FILES:FIRMWARE-HX8=dahdi-fw-hx8.o)
OBJECT_FILES:=$(OBJECT_FILES:FIRMWARE-VPMOCT032=dahdi-fw-vpmoct032.o)

# Force usage of wget, for now
DOWNLOAD=wget
WGET=wget

# If "fetch" is used, --continue is not a valid option.
ifeq ($(WGET),wget)
WGET_ARGS:=--continue
endif

all: $(FIRMWARE)

# Clean up any downloaded/extracted firmware packages
dist-clean: clean
	rm -f dahdi-fw-*.bin
	rm -f dahdi-fw-*.tar.gz
	rm -f dahdi-fwload-*.tar.gz
	rm -f make_firmware_object

# Clean up anything we built
clean:
	rm -f dahdi-fw-*.o

# Download and extract firmware tarballs
dahdi-fw-%.tar.gz:
	@if ( [ "$(HOTPLUG_FIRMWARE)" = "no" ] ) || ( [ -d $(DESTDIR)/usr/lib/hotplug/firmware ] && ! [ -f $(DESTDIR)/usr/lib/hotplug/firmware/.$(subst .tar.gz,,$*) ] ) || ( [ -d $(DESTDIR)/lib/firmware ] && ! [ -f $(DESTDIR)/lib/firmware/.$(subst .tar.gz,,$*) ] ); then \
		echo "Attempting to download $@"; \
		if test ! -f $@; then $(DOWNLOAD) $(WGET_ARGS) $(FIRMWARE_URL)/$@; fi; \
		if test ! -f $@; then exit 1; fi; \
		(cat $@ | gzip -d | tar --no-same-owner -xf -) \
	fi

firmware-loaders: $(FWLOADERS)

.PHONY: dahdi-fwload-vpmadt032-$(VPMADT032_VERSION).tar.gz
dahdi-fwload-vpmadt032-$(VPMADT032_VERSION).tar.gz:
	@if test ! -f $@; then echo "Attempting to download $@"; $(DOWNLOAD) $(WGET_ARGS) $(FIRMWARE_URL)/$@; fi; \
	if test ! -f $@; then exit 1; fi; \
	(cd ../../..; cat drivers/dahdi/firmware/$@ | gzip -d | tar --no-same-owner -xf -)

# Create object files suitable for linking against
object-build: $(FIRMWARE) $(OBJECT_FILES)

$(DESTDIR)/usr/lib/hotplug/firmware $(DESTDIR)/lib/firmware:
	mkdir -p $@

# Install all downloaded firmware images for hotplug usage
hotplug-install: $(DESTDIR)/usr/lib/hotplug/firmware $(DESTDIR)/lib/firmware $(FIRMWARE)
ifeq ($(shell if ( [ -f $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-oct6114-064-$(OCT6114_064_VERSION) ] ) && ( [ -f $(DESTDIR)/lib/firmware/.dahdi-fw-oct6114-064-$(OCT6114_064_VERSION) ] ); then echo "no"; else echo "yes"; fi),yes)
	@echo "Installing dahdi-fw-oct6114-064.bin to hotplug firmware directories"
	@install -m 644 dahdi-fw-oct6114-064.bin $(DESTDIR)/usr/lib/hotplug/firmware
	@rm -rf $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-oct6114-064-*
	@touch $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-oct6114-064-$(OCT6114_064_VERSION)
	@install -m 644 dahdi-fw-oct6114-064.bin $(DESTDIR)/lib/firmware
	@rm -rf $(DESTDIR)/lib/firmware/.dahdi-fw-oct6114-064-*
	@touch $(DESTDIR)/lib/firmware/.dahdi-fw-oct6114-064-$(OCT6114_064_VERSION)
else
	@echo "Firmware dahdi-fw-oct6114-064.bin is already installed with required version $(OCT6114_064_VERSION)"
endif
ifeq ($(shell if ( [ -f $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-oct6114-128-$(OCT6114_128_VERSION) ] ) && ( [ -f $(DESTDIR)/lib/firmware/.dahdi-fw-oct6114-128-$(OCT6114_128_VERSION) ] ); then echo "no"; else echo "yes"; fi),yes)
	@echo "Installing dahdi-fw-oct6114-128.bin to hotplug firmware directories"
	@install -m 644 dahdi-fw-oct6114-128.bin $(DESTDIR)/usr/lib/hotplug/firmware
	@rm -rf $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-oct6114-128-*
	@touch $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-oct6114-128-$(OCT6114_128_VERSION)
	@install -m 644 dahdi-fw-oct6114-128.bin $(DESTDIR)/lib/firmware
	@rm -rf $(DESTDIR)/lib/firmware/.dahdi-fw-oct6114-128-*
	@touch $(DESTDIR)/lib/firmware/.dahdi-fw-oct6114-128-$(OCT6114_128_VERSION)
else
	@echo "Firmware dahdi-fw-oct6114-128.bin is already installed with required version $(OCT6114_128_VERSION)"
endif
ifeq ($(shell if ( [ -f $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-oct6114-256-$(OCT6114_256_VERSION) ] ) && ( [ -f $(DESTDIR)/lib/firmware/.dahdi-fw-oct6114-256-$(OCT6114_256_VERSION) ] ); then echo "no"; else echo "yes"; fi),yes)
	@echo "Installing dahdi-fw-oct6114-256.bin to hotplug firmware directories"
	@install -m 644 dahdi-fw-oct6114-256.bin $(DESTDIR)/usr/lib/hotplug/firmware
	@rm -rf $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-oct6114-256-*
	@touch $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-oct6114-256-$(OCT6114_256_VERSION)
	@install -m 644 dahdi-fw-oct6114-256.bin $(DESTDIR)/lib/firmware
	@rm -rf $(DESTDIR)/lib/firmware/.dahdi-fw-oct6114-256-*
	@touch $(DESTDIR)/lib/firmware/.dahdi-fw-oct6114-256-$(OCT6114_256_VERSION)
else
	@echo "Firmware dahdi-fw-oct6114-256.bin is already installed with required version $(OCT6114_256_VERSION)"
endif
ifeq ($(shell if ( [ -f $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-tc400m-$(TC400M_VERSION) ] ) && ( [ -f $(DESTDIR)/lib/firmware/.dahdi-fw-tc400m-$(TC400M_VERSION) ] ); then echo "no"; else echo "yes"; fi),yes)
	@echo "Installing dahdi-fw-tc400m.bin to hotplug firmware directories"
	@install -m 644 dahdi-fw-tc400m.bin $(DESTDIR)/usr/lib/hotplug/firmware
	@rm -rf $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-tc400m-*
	@touch $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-tc400m-$(TC400M_VERSION)
	@install -m 644 dahdi-fw-tc400m.bin $(DESTDIR)/lib/firmware
	@rm -rf $(DESTDIR)/lib/firmware/.dahdi-fw-tc400m-*
	@touch $(DESTDIR)/lib/firmware/.dahdi-fw-tc400m-$(TC400M_VERSION)
else
	@echo "Firmware dahdi-fw-tc400m.bin is already installed with required version $(TC400M_VERSION)"
endif
ifeq ($(shell if ( [ -f $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-hx8-$(HX8_VERSION) ] ) && ( [ -f $(DESTDIR)/lib/firmware/.dahdi-fw-hx8-$(HX8_VERSION) ] ); then echo "no"; else echo "yes"; fi),yes)
	@echo "Installing dahdi-fw-hx8.bin to hotplug firmware directories"
	@install -m 644 dahdi-fw-hx8.bin $(DESTDIR)/usr/lib/hotplug/firmware
	@rm -rf $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-hx8-*
	@touch $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-hx8-$(HX8_VERSION)
	@install -m 644 dahdi-fw-hx8.bin $(DESTDIR)/lib/firmware
	@rm -rf $(DESTDIR)/lib/firmware/.dahdi-fw-hx8-*
	@touch $(DESTDIR)/lib/firmware/.dahdi-fw-hx8-$(HX8_VERSION)
else
	@echo "Firmware dahdi-fw-hx8.bin is already installed with required version $(HX8_VERSION)"
endif
ifeq ($(shell if ( [ -f $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-vpmoct032-$(VPMOCT032_VERSION) ] ) && ( [ -f $(DESTDIR)/lib/firmware/.dahdi-fw-vpmoct032-$(VPMOCT032_VERSION) ] ); then echo "no"; else echo "yes"; fi),yes)
	@echo "Installing dahdi-fw-vpmoct032.bin to hotplug firmware directories"
	@install -m 644 dahdi-fw-vpmoct032.bin $(DESTDIR)/usr/lib/hotplug/firmware
	@rm -rf $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-vpmoct032-*
	@touch $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-vpmoct032-$(VPMOCT032_VERSION)
	@install -m 644 dahdi-fw-vpmoct032.bin $(DESTDIR)/lib/firmware
	@rm -rf $(DESTDIR)/lib/firmware/.dahdi-fw-vpmoct032-*
	@touch $(DESTDIR)/lib/firmware/.dahdi-fw-vpmoct032-$(VPMOCT032_VERSION)
else
	@echo "Firmware dahdi-fw-vpmoct032.bin is already installed with required version $(VPMOCT032_VERSION)"
endif
ifeq ($(shell if ( [ -f $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-te820-$(WCT820_VERSION) ] ) && ( [ -f $(DESTDIR)/lib/firmware/.dahdi-fw-te820-$(WCT820_VERSION) ] ); then echo "no"; else echo "yes"; fi),yes)
	@echo "Installing dahdi-fw-te820.bin to hotplug firmware directories"
	@install -m 644 dahdi-fw-te820.bin $(DESTDIR)/usr/lib/hotplug/firmware
	@rm -rf $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-te820-*
	@touch $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw-te820-$(WCT820_VERSION)
	@install -m 644 dahdi-fw-te820.bin $(DESTDIR)/lib/firmware
	@rm -rf $(DESTDIR)/lib/firmware/.dahdi-fw-te820-*
	@touch $(DESTDIR)/lib/firmware/.dahdi-fw-te820-$(WCT820_VERSION)
else
	@echo "Firmware dahdi-fw-te820.bin is already installed with required version $(WCT820_VERSION)"
endif

# Uninstall any installed dahdi firmware images from hotplug firmware directories
hotplug-uninstall:
	if [ -d $(DESTDIR)/usr/lib/hotplug/firmware ]; then \
		rm -f $(DESTDIR)/usr/lib/hotplug/firmware/dahdi-fw-*.bin; \
		rm -f $(DESTDIR)/usr/lib/hotplug/firmware/.dahdi-fw*; \
	fi
	if [ -d $(DESTDIR)/lib/firmware ]; then \
		rm -f $(DESTDIR)/lib/firmware/dahdi-fw-*.bin; \
		rm -f $(DESTDIR)/lib/firmware/.dahdi-fw*; \
	fi

make_firmware_object: make_firmware_object.in ../dahdi-base.o
	@export BFDNAME=`LANG=C objdump -f ../dahdi-base.o | grep -e "dahdi-base.o:" | sed "s/.*file format \(.*\)/\1/"`; \
	 export BFDARCH=`LANG=C objdump -f ../dahdi-base.o | grep -e "architecture:" | sed "s/.*ture: \(.*\),.*/\1/"`; \
	 sed -e s/BFDNAME/$${BFDNAME}/ -e s/BFDARCH/$${BFDARCH}/ $< > $@
	@chmod +x $@

# Build object file of an oct6114 064 firmware image for linking
dahdi-fw-oct6114-064.o: dahdi-fw-oct6114-064-$(OCT6114_064_VERSION).tar.gz dahdi-fw-oct6114-064.bin make_firmware_object
	@echo Making firmware object file for dahdi-fw-oct6114-064.bin
	./make_firmware_object dahdi-fw-oct6114-064.bin $@

# Build object file of an oct6114 128 firmware image for linking
dahdi-fw-oct6114-128.o: dahdi-fw-oct6114-128-$(OCT6114_128_VERSION).tar.gz dahdi-fw-oct6114-128.bin make_firmware_object
	@echo Making firmware object file for dahdi-fw-oct6114-128.bin
	./make_firmware_object dahdi-fw-oct6114-128.bin $@

# Build object file of an oct6114 256 firmware image for linking
dahdi-fw-oct6114-256.o: dahdi-fw-oct6114-256-$(OCT6114_256_VERSION).tar.gz dahdi-fw-oct6114-256.bin make_firmware_object
	@echo Making firmware object file for dahdi-fw-oct6114-256.bin
	./make_firmware_object dahdi-fw-oct6114-256.bin $@

# Build object file of a TC400M firmware image for linking
dahdi-fw-tc400m.o: dahdi-fw-tc400m-$(TC400M_VERSION).tar.gz dahdi-fw-tc400m.bin make_firmware_object
	@echo Making firmware object file for dahdi-fw-tc400m.bin
	./make_firmware_object dahdi-fw-tc400m.bin $@

# Build object file of a VPMOCT032 firmware image for linking
dahdi-fw-vpmoct032.o: dahdi-fw-vpmoct032-$(VPMOCT032_VERSION).tar.gz dahdi-fw-vpmoct032.bin make_firmware_object
	@echo Making firmware object file for dahdi-fw-vpmoct032.bin
	./make_firmware_object dahdi-fw-vpmoct032.bin $@
