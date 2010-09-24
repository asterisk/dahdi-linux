#
# Makefile for DAHDI Linux kernel modules
#
# Copyright (C) 2001-2010 Digium, Inc.
#
#

PWD:=$(shell pwd)

DAHDI_MODULES_EXTRA:=$(MODULES_EXTRA:%=%.o) $(SUBDIRS_EXTRA:%=%/)

# If you want to build for a kernel other than the current kernel, set KVERS
ifndef KVERS
KVERS:=$(shell uname -r)
endif
ifndef KSRC
  ifneq (,$(wildcard /lib/modules/$(KVERS)/build))
    KSRC:=/lib/modules/$(KVERS)/build
  else
    KSRC_SEARCH_PATH:=/usr/src/linux
    KSRC:=$(shell for dir in $(KSRC_SEARCH_PATH); do if [ -d $$dir ]; then echo $$dir; break; fi; done)
  endif
endif
KVERS_MAJ:=$(shell echo $(KVERS) | cut -d. -f1-2)
KINCLUDES:=$(KSRC)/include

# We use the kernel's .config file as an indication that the KSRC
# directory is indeed a valid and configured kernel source (or partial
# source) directory.
#
# We also source it, as it has the format of Makefile variables list.
# Thus we will have many CONFIG_* variables from there.
KCONFIG:=$(KSRC)/.config
ifneq (,$(wildcard $(KCONFIG)))
  HAS_KSRC:=yes
  include $(KCONFIG)
else
  HAS_KSRC:=no
endif

CHECKSTACK=$(KSRC)/scripts/checkstack.pl

# Set HOTPLUG_FIRMWARE=no to override automatic building with hotplug support
# if it is enabled in the kernel.

ifeq (yes,$(HAS_KSRC))
  HOTPLUG_FIRMWARE:=$(shell if grep -q '^CONFIG_FW_LOADER=[ym]' $(KCONFIG); then echo "yes"; else echo "no"; fi)
endif

UDEV_DIR:=/etc/udev/rules.d

MODULE_ALIASES:=wcfxs wctdm8xxp wct2xxp

INST_HEADERS:=kernel.h user.h fasthdlc.h wctdm_user.h dahdi_config.h

DAHDI_BUILD_ALL:=m

KMAKE=+$(MAKE) -C $(KSRC) SUBDIRS=$(PWD)/drivers/dahdi DAHDI_INCLUDE=$(PWD)/include DAHDI_MODULES_EXTRA="$(DAHDI_MODULES_EXTRA)" HOTPLUG_FIRMWARE=$(HOTPLUG_FIRMWARE)

ROOT_PREFIX:=

ASCIIDOC:=asciidoc
ASCIIDOC_CMD:=$(ASCIIDOC) -n -a toc -a toclevels=4

GENERATED_DOCS:=README.html

ifneq ($(wildcard .version),)
  DAHDIVERSION:=$(shell cat .version)
else
ifneq ($(wildcard .svn),)
  DAHDIVERSION:=$(shell build_tools/make_version . dahdi/linux)
else
ifneq ($(wildcard .git),)
  DAHDIVERSION:=$(shell build_tools/make_version . dahdi/linux)
endif
endif
endif

all: modules

modules: prereq
ifeq (no,$(HAS_KSRC))
	@echo "You do not appear to have the sources for the $(KVERS) kernel installed."
	@exit 1
endif
	$(KMAKE) modules DAHDI_BUILD_ALL=$(DAHDI_BUILD_ALL)

include/dahdi/version.h: FORCE
	@DAHDIVERSION="${DAHDIVERSION}" build_tools/make_version_h > $@.tmp
	@if cmp -s $@.tmp $@ ; then :; else \
		mv $@.tmp $@ ; \
	fi
	@rm -f $@.tmp

prereq: include/dahdi/version.h firmware-loaders

stackcheck: $(CHECKSTACK) modules
	objdump -d drivers/dahdi/*.ko drivers/dahdi/*/*.ko | $(CHECKSTACK)

install: all install-modules install-devices install-include install-firmware install-xpp-firm
	@echo "###################################################"
	@echo "###"
	@echo "### DAHDI installed successfully."
	@echo "### If you have not done so before, install the package"
	@echo "### dahdi-tools."
	@echo "###"
	@echo "###################################################"

uninstall: uninstall-modules uninstall-devices uninstall-include uninstall-firmware

