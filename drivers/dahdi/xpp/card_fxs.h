#ifndef	CARD_FXS_H
#define	CARD_FXS_H
/*
 * Written by Oron Peled <oron@actcom.co.il>
 * Copyright (C) 2004-2006, Xorcom
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

#include "xpd.h"

enum fxs_opcodes {
	XPROTO_NAME(FXS, SIG_CHANGED)		= 0x06,
/**/
	XPROTO_NAME(FXS, CHAN_POWER)		= 0x0F,	/* Write to SLIC */
	XPROTO_NAME(FXS, CHAN_CID)		= 0x0F,	/* Write to SLIC */
	XPROTO_NAME(FXS, LED)			= 0x0F,	/* Write to SLIC */
};


DEF_RPACKET_DATA(FXS, SIG_CHANGED,
	xpp_line_t	sig_status;	/* channels: lsb=1, msb=8 */
	xpp_line_t	sig_toggles;	/* channels: lsb=1, msb=8 */
	);

#endif	/* CARD_FXS_H */
