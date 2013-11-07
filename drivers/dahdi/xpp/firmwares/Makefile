# Install firmwares and initialization scripts for the Astribank
# drivers

USB_FW		= USB_FW.hex USB_FW.201.hex USB_RECOV.hex
FPGA_FW		= FPGA_FXS.hex FPGA_1141.hex FPGA_1151.hex FPGA_1161.hex  \
		  FPGA_1161.201.hex FPGA_1161.202.hex
PIC_FW		= PIC_TYPE_1.hex PIC_TYPE_2.hex PIC_TYPE_3.hex PIC_TYPE_4.hex  
OCT_FW		= $(wildcard OCT6104E-256D.ima)
FIRMWARES	= $(USB_FW) $(FPGA_FW) $(PIC_FW) $(OCT_FW)
PROTO_VER	= 30
SCRIPTS_BASE	= $(patsubst %,init_card_%_$(PROTO_VER),1 2 3 4 5)
SCRIPTS		= $(SCRIPTS_BASE:%=../%)
TARGET		= $(DESTDIR)/usr/share/dahdi

all:

clean:

install:
	mkdir -p $(TARGET)
	install $(SCRIPTS) $(TARGET)/
	install -m 644 ../XppConfig.pm $(FIRMWARES) $(TARGET)/
	if [ ! -r $(TARGET)/USB_FW.202.hex ]; then \
		ln -s USB_FW.201.hex $(TARGET)/USB_FW.202.hex;\
	fi

