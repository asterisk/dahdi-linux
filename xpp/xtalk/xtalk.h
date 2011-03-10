#ifndef	XTALK_H
#define	XTALK_H
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

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*
 * XTALK - Base protocol for our USB devices
 *         It is meant to provide a common base for layered
 *         protocols (dialects)
 */

#include <stdint.h>
#include <stdlib.h>
/* Definitions common to the firmware (in include/ directory) */
#include <xtalk_defs.h>

#ifdef	__GNUC__
#define	PACKED	__attribute__((packed))
#else
#error "We do not know how your compiler packs structures"
#endif

struct xtalk_device;
struct xtalk_command_desc;

typedef int (*xtalk_cmd_callback_t)(
	struct xtalk_device *xtalk_dev,
	struct xtalk_command_desc *xtalk_cmd);

/* Describe a single xtalk command */
struct xtalk_command_desc {
	uint8_t			op;
	const char		*name;
	xtalk_cmd_callback_t	callback;
	uint16_t		len;	/* Minimal length */
};

/* Define a complete protocol */
struct xtalk_protocol {
	const char			*name;
	uint8_t				proto_version;
	struct xtalk_command_desc	commands[MAX_OPS];
	const char			*ack_statuses[MAX_STATUS];
};

/*
 * The common header of every xtalk command
 * in every xtalk dialect.
 */
struct xtalk_header {
	uint16_t	len;
	uint16_t	seq;
	uint8_t		op;	/* MSB: 0 - to device, 1 - from device */
} PACKED;

struct xtalk_command {
	/* Common part */
	struct xtalk_header	header;
	/* Each dialect has its own data members */
	union private_data {
		uint8_t	raw_data[0];
	} PACKED alt;
} PACKED;

/*
 * Macros to unify access to protocol packets and fields:
 *   p		- signify the dialect prefix (XTALK for base protocol)
 *   o		- signify command op (e.g: ACK)
 *   cmd	- A pointer to struct xtalk_command
 *   field	- field name (e.g: raw_data)
 */
#define XTALK_STRUCT(p,o)	p ## _struct_ ## o
#define XTALK_PDATA(o)		xtalk_privdata_ ## o
#define	CMD_FIELD(cmd, p, o, field)	(((union XTALK_PDATA(p) *)&((cmd)->alt))->XTALK_STRUCT(p, o).field)
#define CMD_DEF(p, o, ...)	struct XTALK_STRUCT(p, o) {	\
					__VA_ARGS__		\
				} PACKED XTALK_STRUCT(p, o)
#define	MEMBER(p, o)	struct XTALK_STRUCT(p, o) XTALK_STRUCT(p, o)

/* Wrappers for transport (xusb) functions */
struct xtalk_ops {
	int	(*send_func)(void *transport_priv, void *data, size_t len, int timeout);
	int	(*recv_func)(void *transport_priv, void *data, size_t maxlen, int timeout);
	int	(*close_func)(void *transport_priv);
};

/*
 * Base XTALK device. A pointer to this struct
 * should be included in the struct representing
 * the dialect.
 */
struct xtalk_device;

/* high-level */
struct xtalk_device *xtalk_new(const struct xtalk_ops *ops, size_t packet_size, void *transport_priv);
void xtalk_delete(struct xtalk_device *dev);
int xtalk_set_protocol(struct xtalk_device *xtalk_dev, const struct xtalk_protocol *xproto);
int xtalk_proto_query(struct xtalk_device *dev);
void xtalk_dump_command(struct xtalk_command *cmd);

/* low-level */
int process_command(
	struct xtalk_device *dev,
	struct xtalk_command *cmd,
	struct xtalk_command **reply_ref);
struct xtalk_command *new_command(
	const struct xtalk_device *xtalk_dev,
	uint8_t op, uint16_t extra_data);
void free_command(struct xtalk_command *cmd);

/*
 * Convenience macros to define entries in a protocol command table:
 *   p		- signify the dialect prefix (XTALK for base protocol)
 *   o		- signify command op (e.g: ACK)
 *   cb		- A callback function (type xtalk_cmd_callback_t)
 */
#define	CMD_RECV(p,o,cb)	\
	[p ## _ ## o | XTALK_REPLY_MASK] {		\
		.op = p ## _ ## o | XTALK_REPLY_MASK,	\
		.name = #o "_reply",			\
		.callback = (cb), 			\
		.len =					\
			sizeof(struct xtalk_header) +	\
			sizeof(struct XTALK_STRUCT(p,o)),	\
	}

#define	CMD_SEND(p,o)	\
	[p ## _ ## o] {		\
		.op = p ## _ ## o,	\
		.name = #o,		\
		.callback = NULL, 	\
		.len =					\
			sizeof(struct xtalk_header) +	\
			sizeof(struct XTALK_STRUCT(p,o)),	\
	}

/*
 * Convenience macro to define statuses:
 *   x		- status code (e.g: OK)
 *   m		- status message (const char *)
 */
#define	ACK_STAT(x,m)	[ STAT_ ## x ] = (m)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* XTALK_H */
