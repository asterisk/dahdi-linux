/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2008, Xorcom
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>
#include "hexfile.h"
#include "astribank_usb.h"
#include "mpp_funcs.h"
#include "debug.h"

static const char rcsid[] = "$Id$";

#define	DBG_MASK	0x02

const char *ack_status_msg(uint8_t status)
{
	const static char	*msgs[] = {
		[STAT_OK] = "Acknowledges previous command",
		[STAT_FAIL] = "Last command failed",
		[STAT_RESET_FAIL] = "Reset failed",
		[STAT_NODEST] = "No destination is selected",
		[STAT_MISMATCH] = "Data mismatch",
		[STAT_NOACCESS] = "No access",
		[STAT_BAD_CMD] = "Bad command",
		[STAT_TOO_SHORT] = "Packet is too short",
		[STAT_ERROFFS] = "Offset error",
		[STAT_NOCODE] = "Source was not burned before",
		[STAT_NO_LEEPROM] = "Large EEPROM was not found",
		[STAT_NO_EEPROM] = "No EEPROM was found",
		[STAT_WRITE_FAIL] = "Writing to device failed",
		[STAT_FPGA_ERR] = "FPGA error",
		[STAT_KEY_ERR] = "Bad Capabilities Key",
		[STAT_NOCAPS_ERR]	= "No matching capability",
		[STAT_NOPWR_ERR]	= "No power on USB connector",
		[STAT_CAPS_FPGA_ERR]	= "Setting of the capabilities while FPGA is loaded",
	};
	if(status > sizeof(msgs)/sizeof(msgs[0]))
		return "ERROR CODE TOO LARGE";
	if(!msgs[status])
		return "MISSING ERROR CODE";
	return msgs[status];
}

const char *eeprom_type2str(enum eeprom_type et)
{
	const static char	*msgs[] = {
		[EEPROM_TYPE_NONE]	= "NONE",
		[EEPROM_TYPE_SMALL]	= "SMALL",
		[EEPROM_TYPE_LARGE]	= "LARGE",
		[EEPROM_TYPE_UNUSED]	= "UNUSED",
	};
	if(et > sizeof(msgs)/sizeof(msgs[0]))
		return NULL;
	return msgs[et];
};

const char *dev_dest2str(enum dev_dest dest)
{
	const static char	*msgs[] = {
		[DEST_NONE]	= "NONE",
		[DEST_FPGA]	= "FPGA",
		[DEST_EEPROM]	= "EEPROM",
	};
	if(dest > sizeof(msgs)/sizeof(msgs[0]))
		return NULL;
	return msgs[dest];
};

struct command_desc {
	uint8_t		op;
	const char	*name;
	uint16_t	len;
};

#define	CMD_RECV(o)	[MPP_ ## o] {	\
		.op = MPP_ ## o,	\
		.name = #o,	\
		.len = sizeof(struct mpp_header) + sizeof(struct d_ ## o),	\
	}

#define	CMD_SEND(o)	[MPP_ ## o] {	\
		.op = MPP_ ## o,	\
		.name = #o,	\
		.len = sizeof(struct mpp_header) + sizeof(struct d_ ## o),	\
	}

