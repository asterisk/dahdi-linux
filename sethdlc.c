/*
 * sethdlc.c
 *
 * Copyright (C) 1999 - 2002 Krzysztof Halasa <khc@pm.waw.pl>
 *
 */

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

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <asm/types.h>
#include <linux/hdlc.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/sockios.h>

#include <dahdi/user.h>
#include "dahdi_tools_version.h"

#if GENERIC_HDLC_VERSION != 4
#error Generic HDLC layer version mismatch, please get correct sethdlc.c
#endif

#if !defined(IF_PROTO_HDLC_ETH) || !defined(IF_PROTO_FR_ETH_PVC)
#warning "No kernel support for Ethernet over Frame Relay / HDLC, skipping it"
#endif


static struct ifreq req;	/* for ioctl */
static int argc;
static char **argv;
int sock;


static void error(const char *format, ...) __attribute__ ((noreturn, format(printf, 1, 2)));

static void error(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	fprintf(stderr, "%s: ", req.ifr_name);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(1);
}



typedef struct {
	const char *name;
	const unsigned int value;
} parsertab;



static int checkkey(const char* name)
{
	if (argc < 1)
		return -1;	/* no enough parameters */

	if (strcmp(name, argv[0]))
		return -1;
	argc--;
	argv++;
	return 0;
}



static int checktab(parsertab *tab, unsigned int *value)
{
	int i;

	if (argc < 1)
		return -1;	/* no enough parameters */
	
	for (i = 0; tab[i].name; i++)
		if (!strcmp(tab[i].name, argv[0])) {
			argc--;
			argv++;
			*value = tab[i].value;
			return 0;
		}

	return -1;		/* Not found */
}



static const char* tabstr(unsigned int value, parsertab *tab,
			  const char* unknown)
{
	int i;
	for (i = 0; tab[i].name; i++)
		if (tab[i].value == value)
			return tab[i].name;

	return unknown;		/* Not found */
}



static unsigned int match(const char* name, unsigned int *value,
			  unsigned int minimum, unsigned int maximum)
{
	char test;

	if (argc < 1)
		return -1;	/* no enough parameters */

	if (name) {
		if (strcmp(name, argv[0]))
			return -1;
		argc--;
		argv++;
	}

	if (argc < 1)
		error("Missing parameter\n");

	if (sscanf(argv[0], "%u%c", value, &test) != 1)
		error("Invalid parameter: %s\n", argv[0]);

	if ((*value > maximum) || (*value < minimum))
		error("Parameter out of range [%u - %u]: %u\n",
		      minimum, maximum, *value);

	argc--;
	argv++;
	return 0;
}


static parsertab ifaces[] = {{ "v35", IF_IFACE_V35 },
			     { "v24", IF_IFACE_V24 },
			     { "x21", IF_IFACE_X21 },
			     { "e1", IF_IFACE_E1 },
			     { "t1", IF_IFACE_T1 },
			     { NULL, 0 }};

static parsertab clocks[] = {{ "int", CLOCK_INT },
			     { "ext", CLOCK_EXT },
			     { "txint", CLOCK_TXINT },
			     { "txfromrx", CLOCK_TXFROMRX },
			     { NULL, 0 }};


static parsertab protos[] = {{ "hdlc", IF_PROTO_HDLC},
			     { "cisco", IF_PROTO_CISCO},
			     { "fr", IF_PROTO_FR},
			     { "ppp", IF_PROTO_PPP},
			     { "x25", IF_PROTO_X25},
#ifdef IF_PROTO_HDLC_ETH
			     { "hdlc-eth", IF_PROTO_HDLC_ETH},
#endif
			     { NULL, 0 }};


static parsertab hdlc_enc[] = {{ "nrz", ENCODING_NRZ },
			       { "nrzi", ENCODING_NRZI },
			       { "fm-mark", ENCODING_FM_MARK },
			       { "fm-space", ENCODING_FM_SPACE },
			       { "manchester", ENCODING_MANCHESTER },
			       { NULL, 0 }};

