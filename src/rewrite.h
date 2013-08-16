/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) 2009-2013 Brice DUBOST
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

#ifndef _REWRITE_H
#define _REWRITE_H


#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "mumudvb.h"
#include "ts.h"
#include "unicast_http.h"
#include <stdint.h>


/** @brief the structure for storing an EIT PID for a particular SID
 * This structure contains the packet and several flags around
 */
typedef struct eit_packet_t{
	  /**The service ID of the EIT*/
	  int service_id;
	  /** The table ID */
	  uint8_t table_id;
	  /**The actual version of the EIT PID*/
	  int version;
	  /**Do the actual full EIT needs to be updated ?*/
	  int needs_update;
	  /** Do we need to see other EIT ?*/
	  int need_others;
	  /** The last_section_number of the current version */
	  int last_section_number;
	  /** Array storing the section numbers we saw */
	  int sections_stored[256];
	  /**Do the full EIT is ok ?*/
	  int full_eit_ok;
	  /** The Complete EIT PID  for each section*/
	  mumudvb_ts_packet_t* full_eit_sections[256];
	  /** The continuity counter of the sent EIT*/
	  int continuity_counter;
	  /** Pointer to the next one */
	  struct eit_packet_t *next;
}eit_packet_t;


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
  /** Do we need to see other SDT ?*/
  int sdt_need_others;
  /** The last_section_number of the current version */
  int sdt_last_section_number;
  /** Array storing the section numbers we saw */
  int sdt_section_numbers_seen[256];
  /**Do the full SDT is ok ?*/
  int full_sdt_ok;
  /** The Complete SDT PID */
  mumudvb_ts_packet_t *full_sdt;
  /** The continuity counter of the sent SDT*/
  int sdt_continuity_counter;
  /** Do we force the EIT presence ? */
  int sdt_force_eit;

  /** Do we sort the EIT PID ?*/
  option_status_t rewrite_eit;
  /**The actual version of the EIT pid*/
  int eit_version;
  /**Do the actual full EIT needs to be updated ?*/
  int eit_needs_update;
  /** The Complete EIT PID  which we are storing*/
  mumudvb_ts_packet_t *full_eit;

  eit_packet_t *eit_packets;

}rewrite_parameters_t;



int read_rewrite_configuration(rewrite_parameters_t *rewrite_vars, char *substring);

void pat_rewrite_new_global_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars);
int pat_rewrite_new_channel_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channel, int curr_channel);


int sdt_rewrite_new_global_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars);
int sdt_rewrite_new_channel_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channel, int curr_channel);

void set_continuity_counter(unsigned char *buf,int continuity_counter);


int eit_rewrite_new_global_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars);
void eit_rewrite_new_channel_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channel,
		multicast_parameters_t *multicast_vars, unicast_parameters_t *unicast_vars, mumudvb_chan_and_pids_t *chan_and_pids,fds_t *fds);

#endif