static const struct command_desc	command_table[] = {
	CMD_RECV(ACK),
	CMD_SEND(PROTO_QUERY),
	CMD_SEND(STATUS_GET),
	CMD_RECV(STATUS_GET_REPLY),
	CMD_SEND(EEPROM_SET),
	CMD_SEND(CAPS_GET),
	CMD_RECV(CAPS_GET_REPLY),
	CMD_SEND(CAPS_SET),
	CMD_SEND(EXTRAINFO_GET),
	CMD_RECV(EXTRAINFO_GET_REPLY),
	CMD_SEND(EXTRAINFO_SET),
	CMD_RECV(PROTO_REPLY),
	CMD_SEND(RENUM),
	CMD_SEND(EEPROM_BLK_RD),
	CMD_RECV(EEPROM_BLK_RD_REPLY),
	CMD_SEND(DEV_SEND_SEG),
	CMD_SEND(DEV_SEND_START),
	CMD_SEND(DEV_SEND_END),
	CMD_SEND(RESET),
	CMD_SEND(HALF_RESET),
	CMD_SEND(SER_SEND),
	CMD_SEND(SER_RECV),
	/* Twinstar */
	CMD_SEND(TWS_WD_MODE_SET),
	CMD_SEND(TWS_WD_MODE_GET),
	CMD_RECV(TWS_WD_MODE_GET_REPLY),
	CMD_SEND(TWS_PORT_SET),
	CMD_SEND(TWS_PORT_GET),
	CMD_RECV(TWS_PORT_GET_REPLY),
	CMD_SEND(TWS_PWR_GET),
	CMD_RECV(TWS_PWR_GET_REPLY),
};

static const struct command_desc	command_table_V13[] = {
	CMD_RECV(ACK),
	CMD_SEND(PROTO_QUERY),
	CMD_SEND(STATUS_GET),
	CMD_RECV(STATUS_GET_REPLY_V13),
	CMD_SEND(EEPROM_SET),
	CMD_SEND(CAPS_GET),
	CMD_RECV(CAPS_GET_REPLY),
	CMD_SEND(CAPS_SET),
	CMD_SEND(EXTRAINFO_GET),
	CMD_RECV(EXTRAINFO_GET_REPLY),
	CMD_SEND(EXTRAINFO_SET),
	CMD_RECV(PROTO_REPLY),
	CMD_SEND(RENUM),
	CMD_SEND(EEPROM_BLK_RD),
	CMD_RECV(EEPROM_BLK_RD_REPLY),
	CMD_SEND(DEV_SEND_SEG),
	CMD_SEND(DEV_SEND_START),
	CMD_SEND(DEV_SEND_END),
	CMD_SEND(RESET),
	CMD_SEND(HALF_RESET),
	CMD_SEND(SER_SEND),
	CMD_SEND(SER_RECV),
	/* Twinstar */
	CMD_SEND(TWS_WD_MODE_SET),
	CMD_SEND(TWS_WD_MODE_GET),
	CMD_RECV(TWS_WD_MODE_GET_REPLY),
	CMD_SEND(TWS_PORT_SET),
	CMD_SEND(TWS_PORT_GET),
	CMD_RECV(TWS_PORT_GET_REPLY),
	CMD_SEND(TWS_PWR_GET),
	CMD_RECV(TWS_PWR_GET_REPLY),
};

#undef	CMD_SEND
#undef	CMD_RECV

struct cmd_queue {
	struct cmd_queue	*next;
	struct cmd_queue	*prev;
	struct mpp_command	*cmd;
};

static struct cmd_queue	output_queue = {
	.next = &output_queue,
	.prev = &output_queue,
	.cmd = NULL
	};

void free_command(struct mpp_command *cmd)
{
	memset(cmd, 0, cmd->header.len);
	free(cmd);
}

const struct command_desc *get_command_desc(uint8_t protocol_version, uint8_t op)
{
	const struct command_desc	*desc;

	switch(protocol_version) {
		case MK_PROTO_VERSION(1,3):
			if(op > sizeof(command_table_V13)/sizeof(command_table_V13[0])) {
				//ERR("Invalid op=0x%X. Bigger than max valid op\n", op);
				return NULL;
			}
			desc = &command_table_V13[op];
			if(!desc->name)
				return NULL;
			break;
		default:
			if(op > sizeof(command_table)/sizeof(command_table[0])) {
				//ERR("Invalid op=0x%X. Bigger than max valid op\n", op);
				return NULL;
			}
			desc = &command_table[op];
			if(!desc->name)
				return NULL;
			break;
	}
	return desc;
}

struct mpp_command *new_command(uint8_t protocol_version, uint8_t op, uint16_t extra_data)
{
	struct mpp_command		*cmd;
	const struct command_desc	*desc;
	uint16_t			len;

