/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2006, 2007, 2008, Xorcom
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include "hexfile.h"

static const char rcsid[] = "$Id$";

static parse_hexfile_report_func_t	report_func = NULL;

parse_hexfile_report_func_t parse_hexfile_set_reporting(parse_hexfile_report_func_t rf)
{
	parse_hexfile_report_func_t	old_rf = report_func;
	report_func = rf;
	return old_rf;
}

static void chomp(char buf[])
{
	size_t	last = strlen(buf) - 1;
	while(last >= 0 && isspace(buf[last]))
		buf[last--] = '\0';
}

static int hexline_checksum(struct hexline *hexline)
{
	unsigned int	i;
	unsigned int	chksm = 0;
	int		ll = hexline->d.content.header.ll;

	for(i = 0; i <= sizeof(hexline->d.content.header) + ll; i++) {
		chksm += hexline->d.raw[i];
	}
	return chksm & 0xFF;
}

int dump_hexline(int recordno, struct hexline *line, FILE *fp)
{
	uint8_t		ll;
	uint16_t	offset;
	uint8_t		tt;
	uint8_t		old_chksum;
	uint8_t		new_chksum;
	uint8_t		*data;
	unsigned int	i;

	ll = line->d.content.header.ll;
	offset = line->d.content.header.offset;
	tt = line->d.content.header.tt;
	fprintf(fp, ":%02X%04X%02X", ll, offset, tt);
	data = line->d.content.tt_data.data;
	for(i = 0; i < ll; i++) {
		fprintf(fp, "%02X", data[i]);
	}
	old_chksum = data[ll];
	data[ll] = 0;
	new_chksum = 0xFF - hexline_checksum(line) + 1;
	data[ll] = old_chksum;
	fprintf(fp, "%02X\n", new_chksum);
	if(new_chksum != old_chksum) {
		if(report_func)
			report_func(LOG_ERR, "record #%d: new_chksum(%02X) != old_chksum(%02X)\n",
					recordno, new_chksum, old_chksum);
		return 0;
	}
	return 1;
}

struct hexline	*new_hexline(uint8_t datalen, uint16_t offset, uint8_t tt)
{
	struct hexline	*hexline;
	size_t		allocsize;

	allocsize = sizeof(struct hexline) + datalen + 1; /* checksum byte */
	if((hexline = malloc(allocsize)) == NULL) {
		if(report_func)
			report_func(LOG_ERR, "No more memory\n");
		return NULL;
	}
	memset(hexline, 0, allocsize);
	hexline->d.content.header.ll = datalen;
	hexline->d.content.header.offset = offset;
	hexline->d.content.header.tt = tt;
	return hexline;
}

