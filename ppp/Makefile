#COPTS	= -O2 -g

-include ../makeopts

CFLAGS		+= $(COPTS) -fPIC
LDFLAGS		+= -shared

INCLUDE_DIR	= $(includedir)/pppd

LIBDIR		= $(libdir)/pppd/$(PPPD_VERSION)

PLUGINS := dahdi.so

all:	$(PLUGINS)

%.so: %.c
ifeq (,$(PPPD_VERSION))
	@echo "pppd version not found (in patchlevel.h)."
	@echo "Install ppp source/headers and/or ./configure --with-ppp=PATH."
	exit 1
endif
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) 

install: $(PLUGINS)
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -m 0644 $? $(DESTDIR)$(LIBDIR)

clean:
	rm -f *.o *.so *.a
