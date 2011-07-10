CFLAGS=-V3.4 -ffunction-sections -I/lib/modules/$(shell uname -r)/build/include -Iinclude -Ioctdeviceapi -Ioctdeviceapi/oct6100api -DGFP_ATOMIC=0 -Dkmalloc=calloc -Dkfree=free
LDFLAGS=-V3.4 -Wl,-Map -Wl,test.map -Wl,--gc-sections

APIDIR=octdeviceapi/oct6100api/oct6100_api

OCTASIC_OBJS=$(APIDIR)/oct6100_adpcm_chan.o \
	    $(APIDIR)/oct6100_channel.o \
	    $(APIDIR)/oct6100_chip_open.o \
	    $(APIDIR)/oct6100_chip_stats.o \
	    $(APIDIR)/oct6100_conf_bridge.o \
	    $(APIDIR)/oct6100_debug.o \
	    $(APIDIR)/oct6100_events.o \
	    $(APIDIR)/oct6100_interrupts.o \
	    $(APIDIR)/oct6100_memory.o \
	    $(APIDIR)/oct6100_miscellaneous.o \
	    $(APIDIR)/oct6100_mixer.o \
	    $(APIDIR)/oct6100_phasing_tsst.o \
	    $(APIDIR)/oct6100_playout_buf.o \
	    $(APIDIR)/oct6100_remote_debug.o \
	    $(APIDIR)/oct6100_tlv.o \
	    $(APIDIR)/oct6100_tone_detection.o \
	    $(APIDIR)/oct6100_tsi_cnct.o \
	    $(APIDIR)/oct6100_tsst.o \
	    $(APIDIR)/oct6100_user.o \
	    apilib/bt/octapi_bt0.o \
	    apilib/largmath/octapi_largmath.o \
	    apilib/llman/octapi_llman.o


all: test

test.o: test.c

test: test.o $(OCTASIC_OBJS)

clean:
	rm -rf test test.o
	rm -rf $(OCTASIC_OBJS)