static parsertab hdlc_par[] = {{ "no-parity", PARITY_NONE },
			       { "crc16", PARITY_CRC16_PR1 },
			       { "crc16-pr0", PARITY_CRC16_PR0 },
			       { "crc16-itu", PARITY_CRC16_PR1_CCITT },
			       { "crc16-itu-pr0", PARITY_CRC16_PR0_CCITT },
			       { "crc32-itu", PARITY_CRC32_PR1_CCITT },
			       { NULL, 0 }};

static parsertab lmi[] = {{ "none", LMI_NONE },
			  { "ansi", LMI_ANSI },
			  { "ccitt", LMI_CCITT },
			  { NULL, 0 }};


static void set_iface(void)
{
	int orig_argc = argc;
	te1_settings te1;

	memset(&te1, 0, sizeof(te1));
	req.ifr_settings.type = IF_IFACE_SYNC_SERIAL;

	while (argc > 0) {
		if (req.ifr_settings.type == IF_IFACE_SYNC_SERIAL)
			if (!checktab(ifaces, &req.ifr_settings.type))
				continue;

		if (!te1.clock_type)
			if (!checkkey("clock")) {
				if (!checktab(clocks, &te1.clock_type))
					continue;
				error("Invalid clock type\n");
			}

		if (!te1.clock_rate &&
		    (te1.clock_type == CLOCK_INT ||
		     te1.clock_type == CLOCK_TXINT))
			if (!match("rate", &te1.clock_rate, 1, 0xFFFFFFFF))
				continue;
		if (!te1.loopback) {
			if (!checkkey("loopback") ||
			    !checkkey("lb")) {
				te1.loopback = 1;
				continue;
			}
		}
		/* slotmap goes here */

		if (orig_argc == argc)
			return;	/* not an iface definition */
		error("Invalid parameter: %s\n", argv[0]);
	}

	if (!te1.clock_rate &&
	    (te1.clock_type == CLOCK_INT ||
	     te1.clock_type == CLOCK_TXINT))
		te1.clock_rate = 64000;

	/* FIXME stupid hack, will remove it later */
	req.ifr_settings.ifs_ifsu.te1 = &te1;
	if (req.ifr_settings.type == IF_IFACE_E1 ||
	    req.ifr_settings.type == IF_IFACE_T1)
		req.ifr_settings.size = sizeof(te1_settings);
	else
		req.ifr_settings.size = sizeof(sync_serial_settings);

	if (ioctl(sock, SIOCWANDEV, &req))
		error("Unable to set interface information: %s\n",
		      strerror(errno));

	exit(0);
}



static void set_proto_fr(void)
{
	unsigned int lmi_type = 0;
	fr_proto fr;

	memset(&fr, 0, sizeof(fr));

	while (argc > 0) {
		if (!lmi_type)
			if (!checkkey("lmi")) {
				if (!checktab(lmi, &lmi_type))
					continue;
				error("Invalid LMI type: %s\n",
				      argv[0]);
			}

		if (lmi_type && lmi_type != LMI_NONE) {
			if (!fr.dce)
				if (!checkkey("dce")) {
					fr.dce = 1;
					continue;
				}

			if (!fr.t391)
				if (!match("t391", &fr.t391,
					   1, 1000))
					continue;
			if (!fr.t392)
				if (!match("t392", &fr.t392,
					   1, 1000))
					continue;
			if (!fr.n391)
				if (!match("n391", &fr.n391,
					   1, 1000))
					continue;
			if (!fr.n392)
				if (!match("n392", &fr.n392,
					   1, 1000))
					continue;
			if (!fr.n393)
				if (!match("n393", &fr.n393,
					   1, 1000))
					continue;
		}
		error("Invalid parameter: %s\n", argv[0]);
	}

	 /* polling verification timer*/
	if (!fr.t391) fr.t391 = 10;
	/* link integrity verification polling timer */
	if (!fr.t392) fr.t392 = 15;
	/* full status polling counter*/
	if (!fr.n391) fr.n391 = 6;
	/* error threshold */
	if (!fr.n392) fr.n392 = 3;
	/* monitored events count */
	if (!fr.n393) fr.n393 = 4;

	if (!lmi_type)
		fr.lmi = LMI_DEFAULT;
	else
		fr.lmi = lmi_type;

	req.ifr_settings.ifs_ifsu.fr = &fr;
	req.ifr_settings.size = sizeof(fr);

	if (ioctl(sock, SIOCWANDEV, &req))
		error("Unable to set FR protocol information: %s\n",
		      strerror(errno));
}



