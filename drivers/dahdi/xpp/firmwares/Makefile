# Install firmwares and initialization scripts for the Astribank
# drivers

FPGA_FW		= FPGA_FXS.hex FPGA_1141.hex FPGA_1151.hex FPGA_1161.hex
PIC_FW		= PIC_TYPE_1.hex PIC_TYPE_2.hex PIC_TYPE_3.hex PIC_TYPE_4.hex  
FIRMWARES	= USB_FW.hex $(FPGA_FW) $(PIC_FW)
PROTO_VER	= 30
SCRIPTS_BASE	= $(patsubst %,init_card_%_$(PROTO_VER),1 2 3 4)
SCRIPTS		= $(SCRIPTS_BASE:%=../%)
TARGET		= $(DESTDIR)/usr/share/dahdi

all:

clean:

install:
	mkdir -p $(TARGET)
	install $(SCRIPTS) $(TARGET)/
	install -m 644 ../XppConfig.pm $(FIRMWARES) $(TARGET)/

