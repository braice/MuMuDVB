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

/**
 * Print a log message on the console or via syslog 
 * depending if mumudvb is daemonized or not
 *
 * @param type : message type MSG_*
 * @param psz_format : the message in the printf format
*/
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

/**
 * Display the list of the streamed channels
 *
 * @param number_of_channels the number of channels
 * @param channels : the channels array
 */
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

/**
 * Generate a file containing the list of the streamed channels and 
 * file containing a list of not streamed channels
 *
 * @param file_streamed_channels_filename The filename for the file containig the list of streamed channels
 * @param file_not_streamed_channels_filename The filename for the file containig the list of NOT streamed channels
 * @param number_of_channels the number of channels
 * @param channels the channels array
 */
void
gen_file_streamed_channels (char *file_streamed_channels_filename, char *file_not_streamed_channels_filename,
			    int number_of_channels, mumudvb_channel_t *channels)
{
  FILE *file_streamed_channels;
  FILE *file_not_streamed_channels;
  int curr_channel;

  file_streamed_channels = fopen (file_streamed_channels_filename, "w");
  if (file_streamed_channels == NULL)
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   file_streamed_channels_filename, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }

  file_not_streamed_channels = fopen (file_not_streamed_channels_filename, "w");
  if (file_not_streamed_channels == NULL)
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   file_not_streamed_channels_filename, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }

  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    // on envoie le old pour annoncer que les chaines qui diffusent au dessus du quota de pauqets
    if (channels[curr_channel].streamed_channel_old)
      fprintf (file_streamed_channels, "%s:%d:%s\n", channels[curr_channel].ipOut, channels[curr_channel].portOut, channels[curr_channel].name);
    else
      fprintf (file_not_streamed_channels, "%s:%d:%s\n", channels[curr_channel].ipOut, channels[curr_channel].portOut, channels[curr_channel].name);
  fclose (file_streamed_channels);
  fclose (file_not_streamed_channels);

}



/**
 * Write a config file with the current parameters
 * in a form understandable per mumudvb
 * This is useful if you want to do fine tuning after autoconf
 * This part generate the header ie take the actual config file and remove useless thing (ie channels, autoconf ...)
 *
 * @param orig_conf_filename The name of the config file used actually by mumudvb
 * @param saving_filename the path of the generated config file
 */
void gen_config_file_header(char *orig_conf_filename, char *saving_filename)
{
  FILE *orig_conf_file;
  FILE *config_file;
  char current_line[CONF_LINELEN];
  char *substring=NULL;
  char delimiteurs[] = " =";
  int autoconf=0;



  orig_conf_file = fopen (orig_conf_filename, "r");
  if (orig_conf_file == NULL)
    {
      log_message( MSG_WARN, "Strange error %s: %s\n",
		   orig_conf_filename, strerror (errno));
      return;
    }


  config_file = fopen (saving_filename, "w");
  if (config_file == NULL)
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   saving_filename, strerror (errno));
      return;
    }
  

  fprintf ( config_file, "#This is a generated configuration file for mumudvb\n");
  fprintf ( config_file, "#\n");


  while (fgets (current_line, CONF_LINELEN, orig_conf_file))
    {
      substring = strtok (current_line, delimiteurs);
      
      //We remove useless parts
      //if (substring[0] == '#')
      //continue; 
      if (!strcmp (substring, "autoconfigure"))
	continue;
      else if (!strcmp (substring, "autoconf_ip_header"))
	continue;
      else if (!strcmp (substring, "ip"))
	continue;
      else if (!strcmp (substring, "port"))
	continue;
      else if (!strcmp (substring, "cam_pmt_pid"))
	continue;
      else if (!strcmp (substring, "pids"))
	continue;
      else if (!strcmp (substring, "sap_group"))
	continue;
      else if (!strcmp (substring, "name"))
	continue;

      //we write the parts we didn't dropped
      fwrite(config_file,current_line);

    }
  fprintf ( config_file, "#End of global part\n#\n");

  fclose(config_file);
  fclose(orig_conf_file);
}


/**
 * Write a config file with the current parameters
 * in a form understandable per mumudvb
 * This is useful if you want to do fine tuning after autoconf
 * this part generates the channels
 *
 * @param number_of_channels the number of channels
 * @param channels the channels array
 * @param saving_filename the path of the generated config file
 */
void gen_config_file(int number_of_channels, mumudvb_channel_t *channels, char *saving_filename)
{
  FILE *config_file;

  int curr_channel;
  int curr_pid;

  //Append mode to avoid erasing the header
  config_file = fopen (saving_filename, "a");
  if (config_file == NULL)
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   saving_filename, strerror (errno));
      return;
    }

  fprintf ( config_file, "#Configuration for %d channel%s\n", number_of_channels,
	       (number_of_channels <= 1 ? "" : "s"));
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      fprintf ( config_file, "#Channel number : %3d\nip=%s\nport=%d\nname=%s\n",
		curr_channel,
		channels[curr_channel].ipOut,
		channels[curr_channel].portOut,
		channels[curr_channel].name);

      if (channels[curr_channel].sap_group[0])
	fprintf ( config_file, "sap_group=%s\n", channels[curr_channel].sap_group);
      if (channels[curr_channel].cam_pmt_pid)
	fprintf ( config_file, "cam_pmt_pid=%d\n", channels[curr_channel].cam_pmt_pid);

      log_message( MSG_info, "pids=");
      for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
	fprintf ( config_file, "%d ", channels[curr_channel].pids[curr_pid]);
      fprintf ( config_file, "\n");
    }
      fprintf ( config_file, "#End of config file\n");

  fclose (config_file);

}

