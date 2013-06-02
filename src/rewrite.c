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

static char *log_module="Rewrite: ";

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
      log_message( log_module, MSG_INFO,
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
      log_message( log_module, MSG_INFO,
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
      log_message( log_module, MSG_INFO,
                   "You have enabled the sort of the EIT PID\n");
    }
    else
      rewrite_vars->eit_sort = OPTION_OFF;
  }
  else if (!strcmp (substring, "sdt_force_eit"))
  {
    substring = strtok (NULL, delimiteurs);
    if(atoi (substring))
    {
      rewrite_vars->sdt_force_eit = OPTION_ON;
      log_message( log_module, MSG_INFO,
                   "You have enabled the forcing of the EIT flag in the SDT rewrite\n");
    }
    else
      rewrite_vars->sdt_force_eit = OPTION_OFF;
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

