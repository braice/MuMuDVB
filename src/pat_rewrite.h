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
 * The pat rewrite is made to announce only the video stram associated with the channel in the PAT pid
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
  /**Do the actual full pat needs to ve updated ?*/
  int needs_update;
  /** The Complete PAT PID */
  mumudvb_ts_packet_t *full_pat;
  /**The generated pats to be sent*/
  unsigned char generated_pats[MAX_CHANNELS][TS_PACKET_SIZE]; /**@todo: allocate dynamically*/ /**@todo : put them in the channel structure*/
  /** The version of the generated pats */
  int generated_pat_version[MAX_CHANNELS];
  /** The continuity counter of the sent PAT*/
  int continuity_counter;
}pat_rewrite_parameters_t;


int pat_channel_rewrite(pat_rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channels, int curr_channel, unsigned char *buf);
int pat_need_update(pat_rewrite_parameters_t *rewrite_vars, unsigned char *buf);
void update_version(pat_rewrite_parameters_t *rewrite_vars);
void pat_rewrite_set_continuity_counter(unsigned char *buf,int continuity_counter);