static int append_hexline(struct hexdata *hexdata, char *buf)
{
	int		ret;
	unsigned int	ll, offset, tt;
	char		*p;
	struct hexline	*hexline;
	unsigned int	i;

	if(hexdata->got_eof) {
		if(report_func)
			report_func(LOG_ERR, "Extranous data after EOF record\n");
		return -EINVAL;
	}
	if(hexdata->last_line >= hexdata->maxlines) {
		if(report_func)
			report_func(LOG_ERR, "Hexfile too large (maxline %d)\n", hexdata->maxlines);
		return -ENOMEM;
	}
	ret = sscanf(buf, "%02X%04X%02X", &ll, &offset, &tt);
	if(ret != 3) {
		if(report_func)
			report_func(LOG_ERR, "Bad line header (only %d items out of 3 parsed)\n", ret);
		return -EINVAL;
	}
	switch(tt) {
		case TT_DATA:
			break;
		case TT_EOF:
			if(ll != 0) {
				if(report_func)
					report_func(LOG_ERR,
						"%d: Record %d(EOF): Bad len = %d\n",
						hexdata->last_line, tt, ll);
				return -EINVAL;
			}
			if(offset != 0) {
				if(report_func)
					report_func(LOG_ERR,
						"%d: Record %d(EOF): Bad offset = %d\n",
						hexdata->last_line, tt, offset);
				return -EINVAL;
			}
			hexdata->got_eof = 1;
			break;
		case TT_EXT_SEG:
			if(ll != 2) {
				if(report_func)
					report_func(LOG_ERR,
						"%d: Record %d(EXT_SEG): Bad len = %d\n",
						hexdata->last_line, tt, ll);
				return -EINVAL;
			}
			if(offset != 0) {
				if(report_func)
					report_func(LOG_ERR,
						"%d: Record %d(EXT_SEG): Bad offset = %d\n",
						hexdata->last_line, tt, offset);
				return -EINVAL;
			}
			break;
		case TT_START_SEG:
			if(ll != 4) {
				if(report_func)
					report_func(LOG_ERR,
						"%d: Record %d(START_SEG): Bad len = %d\n",
						hexdata->last_line, tt, ll);
				return -EINVAL;
			}
			if(offset != 0) {
				if(report_func)
					report_func(LOG_ERR,
						"%d: Record %d(START_SEG): Bad offset = %d\n",
						hexdata->last_line, tt, offset);
				return -EINVAL;
			}
			break;
		case TT_EXT_LIN:
			if(ll != 2) {
				if(report_func)
					report_func(LOG_ERR,
						"%d: Record %d(EXT_LIN): Bad len = %d\n",
						hexdata->last_line, tt, ll);
				return -EINVAL;
			}
			if(offset != 0) {
				if(report_func)
					report_func(LOG_ERR,
						"%d: Record %d(EXT_LIN): Bad offset = %d\n",
						hexdata->last_line, tt, ll);
				return -EINVAL;
			}
			break;
		case TT_START_LIN:	/* Unimplemented */
			if(ll != 4) {
				if(report_func)
					report_func(LOG_ERR,
						"%d: Record %d(EXT_LIN): Bad len = %d\n",
						hexdata->last_line, tt, ll);
				return -EINVAL;
			}
			if(offset != 0) {
				if(report_func)
					report_func(LOG_ERR,
						"%d: Record %d(EXT_LIN): Bad offset = %d\n",
						hexdata->last_line, tt, ll);
				return -EINVAL;
			}
			break;
		default:
			if(report_func)
				report_func(LOG_ERR, "%d: Unimplemented record type %d: %s\n",
					hexdata->last_line, tt, buf);
			return -EINVAL;
	}
	buf += 8;	/* Skip header */
	if((hexline = new_hexline(ll, offset, tt)) == NULL) {
		if(report_func)
			report_func(LOG_ERR, "No more memory for hexfile lines\n");
		return -EINVAL;
	}
	p = buf;
	for(i = 0; i < ll + 1; i++) {	/* include checksum */
		unsigned int	val;

		if((*p == '\0') || (*(p+1) == '\0')) {
			if(report_func)
				report_func(LOG_ERR, "Short data string '%s'\n", buf);
			return -EINVAL;
		}
		ret = sscanf(p, "%02X", &val);
		if(ret != 1) {
			if(report_func)
				report_func(LOG_ERR, "Bad data byte #%d\n", i);
			return -EINVAL;
		}
		hexline->d.content.tt_data.data[i] = val;
		p += 2;
	}
	if(hexline_checksum(hexline) != 0) {
		if(report_func) {
			report_func(LOG_ERR, "Bad checksum (%d instead of 0)\n",
				hexline_checksum(hexline));
			dump_hexline(hexdata->last_line, hexline, stderr);
		}
		return -EINVAL;
	}
	hexdata->lines[hexdata->last_line] = hexline;
	if(hexdata->got_eof)
		return 0;
	hexdata->last_line++;
	return 1;
}

void free_hexdata(struct hexdata *hexdata)
{
	if(hexdata) {
		unsigned int	i;

		for(i = 0; i < hexdata->maxlines; i++)
			if(hexdata->lines[i] != NULL)
				free(hexdata->lines[i]);
		free(hexdata);
	}
}

int dump_hexfile(struct hexdata *hexdata, const char *outfile)
{
	FILE		*fp;
	unsigned int	i;

	if(report_func)
		report_func(LOG_INFO, "Dumping hex data into '%s'\n", outfile);
	if(!outfile || strcmp(outfile, "-") == 0)
		fp = stdout;
	else if((fp = fopen(outfile, "w")) == NULL) {
		perror(outfile);
		exit(1);
	}
	for(i = 0; i <= hexdata->last_line; i++) {
		struct hexline	*line = hexdata->lines[i];
		if(!line) {
			if(report_func)
				report_func(LOG_ERR, "Missing line at #%d\n", i);
			return -EINVAL;
		}
		if(!dump_hexline(i, line, fp))
			return -EINVAL;
	}
	return 0;
}

