#
# Makefile for DAHDI tools
#
# Copyright (C) 2001-2010 Digium, Inc.
#
#

# If the file .dahdi.makeopts is present in your home directory, you can
# include all of your favorite menuselect options so that every time you download
# a new version of Asterisk, you don't have to run menuselect to set them.
# The file /etc/dahdi.makeopts will also be included but can be overridden
# by the file in your home directory.

GLOBAL_MAKEOPTS=$(wildcard /etc/dahdi.makeopts)
USER_MAKEOPTS=$(wildcard ~/.dahdi.makeopts)

ifeq ($(strip $(foreach var,clean distclean dist-clean update,$(findstring $(var),$(MAKECMDGOALS)))),)
 ifneq ($(wildcard menuselect.makeopts),)
  include menuselect.makeopts
 endif
endif

ifeq ($(strip $(foreach var,clean distclean dist-clean update,$(findstring $(var),$(MAKECMDGOALS)))),)
 ifneq ($(wildcard makeopts),)
  include makeopts
 endif
endif

SUBDIRS_UTILS_ALL:= ppp
SUBDIRS_UTILS := xpp

OPTFLAGS=-O2
CFLAGS+=-I. $(OPTFLAGS) -g -fPIC -Wall -DBUILDING_TONEZONE #-DTONEZONE_DRIVER
ifneq (,$(findstring ppc,$(UNAME_M)))
CFLAGS+=-fsigned-char
endif
ifneq (,$(findstring x86_64,$(UNAME_M)))
CFLAGS+=-m64
endif

ifeq ($(DAHDI_DEVMODE),yes)
  CFLAGS+=-Werror -Wunused -Wundef $(DAHDI_DECLARATION_AFTER_STATEMENT) -Wmissing-format-attribute -Wformat-security #-Wformat=2
endif

ROOT_PREFIX=

# extra cflags to build dependencies. Recursively expanded.
MAKE_DEPS= -MD -MT $@ -MF .$(subst /,_,$@).d -MP

CFLAGS+=$(DAHDI_INCLUDE)

CHKCONFIG	:= $(wildcard /sbin/chkconfig)
UPDATE_RCD	:= $(wildcard /usr/sbin/update-rc.d)
ifeq (,$(DESTDIR))
  ifneq (,$(CHKCONFIG))
    ADD_INITD	:= $(CHKCONFIG) --add dahdi
  else
    ifneq (,$(UPDATE_RCD))
      ADD_INITD	:= $(UPDATE_RCD) dahdi defaults 15 30
    endif
  endif
endif

INITRD_DIR	:= $(firstword $(wildcard $(DESTDIR)/etc/rc.d/init.d $(DESTDIR)/etc/init.d))
ifneq (,$(INITRD_DIR))
  INIT_TARGET	:= $(INITRD_DIR)/dahdi
  COPY_INITD	:= install -D dahdi.init $(INIT_TARGET)
endif

RCCONF_FILE	= /etc/dahdi/init.conf
MODULES_FILE	= /etc/dahdi/modules
GENCONF_FILE	= /etc/dahdi/genconf_parameters
MODPROBE_FILE	= /etc/modprobe.d/dahdi.conf
BLACKLIST_FILE	= /etc/modprobe.d/dahdi.blacklist.conf

NETSCR_DIR	:= $(firstword $(wildcard $(DESTDIR)/etc/sysconfig/network-scripts ))
ifneq (,$(NETSCR_DIR))
  NETSCR_TARGET	:= $(NETSCR_DIR)/ifup-hdlc
  COPY_NETSCR	:= install -D ifup-hdlc $(NETSCR_TARGET)
endif

ifneq ($(wildcard .version),)
  TOOLSVERSION:=$(shell cat .version)
else
ifneq ($(wildcard .svn),)
  TOOLSVERSION=$(shell build_tools/make_version . dahdi/tools)
endif
endif

LTZ_A:=libtonezone.a
LTZ_A_OBJS:=zonedata.o tonezone.o version.o
LTZ_SO:=libtonezone.so
LTZ_SO_OBJS:=zonedata.lo tonezone.lo version.o
LTZ_SO_MAJOR_VER:=2
LTZ_SO_MINOR_VER:=0

# sbindir, libdir, includedir and mandir are defined in makeopts
# (from configure).
BIN_DIR:=$(sbindir)
LIB_DIR:=$(libdir)
INC_DIR:=$(includedir)/dahdi
MAN_DIR:=$(mandir)/man8
CONFIG_DIR:=$(sysconfdir)/dahdi
CONFIG_FILE:=$(CONFIG_DIR)/system.conf

