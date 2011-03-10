#ifndef	XTALK_DEFS_H
#define	XTALK_DEFS_H

#define	MAX_OPS			256	/* single byte */
#define	MAX_STATUS		256	/* single byte */

#define	XTALK_REPLY_MASK	0x80	/* Every reply has this bit */

#define	PRIVATE_OP_FIRST	0x05
#define	PRIVATE_OP_LAST		0x7F
#define	IS_PRIVATE_OP(x)	(	\
					(((x) & ~(XTALK_REPLY_MASK)) >= PRIVATE_OP_FIRST) &&	\
					(((x) & ~(XTALK_REPLY_MASK)) <= PRIVATE_OP_LAST)	\
				)

#define	XTALK_ACK		0x80
#define	XTALK_PROTO_GET		0x01
#define	XTALK_PROTO_GET_REPLY	(XTALK_PROTO_GET | XTALK_REPLY_MASK)
#define	XTALK_FWVERS_GET	0x11
#define	XTALK_FWVERS_GET_REPLY	(XTALK_FWVERS_GET | XTALK_REPLY_MASK)
#define XTALK_CAPS_GET		0x0E	/* Get EEPROM table contents Product/Vendor Id ... */
#define XTALK_CAPS_GET_REPLY	(XTALK_CAPS_GET | XTALK_REPLY_MASK)

/*------------- XTALK: statuses in ACK ---------------------------------------*/
#define	STAT_OK			0x00	/* OK                         */
#define	STAT_FAIL		0x01	/* last command failed        */
#define	STAT_RESET_FAIL		0x02	/* reset  failed              */
#define	STAT_NODEST		0x03	/* No destination is selected */
#define	STAT_MISMATCH		0x04	/* Data mismatch              */
#define	STAT_NOACCESS		0x05	/* No access                  */
#define	STAT_BAD_CMD		0x06	/* Bad command                */
#define	STAT_TOO_SHORT		0x07	/* Packet is too short        */
#define	STAT_ERROFFS		0x08	/* Offset error (not used)    */
#define	STAT_NO_LEEPROM		0x0A	/* Large EEPROM was not found */
#define	STAT_NO_EEPROM		0x0B	/* No EEPROM was found        */
#define	STAT_WRITE_FAIL		0x0C	/* Writing to device failed   */
#define	STAT_NOPWR_ERR		0x10	/* No power on USB connector  */


#endif	/* XTALK_DEFS_H */
