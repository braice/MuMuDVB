/*
 * MuMuDVB - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 *
 * (C) 2009 Brice DUBOST
 *
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
 
 /** @file
 * @brief File for Multicast related functions
 * @author Brice DUBOST
 * @date 2009
  */

#include "mumudvb.h"
#include "log.h"
#include <string.h>


 /** @brief Read a line of the configuration file to check if there is a cam parameter
 *
 * @param multicast_vars the multicast parameters
 * @param substring The currrent line
  */
int read_multicast_configuration(multicast_parameters_t *multicast_vars, mumudvb_channel_t *current_channel, int *ip_ok, char *substring)
{
  char delimiteurs[] = CONFIG_FILE_SEPARATOR;

  if (!strcmp (substring, "common_port"))
  {
    if ( *ip_ok )
    {
      log_message( MSG_ERROR,
                   "You have to set common_port before the channels\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    multicast_vars->common_port = atoi (substring);
  }
  else if (!strcmp (substring, "multicast_ttl"))
  {
    substring = strtok (NULL, delimiteurs);
    multicast_vars->ttl = atoi (substring);
  }
  else if (!strcmp (substring, "multicast_auto_join"))
  {
    substring = strtok (NULL, delimiteurs);
    multicast_vars->auto_join = atoi (substring);
  }
  else if (!strcmp (substring, "ip"))
  {
    if ( *ip_ok )
    {
      log_message( MSG_ERROR,
                   "You must precise the pids last, or you forgot the pids\n");
      return -1;
    }

    substring = strtok (NULL, delimiteurs);
    if(strlen(substring)>19)
    {
      log_message( MSG_ERROR,
                   "The Ip address %s is too long.\n", substring);
      return -1;
    }
    sscanf (substring, "%s\n", current_channel->ipOut);
    *ip_ok = 1;
  }
  else if (!strcmp (substring, "port"))
  {
    if ( *ip_ok == 0)
    {
      log_message( MSG_ERROR,
                   "port : You must precise ip first\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->portOut = atoi (substring);
  }
  else
    return 0; //Nothing concerning multicast, we return 0 to explore the other possibilities

  return 1;//We found something for multicast, we tell main to go for the next line
  
}