	desc = get_command_desc(protocol_version, op);
	if(!desc) {
		ERR("Unknown op=0x%X.\n", op);
		return NULL;
	}
	DBG("OP=0x%X [%s] (extra_data %d)\n", op, desc->name, extra_data);
	len = desc->len + extra_data;
	if((cmd = malloc(len)) == NULL) {
		ERR("Out of memory\n");
		return NULL;
	}
	cmd->header.op = op;
	cmd->header.len = len;
	cmd->header.seq = 0;	/* Overwritten in send_usb() */
	return cmd;
}

void dump_command(struct mpp_command *cmd)
{
	uint16_t	len;
	int		i;

	len = cmd->header.len;
	if(len < sizeof(struct mpp_header)) {
		ERR("Command too short (%d)\n", len);
		return;
	}
	INFO("DUMP: OP=0x%X len=%d seq=%d\n",
		cmd->header.op, cmd->header.len, cmd->header.seq);
	for(i = 0; i < len - sizeof(struct mpp_header); i++) {
		INFO("  %2d. 0x%X\n", i, cmd->alt.raw_data[i]);
	}
}

int send_command(struct astribank_device *astribank, struct mpp_command *cmd, int timeout)
{
	int		ret;
	int		len;
	char		*buf;

	len = cmd->header.len;
	cmd->header.seq = astribank->tx_sequenceno;

	buf = (char *)cmd;
	//printf("%s: len=%d\n", __FUNCTION__, len);
#if 0
	extern	FILE	*fp;
	if(fp) {
		int	i;

		fprintf(fp, "%05d:", cmd->header.seq);
		for(i = 0; i < len; i++)
			fprintf(fp, " %02X", (uint8_t)buf[i]);
		fprintf(fp, "\n");
	}
#endif
	ret = send_usb(astribank, (char *)cmd, len, timeout);
	if(ret < 0) {
		DBG("send_usb failed ret=%d\n", ret);
	}
	astribank->tx_sequenceno++;
	return ret;
}

struct mpp_command *recv_command(struct astribank_device *astribank, int timeout)
{
	struct mpp_command	*reply;
	int			ret;

	if((reply = malloc(PACKET_SIZE)) == NULL) {
		ERR("Out of memory\n");
		goto err;
	}
	reply->header.len = 0;
	ret = recv_usb(astribank, (char *)reply, PACKET_SIZE, timeout);
	if(ret < 0) {
		ERR("Receive from usb failed.\n");
		goto err;
	} else if(ret == 0) {
		goto err;	/* No reply */
	}
	if(ret != reply->header.len) {
		ERR("Wrong length received: got %d bytes, but length field says %d bytes%s\n",
				ret, reply->header.len,
				(ret == 1)? ". Old USB firmware?": "");
		goto err;
	}
	//dump_packet(LOG_DEBUG, __FUNCTION__, (char *)reply, ret);
	return reply;
err:
	if(reply) {
		memset(reply, 0, PACKET_SIZE);
		free_command(reply);
	}
	return NULL;
}


