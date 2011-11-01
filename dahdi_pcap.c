/*
 * Capturing a pcap from the DAHDI interface
 * 
 * Copyright (C) 2011 Torrey Searle
 * 
 * ISDN support added by Horacio Peña 
 * Command line cleanups by Sverker Abrahamsson
 * 
 * Requirements:
 * - pcap development library
 * - DAHDI_MIRROR ioctl which isn't enabled by default in dahdi-linux
 *   To enable this unsupported feature, #define CONFIG_DAHDI_MIRROR
 *   in dahdi-linux
 * - To build this program call the 'make dahdi_pcap' target
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

#include <stdio.h>
#include <fcntl.h>
#include <dahdi/user.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pcap.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <getopt.h>

#define BLOCK_SIZE 512
#define MAX_CHAN 16
//char ETH_P_LAPD[2] = {0x00, 0x30};

struct mtp2_phdr {
        u_int8_t  sent;
        u_int8_t  annex_a_used;
        u_int16_t link_number;
};


struct lapd_sll_hdr {
    u_int16_t sll_pkttype;    /* packet type */
    u_int16_t sll_hatype;
    u_int16_t sll_halen;
    u_int8_t sll_addr[8];
    u_int8_t sll_protocol[2];   /* protocol, should be ETH_P_LAPD */
};


struct chan_fds {
	int rfd;
	int tfd;
	int chan_id;
	int proto;
	char tx_buf[BLOCK_SIZE * 4];
	int tx_len;
	char rx_buf[BLOCK_SIZE * 4];
	int rx_len;
};

int make_mirror(long type, int chan)
{
	int res = 0;
	int fd = 0;	
	struct dahdi_bufferinfo bi;
	fd = open("/dev/dahdi/pseudo", O_RDONLY);

	memset(&bi, 0, sizeof(bi));
        bi.txbufpolicy = DAHDI_POLICY_IMMEDIATE;
        bi.rxbufpolicy = DAHDI_POLICY_IMMEDIATE;
        bi.numbufs = 32;
        bi.bufsize = BLOCK_SIZE;

	ioctl(fd, DAHDI_SET_BUFINFO, &bi);

	res = ioctl(fd, type, &chan);

	if(res)
	{
		printf("error setting channel err=%d!\n", res);
		return -1;
	}

	
	return fd;
}

int log_packet(struct chan_fds * fd, char is_read, pcap_dumper_t * dump)
{
	unsigned char buf[BLOCK_SIZE * 4];
	int res = 0;

	struct pcap_pkthdr hdr;
	struct mtp2_phdr * mtp2 = (struct mtp2_phdr *)buf;
	struct lapd_sll_hdr * lapd = (struct lapd_sll_hdr *)buf;

	unsigned char *dataptr = buf;
	int datasize = sizeof(buf);

	if(fd->proto == DLT_LINUX_LAPD)
	{
		dataptr += sizeof(struct lapd_sll_hdr);
		datasize -= sizeof(struct lapd_sll_hdr);
	}
	else
	{
		dataptr += sizeof(struct mtp2_phdr);
		datasize -= sizeof(struct mtp2_phdr);
	}

	memset(buf, 0, sizeof(buf));
	if(is_read)
	{
		res = read(fd->rfd, dataptr, datasize);
		if(fd->rx_len > 0 && res == fd->rx_len && !memcmp(fd->rx_buf, dataptr, res) )
		{
			//skipping dup
			return 0;
		}

		memcpy(fd->rx_buf,  dataptr, res);
		fd->rx_len = res;
	}
	else
	{
		res = read(fd->tfd, dataptr, datasize);
		if(fd->tx_len > 0 && res == fd->tx_len && !memcmp(fd->tx_buf, dataptr, res) )
		{
			//skipping dup
			return 0;
		}

		memcpy(fd->tx_buf,  dataptr, res);
		fd->tx_len = res;
	}

	gettimeofday(&hdr.ts, NULL);

	

	
	if(res > 0)
	{
		if(fd->proto == DLT_LINUX_LAPD)
		{
			hdr.caplen = res+sizeof(struct lapd_sll_hdr)-2;
			hdr.len = res+sizeof(struct lapd_sll_hdr)-2;
			
			lapd->sll_pkttype = 3;
			lapd->sll_hatype = 0;
			lapd->sll_halen = res;
	//                lapd->sll_addr = ???
			lapd->sll_protocol[0] = 0x00;
			lapd->sll_protocol[1] = 0x30;

		}
		else
		{
			hdr.caplen = res+sizeof(struct mtp2_phdr);
			hdr.len = res+sizeof(struct mtp2_phdr);
			
			if(is_read)
			{
				mtp2->sent = 0;
				mtp2->annex_a_used = 0;
			}
			else
			{
				mtp2->sent = 1;
				mtp2->annex_a_used = 0;
			}
			mtp2->link_number = htons(fd->chan_id);
		}
		pcap_dump((u_char*)dump, &hdr, buf);
		pcap_dump_flush(dump);
	}
	return 1;
}