static void set_proto_hdlc(int eth)
{
	unsigned int enc = 0, par = 0;
	raw_hdlc_proto raw;

	memset(&raw, 0, sizeof(raw));

	while (argc > 0) {
		if (!enc)
			if (!checktab(hdlc_enc, &enc))
				continue;
		if (!par)
			if (!checktab(hdlc_par, &par))
				continue;

		error("Invalid parameter: %s\n", argv[0]);
	}

	if (!enc)
		raw.encoding = ENCODING_DEFAULT;
	else
		raw.encoding = enc;

	if (!par)
		raw.parity = ENCODING_DEFAULT;
	else
		raw.parity = par;

	req.ifr_settings.ifs_ifsu.raw_hdlc = &raw;
	req.ifr_settings.size = sizeof(raw);

	if (ioctl(sock, SIOCWANDEV, &req))
		error("Unable to set HDLC%s protocol information: %s\n",
		      eth ? "-ETH" : "", strerror(errno));
}



static void set_proto_cisco(void)
{
	cisco_proto cisco;
	memset(&cisco, 0, sizeof(cisco));

	while (argc > 0) {
		if (!cisco.interval)
			if (!match("interval", &cisco.interval,
				   1, 100))
				continue;
		if (!cisco.timeout)
			if (!match("timeout", &cisco.timeout,
				   1, 100))
				continue;

		error("Invalid parameter: %s\n",
		      argv[0]);
	}

	if (!cisco.interval)
		cisco.interval = 10;
	if (!cisco.timeout)
		cisco.timeout = 25;

	req.ifr_settings.ifs_ifsu.cisco = &cisco;
	req.ifr_settings.size = sizeof(cisco);

	if (ioctl(sock, SIOCWANDEV, &req))
		error("Unable to set Cisco HDLC protocol information: %s\n",
		      strerror(errno));
}



static void set_proto(void)
{
	if (checktab(protos, &req.ifr_settings.type))
		return;

	switch(req.ifr_settings.type) {
	case IF_PROTO_HDLC: set_proto_hdlc(0); break;
#ifdef IF_PROTO_HDLC_ETH
	case IF_PROTO_HDLC_ETH: set_proto_hdlc(1); break;
#endif
	case IF_PROTO_CISCO: set_proto_cisco(); break;
	case IF_PROTO_FR: set_proto_fr(); break;

	case IF_PROTO_PPP:
	case IF_PROTO_X25:
		req.ifr_settings.ifs_ifsu.sync = NULL; /* FIXME */
		req.ifr_settings.size = 0;

		if (!ioctl(sock, SIOCWANDEV, &req))
			break;

		error("Unable to set %s protocol information: %s\n",
		      req.ifr_settings.type == IF_PROTO_PPP
		      ? "PPP" : "X.25", strerror(errno));

	default: error("Unknown protocol %u\n", req.ifr_settings.type);
	}

	if (argc > 0)
		error("Unexpected parameter: %s\n", argv[0]);

	close(sock);
	exit(0);
}



