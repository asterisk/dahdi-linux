PEDANTIC	= -ansi -pedantic -std=c99

INSTALL		= install
INSTALL_DATA	= install -m 644

#
# Ugly hack to find kernel directories before/after the split
# to kernel/user-space.
#
# These variables should be passed to us. But until then...
#
DAHDI_TOOLSDIR	?= ..
DAHDI_KERNELDIR	=

-include $(DAHDI_TOOLSDIR)/makeopts

INSTALL_DATA	= $(INSTALL) -m 644

# In 1.4 those are provided by autoconf through makeopts
prefix		?= /usr
datadir		?= $(prefix)/share
mandir		?= $(datadir)/man
INSTALL		?= install

INSTALL_DATA	= $(INSTALL) -m 644

SBINDIR		= $(prefix)/sbin
DATADIR		= $(datadir)/dahdi
MANDIR		= $(mandir)/man8
HOTPLUG_USB_DIR	= $(sysconfdir)/hotplug/usb
PERLLIBDIR	:= $(shell eval `perl -V:sitelib`; echo "$$sitelib")
PERL_DIRS	:= $(shell cd perl_modules; find * -name '[A-Z]*' -type d| xargs)
PERL_MODS_PAT	:= *.pm $(PERL_DIRS:%=%/*.pm)
PERL_MODS	:= $(shell cd perl_modules; echo $(PERL_MODS_PAT))

# Variables that should be defined above, but need sane defaults:
# FIXME: Are those values really sane?
HOSTCC		?= $(CC)


CFLAGS		+= -g -Wall $(USB_INCLUDE)

%.8: %
	pod2man --section 8 $^ > $@ || $(RM) $@
PERL_SCRIPTS	=	\
		dahdi_registration	\
		xpp_sync	\
		lsdahdi		\
		xpp_blink	\
		dahdi_genconf	\
		dahdi_hardware	\
		twinstar	\
		#

PERL_MANS	= $(PERL_SCRIPTS:%=%.8)

ABHEXLOAD_OBJS		= astribank_hexload.o hexfile.o pic_loader.o astribank_usb.o mpp_funcs.o debug.o
ABTOOL_OBJS		= astribank_tool.o astribank_usb.o mpp_funcs.o debug.o
ABALLOW_OBJS		= astribank_allow.o astribank_usb.o mpp_funcs.o debug.o

TARGETS	= .perlcheck astribank_is_starting
PROG_INSTALL	= astribank_is_starting
MAN_INSTALL	= $(PROG_INSTALL:%=%.8)
ifeq	(1,$(PBX_USB))
TARGETS	+= fpga_load		\
	   astribank_tool	\
	   astribank_hexload	\
	   astribank_allow	\
	   test_parse
PROG_INSTALL	+= fpga_load astribank_tool astribank_hexload astribank_allow
endif
ifneq	(,$(PERLLIBDIR))
PROG_INSTALL	+= $(PERL_SCRIPTS)
TARGETS		+= $(PERL_MANS)
endif

all: $(TARGETS)

docs: $(PERL_MANS)

install: all
	$(INSTALL) -d $(DESTDIR)$(SBINDIR)
	$(INSTALL) $(PROG_INSTALL) $(DESTDIR)$(SBINDIR)/
	$(INSTALL) -d $(DESTDIR)$(DATADIR)
	$(INSTALL) xpp_fxloader astribank_hook $(DESTDIR)$(DATADIR)/
	$(INSTALL) waitfor_xpds $(DESTDIR)$(DATADIR)/
	$(INSTALL) -d $(DESTDIR)$(MANDIR)
	$(INSTALL_DATA) $(MAN_INSTALL) $(DESTDIR)$(MANDIR)/
	$(INSTALL) -d $(DESTDIR)$(HOTPLUG_USB_DIR)
	$(INSTALL_DATA) xpp_fxloader.usermap $(DESTDIR)$(HOTPLUG_USB_DIR)/
	# for backward compatibility and for hotplug users:
	ln -sf $(DATADIR)/xpp_fxloader $(DESTDIR)$(HOTPLUG_USB_DIR)/
ifneq	(,$(PERLLIBDIR))
	$(INSTALL) -d $(DESTDIR)$(PERLLIBDIR)
	for i in $(PERL_DIRS); \
	do \
		$(INSTALL) -d "$(DESTDIR)$(PERLLIBDIR)/$$i"; \
	done
	for i in $(PERL_MODS); \
	do \
		$(INSTALL_DATA) "perl_modules/$$i" "$(DESTDIR)$(PERLLIBDIR)/$$i"; \
	done
endif

fpga_load: fpga_load.o hexfile.o
fpga_load: LIBS+=$(EXTRA_LIBS) $(USB_LIB)

astribank_hexload: $(ABHEXLOAD_OBJS)
astribank_hexload: LIBS+=$(EXTRA_LIBS) $(USB_LIB)

astribank_tool: $(ABTOOL_OBJS)
astribank_tool: LIBS+=$(EXTRA_LIBS) $(USB_LIB)

astribank_allow: $(ABALLOW_OBJS)
astribank_allow: LIBS+=$(EXTRA_LIBS) $(USB_LIB)

astribank_is_starting: astribank_is_starting.o
astribank_is_starting: LIBS+=$(EXTRA_LIBS)

fpga_load.o: CFLAGS+=-D_GNU_SOURCE	# We use memrchr()

test_parse: test_parse.o hexfile.o
test_parse: LIBS+=$(EXTRA_LIBS) $(USB_LIB)

%: %.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

.perlcheck: $(PERL_SCRIPTS)
	for i in $^; do perl -I./perl_modules -c $$i || exit 1; done
	touch $@

clean:
	$(RM) .depend *.o $(TARGETS)

.PHONY: depend
depend: .depend
.depend: *.c *.h
	@$(CC) -MM *.c > $@ || rm -f $@

include .depend