void usage() 
{
	printf("Usage: dahdi_pcap [OPTIONS]\n");
	printf("Capture packets from DAHDI channels to pcap file\n\n");
	printf("Options:\n");
	printf("  -p, --proto=[mtp2|lapd] The protocol to capture, default mtp2\n");
	printf("  -c, --chan=<channels>   Comma separated list of channels to capture from, max %d. Mandatory\n", MAX_CHAN);
	printf("  -f, --file=<filename>   The pcap file to capture to. Mandatory\n");
	printf("  -h, --help              Display this text\n");
}

int main(int argc, char **argv)
{
	struct chan_fds chans[MAX_CHAN];
	char *filename = NULL;
	int num_chans = 0;
	int max_fd = 0;
	int proto = DLT_MTP2_WITH_PHDR;

	int i;
	int packetcount;
	int c;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"proto", required_argument, 0, 'p'},
			{"chan", required_argument, 0, 'c'},
			{"file", required_argument, 0, 'f'},
			{"help", 0, 0, 'h'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "p:c:f:?",
			  long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'p':
				// Protocol
				if(strcasecmp("LAPD", optarg)==0)
				{
					proto = DLT_LINUX_LAPD;
				}
				else if(argc > 0 && strcasecmp("MTP2", argv[1])==0)
				{
					proto = DLT_MTP2_WITH_PHDR;
				}
				break;
			case 'c':
				// TODO Should it be possible to override protocol per channel?
				// Channels, comma separated list
				while(optarg != NULL && num_chans < MAX_CHAN)
				{
					int chan = atoi(strsep(&optarg, ","));


					chans[num_chans].tfd = make_mirror(DAHDI_TXMIRROR, chan);
					chans[num_chans].rfd = make_mirror(DAHDI_RXMIRROR, chan);
					chans[num_chans].chan_id = chan;
					chans[num_chans].proto = proto;

					if(chans[num_chans].tfd > max_fd)
					{
						max_fd = chans[num_chans].tfd;
					}
					if(chans[num_chans].rfd > max_fd)
					{
						max_fd = chans[num_chans].rfd;
					}

					num_chans++;
				}
				max_fd++;
				break;
			case 'f':
				// File to capture to
				filename=optarg;
				break;
			case 'h':
			default:
				// Usage
				usage();
				exit(0);
		  }
	}
	if((num_chans == 0) || (filename == NULL)) {
		usage();
		exit(0);
	}
	else
	{
		printf("Capturing protocol %s on channels ", (proto == DLT_MTP2_WITH_PHDR ? "mtp2":"lapd"));
		for(i = 0; i < num_chans; i++)
		{
			printf("%d", chans[i].chan_id);
			if(i<num_chans-1)
			{
				printf(", ");
			}
		}
		printf(" to file %s\n", filename);
	}

	pcap_t * pcap = pcap_open_dead(chans[0].proto, BLOCK_SIZE*4);
	pcap_dumper_t * dump = pcap_dump_open(pcap, filename);
	
	packetcount=0;
	while(1)
	{
		fd_set rd_set;
		FD_ZERO(&rd_set);
		for(i = 0; i < num_chans; i++)
		{
			FD_SET(chans[i].tfd, &rd_set);
			FD_SET(chans[i].rfd, &rd_set);
		}

		select(max_fd, &rd_set, NULL, NULL, NULL);

		for(i = 0; i < num_chans; i++)
		{
			if(FD_ISSET(chans[i].rfd, &rd_set))
			{
				packetcount += log_packet(&chans[i], 1, dump);
			}
			if(FD_ISSET(chans[i].tfd, &rd_set))
			{
				packetcount += log_packet(&chans[i], 0, dump);
			}
		}
		printf("Packets captured: %d\r", packetcount);
		fflush(stdout);
	}

	return 0;
}
