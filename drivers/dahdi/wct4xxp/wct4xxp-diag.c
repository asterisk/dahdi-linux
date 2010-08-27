/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <dahdi/user.h>
#include "wct4xxp.h"

struct t4_reg_def {
	int reg;
	char *name;
	int global;
};
static struct t4_reg_def xreginfo[] = {
	{ 0x00, "RDADDR" },
	{ 0x01, "WRADDR" },
	{ 0x02, "COUNT" },
	{ 0x03, "DMACTRL" },
	{ 0x04, "WCINTR" },
	{ 0x06, "VERSION" },
	{ 0x07, "LEDS" },
	{ 0x08, "GPIOCTL" },
	{ 0x09, "GPIO" }, 
	{ 0x0A, "LADDR" },
	{ 0x0b, "LDATA" },
};

static struct t4_reg_def reginfo[] = {
	{ 0x00, "XFIFO" },
	{ 0x01, "XFIFO" },
	{ 0x02, "CMDR" },
	{ 0x03, "MODE" },
	{ 0x04, "RAH1" },
	{ 0x05, "RAH2" },
	{ 0x06, "RAL1" },
	{ 0x07, "RAL2" },
	{ 0x08, "IPC", 1 },
	{ 0x09, "CCR1" },
	{ 0x0a, "CCR2" },
	{ 0x0c, "RTR1" },
	{ 0x0d, "RTR2" },
	{ 0x0e, "RTR3" },
	{ 0x0f, "RTR4" },
	{ 0x10, "TTR1" },
	{ 0x11, "TTR2" },
	{ 0x12, "TTR3" },
	{ 0x13, "TTR4" },
	{ 0x14, "IMR0" },
	{ 0x15, "IMR1" },
	{ 0x16, "IMR2" },
	{ 0x17, "IMR3" },
	{ 0x18, "IMR4" },
	{ 0x1b, "IERR" },
	{ 0x1c, "FMR0" },
	{ 0x1d, "FMR1" },
	{ 0x1e, "FMR2" },
	{ 0x1f, "LOOP" },
	{ 0x20, "XSW" },
	{ 0x21, "XSP" },
	{ 0x22, "XC0" },
	{ 0x23, "XC1" },
	{ 0x24, "RC0" },
	{ 0x25, "RC1" },
	{ 0x26, "XPM0" },
	{ 0x27, "XPM1" },
	{ 0x28, "XPM2" },
	{ 0x29, "TSWM" },
	{ 0x2b, "IDLE" },
	{ 0x2c, "XSA4" },
	{ 0x2d, "XSA5" },
	{ 0x2e, "XSA6" },
	{ 0x2f, "XSA7" },
	{ 0x30, "XSA8" },
	{ 0x31, "FMR3" },
	{ 0x32, "ICB1" },
	{ 0x33, "ICB2" },
	{ 0x34, "ICB3" },
	{ 0x35, "ICB4" },
	{ 0x36, "LIM0" },
	{ 0x37, "LIM1" },
	{ 0x38, "PCD" },
	{ 0x39, "PCR" },
	{ 0x3a, "LIM2" },
	{ 0x3b, "LCR1" },
	{ 0x3c, "LCR2" },
	{ 0x3d, "LCR3" },
	{ 0x3e, "SIC1" },
	{ 0x3f, "SIC2" },
	{ 0x40, "SIC3" },
	{ 0x44, "CMR1" },
	{ 0x45, "CMR2" },
	{ 0x46, "GCR" },
	{ 0x47, "ESM" },
	{ 0x60, "DEC" },
	{ 0x70, "XS1" },
	{ 0x71, "XS2" },
	{ 0x72, "XS3" },
	{ 0x73, "XS4" },
	{ 0x74, "XS5" },
	{ 0x75, "XS6" },
	{ 0x76, "XS7" },
	{ 0x77, "XS8" },
	{ 0x78, "XS9" },
	{ 0x79, "XS10" },
	{ 0x7a, "XS11" },
	{ 0x7b, "XS12" },
	{ 0x7c, "XS13" },
	{ 0x7d, "XS14" },
	{ 0x7e, "XS15" },
	{ 0x7f, "XS16" },
	{ 0x80, "PC1" },
	{ 0x81, "PC2" },
	{ 0x82, "PC3" },
	{ 0x83, "PC4" },
	{ 0x84, "PC5" },
	{ 0x85, "GPC1", 1 },
	{ 0x87, "CMDR2" },
	{ 0x8d, "CCR5" },
	{ 0x92, "GCM1", 1 },
	{ 0x93, "GCM2", 1 },
	{ 0x94, "GCM3", 1 },
	{ 0x95, "GCM4", 1 },
	{ 0x96, "GCM5", 1 },
	{ 0x97, "GCM6", 1 },
	{ 0x98, "GCM7", 1 },
	{ 0x99, "GCM8", 1 },
	{ 0xa0, "TSEO" },
	{ 0xa1, "TSBS1" },
	{ 0xa8, "TPC0" },	
};

