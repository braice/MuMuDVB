/*
 * MuMuDVB - Stream a DVB transport stream.
 *
 * (C) 2004-2013 Brice DUBOST
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
 * @brief This file contains the function for rewriting the EIT pid
 *
 * The EIT rewrite is made to announce only the video stream associated with the channel in the EIT pid
 * It avoids to have ghost channels which can disturb the clients
 */

#include <stdlib.h>
#include <string.h>

#include "mumudvb.h"
#include "ts.h"
#include "rewrite.h"
#include "log.h"
#include <stdint.h>

extern uint32_t       crc32_table[256];

static char *log_module="EIT rewrite: ";

/** @brief This function tells if we have to send the EIT packet
 */
int eit_sort_new_packet(unsigned char *ts_packet, mumudvb_channel_t *channel)
{
  int send_packet=1;
  ts_header_t *ts_header=(ts_header_t *)ts_packet;
  eit_t       *eit_header=(eit_t*)(get_ts_begin(ts_packet));
  if(ts_header->payload_unit_start_indicator && eit_header) //New packet ?
  {
    if((channel->service_id) &&
        (channel->service_id!= (HILO(eit_header->service_id))))
    {
      send_packet=0;
      channel->eit_dropping=1; //We say that we will drop all the other parts of this packet
    }
    else
    {
      channel->eit_dropping=0;//We say that we will keep all the other parts of this packet
    }
  }
  else if(channel->eit_dropping) //It's not the beginning of a new packet and we drop the beginning, we continue dropping
    send_packet=0;

  if(send_packet)
  {
    /*We set the continuity counter*/
    set_continuity_counter(ts_packet,channel->eit_continuity_counter);
    /*To avoid discontinuities, we have to update the continuity counter*/
    channel->eit_continuity_counter++;
    channel->eit_continuity_counter= channel->eit_continuity_counter % 16;
    return 1;
  }
  return 0;
}


