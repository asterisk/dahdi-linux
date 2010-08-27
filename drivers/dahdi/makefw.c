/* Xilinx firmware convertor program.
 *
 * Written by Jim Dixon <jim@lambdatel.com>.
 *
 * Copyright (C) 2001 Jim Dixon / Zapata Telephony.
 * Copyright (C) 2001-2008 Digium, Inc.
 *
 * All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	SWATH 12

int main(int argc, char *argv[])
{
FILE *fp;
int i,j,nbytes;
unsigned char c;
char	buf[300];

	if (argc < 3)
	   {
		puts("Usage... makefw  filename.rbt  array_name");
		exit(1);
	   }	

	fp = fopen(argv[1],"r");
	if (!fp)
	   {
		perror("bit file open");
		exit(1);
	   }
	nbytes = 0;
	printf("static unsigned char %s[] = {\n",argv[2]);
	i = 0;
	while(fgets(buf,sizeof(buf) - 1,fp))
	   {
		if (!buf[0]) continue;
		if (buf[strlen(buf) - 1] < ' ') buf[strlen(buf) - 1] = 0;
		if (!buf[0]) continue;
		if (buf[strlen(buf) - 1] < ' ') buf[strlen(buf) - 1] = 0;
		if (!buf[0]) continue;
		if (strlen(buf) < 32) continue;
		if ((buf[0] != '0') && (buf[0] != '1')) continue;
		c = 0;
		for(j = 0; buf[j]; j++)
		   {
			if (buf[j] > '0') c |= 1 << (j & 7);
			if ((j & 7) == 7)
			   {
				nbytes++;
				if (i) printf(",");				
				printf("0x%02x",c);
				if (i++ == SWATH) {
					printf(",\n");
					i = 0;
				}
				c = 0;
			   }
		   }
	   }
	printf("\n};\n\n");
	fprintf(stderr,"Loaded %d bytes from file\n",nbytes);
	fclose(fp);
	exit(0);
}