int dump_hexfile2(struct hexdata *hexdata, const char *outfile, uint8_t maxwidth)
{
	FILE		*fp;
	uint8_t		tt;
	unsigned int	i;
	struct hexline	*line;

	if(report_func)
		report_func(LOG_INFO,
			"Dumping hex data into '%s' (maxwidth=%d)\n",
			outfile, maxwidth);
	if(!outfile || strcmp(outfile, "-") == 0)
		fp = stdout;
	else if((fp = fopen(outfile, "w")) == NULL) {
		perror(outfile);
		exit(1);
	}
	if(maxwidth == 0)
		maxwidth = UINT8_MAX;
	for(i = 0; i <= hexdata->last_line; i++) {
		int		bytesleft = 0;
		int		extra_offset = 0;
		int		base_offset;
		uint8_t		*base_data;
		
		line = hexdata->lines[i];
		if(!line) {
			if(report_func)
				report_func(LOG_ERR, "Missing line at #%d\n", i);
			return -EINVAL;
		}
		bytesleft = line->d.content.header.ll;
		/* split the line into several lines */
		tt = line->d.content.header.tt;
		base_offset = line->d.content.header.offset;
		base_data = line->d.content.tt_data.data;
		while (bytesleft > 0) {
			struct hexline	*extraline;
			uint8_t		new_chksum;
			unsigned int	curr_bytes = (bytesleft >= maxwidth) ? maxwidth : bytesleft;

			/* generate the new line */
			if((extraline = new_hexline(curr_bytes, base_offset + extra_offset, tt)) == NULL) {
				if(report_func)
					report_func(LOG_ERR, "No more memory for hexfile lines\n");
				return -EINVAL;
			}
			memcpy(extraline->d.content.tt_data.data, base_data + extra_offset, curr_bytes);
			new_chksum = 0xFF - hexline_checksum(extraline) + 1;
			extraline->d.content.tt_data.data[curr_bytes] = new_chksum;
			/* print it */
			dump_hexline(i, extraline, fp);
			/* cleanups */
			free(extraline);
			extra_offset += curr_bytes;
			bytesleft -= curr_bytes;
		}
	}
	if(tt != TT_EOF) {
		if(report_func)
			report_func(LOG_ERR, "Missing EOF record\n");
		return -EINVAL;
	}
	dump_hexline(i, line, fp);
	return 0;
}

void process_comment(struct hexdata *hexdata, char buf[])
{
	char		*dollar_start;
	char		*dollar_end;
	const char	id_prefix[] = "Id: ";
	char		tmp[BUFSIZ];
	char		*p;
	int		len;

	if(report_func)
		report_func(LOG_INFO, "Comment: %s\n", buf + 1);
	/* Search for RCS keywords */
	if((dollar_start = strchr(buf, '$')) == NULL)
		return;
	if((dollar_end = strchr(dollar_start + 1, '$')) == NULL)
		return;
	/* Crop the '$' signs */
	len = dollar_end - dollar_start;
	len -= 2;
	memcpy(tmp, dollar_start + 1, len);
	tmp[len] = '\0';
	p = tmp;
	if(strstr(tmp, id_prefix) == NULL)
		return;
	p += strlen(id_prefix);
	if((p = strchr(p, ' ')) == NULL)
		return;
	p++;
	snprintf(hexdata->version_info, BUFSIZ, "%s", p);
	if((p = strchr(hexdata->version_info, ' ')) != NULL)
		*p = '\0';
}

struct hexdata *parse_hexfile(const char *fname, unsigned int maxlines)
{
	FILE		*fp;
	struct hexdata	*hexdata = NULL;
	int		datasize;
	char		buf[BUFSIZ];
	int		line;
	int		dos_eof = 0;
	int		ret;

