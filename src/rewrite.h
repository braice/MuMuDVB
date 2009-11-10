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


/** @brief the parameters for the rewriting
 * This structure contain the parameters needed for rewriting
 */
typedef struct rewrite_parameters_t{
  /**Do we rewrite the PAT pid ?*/
  option_status_t rewrite_pat;
  /**The actual version of the PAT pid*/
  int pat_version;
  /**Do the actual full PAT needs to be updated ?*/
  int pat_needs_update;
  /**Do the full PAT is ok ?*/
  int full_pat_ok;
  /** The Complete PAT PID */
  mumudvb_ts_packet_t *full_pat;
  /** The continuity counter of the sent PAT*/
  int pat_continuity_counter;

  /**Do we rewrite the SDT pid ?*/
  option_status_t rewrite_sdt;
  /**The actual version of the SDT pid*/
  int sdt_version;
  /**Do the actual full SDT needs to be updated ?*/
  int sdt_needs_update;
  /**Do the full SDT is ok ?*/
  int full_sdt_ok;
  /** The Complete SDT PID */
  mumudvb_ts_packet_t *full_sdt;
  /** The continuity counter of the sent SDT*/
  int sdt_continuity_counter;

  /** Do we sort the EIT PID ?*/
  option_status_t eit_sort;

}rewrite_parameters_t;

int read_rewrite_configuration(rewrite_parameters_t *rewrite_vars, char *substring);

void pat_rewrite_new_global_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars);
int pat_rewrite_new_channel_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channel, int curr_channel);


void sdt_rewrite_new_global_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars);
int sdt_rewrite_new_channel_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channel, int curr_channel);

void set_continuity_counter(unsigned char *buf,int continuity_counter);

int eit_sort_new_packet(unsigned char *ts_packet, mumudvb_channel_t *channel);