static struct t4_reg_def t1_reginfo[] = {
	{ 0x00, "XFIFO" },
	{ 0x01, "XFIFO" },
	{ 0x02, "CMDR" },
	{ 0x03, "MODE" },
	{ 0x04, "RAH1" },
	{ 0x05, "RAH2" },
	{ 0x06, "RAL1" },
	{ 0x07, "RAL2" },
	{ 0x08, "IPC", 1 },
	{ 0x09, "CCR1" },
	{ 0x0a, "CCR2" },
	{ 0x0c, "RTR1" },
	{ 0x0d, "RTR2" },
	{ 0x0e, "RTR3" },
	{ 0x0f, "RTR4" },
	{ 0x10, "TTR1" },
	{ 0x11, "TTR2" },
	{ 0x12, "TTR3" },
	{ 0x13, "TTR4" },
	{ 0x14, "IMR0" },
	{ 0x15, "IMR1" },
	{ 0x16, "IMR2" },
	{ 0x17, "IMR3" },
	{ 0x18, "IMR4" },
	{ 0x1b, "IERR" },
	{ 0x1c, "FMR0" },
	{ 0x1d, "FMR1" },
	{ 0x1e, "FMR2" },
	{ 0x1f, "LOOP" },
	{ 0x20, "FMR4" },
	{ 0x21, "FMR5" },
	{ 0x22, "XC0" },
	{ 0x23, "XC1" },
	{ 0x24, "RC0" },
	{ 0x25, "RC1" },
	{ 0x26, "XPM0" },
	{ 0x27, "XPM1" },
	{ 0x28, "XPM2" },
	{ 0x2b, "IDLE" },
	{ 0x2c, "XDL1" },
	{ 0x2d, "XDL2" },
	{ 0x2e, "XDL3" },
	{ 0x2f, "CCB1" },
	{ 0x30, "CCB2" },
	{ 0x31, "CCB3" },
	{ 0x32, "ICB1" },
	{ 0x33, "ICB2" },
	{ 0x34, "ICB3" },
	{ 0x36, "LIM0" },
	{ 0x37, "LIM1" },
	{ 0x38, "PCD" },
	{ 0x39, "PCR" },
	{ 0x3a, "LIM2" },
	{ 0x3b, "LCR1" },
	{ 0x3c, "LCR2" },
	{ 0x3d, "LCR3" },
	{ 0x3e, "SIC1" },
	{ 0x3f, "SIC2" },
	{ 0x40, "SIC3" },
	{ 0x44, "CMR1" },
	{ 0x45, "CMR2" },
	{ 0x46, "GCR" },
	{ 0x47, "ESM" },
	{ 0x60, "DEC" },
	{ 0x70, "XS1" },
	{ 0x71, "XS2" },
	{ 0x72, "XS3" },
	{ 0x73, "XS4" },
	{ 0x74, "XS5" },
	{ 0x75, "XS6" },
	{ 0x76, "XS7" },
	{ 0x77, "XS8" },
	{ 0x78, "XS9" },
	{ 0x79, "XS10" },
	{ 0x7a, "XS11" },
	{ 0x7b, "XS12" },
	{ 0x80, "PC1" },
	{ 0x81, "PC2" },
	{ 0x82, "PC3" },
	{ 0x83, "PC4" },
	{ 0x84, "PC5" },
	{ 0x85, "GPC1", 1 },
	{ 0x87, "CMDR2" },
	{ 0x8d, "CCR5" },
	{ 0x92, "GCM1", 1 },
	{ 0x93, "GCM2", 1 },
	{ 0x94, "GCM3", 1 },
	{ 0x95, "GCM4", 1 },
	{ 0x96, "GCM5", 1 },
	{ 0x97, "GCM6", 1 },
	{ 0x98, "GCM7", 1 },
	{ 0x99, "GCM8", 1 },
	{ 0xa0, "TSEO" },
	{ 0xa1, "TSBS1" },
	{ 0xa8, "TPC0" },	
};

