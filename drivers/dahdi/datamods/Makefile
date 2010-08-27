.EXPORT_ALL_VARIABLES:
MODULES= \
	hdlc_cisco  hdlc_generic  hdlc_raw      syncppp \
	hdlc_fr     hdlc_ppp      hdlc_raw_eth  


PWD=$(shell pwd)

MODULESO:=$(MODULES:%=%.o)
MODULESKO:=$(MODULES:%=%.ko)
KMAKE = $(MAKE) -C $(KSRC) SUBDIRS=$(PWD)
KMAKE_INST = $(KMAKE) \
  INSTALL_MOD_PATH=$(INSTALL_PREFIX) INSTALL_MOD_DIR=misc modules_install

obj-m := $(MODULESO)
#obj-m:=hdlc_raw.o hdlc_cisco.o
#obj-m := hdlc_cisco.o hdlc_cisco.mod.o hdlc_fr.o hdlc_generic.o hdlc_ppp.o hdlc_raw.o hdlc_raw_eth.o hdlc_raw.mod.o hdlc_x25.o

all: 
	@echo "You don't want to do make here.  Do it from up above"

clean: 
	$(KMAKE) clean

install: $(MODULESKO)
	$(KMAKE_INST)

datamods:
	@echo "To build: $(obj-m)"
	@echo $(KSRC)
	@if [ -z "$(KSRC)" -o ! -d "$(KSRC)" ]; then echo "You do not appear to have the sources for the $(KVERS) kernel installed."; exit 1 ; fi
	$(KMAKE) modules