static void set_pvc(void)
{
	char *op = argv[0];
	parsertab ops[] = {{ "create", IF_PROTO_FR_ADD_PVC },
			   { "delete", IF_PROTO_FR_DEL_PVC },
			   { NULL, 0 }};
	fr_proto_pvc pvc;

	memset(&pvc, 0, sizeof(pvc));

	if (checktab(ops, &req.ifr_settings.type))
		return;

#ifdef IF_PROTO_FR_ETH_PVC
	if (!match("ether", &pvc.dlci, 0, 1023)) {
		if (req.ifr_settings.type == IF_PROTO_FR_ADD_PVC)
			req.ifr_settings.type = IF_PROTO_FR_ADD_ETH_PVC;
		else
			req.ifr_settings.type = IF_PROTO_FR_DEL_ETH_PVC;

	} else
#endif
		if (match(NULL, &pvc.dlci, 0, 1023))
			return;

	if (argc != 0)
		return;

	req.ifr_settings.ifs_ifsu.fr_pvc = &pvc;
	req.ifr_settings.size = sizeof(pvc);

	if (ioctl(sock, SIOCWANDEV, &req))
		error("Unable to %s PVC: %s\n", op, strerror(errno));
	exit(0);
}



static void private(void)
{
	if (argc < 1)
		return;

	if (!strcmp(argv[0], "private")) {
		if (argc != 1)
			return;
		if (ioctl(sock, SIOCDEVPRIVATE, &req))
			error("SIOCDEVPRIVATE: %s\n", strerror(errno));
		exit(0);
	}
}



static void show_port(void)
{
	const char *s;
	char buffer[128];
	const te1_settings *te1 = (void*)buffer;
	const raw_hdlc_proto *raw = (void*)buffer;
	const cisco_proto *cisco = (void*)buffer;
	const fr_proto *fr = (void*)buffer;
#ifdef IF_PROTO_FR_PVC
	const fr_proto_pvc_info *pvc = (void*)buffer;
#endif
	req.ifr_settings.ifs_ifsu.sync = (void*)buffer; /* FIXME */

	printf("%s: ", req.ifr_name);

	req.ifr_settings.size = sizeof(buffer);
	req.ifr_settings.type = IF_GET_IFACE;

	if (ioctl(sock, SIOCWANDEV, &req))
		if (errno != EINVAL) {
			printf("unable to get interface information: %s\n",
			       strerror(errno));
			close(sock);
			exit(1);
		}
	
	/* Get and print physical interface settings */
	if (req.ifr_settings.type == IF_IFACE_SYNC_SERIAL)
		s = "";		/* Unspecified serial interface */
	else
		s = tabstr(req.ifr_settings.type, ifaces, NULL);

	if (!s)
		printf("unknown interface 0x%x\n", req.ifr_settings.type);
	else {
		if (*s)
			printf("interface %s ", s);

		printf("clock %s", tabstr(te1->clock_type, clocks,
					  "type unknown"));
		if (te1->clock_type == CLOCK_INT ||
		    te1->clock_type == CLOCK_TXINT)
			printf(" rate %u", te1->clock_rate);

		if (te1->loopback)
			printf(" loopback");

		if (req.ifr_settings.type == IF_IFACE_E1 ||
		    req.ifr_settings.type == IF_IFACE_T1) {
			unsigned int u;
			printf(" slotmap ");
			for (u = te1->slot_map; u != 0; u /= 2)
				printf("%u", u % 2);
		}
		printf("\n");
	}

	/* Get and print protocol settings */
	do {
		printf("\t");
		req.ifr_settings.size = sizeof(buffer);
		req.ifr_settings.type = IF_GET_PROTO;

		if (ioctl(sock, SIOCWANDEV, &req)) {
			if (errno == EINVAL)
				printf("no protocol set\n");
			else
				printf("unable to get protocol information: "
				       "%s\n", strerror(errno));
			break;
		}

		switch(req.ifr_settings.type) {
		case IF_PROTO_FR:
			printf("protocol fr lmi %s",
			       tabstr(fr->lmi, lmi, "unknown"));
			if (fr->lmi == LMI_ANSI ||
			    fr->lmi == LMI_CCITT)
				printf("%s t391 %u t392 %u n391 %u n392 %u "
				       "n393 %u\n",
				       fr->dce ? " dce" : "",
				       fr->t391,
				       fr->t392,
				       fr->n391,
				       fr->n392,
				       fr->n393);
			else
				putchar('\n');
			break;

#ifdef IF_PROTO_FR_PVC
		case IF_PROTO_FR_PVC:
			printf("Frame-Relay PVC: DLCI %u, master device %s\n",
			       pvc->dlci, pvc->master);
			break;
#endif

#ifdef IF_PROTO_FR_ETH_PVC
		case IF_PROTO_FR_ETH_PVC:
			printf("Frame-Relay PVC (Ethernet emulation): DLCI %u,"
			       " master device %s\n", pvc->dlci, pvc->master);
			break;
#endif

		case IF_PROTO_HDLC:
			printf("protocol hdlc %s %s\n",
			       tabstr(raw->encoding, hdlc_enc, "unknown"),
			       tabstr(raw->parity, hdlc_par, "unknown"));
			break;

#ifdef IF_PROTO_HDLC_ETH
		case IF_PROTO_HDLC_ETH:
			printf("protocol hdlc-eth %s %s\n",
			       tabstr(raw->encoding, hdlc_enc, "unknown"),
			       tabstr(raw->parity, hdlc_par, "unknown"));
			break;
#endif

		case IF_PROTO_CISCO:
			printf("protocol cisco interval %u timeout %u\n",
			       cisco->interval,
			       cisco->timeout);
			break;

		case IF_PROTO_PPP:
			printf("protocol ppp\n");
			break;

		case IF_PROTO_X25:
			printf("protocol x25\n");
			break;

		default:
			printf("unknown protocol %u\n", req.ifr_settings.type);
		}
	}while(0);

	close(sock);
	exit(0);
}



