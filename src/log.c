/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2008 Brice DUBOST
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>

#include "mumudvb.h"
#include "errors.h"

extern int no_daemon;
extern int verbosity;
extern int log_initialised;

void log_message( int type,
                    const char *psz_format, ... )
{
  va_list args;
  int priority;

  priority=LOG_USER;
  va_start( args, psz_format );
  

  if(type<verbosity)
    {
      if (no_daemon || !log_initialised)
	vfprintf(stderr, psz_format, args );
      else
	{
	  //what is the priority ?
	  switch(type)
	    {
	    case MSG_ERROR:
	      priority|=LOG_ERR;
	      break;
	    case MSG_WARN:
	      priority|=LOG_WARNING;
	      break;
	    case MSG_INFO:
	      priority|=LOG_INFO;
	      break;
	    case MSG_DETAIL:
	      priority|=LOG_NOTICE;
	    break;
	    case MSG_DEBUG:
	      priority|=LOG_DEBUG;
	      break;
	    }
	  vsyslog (priority, psz_format, args );
	}
    }

  va_end( args );
}


void log_streamed_channels(int number_of_channels, mumudvb_channel_t *channels)
{
  int curr_channel;
  int curr_pid;

  log_message( MSG_INFO, "Diffusion %d channel%s\n", number_of_channels,
	       (number_of_channels <= 1 ? "" : "s"));
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      log_message( MSG_INFO, "Channel number : %3d, ip : %s:%d, name : \"%s\"\n",
		   curr_channel, channels[curr_channel].ipOut, channels[curr_channel].portOut, channels[curr_channel].name);
      log_message( MSG_DETAIL, "        pids : ");
      for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
	log_message( MSG_DETAIL, "%d ", channels[curr_channel].pids[curr_pid]);
      log_message( MSG_DETAIL, "\n");
    }
}


void
gen_chaines_diff (char *nom_fich_chaines_diff, char *nom_fich_chaines_non_diff, int nb_flux, mumudvb_channel_t *channels)
{
  FILE *chaines_diff;
  FILE *chaines_non_diff;
  int curr_channel;

  chaines_diff = fopen (nom_fich_chaines_diff, "w");
  if (chaines_diff == NULL)
    {
      log_message( MSG_INFO,
		   "%s: %s\n",
		   nom_fich_chaines_diff, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }

  chaines_non_diff = fopen (nom_fich_chaines_non_diff, "w");
  if (chaines_non_diff == NULL)
    {
      log_message( MSG_INFO,
		   "%s: %s\n",
		   nom_fich_chaines_non_diff, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }

  for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
    // on envoie le old pour annoncer que les chaines qui diffusent au dessus du quota de pauqets
    if (channels[curr_channel].streamed_channel_old)
      fprintf (chaines_diff, "%s:%d:%s\n", channels[curr_channel].ipOut, channels[curr_channel].portOut, channels[curr_channel].name);
    else
      fprintf (chaines_non_diff, "%s:%d:%s\n", channels[curr_channel].ipOut, channels[curr_channel].portOut, channels[curr_channel].name);
  fclose (chaines_diff);
  fclose (chaines_non_diff);

}