install-modconf:
	build_tools/genmodconf $(BUILDVER) "$(ROOT_PREFIX)" "$(filter-out dahdi dahdi_dummy xpp dahdi_transcode dahdi_dynamic,$(BUILD_MODULES)) $(MODULE_ALIASES)"
	@if [ -d /etc/modutils ]; then \
		/sbin/update-modules ; \
	fi

install-xpp-firm:
	$(MAKE) -C drivers/dahdi/xpp/firmwares install

install-firmware:
ifeq ($(HOTPLUG_FIRMWARE),yes)
	$(MAKE) -C drivers/dahdi/firmware hotplug-install DESTDIR=$(DESTDIR) HOTPLUG_FIRMWARE=$(HOTPLUG_FIRMWARE)
endif

uninstall-firmware:
	$(MAKE) -C drivers/dahdi/firmware hotplug-uninstall DESTDIR=$(DESTDIR)

firmware-loaders:
	$(MAKE) -C drivers/dahdi/firmware firmware-loaders

install-include:
	for hdr in $(INST_HEADERS); do \
		install -D -m 644 include/dahdi/$$hdr $(DESTDIR)/usr/include/dahdi/$$hdr; \
	done
	@rm -rf $(DESTDIR)/usr/include/zaptel

uninstall-include:
	for hdr in $(INST_HEADERS); do \
		rm -f $(DESTDIR)/usr/include/dahdi/$$hdr; \
	done
	-rmdir $(DESTDIR)/usr/include/dahdi

install-devices:
	install -d $(DESTDIR)$(UDEV_DIR)
	build_tools/genudevrules > $(DESTDIR)$(UDEV_DIR)/dahdi.rules
	install -m 644 drivers/dahdi/xpp/xpp.rules $(DESTDIR)$(UDEV_DIR)/

uninstall-devices:
	rm -f $(DESTDIR)$(UDEV_DIR)/dahdi.rules

install-modules: modules
ifndef DESTDIR
	@if modinfo zaptel > /dev/null 2>&1; then \
		echo -n "Removing Zaptel modules for kernel $(KVERS), please wait..."; \
		build_tools/uninstall-modules zaptel $(KVERS); \
		rm -rf /lib/modules/$(KVERS)/zaptel; \
		echo "done."; \
	fi
	build_tools/uninstall-modules dahdi $(KVERS)
endif
	$(KMAKE) INSTALL_MOD_PATH=$(DESTDIR) INSTALL_MOD_DIR=dahdi modules_install
	[ `id -u` = 0 ] && /sbin/depmod -a $(KVERS) || :

uninstall-modules:
ifdef DESTDIR
	@echo "Uninstalling modules is not supported with a DESTDIR specified."
	@exit 1
else
	@if modinfo dahdi > /dev/null 2>&1 ; then \
		echo -n "Removing DAHDI modules for kernel $(KVERS), please wait..."; \
		build_tools/uninstall-modules dahdi $(KVERS); \
		rm -rf /lib/modules/$(KVERS)/dahdi; \
		echo "done."; \
	fi
	[ `id -u` = 0 ] && /sbin/depmod -a $(KVERS) || :
endif

update:
	@if [ -d .svn ]; then \
		echo "Updating from Subversion..." ; \
		svn update | tee update.out; \
		rm -f .version; \
		if [ `grep -c ^C update.out` -gt 0 ]; then \
			echo ; echo "The following files have conflicts:" ; \
			grep ^C update.out | cut -b4- ; \
		fi ; \
		rm -f update.out; \
	else \
		echo "Not under version control";  \
	fi

clean:
ifneq (no,$(HAS_KSRC))
	$(KMAKE) clean
endif
	@rm -f $(GENERATED_DOCS)
	$(MAKE) -C drivers/dahdi/firmware clean

distclean: dist-clean

dist-clean: clean
	@rm -f include/dahdi/version.h
	@$(MAKE) -C drivers/dahdi/firmware dist-clean
	@rm -f drivers/dahdi/vpmadt032_loader/*.o_shipped

firmware-download:
	@$(MAKE) -C drivers/dahdi/firmware all

test:
	./test-script $(DESTDIR)/lib/modules/$(KVERS) dahdi

docs: $(GENERATED_DOCS)

README.html: README
	$(ASCIIDOC_CMD) -o $@ $<

dahdi-api.html: drivers/dahdi/dahdi-base.c
	build_tools/kernel-doc --kernel $(KSRC) $^ >$@

.PHONY: distclean dist-clean clean all install devices modules stackcheck install-udev update install-modules install-include uninstall-modules firmware-download install-xpp-firm firmware-loaders

FORCE:
