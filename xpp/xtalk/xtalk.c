/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2009, Xorcom
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>
#include <xtalk.h>
#include <debug.h>

#define	DBG_MASK	0x02

#define	TIMEOUT		6000

/*
 * Base XTALK device. A pointer to this struct
 * should be included in the struct representing
 * the dialect.
 */
struct xtalk_device {
	void			*transport_priv;	/* e.g: struct xusb */
	struct xtalk_ops	ops;
	struct xtalk_protocol	xproto;
	uint8_t			xtalk_proto_version;
	uint8_t                 status;
	size_t			packet_size;
	uint16_t                tx_sequenceno;
};

CMD_DEF(XTALK, ACK,
	uint8_t stat;
	);

CMD_DEF(XTALK, PROTO_GET,
	uint8_t proto_version;
	uint8_t reserved;
	);

CMD_DEF(XTALK, PROTO_GET_REPLY,
	uint8_t proto_version;
	uint8_t reserved;
	);

union XTALK_PDATA(XTALK) {
	MEMBER(XTALK, ACK);
	MEMBER(XTALK, PROTO_GET);
	MEMBER(XTALK, PROTO_GET_REPLY);
} PACKED members;

struct xtalk_protocol	xtalk_base = {
	.name	= "XTALK",
	.proto_version = 0,
	.commands = {
		CMD_RECV(XTALK, ACK, NULL),
		CMD_SEND(XTALK, PROTO_GET),
		CMD_RECV(XTALK, PROTO_GET_REPLY, NULL),
	},
	.ack_statuses = {
		ACK_STAT(OK, "Acknowledges previous command"),
		ACK_STAT(FAIL, "Last command failed"),
		ACK_STAT(RESET_FAIL, "reset failed"),
		ACK_STAT(NODEST, "No destination is selected"),
		ACK_STAT(MISMATCH, "Data mismatch"),
		ACK_STAT(NOACCESS, "No access"),
		ACK_STAT(BAD_CMD, "Bad command"),
		ACK_STAT(TOO_SHORT, "Packet is too short"),
		ACK_STAT(ERROFFS, "Offset error (not used)"),
		ACK_STAT(NO_LEEPROM, "Large EEPROM was not found"),
		ACK_STAT(NO_EEPROM, "No EEPROM was found"),
		ACK_STAT(WRITE_FAIL, "Writing to device failed"),
		ACK_STAT(NOPWR_ERR, "No power on USB connector"),
	}
};

void free_command(struct xtalk_command *cmd)
{
	if (!cmd)
		return;
	memset(cmd, 0, cmd->header.len);
	free(cmd);
}

static const struct xtalk_command_desc *get_command_desc(
		const struct xtalk_protocol *xproto, uint8_t op)
{
	const struct xtalk_command_desc	*desc;

	if (!xproto)
		return NULL;
	desc = &xproto->commands[op];
	if (!desc->name)
		return NULL;
#if 0
	DBG("%s version=%d, op=0x%X (%s)\n",
		xproto->name, xproto->proto_version,
		op, desc->name);
#endif
	return desc;
}

static const char *ack_status_msg(const struct xtalk_protocol *xproto,
		uint8_t status)
{
	const char	*ack_status;

	if (!xproto)
		return NULL;
	ack_status = xproto->ack_statuses[status];
	DBG("%s status=0x%X (%s)\n", xproto->name, status, ack_status);
	return ack_status;
}

int xtalk_set_protocol(struct xtalk_device *xtalk_dev,
		const struct xtalk_protocol *xproto)
{
	const char	*protoname = (xproto) ? xproto->name : "GLOBAL";
	int		i;

	DBG("%s\n", protoname);
	memset(&xtalk_dev->xproto, 0, sizeof(xtalk_dev->xproto));
	for (i = 0; i < MAX_OPS; i++) {
		const struct xtalk_command_desc	*desc;

		desc = get_command_desc(xproto, i);
		if (desc) {
			if (!IS_PRIVATE_OP(i)) {
				ERR("Bad op=0x%X "
					"(should be in the range [0x%X-0x%X]\n",
					i, PRIVATE_OP_FIRST, PRIVATE_OP_LAST);
				return -EINVAL;
			}
			xtalk_dev->xproto.commands[i] = *desc;
			DBG("private: op=0x%X (%s)\n", i, desc->name);
		} else {
			if (!IS_PRIVATE_OP(i)) {
				const char	*name;

				xtalk_dev->xproto.commands[i] =
					xtalk_base.commands[i];
				name = xtalk_dev->xproto.commands[i].name;
				if (name)
					DBG("global: op=0x%X (%s)\n", i, name);
			}
		}
	}
	for (i = 0; i < MAX_STATUS; i++) {
		const char	*stat_msg;

		stat_msg = (xproto) ? xproto->ack_statuses[i] : NULL;
		if (stat_msg) {
			if (!IS_PRIVATE_OP(i)) {
				ERR("Bad status=0x%X "
					"(should be in the range [0x%X-0x%X]\n",
					i, PRIVATE_OP_FIRST, PRIVATE_OP_LAST);
				return -EINVAL;
			}
			xtalk_dev->xproto.ack_statuses[i] = stat_msg;
			DBG("private: status=0x%X (%s)\n", i, stat_msg);
		} else {
			if (!IS_PRIVATE_OP(i)) {
				const char	*stat_msg;

				xtalk_dev->xproto.ack_statuses[i] =
					xtalk_base.ack_statuses[i];
				stat_msg = xtalk_dev->xproto.ack_statuses[i];
				if (stat_msg)
					DBG("global: status=0x%X (%s)\n",
						i, stat_msg);
			}
		}
	}
	xtalk_dev->xproto.name = protoname;
	xtalk_dev->xproto.proto_version = (xproto) ? xproto->proto_version : 0;
	return 0;
}

struct xtalk_command *new_command(
	const struct xtalk_device *xtalk_dev,
	uint8_t op, uint16_t extra_data)
{
	const struct xtalk_protocol	*xproto;
	struct xtalk_command		*cmd;
	const struct xtalk_command_desc	*desc;
	uint16_t			len;

	xproto = &xtalk_dev->xproto;
	desc = get_command_desc(xproto, op);
	if (!desc) {
		ERR("Unknown op=0x%X.\n", op);
		return NULL;
	}
	DBG("OP=0x%X [%s] (extra_data %d)\n", op, desc->name, extra_data);
	len = desc->len + extra_data;
	cmd = malloc(len);
	if (!cmd) {
		ERR("Out of memory\n");
		return NULL;
	}
	if (extra_data) {
		uint8_t	*ptr = (uint8_t *)cmd;

		DBG("clear extra_data (%d bytes)\n", extra_data);
		memset(ptr + desc->len, 0, extra_data);
	}
	cmd->header.op = op;
	cmd->header.len = len;
	cmd->header.seq = 0;	/* Overwritten in send_usb() */
	return cmd;
}

void xtalk_dump_command(struct xtalk_command *cmd)
{
	uint16_t	len;
	int		i;

	len = cmd->header.len;
	if (len < sizeof(struct xtalk_header)) {
		ERR("Command too short (%d)\n", len);
		return;
	}
	INFO("DUMP: OP=0x%X len=%d seq=%d\n",
		cmd->header.op, cmd->header.len, cmd->header.seq);
	for (i = 0; i < len - sizeof(struct xtalk_header); i++)
		INFO("  %2d. 0x%X\n", i, cmd->alt.raw_data[i]);
}

static int send_command(struct xtalk_device *xtalk_dev,
		struct xtalk_command *cmd, int timeout)
{
	int		ret;
	int		len;
	void		*priv = xtalk_dev->transport_priv;

	len = cmd->header.len;
	cmd->header.seq = xtalk_dev->tx_sequenceno;

	ret = xtalk_dev->ops.send_func(priv, (char *)cmd, len, timeout);
	if (ret < 0)
		DBG("send_func failed ret=%d\n", ret);
	xtalk_dev->tx_sequenceno++;
	return ret;
}

static struct xtalk_command *recv_command(struct xtalk_device *xtalk_dev,
		int timeout)
{
	struct xtalk_command	*reply;
	void			*priv = xtalk_dev->transport_priv;
	size_t			psize = xtalk_dev->packet_size;
	int			ret;

	reply = malloc(psize);
	if (!reply) {
		ERR("Out of memory\n");
		goto err;
	}
	reply->header.len = 0;
	ret = xtalk_dev->ops.recv_func(priv, (char *)reply, psize, timeout);
	if (ret < 0) {
		ERR("Receive from usb failed.\n");
		goto err;
	} else if (ret == 0) {
		goto err;	/* No reply */
	}
	if (ret != reply->header.len) {
		ERR("Wrong length received: got %d bytes, "
			"but length field says %d bytes%s\n",
			ret, reply->header.len,
			(ret == 1) ? ". Old USB firmware?" : "");
		goto err;
	}
	/* dump_packet(LOG_DEBUG, DBG_MASK, __func__, (char *)reply, ret); */
	return reply;
err:
	if (reply) {
		memset(reply, 0, psize);
		free_command(reply);
	}
	return NULL;
}


__attribute__((warn_unused_result))
int process_command(
	struct xtalk_device *xtalk_dev,
	struct xtalk_command *cmd,
	struct xtalk_command **reply_ref)
{
	const struct xtalk_protocol	*xproto;
	struct xtalk_command		*reply = NULL;
	const struct xtalk_command_desc	*reply_desc;
	const struct xtalk_command_desc	*expected;
	const struct xtalk_command_desc	*cmd_desc;
	uint8_t				reply_op;
	const char			*protoname;
	int				ret;

	xproto = &xtalk_dev->xproto;
	protoname = (xproto) ? xproto->name : "GLOBAL";
	/* So the caller knows if a reply was received */
	if (reply_ref)
		*reply_ref = NULL;
	reply_op = cmd->header.op | XTALK_REPLY_MASK;
	cmd_desc = get_command_desc(xproto, cmd->header.op);
	expected = get_command_desc(xproto, reply_op);
	ret = send_command(xtalk_dev, cmd, TIMEOUT);
	if (!reply_ref) {
		DBG("No reply requested\n");
		goto out;
	}
	if (ret < 0) {
		ERR("send_command failed: %d\n", ret);
		goto out;
	}
	reply = recv_command(xtalk_dev, TIMEOUT);
	if (!reply) {
		ERR("recv_command failed\n");
		ret = -EPROTO;
		goto out;
	}
	*reply_ref = reply;
	if ((reply->header.op & 0x80) != 0x80) {
		ERR("Unexpected reply op=0x%02X, should have MSB set.\n",
			reply->header.op);
		ret = -EPROTO;
		goto out;
	}
	DBG("REPLY OP: 0x%X\n", reply->header.op);
	reply_desc = get_command_desc(xproto, reply->header.op);
	if (!reply_desc) {
		ERR("Unknown reply (proto=%s) op=0x%02X\n",
			protoname, reply->header.op);
		ret = -EPROTO;
		goto out;
	}
	DBG("REPLY NAME: %s\n", reply_desc->name);
	if (reply->header.op == XTALK_ACK) {
		int	status = CMD_FIELD(reply, XTALK, ACK, stat);

		if (expected) {
			ERR("Expected OP=0x%02X: Got ACK(%d): %s\n",
				reply_op,
				status,
				ack_status_msg(xproto, status));
			ret = -EPROTO;
			goto out;
		} else if (status != STAT_OK) {

			ERR("Got ACK (for OP=0x%X [%s]): %d %s\n",
				cmd->header.op,
				cmd_desc->name,
				status, ack_status_msg(xproto, status));
			ret = -EPROTO;
			goto out;
		}
		/* Good expected ACK ... */
	} else if (reply->header.op != reply_op) {
			ERR("Expected OP=0x%02X: Got OP=0x%02X\n",
				reply_op, reply->header.op);
			ret = -EPROTO;
			goto out;
	}
	if (expected && expected->len > reply->header.len) {
			ERR("Expected len=%d: Got len=%d\n",
				expected->len, reply->header.len);
			ret = -EPROTO;
			goto out;
	}
	if (cmd->header.seq != reply->header.seq) {
			ERR("Expected seq=%d: Got seq=%d\n",
				cmd->header.seq, reply->header.seq);
			ret = -EPROTO;
			goto out;
	}
	ret = reply->header.len;	/* All good, return the length */
	DBG("returning reply op 0x%X (%d bytes)\n", reply->header.op, ret);
out:
	free_command(cmd);
	if (!reply_ref && reply)
		free_command(reply);
	return ret;
}