__attribute__((warn_unused_result))
int process_command(struct astribank_device *astribank, struct mpp_command *cmd, struct mpp_command **reply_ref)
{
	struct mpp_command		*reply = NULL;
	const struct command_desc	*reply_desc;
	const struct command_desc	*expected;
	const struct command_desc	*cmd_desc;
	uint8_t				reply_op;
	int				ret;

	if(reply_ref)
		*reply_ref = NULL;	/* So the caller knows if a reply was received */
	reply_op = cmd->header.op | 0x80;
	if(cmd->header.op == MPP_PROTO_QUERY)
		astribank->mpp_proto_version = MPP_PROTOCOL_VERSION;	/* bootstrap */
	cmd_desc = get_command_desc(astribank->mpp_proto_version, cmd->header.op);
	expected = get_command_desc(astribank->mpp_proto_version, reply_op);
	//printf("%s: len=%d\n", __FUNCTION__, cmd->header.len);
	ret = send_command(astribank, cmd, TIMEOUT);
	if(!reply_ref) {
		DBG("No reply requested\n");
		goto out;
	}
	if(ret < 0) {
		ERR("send_command failed: %d\n", ret);
		goto out;
	}
	reply = recv_command(astribank, TIMEOUT);
	if(!reply) {
		ERR("recv_command failed\n");
		ret = -EPROTO;
		goto out;
	}
	*reply_ref = reply;
	if((reply->header.op & 0x80) != 0x80) {
		ERR("Unexpected reply op=0x%02X, should have MSB set.\n", reply->header.op);
		ret = -EPROTO;
		goto out;
	}
	DBG("REPLY OP: 0x%X\n", reply->header.op);
	reply_desc = get_command_desc(astribank->mpp_proto_version, reply->header.op);
	if(!reply_desc) {
		ERR("Unknown reply op=0x%02X\n", reply->header.op);
		ret = -EPROTO;
		goto out;
	}
	DBG("REPLY NAME: %s\n", reply_desc->name);
	if(reply->header.op == MPP_ACK) {
		int	status = CMD_FIELD(reply, ACK, stat);

		if(expected) {
			ERR("Expected OP=0x%02X: Got ACK(%d): %s\n",
				reply_op, status, ack_status_msg(status));
			ret = -EPROTO;
			goto out;
		} else if(status != STAT_OK) {

			ERR("Got ACK (for OP=0x%X [%s]): %d - %s\n",
				cmd->header.op,
				cmd_desc->name,
				status,
				ack_status_msg(status));
#if 0
			extern	FILE	*fp;
			if(fp) {
				fprintf(fp, "Got ACK(%d)\n", status);
			}
#endif
			ret = -EPROTO;
			goto out;
		}
		/* Good expected ACK ... */
	} else if(reply->header.op != reply_op) {
			ERR("Expected OP=0x%02X: Got OP=0x%02X\n",
				reply_op, reply->header.op);
			ret = -EPROTO;
			goto out;
	}
	if(expected && expected->op != MPP_SER_RECV && expected->len != reply->header.len) {
			ERR("Expected len=%d: Got len=%d\n",
				expected->len, reply->header.len);
			ret = -EPROTO;
			goto out;
	}
	if(cmd->header.seq != reply->header.seq) {
			ERR("Expected seq=%d: Got seq=%d\n",
				cmd->header.seq, reply->header.seq);
			ret = -EPROTO;
			goto out;
	}
	ret = reply->header.len;	/* All good, return the length */
	DBG("returning reply op 0x%X (%d bytes)\n", reply->header.op, ret);
out:
	free_command(cmd);
	if(!reply_ref && reply)
		free_command(reply);
	return ret;
}

static int set_ihex_version(char *dst, const char *src)
{
	memcpy(dst, src, VERSION_LEN);
	return 0;
}

/*
 * Protocol Commands
 */

int mpp_proto_query(struct astribank_device *astribank)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_PROTO_QUERY, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	CMD_FIELD(cmd, PROTO_QUERY, proto_version) = MPP_PROTOCOL_VERSION;	/* Protocol Version */
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	astribank->mpp_proto_version = CMD_FIELD(reply, PROTO_REPLY, proto_version);
	if(! MPP_SUPPORTED_VERSION(astribank->mpp_proto_version)) {
		ERR("Got mpp protocol version: %02x (expected %02x)\n",
			astribank->mpp_proto_version,
			MPP_PROTOCOL_VERSION);
		ret = -EPROTO;
		goto out;
	}
	if(astribank->mpp_proto_version != MPP_PROTOCOL_VERSION) {
		ERR("Deprecated (but working) MPP protocol version [%X]. Please upgrade to [%X] ASAP\n",
			astribank->mpp_proto_version, MPP_PROTOCOL_VERSION);
	}
	DBG("Protocol version: %02x\n", astribank->mpp_proto_version);
	ret = astribank->mpp_proto_version;
	free_command(reply);
out:
	return ret;
}

