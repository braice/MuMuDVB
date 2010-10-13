/* 
 * MuMuDVB - Stream a DVB transport stream.
 * 
 * (C) 2004-2010 Brice DUBOST
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
 * @brief This file contains the general functions for rewriting
 */

#include <stdlib.h>
#include <string.h>

#include "mumudvb.h"
#include "ts.h"
#include "rewrite.h"
#include "log.h"
#include <stdint.h>


/** @brief Read a line of the configuration file to check if there is a rewrite parameter
 *
 * @param rewrite_vars the autoconfiguration parameters
 * @param substring The currrent line
 */
int read_rewrite_configuration(rewrite_parameters_t *rewrite_vars, char *substring)
{
  char delimiteurs[] = CONFIG_FILE_SEPARATOR;
  if (!strcmp (substring, "rewrite_pat"))
  {
    substring = strtok (NULL, delimiteurs);
    if(atoi (substring))
    {
      rewrite_vars->rewrite_pat = OPTION_ON;
      log_message( NULL, MSG_INFO,
                   "You have enabled the PAT Rewriting\n");
    }
    else
      rewrite_vars->rewrite_pat = OPTION_OFF;
  }
  else if (!strcmp (substring, "rewrite_sdt"))
  {
    substring = strtok (NULL, delimiteurs);
    if(atoi (substring))
    {
      rewrite_vars->rewrite_sdt = OPTION_ON;
      log_message( NULL, MSG_INFO,
                   "You have enabled the SDT Rewriting\n");
    }
    else
      rewrite_vars->rewrite_sdt = OPTION_OFF;

  }
  else if (!strcmp (substring, "sort_eit"))
  {
    substring = strtok (NULL, delimiteurs);
    if(atoi (substring))
    {
      rewrite_vars->eit_sort = OPTION_ON;
      log_message( NULL, MSG_INFO,
                   "You have enabled the sort of the EIT PID\n");
    }
    else
      rewrite_vars->eit_sort = OPTION_OFF;

  }
  else
    return 0; //Nothing concerning rewrite, we return 0 to explore the other possibilities

  return 1;//We found something for rewrite, we tell main to go for the next line
}


/** @brief Just a small function to change the continuity counter of a packet
 * This function will overwrite the continuity counter of the packet with the one given in argument
 *
 */
void set_continuity_counter(unsigned char *buf,int continuity_counter)
{
  ts_header_t *ts_header=(ts_header_t *)buf;
  ts_header->continuity_counter=continuity_counter;
}

/** @brief This function tells if we have to send the EIT packet
 */
int eit_sort_new_packet(unsigned char *ts_packet, mumudvb_channel_t *channel)
{
  int send_packet=1;
  ts_header_t *ts_header=(ts_header_t *)ts_packet;
  eit_t       *eit_header=(eit_t*)(ts_packet+TS_HEADER_LEN);
  if(ts_header->payload_unit_start_indicator) //New packet ?
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
    channel->eit_continuity_counter= channel->eit_continuity_counter % 32;
    return 1;
  }
  return 0;
}