static void usage(void)
{
	fprintf(stderr, "sethdlc version 1.15\n"
		"Copyright (C) 2000 - 2003 Krzysztof Halasa <khc@pm.waw.pl>\n"
		"\n"
		"Usage: sethdlc INTERFACE [PHYSICAL] [clock CLOCK] [LOOPBACK] "
		"[slotmap SLOTMAP]\n"
		"       sethdlc INTERFACE [PROTOCOL]\n"
		"       sethdlc INTERFACE create | delete"
#ifdef IF_PROTO_FR_ETH_PVC
		" [ether]"
#endif
		" DLCI\n"
		"       sethdlc INTERFACE private...\n"
		"\n"
		"PHYSICAL := v24 | v35 | x21 | e1 | t1\n"
		"CLOCK := int [rate RATE] | ext | txint [rate RATE] | txfromrx\n"
		"LOOPBACK := loopback | lb\n"
		"\n"
		"PROTOCOL := hdlc [ENCODING] [PARITY] |\n"
#ifdef IF_PROTO_HDLC_ETH
		"            hdlc-eth [ENCODING] [PARITY] |\n"
#endif
		"            cisco [interval val] [timeout val] |\n"
		"            fr [lmi LMI] |\n"
		"            ppp |\n"
		"            x25\n"
		"\n"
		"ENCODING := nrz | nrzi | fm-mark | fm-space | manchester\n"
		"PARITY := no-parity | crc16 | crc16-pr0 | crc16-itu | crc16-itu-pr0 | crc32-itu\n"
		"LMI := none | ansi [LMI_SPEC] | ccitt [LMI_SPEC]\n"
		"LMI_SPEC := [dce] [t391 val] [t392 val] [n391 val] [n392 val] [n393 val]\n");
	exit(0);
}



int main(int arg_c, char *arg_v[])
{
	argc = arg_c;
	argv = arg_v;

	if (argc <= 1)
		usage();
  
	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock < 0)
		error("Unable to create socket: %s\n", strerror(errno));
  
	dahdi_copy_string(req.ifr_name, argv[1], sizeof(req.ifr_name)); /* Device name */

	if (argc == 2)
		show_port();

	argc -= 2;
	argv += 2;

	set_iface();
	set_proto();
	set_pvc();
	private();

	close(sock);
	usage();
	exit(0);
}