# Utilities we build with a standard build procedure:
UTILS		= dahdi_tool dahdi_test dahdi_monitor dahdi_speed sethdlc dahdi_cfg \
		  fxstest fxotune dahdi_diag dahdi_scan

# some tests:
UTILS		+= patgen pattest patlooptest hdlcstress hdlctest hdlcgen \
		   hdlcverify timertest dahdi_maint

BINS:=fxotune fxstest sethdlc dahdi_cfg dahdi_diag dahdi_monitor dahdi_speed dahdi_test dahdi_scan dahdi_tool dahdi_maint
BINS:=$(filter-out $(MENUSELECT_UTILS),$(BINS))
MAN_PAGES:=$(wildcard $(BINS:%=doc/%.8))

TEST_BINS:=patgen pattest patlooptest hdlcstress hdlctest hdlcgen hdlcverify timertest dahdi_maint
# All the man pages. Not just installed ones:
GROFF_PAGES	:= $(wildcard doc/*.8 xpp/*.8)
GROFF_HTML	:= $(GROFF_PAGES:%=%.html)

GENERATED_DOCS	:= $(GROFF_HTML) README.html README.Astribank.html

all: menuselect.makeopts 
	@$(MAKE) _all

_all: prereq programs

libs: $(LTZ_SO) $(LTZ_A)

utils-subdirs:
	@for dir in $(SUBDIRS_UTILS); do \
		$(MAKE) -C $$dir; \
	done

programs: libs utils

utils: $(BINS) utils-subdirs

version.c: FORCE
	@TOOLSVERSION="${TOOLSVERSION}" build_tools/make_version_c > $@.tmp
	@if cmp -s $@.tmp $@ ; then :; else \
		mv $@.tmp $@ ; \
	fi
	@rm -f $@.tmp

tests: $(TEST_BINS)

$(UTILS): %: %.o

$(UTILS): version.o

%.o: %.c
	$(CC) $(CFLAGS) $(MAKE_DEPS) -c -o $@ $<

%.lo: %.c
	$(CC) $(CFLAGS) $(MAKE_DEPS) -c -o $@ $<

%: %.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

prereq: config.status

dahdi_tool: CFLAGS+=$(NEWT_INCLUDE)
dahdi_tool: LIBS+=$(NEWT_LIB)

dahdi_speed: CFLAGS+=-O0

$(LTZ_A): $(LTZ_A_OBJS)
	ar rcs $@ $^
	ranlib $@

$(LTZ_SO): $(LTZ_SO_OBJS)
	$(CC) $(CFLAGS) -shared -Wl,-soname,$(LTZ_SO).$(LTZ_SO_MAJOR_VER).$(LTZ_SO_MINOR_VER) -o $@ $^ -lm

dahdi_cfg: $(LTZ_A)
dahdi_cfg: LIBS+=-lm
dahdi_pcap:
	$(CC) $(CFLAGS) dahdi_pcap.c -lpcap -o $@ $<
	

fxstest: $(LTZ_SO)
fxstest: LIBS+=-lm
fxotune: LIBS+=-lm

tonezones.txt: zonedata.c
	perl -ne 'next unless (/\.(country|description) = *"([^"]*)/); \
		print (($$1 eq "country")? "* $$2\t":"$$2\n");' $<  \
	>$@

%.asciidoc: %.sample
	perl -n -e \
		'if (/^#($$|\s)(.*)/){ if (!$$in_doc){print "\n"}; $$in_doc=1; print "$$2\n" } else { if ($$in_doc){print "\n"}; $$in_doc=0; print "  $$_" }' \
		$< \
	| perl -p -e 'if (/^  #?(\w+)=/ && ! exists $$cfgs{$$1}){my $$cfg = $$1; $$cfgs{$$cfg} = 1; s/^/\n[[cfg_$$cfg]]\n/}'  >$@

docs: $(GENERATED_DOCS)

genconf_parameters.sample: xpp/genconf_parameters
	cp $< $@

README.html: README system.conf.asciidoc init.conf.asciidoc tonezones.txt \
  UPGRADE.txt genconf_parameters.asciidoc
	$(ASCIIDOC) -n -a toc -a toclevels=3 $<

README.Astribank.html: xpp/README.Astribank
	$(ASCIIDOC) -o $@ -n -a toc -a toclevels=4 $<

# on Debian: this requires the full groof, not just groff-base.
%.8.html: %.8
	man -Thtml $^ >$@

htmlman: $(GROFF_HTML)

install: all install-programs
	@echo "###################################################"
	@echo "###"
	@echo "### DAHDI tools installed successfully."
	@echo "### If you have not done so before, install init scripts with:"
	@echo "###"
	@echo "###   make config"
	@echo "###"
	@echo "###################################################"

install-programs: install-utils install-libs

install-utils: utils install-utils-subdirs
ifneq (,$(BINS))
	install -d $(DESTDIR)$(BIN_DIR)
	install  $(BINS) $(DESTDIR)$(BIN_DIR)/
	install -d $(DESTDIR)$(MAN_DIR)
	install -m 644 $(MAN_PAGES) $(DESTDIR)$(MAN_DIR)/
endif
ifeq (,$(wildcard $(DESTDIR)$(CONFIG_FILE)))
	$(INSTALL) -d $(DESTDIR)$(CONFIG_DIR)
	$(INSTALL) -m 644 system.conf.sample $(DESTDIR)$(CONFIG_FILE)
endif

install-libs: libs
	$(INSTALL) -d -m 755 $(DESTDIR)/$(LIB_DIR)
	$(INSTALL) -m 755 $(LTZ_A) $(DESTDIR)$(LIB_DIR)/
	$(INSTALL) -m 755 $(LTZ_SO) $(DESTDIR)$(LIB_DIR)/$(LTZ_SO).$(LTZ_SO_MAJOR_VER).$(LTZ_SO_MINOR_VER)
ifeq (,$(DESTDIR))
	if [ `id -u` = 0 ]; then \
		/sbin/ldconfig || : ;\
	fi
endif
	rm -f $(DESTDIR)$(LIB_DIR)/$(LTZ_SO)
	$(LN) -sf $(LTZ_SO).$(LTZ_SO_MAJOR_VER).$(LTZ_SO_MINOR_VER) \
		$(DESTDIR)$(LIB_DIR)/$(LTZ_SO).$(LTZ_SO_MAJOR_VER)
	$(LN) -sf $(LTZ_SO).$(LTZ_SO_MAJOR_VER).$(LTZ_SO_MINOR_VER) \
		$(DESTDIR)$(LIB_DIR)/$(LTZ_SO)
	# Overwrite the 1.0 links out there.  dahdi-tools 2.0.0 installed
	# 1.0 links but dahdi-tools changed them to 2.0 in order to explicitly
	# break applications linked with zaptel.  But, this also meant that
	# applications linked with libtonezone.so.1.0 broke when dahdi-tools
	# 2.1.0 was installed.
	$(LN) -sf $(LTZ_SO).$(LTZ_SO_MAJOR_VER).$(LTZ_SO_MINOR_VER) \
		$(DESTDIR)$(LIB_DIR)/$(LTZ_SO).1.0
	$(LN) -sf $(LTZ_SO).$(LTZ_SO_MAJOR_VER).$(LTZ_SO_MINOR_VER) \
		$(DESTDIR)$(LIB_DIR)/$(LTZ_SO).1
ifneq (no,$(USE_SELINUX))
  ifeq (,$(DESTDIR))
	/sbin/restorecon -v $(DESTDIR)$(LIB_DIR)/$(LTZ_SO)
  endif
endif
	$(INSTALL) -d -m 755 $(DESTDIR)/$(INC_DIR)
	$(INSTALL) -m 644 tonezone.h $(DESTDIR)$(INC_DIR)/

install-utils-subdirs:
	@for dir in $(SUBDIRS_UTILS); do \
		$(MAKE) -C $$dir install; \
	done

install-tests: tests
ifneq (,$(TEST_BINS))
	install -d $(DESTDIR)$(BIN_DIR)
	install  $(TEST_BINS) $(DESTDIR)$(BIN_DIR)/
endif

config:
ifneq (,$(COPY_INITD))
	$(COPY_INITD)
endif
ifeq (,$(wildcard $(DESTDIR)$(RCCONF_FILE)))
	$(INSTALL) -D -m 644 init.conf.sample $(DESTDIR)$(RCCONF_FILE)
endif
ifeq (,$(wildcard $(DESTDIR)$(MODULES_FILE)))
	$(INSTALL) -D -m 644 modules.sample $(DESTDIR)$(MODULES_FILE)
endif
ifeq (,$(wildcard $(DESTDIR)$(GENCONF_FILE)))
	$(INSTALL) -D -m 644 xpp/genconf_parameters $(DESTDIR)$(GENCONF_FILE)
endif
ifeq (,$(wildcard $(DESTDIR)$(MODPROBE_FILE)))
	$(INSTALL) -D -m 644 modprobe.conf.sample $(DESTDIR)$(MODPROBE_FILE)
endif
ifeq (,$(wildcard $(DESTDIR)$(BLACKLIST_FILE)))
	$(INSTALL) -D -m 644 blacklist.sample $(DESTDIR)$(BLACKLIST_FILE)
endif
ifneq (,$(COPY_NETSCR))
	$(COPY_NETSCR)
endif
ifneq (,$(ADD_INITD))
	$(ADD_INITD)
endif
	@echo "DAHDI has been configured."
	@echo ""
	@echo "List of detected DAHDI devices:"
	@echo ""
	@if [ `xpp/dahdi_hardware | tee /dev/stderr | wc -l` -eq 0 ]; then \
		echo "No hardware found"; \
	else \
		echo ""; \
		echo "run 'dahdi_genconf modules' to load support for only " ;\
		echo "the DAHDI hardware installed in this system.  By "; \
		echo "default support for all DAHDI hardware is loaded at "; \
		echo "DAHDI start. "; \
	fi

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
	-@$(MAKE) -C menuselect clean
	rm -f $(BINS) $(TEST_BINS)
	rm -f *.o dahdi_cfg tzdriver sethdlc
	rm -f $(LTZ_SO) $(LTZ_A) *.lo
	@for dir in $(SUBDIRS_UTILS_ALL); do \
		$(MAKE) -C $$dir clean; \
	done
	@for dir in $(SUBDIRS_UTILS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -f libtonezone*
	rm -f fxotune
	rm -f core
	rm -f dahdi_cfg-shared fxstest
	rm -rf $(GENERATED_DOCS) *.asciidoc tonezones.txt
	rm -f dahdi_pcap

distclean: dist-clean

dist-clean: clean
	@$(MAKE) -C menuselect dist-clean
	rm -f makeopts menuselect.makeopts menuselect-tree build_tools/menuselect-deps
	rm -f config.log config.status
	rm -f .*.d

config.status: configure
	@CFLAGS="" ./configure
	@echo "****"
	@echo "**** The configure script was just executed, so 'make' needs to be"
	@echo "**** restarted."
	@echo "****"
	@exit 1

menuselect.makeopts: menuselect/menuselect menuselect-tree makeopts
	menuselect/menuselect --check-deps $@ $(GLOBAL_MAKEOPTS) $(USER_MAKEOPTS)

menuconfig: menuselect

cmenuconfig: cmenuselect

gmenuconfig: gmenuselect

nmenuconfig: nmenuselect

menuselect: menuselect/cmenuselect menuselect/nmenuselect menuselect/gmenuselect
	@if [ -x menuselect/nmenuselect ]; then \
		$(MAKE) nmenuselect; \
	elif [ -x menuselect/cmenuselect ]; then \
		$(MAKE) cmenuselect; \
	elif [ -x menuselect/gmenuselect ]; then \
		$(MAKE) gmenuselect; \
	else \
		echo "No menuselect user interface found. Install ncurses,"; \
		echo "newt or GTK libraries to build one and re-rerun"; \
		echo "'make menuselect'."; \
	fi

cmenuselect: menuselect/cmenuselect menuselect-tree
	-@menuselect/cmenuselect menuselect.makeopts $(GLOBAL_MAKEOPTS) $(USER_MAKEOPTS) && echo "menuselect changes saved!" || echo "menuselect changes NOT saved!"

gmenuselect: menuselect/gmenuselect menuselect-tree
	-@menuselect/gmenuselect menuselect.makeopts $(GLOBAL_MAKEOPTS) $(USER_MAKEOPTS) && echo "menuselect changes saved!" || echo "menuselect changes NOT saved!"

nmenuselect: menuselect/nmenuselect menuselect-tree
	-@menuselect/nmenuselect menuselect.makeopts $(GLOBAL_MAKEOPTS) $(USER_MAKEOPTS) && echo "menuselect changes saved!" || echo "menuselect changes NOT saved!"

# options for make in menuselect/
MAKE_MENUSELECT=CC="$(HOST_CC)" CXX="$(CXX)" LD="" AR="" RANLIB="" CFLAGS="" $(MAKE) -C menuselect CONFIGURE_SILENT="--silent"

menuselect/menuselect: menuselect/makeopts
	+$(MAKE_MENUSELECT) menuselect

menuselect/cmenuselect: menuselect/makeopts
	+$(MAKE_MENUSELECT) cmenuselect

menuselect/gmenuselect: menuselect/makeopts
	+$(MAKE_MENUSELECT) gmenuselect

menuselect/nmenuselect: menuselect/makeopts
	+$(MAKE_MENUSELECT) nmenuselect

menuselect/makeopts: makeopts
	+$(MAKE_MENUSELECT) makeopts

menuselect-tree: dahdi.xml
	@echo "Generating input for menuselect ..."
	@build_tools/make_tree > $@

.PHONY: menuselect distclean dist-clean clean all _all install programs tests devel data config update install-programs install-libs install-utils-subdirs utils-subdirs prereq

FORCE:

ifneq ($(wildcard .*.d),)
   include .*.d
endif