static struct t4_reg_def t1_sreginfo[] = {
	{ 0x00, "RFIFO" },
	{ 0x01, "RFIFO" },
	{ 0x49, "RBD" },
	{ 0x4a, "VSTR", 1 },
	{ 0x4b, "RES" },
	{ 0x4c, "FRS0" },
	{ 0x4d, "FRS1" },
	{ 0x4e, "FRS2" },
	{ 0x4f, "Old FRS1" },
	{ 0x50, "FECL" },
	{ 0x51, "FECH" },
	{ 0x52, "CVCL" },
	{ 0x53, "CVCH" },
	{ 0x54, "CECL" },
	{ 0x55, "CECH" },
	{ 0x56, "EBCL" },
	{ 0x57, "EBCH" },
	{ 0x58, "BECL" },
	{ 0x59, "BECH" },
	{ 0x5a, "COEC" },
	{ 0x5c, "RDL1" },
	{ 0x5d, "RDL2" },
	{ 0x5e, "RDL3" },
	{ 0x62, "RSP1" },
	{ 0x63, "RSP2" },
	{ 0x64, "SIS" },
	{ 0x65, "RSIS" },
	{ 0x66, "RBCL" },
	{ 0x67, "RBCH" },
	{ 0x68, "ISR0" },
	{ 0x69, "ISR1" },
	{ 0x6a, "ISR2" },
	{ 0x6b, "ISR3" },
	{ 0x6c, "ISR4" },
	{ 0x6e, "GIS" },
	{ 0x6f, "CIS", 1 },
	{ 0x70, "RS1" },
	{ 0x71, "RS2" },
	{ 0x72, "RS3" },
	{ 0x73, "RS4" },
	{ 0x74, "RS5" },
	{ 0x75, "RS6" },
	{ 0x76, "RS7" },
	{ 0x77, "RS8" },
	{ 0x78, "RS9" },
	{ 0x79, "RS10" },
	{ 0x7a, "RS11" },
	{ 0x7b, "RS12" },
};

static struct t4_reg_def sreginfo[] = {
	{ 0x00, "RFIFO" },
	{ 0x01, "RFIFO" },
	{ 0x49, "RBD" },
	{ 0x4a, "VSTR", 1 },
	{ 0x4b, "RES" },
	{ 0x4c, "FRS0" },
	{ 0x4d, "FRS1" },
	{ 0x4e, "RSW" },
	{ 0x4f, "RSP" },
	{ 0x50, "FECL" },
	{ 0x51, "FECH" },
	{ 0x52, "CVCL" },
	{ 0x53, "CVCH" },
	{ 0x54, "CEC1L" },
	{ 0x55, "CEC1H" },
	{ 0x56, "EBCL" },
	{ 0x57, "EBCH" },
	{ 0x58, "CEC2L" },
	{ 0x59, "CEC2H" },
	{ 0x5a, "CEC3L" },
	{ 0x5b, "CEC3H" },
	{ 0x5c, "RSA4" },
	{ 0x5d, "RSA5" },
	{ 0x5e, "RSA6" },
	{ 0x5f, "RSA7" },
	{ 0x60, "RSA8" },
	{ 0x61, "RSA6S" },
	{ 0x62, "RSP1" },
	{ 0x63, "RSP2" },
	{ 0x64, "SIS" },
	{ 0x65, "RSIS" },
	{ 0x66, "RBCL" },
	{ 0x67, "RBCH" },
	{ 0x68, "ISR0" },
	{ 0x69, "ISR1" },
	{ 0x6a, "ISR2" },
	{ 0x6b, "ISR3" },
	{ 0x6c, "ISR4" },
	{ 0x6e, "GIS" },
	{ 0x6f, "CIS", 1 },
	{ 0x70, "RS1" },
	{ 0x71, "RS2" },
	{ 0x72, "RS3" },
	{ 0x73, "RS4" },
	{ 0x74, "RS5" },
	{ 0x75, "RS6" },
	{ 0x76, "RS7" },
	{ 0x77, "RS8" },
	{ 0x78, "RS9" },
	{ 0x79, "RS10" },
	{ 0x7a, "RS11" },
	{ 0x7b, "RS12" },
	{ 0x7c, "RS13" },
	{ 0x7d, "RS14" },
	{ 0x7e, "RS15" },
	{ 0x7f, "RS16" },
};

