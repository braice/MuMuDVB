/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) 2009 Brice DUBOST
 * 
 * The latest version can be found at http://mumudvb.braice.net
 * 
 * Copyright notice:
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

/**@file
 * @brief This file contains the headers for the functions for rewriting the pat pid
 *
 * The pat rewrite is made to announce only the video stream associated with the channel in the PAT pid
 * Some set top boxes need it
 */

#include <stdlib.h>
#include <string.h>

#include "mumudvb.h"
#include "ts.h"
#include <stdint.h>


/** @brief the parameters for the Pat rewriting
 * This structure contain the parameters needed for Pat rewriting
 */
typedef struct pat_rewrite_parameters_t{
  /**Do we rewrite the PAT pid ?*/
  int rewrite_pat;
  /**The actual version of the pat pid*/
  int pat_version;
  /**Do the actual full pat needs to be updated ?*/
  int pat_needs_update;
  /**Do the full pat is ok ?*/
  int full_pat_ok;
  /** The Complete PAT PID */
  mumudvb_ts_packet_t *full_pat;
  /** The continuity counter of the sent PAT*/
  int pat_continuity_counter;
}pat_rewrite_parameters_t;


void pat_rewrite_new_global_packet(unsigned char *ts_packet, pat_rewrite_parameters_t *rewrite_vars);
int pat_rewrite_new_channel_packet(unsigned char *ts_packet, pat_rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channel, int curr_channel);