int mpp_status_query(struct astribank_device *astribank)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_STATUS_GET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	astribank->eeprom_type = 0x3 & (CMD_FIELD(reply, STATUS_GET_REPLY, i2cs_data) >> 3);
	astribank->status = CMD_FIELD(reply, STATUS_GET_REPLY, status);
	astribank->fw_versions = CMD_FIELD(reply, STATUS_GET_REPLY, fw_versions);
	DBG("EEPROM TYPE: %02x\n", astribank->eeprom_type);
	DBG("FPGA Firmware: %s\n", (astribank->status & 0x1) ? "Loaded" : "Empty");
	DBG("Firmware Versions: USB='%s' FPGA='%s' EEPROM='%s'\n",
		astribank->fw_versions.usb,
		astribank->fw_versions.fpga,
		astribank->fw_versions.eeprom);
	free_command(reply);
	return ret;
}

int mpp_eeprom_set(struct astribank_device *astribank, const struct eeprom_table *et)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_EEPROM_SET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	memcpy(&CMD_FIELD(cmd, EEPROM_SET, data), et, sizeof(*et));
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	free_command(reply);
	return 0;
}

int mpp_renumerate(struct astribank_device *astribank)
{
	struct mpp_command	*cmd;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_RENUM, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	ret = process_command(astribank, cmd, NULL);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	return 0;
}

int mpp_caps_get(struct astribank_device *astribank,
	struct eeprom_table *eeprom_table,
	struct capabilities *capabilities,
	struct capkey *key)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_CAPS_GET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	assert(reply->header.op == MPP_CAPS_GET_REPLY);
	if(eeprom_table) {
		memcpy(eeprom_table, (void *)&CMD_FIELD(reply, CAPS_GET_REPLY, data), sizeof(*eeprom_table));
	}
	if(capabilities) {
		const struct capabilities	*cap = &CMD_FIELD(reply, CAPS_GET_REPLY, capabilities);

		memcpy(capabilities, cap, sizeof(*capabilities));
	}
	if(key) {
		const struct capkey	*k = &CMD_FIELD(reply, CAPS_GET_REPLY, key);

		memcpy(key, k, sizeof(*key));
	}
	free_command(reply);
	return 0;
}

int mpp_caps_set(struct astribank_device *astribank,
	const struct eeprom_table *eeprom_table,
	const struct capabilities *capabilities,
	const struct capkey *key)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_CAPS_SET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	memcpy(&CMD_FIELD(cmd, CAPS_SET, data), eeprom_table, sizeof(*eeprom_table));
	memcpy(&CMD_FIELD(cmd, CAPS_SET, capabilities), capabilities, sizeof(*capabilities));
	memcpy(&CMD_FIELD(cmd, CAPS_SET, key), key, sizeof(*key));
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	free_command(reply);
	return 0;
}

int mpp_extrainfo_get(struct astribank_device *astribank, struct extrainfo *info)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_EXTRAINFO_GET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	assert(reply->header.op == MPP_EXTRAINFO_GET_REPLY);
	if(info) {
		memcpy(info, (void *)&CMD_FIELD(reply, EXTRAINFO_GET_REPLY, info), sizeof(*info));
	}
	free_command(reply);
	return 0;
}

int mpp_extrainfo_set(struct astribank_device *astribank, const struct extrainfo *info)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_EXTRAINFO_SET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	memcpy(&CMD_FIELD(cmd, EXTRAINFO_SET, info), info, sizeof(*info));
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	free_command(reply);
	return 0;
}

