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

USE_OCTASIC	:= yes
OCTASIC_DIR	:= oct612x

ifneq (no,$(USE_OCTASIC))

OCT_OBJS	= $(shell $(OCTASIC_DIR)/octasic-helper objects $(OCTASIC_DIR))
OCT_SRCS	= $(shell echo $(OCT_OBJS) | tr -s ' ' '\n' | sed 's/\.o$$/.c/g')
OCT_HERE_OBJS	= $(shell echo $(OCT_OBJS) | tr -s ' ' '\n' | sed 's,^.*/,,')
OCT_CFLAGS	= $(shell $(OCTASIC_DIR)/octasic-helper cflags $(OCTASIC_DIR))
OCT_DEFINES	= \
	-DPTR_TYPE=uint32_t	\
	-DcOCT6100_INTERNAL_SUPER_ARRAY_SIZE=1024	\
	-DcOCT6100_MAX_ECHO_CHANNELS=672		\
	-DcOCT6100_MAX_MIXER_EVENTS=1344

ECHO_LOADER	= echo_loader.o
endif

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

XTALK_OBJS		= xtalk/xtalk.o xtalk/xusb.o xtalk/xlist.o xtalk/debug.o
ASTRIBANK_OBJS		= astribank_usb.o mpptalk.o $(XTALK_OBJS)

ABHEXLOAD_OBJS		= astribank_hexload.o hexfile.o pic_loader.o $(ECHO_LOADER) $(ASTRIBANK_OBJS) $(OCT_HERE_OBJS)
ABTOOL_OBJS		= astribank_tool.o $(ASTRIBANK_OBJS)
ABALLOW_OBJS		= astribank_allow.o astribank_license.o $(ASTRIBANK_OBJS)

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

CFLAGS		+= -I. -Ixtalk

fpga_load: fpga_load.o hexfile.o
fpga_load: LIBS+=$(EXTRA_LIBS) $(USB_LIB)

astribank_hexload: $(ABHEXLOAD_OBJS)
astribank_hexload: LIBS+=$(EXTRA_LIBS) $(USB_LIB)
astribank_hexload: CFLAGS+=$(OCT_CFLAGS)

astribank_tool: $(ABTOOL_OBJS)
astribank_tool: LIBS+=$(EXTRA_LIBS) $(USB_LIB)

astribank_allow: $(ABALLOW_OBJS)
astribank_allow: LIBS+=$(EXTRA_LIBS) $(USB_LIB)

astribank_is_starting: astribank_is_starting.o
astribank_is_starting: LIBS+=$(EXTRA_LIBS)

fpga_load.o: CFLAGS+=-D_GNU_SOURCE	# We use memrchr()

hex2iic: hex2iic.o iic.o hexfile.o

test_parse: test_parse.o hexfile.o
test_parse: LIBS+=$(EXTRA_LIBS) $(USB_LIB)

ifneq (no,$(USE_OCTASIC))
.octasic.depend: $(OCTASIC_DIR)/octasic-helper Makefile ../config.status
	$(CC) -MM $(OCT_CFLAGS) \
		`$(OCTASIC_DIR)/octasic-helper objects | \
		tr -s ' ' '\n' | \
		sed -e 's,.*,$(OCTASIC_DIR)/&,' -e 's/\.o$$/.c/'` > $@

-include .octasic.depend

$(OCT_HERE_OBJS): Makefile
	$(CC) -c $(CFLAGS) $(OCT_CFLAGS) $(OCT_DEFINES) $(OCT_SRCS)

endif


%: %.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

.perlcheck: $(PERL_SCRIPTS)
	for i in $^; do perl -I./perl_modules -c $$i || exit 1; done
	touch $@

clean:
	$(RM) .depend .octasic.depend *.o xtalk/*.o $(OCT_HERE_OBJS) $(TARGETS)

.PHONY: depend
depend: .depend
.depend: *.c *.h xtalk/*.c xtalk/*.h
	@$(CC) $(CFLAGS) -MM *.c xtalk/*.c > $@ || rm -f $@

include .depend