static char *tobin(int x)
{
	static char s[9] = "";
	int y,z=0;
	for (y=7;y>=0;y--) {
		if (x & (1 << y))
			s[z++] = '1';
		else
			s[z++] = '0';
	}
	s[z] = '\0';
	return s;
}

static char *tobin32(unsigned int x)
{
	static char s[33] = "";
	int y,z=0;
	for (y=31;y>=0;y--) {
		if (x & (1 << y))
			s[z++] = '1';
		else
			s[z++] = '0';
	}
	s[z] = '\0';
	return s;
}

int main(int argc, char *argv[])
{
	int fd;
	int x;
	char fn[256];
	struct t4_regs regs;
	if ((argc < 2) || ((*(argv[1]) != '/') && !atoi(argv[1]))) {
		fprintf(stderr, "Usage: wct4xxp-diag <channel>\n");
		exit(1);
	}
	if (*(argv[1]) == '/')
		dahdi_copy_string(fn, argv[1], sizeof(fn));
	else
		snprintf(fn, sizeof(fn), "/dev/dahdi/%d", atoi(argv[1]));
	fd = open(fn, O_RDWR);
	if (fd <0) {
		fprintf(stderr, "Unable to open '%s': %s\n", fn, strerror(errno));
		exit(1);
	}
	if (ioctl(fd, WCT4_GET_REGS, &regs)) {
		fprintf(stderr, "Unable to get registers: %s\n", strerror(errno));
		exit(1);
	}
	printf("PCI Registers:\n");
	for (x=0;x<sizeof(xreginfo) / sizeof(xreginfo[0]);x++) {
		fprintf(stdout, "%s (%02x): %08x (%s)\n", xreginfo[x].name, xreginfo[x].reg, regs.pci[xreginfo[x].reg], tobin32(regs.pci[xreginfo[x].reg]));
	}
	printf("\nE1 Control Registers:\n");
	for (x=0;x<sizeof(reginfo) / sizeof(reginfo[0]);x++) {
		fprintf(stdout, "%s (%02x): %02x (%s)\n", reginfo[x].name, reginfo[x].reg, regs.regs[reginfo[x].reg], tobin(regs.regs[reginfo[x].reg]));
	}
	printf("\nE1 Status Registers:\n");
	for (x=0;x<sizeof(sreginfo) / sizeof(sreginfo[0]);x++) {
		fprintf(stdout, "%s (%02x): %02x (%s)\n", sreginfo[x].name, sreginfo[x].reg, regs.regs[sreginfo[x].reg], tobin(regs.regs[sreginfo[x].reg]));
	}
	printf("\nT1 Control Registers:\n");
	for (x=0;x<sizeof(t1_reginfo) / sizeof(t1_reginfo[0]);x++) {
		fprintf(stdout, "%s (%02x): %02x (%s)\n", t1_reginfo[x].name, t1_reginfo[x].reg, regs.regs[t1_reginfo[x].reg], tobin(regs.regs[t1_reginfo[x].reg]));
	}
	printf("\nT1 Status Registers:\n");
	for (x=0;x<sizeof(t1_sreginfo) / sizeof(t1_sreginfo[0]);x++) {
		fprintf(stdout, "%s (%02x): %02x (%s)\n", t1_sreginfo[x].name, t1_sreginfo[x].reg, regs.regs[t1_sreginfo[x].reg], tobin(regs.regs[t1_sreginfo[x].reg]));
	}
	exit(0);
}