int mpp_eeprom_blk_rd(struct astribank_device *astribank, uint8_t *buf, uint16_t offset, uint16_t len)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;
	int			size;

	DBG("len = %d, offset = %d\n", len, offset);
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_EEPROM_BLK_RD, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	CMD_FIELD(cmd, EEPROM_BLK_RD, len) = len;
	CMD_FIELD(cmd, EEPROM_BLK_RD, offset) = offset;
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		size = ret;
		goto out;
	}
	size = reply->header.len - sizeof(struct mpp_header) - sizeof(struct d_EEPROM_BLK_RD_REPLY);
	INFO("size=%d offset=0x%X\n", size, CMD_FIELD(reply, EEPROM_BLK_RD_REPLY, offset));
	dump_packet(LOG_DEBUG, "BLK_RD", (char *)reply, ret);
	if(size > len) {
		ERR("Truncating reply (was %d, now %d)\n", size, len);
		size = len;
	}
	memcpy(buf, CMD_FIELD(reply, EEPROM_BLK_RD_REPLY, data), size);
out:
	free_command(reply);
	return size;
}

int mpp_send_start(struct astribank_device *astribank, enum dev_dest dest, const char *ihex_version)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply = NULL;
	int			ret = 0;

	DBG("dest = %s ihex_version = '%s'\n", dev_dest2str(dest), ihex_version);
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_DEV_SEND_START, 0)) == NULL) {
		ERR("new_command failed\n");
		ret = -ENOMEM;
		goto out;
	}
	CMD_FIELD(cmd, DEV_SEND_START, dest) = dest;
	set_ihex_version(CMD_FIELD(cmd, DEV_SEND_START, ihex_version), ihex_version);
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		goto out;
	}
out:
	if(reply)
		free_command(reply);
	astribank->burn_state = (ret == 0)
		? BURN_STATE_STARTED
		: BURN_STATE_FAILED;
	return ret;
}

int mpp_send_end(struct astribank_device *astribank)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply = NULL;
	int			ret = 0;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_DEV_SEND_END, 0)) == NULL) {
		ERR("new_command failed\n");
		ret = -ENOMEM;
		goto out;
	}
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		goto out;
	}
out:
	if(reply)
		free_command(reply);
	astribank->burn_state = (ret == 0)
		? BURN_STATE_ENDED
		: BURN_STATE_FAILED;
	return ret;
}

int mpp_send_seg(struct astribank_device *astribank, const uint8_t *data, uint16_t offset, uint16_t len)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	if(!astribank->burn_state == BURN_STATE_STARTED) {
		ERR("Tried to send a segment while burn_state=%d\n",
				astribank->burn_state);
		return -EINVAL;
	}
	DBG("len = %d, offset = %d (0x%02X, 0x%02X)\n", len, offset, *data, *(data + 1));
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_DEV_SEND_SEG, len)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	CMD_FIELD(cmd, DEV_SEND_SEG, offset) = offset;
	memcpy(CMD_FIELD(cmd, DEV_SEND_SEG, data), data, len);
#if 0
	{
		FILE			*fp;
		if((fp = fopen("seg_data.bin", "a")) == NULL) {
			perror("seg_data.bin");
			exit(1);
		}
		if(fwrite(CMD_FIELD(cmd, DEV_SEND_SEG, data), len, 1, fp) != 1) {
			perror("fwrite");
			exit(1);
		}
		fclose(fp);
	}
#endif
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	free_command(reply);
	return 0;
}

int mpp_reset(struct astribank_device *astribank, int full_reset)
{
	struct mpp_command	*cmd;
	int			ret;
	int			op = (full_reset) ? MPP_RESET: MPP_HALF_RESET;

	DBG("full = %s\n", (full_reset) ? "YES" : "NO");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, op, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	ret = process_command(astribank, cmd, NULL);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	return 0;
}

int mpp_serial_cmd(struct astribank_device *astribank, const uint8_t *in, uint8_t *out, uint16_t len)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;
	uint8_t			*data;

	DBG("len=%d\n", len);
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_SER_SEND, len)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	data = CMD_FIELD(cmd, SER_SEND, data);
	memcpy(data, in, len);
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	assert(reply->header.op == MPP_SER_RECV);
	data = CMD_FIELD(reply, SER_RECV, data);
	memcpy(out, data, len);
	free_command(reply);
	return 0;
}