	assert(fname != NULL);
	if(report_func)
		report_func(LOG_INFO, "Parsing %s\n", fname);
	datasize = sizeof(struct hexdata) + maxlines * sizeof(char *);
	hexdata = (struct hexdata *)malloc(datasize);
	if(!hexdata) {
		if(report_func)
			report_func(LOG_ERR, "Failed to allocate %d bytes for hexfile contents\n", datasize);
		goto err;
	}
	memset(hexdata, 0, datasize);
	hexdata->maxlines = maxlines;
	if((fp = fopen(fname, "r")) == NULL) {
		if(report_func)
			report_func(LOG_ERR, "Failed to open hexfile '%s'\n", fname);
		goto err;
	}
	snprintf(hexdata->fname, PATH_MAX, "%s", fname);
	for(line = 1; fgets(buf, BUFSIZ, fp); line++) {
		if(dos_eof) {
			if(report_func)
				report_func(LOG_ERR, "%s:%d - Got DOS EOF character before true EOF\n", fname, line);
			goto err;
		}
		if(buf[0] == 0x1A && buf[1] == '\0') { /* DOS EOF char */
			dos_eof = 1;
			continue;
		}
		chomp(buf);
		if(buf[0] == '\0') {
				if(report_func)
					report_func(LOG_ERR, "%s:%d - Short line\n", fname, line);
				goto err;
		}
		if(buf[0] == '#') {
			process_comment(hexdata, buf);
			continue;
		}
		if(buf[0] != ':') {
			if(report_func)
				report_func(LOG_ERR, "%s:%d - Line begins with 0x%X\n", fname, line, buf[0]);
			goto err;
		}
		if((ret = append_hexline(hexdata, buf + 1)) < 0) {
			if(report_func)
				report_func(LOG_ERR, "%s:%d - Failed parsing.\n", fname, line);
			goto err;
		}
	}
	fclose(fp);
	if(report_func)
		report_func(LOG_INFO, "%s parsed OK\n", fname);
	return hexdata;
err:
	free_hexdata(hexdata);
	return NULL;
}

void dump_binary(struct hexdata *hexdata, const char *outfile)
{
	FILE		*fp;
	unsigned int	i;
	size_t		len;

	if(report_func)
		report_func(LOG_INFO, "Dumping binary data into '%s'\n", outfile);
	if((fp = fopen(outfile, "w")) == NULL) {
		perror(outfile);
		exit(1);
	}
	for(i = 0; i < hexdata->maxlines; i++) {
		struct hexline	*hexline = hexdata->lines[i];

		if(!hexline)
			break;
		switch(hexline->d.content.header.tt) {
		case TT_EOF:
			if(report_func)
				report_func(LOG_INFO, "\ndump: good EOF record");
			break;
		case TT_DATA:
			if(report_func)
				report_func(LOG_INFO, "dump: %6d\r", i);
			len = hexline->d.content.header.ll;
			if(fwrite(hexline->d.content.tt_data.data, 1, len, fp) != len) {
				perror("write");
				exit(1);
			}
			break;
		case TT_EXT_SEG:
		case TT_START_SEG:
		case TT_EXT_LIN:
		case TT_START_LIN:
			if(report_func)
				report_func(LOG_INFO,
					"\ndump(%d): ignored record type %d",
					i, hexline->d.content.header.tt);
			break;
		default:
			if(report_func)
				report_func(LOG_ERR, "dump: Unknown record type %d\n",
					hexline->d.content.header.tt);
			exit(1);
		}
	}
	if(report_func)
		report_func(LOG_INFO, "\nDump finished\n");
	fclose(fp);
}

void gen_hexline(const uint8_t *data, uint16_t addr, size_t len, FILE *output)
{
	struct hexline	*hexline;

	if(!data) {
		fprintf(output, ":%02X%04X%02XFF\n", 0, 0, TT_EOF);
		return;
	}
	if((hexline = new_hexline(len, addr, (!data) ? TT_EOF : TT_DATA)) == NULL) {
		if(report_func)
			report_func(LOG_ERR, "No more memory\n");
		return;
	}
	if(data)
		memcpy(&hexline->d.content.tt_data, data, len);
	dump_hexline(0, hexline, output);
	free(hexline);
}

/*
 * Algorithm lifted of sum(1) implementation from coreutils.
 * We chose the default algorithm (BSD style).
 */
int bsd_checksum(struct hexdata *hexdata)
{
	unsigned int	i;
	size_t		len;
	int		ck = 0;

	for(i = 0; i < hexdata->maxlines; i++) {
		struct hexline	*hexline = hexdata->lines[i];
		unsigned char	*p;

		if(!hexline)
			break;
		if(hexline->d.content.header.tt == TT_EOF)
			continue;
		len = hexline->d.content.header.ll;
		p = hexline->d.content.tt_data.data;
		for(; len; p++, len--) {
			ck = (ck >> 1) + ((ck & 1) << 15);
			ck += *p;
			ck &= 0xffff;	/* Keep it within bounds. */
		}
	}
	return ck;
}