/*
 * Protocol Commands
 */

int xtalk_proto_query(struct xtalk_device *xtalk_dev)
{
	struct xtalk_command		*cmd;
	struct xtalk_command		*reply;
	uint8_t				proto_version;
	int				ret;

	DBG("\n");
	assert(xtalk_dev != NULL);
	proto_version = xtalk_dev->xproto.proto_version;
	cmd = new_command(xtalk_dev, XTALK_PROTO_GET, 0);
	if (!cmd) {
		ERR("new_command failed\n");
		return -ENOMEM;
	}
	/* Protocol Version */
	CMD_FIELD(cmd, XTALK, PROTO_GET, proto_version) = proto_version;
	ret = process_command(xtalk_dev, cmd, &reply);
	if (ret < 0) {
		ERR("process_command failed: %d\n", ret);
		goto out;
	}
	xtalk_dev->xtalk_proto_version =
		CMD_FIELD(reply, XTALK, PROTO_GET_REPLY, proto_version);
	if (xtalk_dev->xtalk_proto_version != proto_version) {
		DBG("Got %s protocol version: 0x%02x (expected 0x%02x)\n",
			xtalk_dev->xproto.name,
			xtalk_dev->xtalk_proto_version,
			proto_version);
		ret = xtalk_dev->xtalk_proto_version;
		goto out;
	}
	DBG("Protocol version: %02x\n", xtalk_dev->xtalk_proto_version);
	ret = xtalk_dev->xtalk_proto_version;
out:
	free_command(reply);
	return ret;
}

/*
 * Wrappers
 */

struct xtalk_device *xtalk_new(const struct xtalk_ops *ops,
	size_t packet_size, void *priv)
{
	struct xtalk_device	*xtalk_dev;
	int			ret;

	DBG("\n");
	assert(ops != NULL);
	xtalk_dev = malloc(sizeof(*xtalk_dev));
	if (!xtalk_dev) {
		ERR("Allocating XTALK device memory failed\n");
		return NULL;
	}
	memset(xtalk_dev, 0, sizeof(*xtalk_dev));
	memcpy((void *)&xtalk_dev->ops, (const void *)ops,
		sizeof(xtalk_dev->ops));
	xtalk_dev->transport_priv = priv;
	xtalk_dev->packet_size = packet_size;
	xtalk_dev->tx_sequenceno = 1;
	ret = xtalk_set_protocol(xtalk_dev, NULL);
	if (ret < 0) {
		ERR("GLOBAL Protocol registration failed: %d\n", ret);
		goto err;
	}
	return xtalk_dev;

err:
	if (xtalk_dev)
		xtalk_delete(xtalk_dev);
	return NULL;
}

void xtalk_delete(struct xtalk_device *xtalk_dev)
{
	void	*priv;

	if (!xtalk_dev)
		return;
	DBG("\n");
	priv = xtalk_dev->transport_priv;
	assert(priv);
	xtalk_dev->tx_sequenceno = 0;
	assert(&xtalk_dev->ops != NULL);
	assert(&xtalk_dev->ops.close_func != NULL);
	xtalk_dev->ops.close_func(priv);
}