int mpps_card_info(struct astribank_device *astribank, int unit, uint8_t *card_type, uint8_t *card_status)
{
	struct card_info_send {
		uint8_t	ser_op;
		uint8_t	addr;
	} *card_info_send;
	struct card_info_recv {
		uint8_t	ser_op_undef;	/* invalid data */
		uint8_t	addr;
		uint8_t	card_full_type;	/* (type << 4 | subtype) */
		uint8_t	card_status;	/* BIT(0) - PIC burned */
	} *card_info_recv;
	uint8_t	in[sizeof(struct card_info_recv)];
	uint8_t	out[sizeof(struct card_info_recv)];
	int	len;
	int	ret;

	len = sizeof(struct card_info_recv);
	memset(in, 0, len);
	memset(out, 0, len);
	card_info_send = (struct card_info_send *)&in;
	card_info_recv = (struct card_info_recv *)&out;
	card_info_send->ser_op = SER_CARD_INFO_GET;
	card_info_send->addr = (unit << 4);	/* low nibble is subunit */
	ret = mpp_serial_cmd(astribank, in, out, len);
	if(ret < 0)
		return ret;
	*card_type = card_info_recv->card_full_type;
	*card_status = card_info_recv->card_status;
	return 0;
}

int mpp_tws_watchdog(struct astribank_device *astribank)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_TWS_WD_MODE_GET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	ret = CMD_FIELD(reply, TWS_WD_MODE_GET_REPLY, wd_active);
	DBG("wd_active=0x%X\n", ret);
	free_command(reply);
	return ret == 1;
}

int mpp_tws_setwatchdog(struct astribank_device *astribank, int yes)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("%s\n", (yes) ? "YES" : "NO");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_TWS_WD_MODE_SET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	CMD_FIELD(cmd, TWS_WD_MODE_SET, wd_active) = (yes) ? 1 : 0;
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	free_command(reply);
	return 0;
}

int mpp_tws_powerstate(struct astribank_device *astribank)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_TWS_PWR_GET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	ret = CMD_FIELD(reply, TWS_PWR_GET_REPLY, power);
	DBG("power=0x%X\n", ret);
	free_command(reply);
	return ret;
}

