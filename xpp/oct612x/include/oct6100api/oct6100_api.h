/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_api.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	Header file containing all definitions used throughout the API.

This file is part of the Octasic OCT6100 GPL API . The OCT6100 GPL API  is 
free software; you can redistribute it and/or modify it under the terms of 
the GNU General Public License as published by the Free Software Foundation; 
either version 2 of the License, or (at your option) any later version.

The OCT6100 GPL API is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
for more details. 

You should have received a copy of the GNU General Public License 
along with the OCT6100 GPL API; if not, write to the Free Software 
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

$Octasic_Release: OCT612xAPI-01.00-PR49 $

$Octasic_Revision: 23 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_API_H__
#define __OCT6100_API_H__

#ifdef __cplusplus
extern "C" {
#endif

/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

#include "oct6100_defines.h"
#include "oct6100_errors.h"

#include "oct6100_apiud.h"
#include "oct6100_tlv_inst.h"
#include "oct6100_chip_stats_inst.h"
#include "oct6100_tsi_cnct_inst.h"
#include "oct6100_mixer_inst.h"
#include "oct6100_events_inst.h"
#include "oct6100_tone_detection_inst.h"
#include "oct6100_conf_bridge_inst.h"
#include "oct6100_playout_buf_inst.h"

#include "oct6100_adpcm_chan_inst.h"
#include "oct6100_phasing_tsst_inst.h"
#include "oct6100_channel_inst.h"
#include "oct6100_interrupts_inst.h"
#include "oct6100_remote_debug_inst.h"
#include "oct6100_debug_inst.h"
#include "oct6100_chip_open_inst.h"
#include "oct6100_api_inst.h"

#include "oct6100_interrupts_pub.h"
#include "oct6100_tsi_cnct_pub.h"
#include "oct6100_events_pub.h"
#include "oct6100_tone_detection_pub.h"
#include "oct6100_mixer_pub.h"
#include "oct6100_conf_bridge_pub.h"
#include "oct6100_playout_buf_pub.h"

#include "oct6100_channel_pub.h"
#include "oct6100_remote_debug_pub.h"
#include "oct6100_debug_pub.h"
#include "oct6100_chip_open_pub.h"
#include "oct6100_chip_stats_pub.h"
#include "oct6100_adpcm_chan_pub.h"
#include "oct6100_phasing_tsst_pub.h"

#ifdef __cplusplus
}
#endif

#endif /* __OCT6100_API_H__ */