int mpp_tws_portnum(struct astribank_device *astribank)
{
	struct mpp_command	*cmd;
	struct mpp_command	*reply;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if((cmd = new_command(astribank->mpp_proto_version, MPP_TWS_PORT_GET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	ret = process_command(astribank, cmd, &reply);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	ret = CMD_FIELD(reply, TWS_PORT_GET_REPLY, portnum);
	DBG("portnum=0x%X\n", ret);
	free_command(reply);
	return ret;
}

int mpp_tws_setportnum(struct astribank_device *astribank, uint8_t portnum)
{
	struct mpp_command	*cmd;
	int			ret;

	DBG("\n");
	assert(astribank != NULL);
	if(portnum >= 2) {
		ERR("Invalid portnum (%d)\n", portnum);
		return -EINVAL;
	}
	if((cmd = new_command(astribank->mpp_proto_version, MPP_TWS_PORT_SET, 0)) == NULL) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	CMD_FIELD(cmd, TWS_PORT_SET, portnum) = portnum;
	ret = process_command(astribank, cmd, NULL);
	if(ret < 0) {
		ERR("process_command failed: %d\n", ret);
		return ret;
	}
	return 0;
}

/*
 * Wrappers
 */

struct astribank_device *mpp_init(const char devpath[])
{
	struct astribank_device *astribank;
	int			ret;

	DBG("devpath='%s'\n", devpath);
	if((astribank = astribank_open(devpath, 1)) == NULL) {
		ERR("Opening astribank failed\n");
		return NULL;
	}
	ret = mpp_proto_query(astribank);
	if(ret < 0) {
		ERR("Protocol handshake failed: %d\n", ret);
		goto err;
	}
	ret = mpp_status_query(astribank);
	if(ret < 0) {
		ERR("Status query failed: %d\n", ret);
		goto err;
	}
	return astribank;

err:
	if (astribank)
		astribank_close(astribank, 0);
	return NULL;
}

void mpp_exit(struct astribank_device *astribank)
{
	DBG("\n");
	astribank_close(astribank, 0);
}

/*
 * data structures
 */

void show_eeprom(const struct eeprom_table *eprm, FILE *fp)
{
	int	rmajor = (eprm->release >> 8) & 0xFF;
	int	rminor = eprm->release & 0xFF;;
	char	buf[BUFSIZ];

	memset(buf, 0, LABEL_SIZE + 1);
	memcpy(buf, eprm->label, LABEL_SIZE);
	fprintf(fp, "EEPROM: %-15s: 0x%02X\n", "Source", eprm->source);
	fprintf(fp, "EEPROM: %-15s: 0x%04X\n", "Vendor", eprm->vendor);
	fprintf(fp, "EEPROM: %-15s: 0x%04X\n", "Product", eprm->product);
	fprintf(fp, "EEPROM: %-15s: %d.%d\n", "Release", rmajor, rminor);
	fprintf(fp, "EEPROM: %-15s: 0x%02X\n", "Config", eprm->config_byte);
	fprintf(fp, "EEPROM: %-15s: '%s'\n", "Label", buf);
}

void show_capabilities(const struct capabilities *capabilities, FILE *fp)
{
	fprintf(fp, "Capabilities: FXS ports: %2d\n", capabilities->ports_fxs);
	fprintf(fp, "Capabilities: FXO ports: %2d\n", capabilities->ports_fxo);
	fprintf(fp, "Capabilities: BRI ports: %2d\n", capabilities->ports_bri);
	fprintf(fp, "Capabilities: PRI ports: %2d\n", capabilities->ports_pri);
	fprintf(fp, "Capabilities: TwinStar : %s\n",
		(CAP_EXTRA_TWINSTAR(capabilities)) ? "Yes" : "No");
}

void show_astribank_status(struct astribank_device *astribank, FILE *fp)
{
	char	version_buf[BUFSIZ];
	int	is_loaded = STATUS_FPGA_LOADED(astribank->status);

	fprintf(fp, "Astribank: EEPROM      : %s\n",
		eeprom_type2str(astribank->eeprom_type));
	fprintf(fp, "Astribank: FPGA status : %s\n",
		is_loaded ? "Loaded" : "Empty");
	if(is_loaded) {
		memset(version_buf, 0, sizeof(version_buf));
		memcpy(version_buf, astribank->fw_versions.fpga, VERSION_LEN);
		fprintf(fp, "Astribank: FPGA version: %s\n",
			version_buf);
	}
}

void show_extrainfo(const struct extrainfo *extrainfo, FILE *fp)
{
	fprintf(fp, "Extrainfo:             : %s\n", (const char *)(extrainfo->text));
}

int twinstar_show(struct astribank_device *astribank, FILE *fp)
{
	int	watchdog;
	int	powerstate;
	int	portnum;
	int	i;

	if(!astribank_has_twinstar(astribank)) {
		fprintf(fp, "TwinStar: NO\n");
		return 0;
	}
	if((watchdog = mpp_tws_watchdog(astribank)) < 0) {
		ERR("Failed getting TwinStar information\n");
		return watchdog;
	}
	if((powerstate = mpp_tws_powerstate(astribank)) < 0) {
		ERR("Failed getting TwinStar powerstate\n");
		return powerstate;
	}
	if((portnum = mpp_tws_portnum(astribank)) < 0) {
		ERR("Failed getting TwinStar portnum\n");
		return portnum;
	}
	fprintf(fp, "TwinStar: Connected to : USB-%1d\n", portnum);
	fprintf(fp, "TwinStar: Watchdog     : %s\n",
		(watchdog) ? "on-guard" : "off-guard");
	for(i = 0; i < 2; i++) {
		int	pw = (1 << i) & powerstate; 

		fprintf(fp, "TwinStar: USB-%1d POWER  : %s\n",
			i, (pw) ? "ON" : "OFF");
	}
	return 0;
}

