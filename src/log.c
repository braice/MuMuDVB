/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2009 Brice DUBOST
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
 * @brief Log functions for mumudvb
 * 
 * This file contains functions to log messages or write logging information to a file
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>
#include <linux/dvb/version.h>

#include "mumudvb.h"
#include "errors.h"
#include "log.h"


extern int no_daemon;
extern int verbosity;
extern int log_initialised;

/**
 * @brief Print a log message on the console or via syslog 
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
 * @brief Display the list of the streamed channels
 *
 * @param number_of_channels the number of channels
 * @param channels : the channels array
 */
void log_streamed_channels(int number_of_channels, mumudvb_channel_t *channels, int multicast, int unicast, int unicast_master_port, char *unicastipOut)
{
  int curr_channel;
  int curr_pid;

  log_message( MSG_INFO, "Diffusion %d channel%s\n", number_of_channels,
	       (number_of_channels <= 1 ? "" : "s"));
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
	  log_message( MSG_INFO, "Channel number : %3d, name : \"%s\"  TS id %d \n", curr_channel, channels[curr_channel].name, channels[curr_channel].ts_id);
      if(multicast)
	log_message( MSG_INFO, "\tMulticast ip : %s:%d\n", channels[curr_channel].ipOut, channels[curr_channel].portOut);
      if(unicast)
      {
	log_message( MSG_INFO, "\tUnicast : Channel accessible via the master connection, %s:%d\n",unicastipOut, unicast_master_port);
	if(channels[curr_channel].unicast_port)
	  log_message( MSG_INFO, "\tUnicast : Channel accessible directly via %s:%d\n",unicastipOut, channels[curr_channel].unicast_port);
      }
      log_message( MSG_DETAIL, "        pids : ");/**@todo Generate a strind and call log_message after, in syslog it generates one line per pid*/
      for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
	log_message( MSG_DETAIL, "%d (%s) ", channels[curr_channel].pids[curr_pid], pid_type_to_str(channels[curr_channel].pids_type[curr_pid]));
      log_message( MSG_DETAIL, "\n");
    }
}

/**
 * @brief Generate a file containing the list of the streamed channels
 * and a file containing a list of not streamed channels
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
  /**todo : adapt it for unicast (json ?) */
  FILE *file_streamed_channels;
  FILE *file_not_streamed_channels;
  int curr_channel;

  file_streamed_channels = fopen (file_streamed_channels_filename, "w");
  if (file_streamed_channels == NULL)
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   file_streamed_channels_filename, strerror (errno));
      return;
    }

  file_not_streamed_channels = fopen (file_not_streamed_channels_filename, "w");
  if (file_not_streamed_channels == NULL)
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   file_not_streamed_channels_filename, strerror (errno));
      return;
    }

  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    //We store the old to be sure that we store only channels over the minimum packets limit
    if (channels[curr_channel].streamed_channel_old)
      {
	fprintf (file_streamed_channels, "%s:%d:%s", channels[curr_channel].ipOut, channels[curr_channel].portOut, channels[curr_channel].name);
	if (channels[curr_channel].scrambled_channel_old == FULLY_UNSCRAMBLED)
	  fprintf (file_streamed_channels, ":FullyUnscrambled\n");
	else if (channels[curr_channel].scrambled_channel_old == PARTIALLY_UNSCRAMBLED)
	  fprintf (file_streamed_channels, ":PartiallyUnscrambled\n");
	else //HIGHLY_SCRAMBLED
	  fprintf (file_streamed_channels, ":HighlyScrambled\n");
      }
    else
      fprintf (file_not_streamed_channels, "%s:%d:%s\n", channels[curr_channel].ipOut, channels[curr_channel].portOut, channels[curr_channel].name);
  fclose (file_streamed_channels);
  fclose (file_not_streamed_channels);

}



/**
 * @brief Write a config file with the current parameters
 * in a form understandable by mumudvb
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
  char current_line_temp[CONF_LINELEN];
  char *substring=NULL;
  char delimiteurs[] = " =";


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
  

  fprintf ( config_file, "# !!!!!!! This is a generated configuration file for MuMuDVB !!!!!!!!!!!\n");
  fprintf ( config_file, "#\n");


  while (fgets (current_line, CONF_LINELEN, orig_conf_file))
    {
      strcpy(current_line_temp,current_line);
      substring = strtok (current_line_temp, delimiteurs);

      //We remove the channels and parameters concerning autoconfiguration
      if (!strcmp (substring, "autoconfiguration"))
	continue;
      else if (!strcmp (substring, "autoconf_ip_header"))
	continue;
      else if (!strcmp (substring, "autoconf_scrambled"))
	continue;
      else if (!strcmp (substring, "autoconf_radios"))
	continue;
      else if (!strcmp (substring, "autoconf_unicast_start_port"))
        continue;
      else if (!strcmp (substring, "autoconf_pid_update"))
        continue;
      else if (!strcmp (substring, "autoconf_tsid_list"))
        continue;
      else if (!strcmp (substring, "channel_next"))
        continue;
      else if (!strcmp (substring, "ip"))
	continue;
      else if (!strcmp (substring, "port"))
        continue;
      else if (!strcmp (substring, "unicast_port"))
        continue;
      else if (!strcmp (substring, "ts_id"))
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
      fprintf(config_file,"%s",current_line);

    }
  fprintf ( config_file, "\n#End of global part\n#\n");

  fclose(config_file);
  fclose(orig_conf_file);
}


/**
 * @brief Write a config file with the current parameters
 * in a form understandable by mumudvb
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
      if (channels[curr_channel].need_cam_ask)
        fprintf ( config_file, "cam_pmt_pid=%d\n", channels[curr_channel].pmt_pid);
      if (channels[curr_channel].ts_id)
        fprintf ( config_file, "ts_id=%d\n", channels[curr_channel].ts_id);
      if (channels[curr_channel].unicast_port)
        fprintf ( config_file, "unicast_port=%d\n", channels[curr_channel].unicast_port);
      fprintf ( config_file, "pids=");
      for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
	fprintf ( config_file, "%d ", channels[curr_channel].pids[curr_pid]);
      fprintf ( config_file, "\n");
    }
      fprintf ( config_file, "#End of config file\n");

  fclose (config_file);

}


/** @brief Display the ca system id according to ETR 162 
 *
 * @param id the id to display
 */

void display_ca_sys_id(int id)
{
  //cf ETR 162

  if(id<=0xFF)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Standardized systems\n",id);
  else if(id>=0x0100&&id<0x01ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Canal Plus\n",id);
  else if(id>=0x0200&&id<0x02ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : CCETT\n",id);
  else if(id>=0x0300&&id<0x03ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Deutsche Telecom\n",id);
  else if(id>=0x0400&&id<0x04ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Eurodec\n",id);
  else if(id>=0x0500&&id<0x05ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : France Telecom\n",id);
  else if(id>=0x0600&&id<0x06ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Irdeto\n",id);
  else if(id>=0x0700&&id<0x07ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Jerrold/GI\n",id);
  else if(id>=0x0800&&id<0x08ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Matra Communication\n",id);
  else if(id>=0x0900&&id<0x09ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : News Datacom\n",id);
  else if(id>=0x0A00&&id<0x0Aff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Nokia\n",id);
  else if(id>=0x0B00&&id<0x0Bff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Norwegian Telekom\n",id);
  else if(id>=0x0C00&&id<0x0Cff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : NTL\n",id);
  else if(id>=0x0D00&&id<0x0Dff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Philips\n",id);
  else if(id>=0x0E00&&id<0x0Eff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Scientific Atlanta\n",id);
  else if(id>=0x0F00&&id<0x0Fff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Sony\n",id);
  else if(id>=0x1000&&id<0x10ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Tandberg Television\n",id);
  else if(id>=0x1100&&id<0x11ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Thomson\n",id);
  else if(id>=0x1200&&id<0x12ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : TV/Com\n",id);
  else if(id>=0x1300&&id<0x13ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : HPT - Croatian Post and Telecommunications\n",id);
  else if(id>=0x1400&&id<0x14ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : HRT - Croatian Radio and Television\n",id);
  else if(id>=0x1500&&id<0x15ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : IBM\n",id);
  else if(id>=0x1600&&id<0x16ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : Nera\n",id);
  else if(id>=0x1700&&id<0x17ff)
    log_message( MSG_DETAIL,"Ca system id 0x%04x : BetaTechnik\n",id);
  else
    log_message( MSG_DETAIL,"Ca system id 0x%04x : UNKNOWN\n",id);

}


/** @brief Convert the service type to str according to EN 300 468 v1.10.1 table 81
 *
 * @param type the type to display
 * @param dest : the destination string
 */
char *service_type_to_str(int type)
{
  if(type>=0x80 && type<=0xFE)
    return "User defined";

  switch(type)
  {
    case 0x01:
      return "Television";
    case 0x02:
      return "Radio";
    case 0x03:
      return "Teletext";
    case 0x04:
      return "NVOD Reference service";
    case 0x05:
      return "NVOD Time shifted service";
    case 0x06:
      return "Mosaic service";
    case 0x0a:
      return "Advanced codec Radio";
    case 0x0b:
      return "Advanced codec mosaic";
    case 0x0c:
      return "Data broadcast service";
    case 0x0d:
      return "Reserved for common interface usage";
    case 0x0e:
      return "RCS Map";
    case 0x0f:
      return "RCS FLS";
    case 0x10:
      return "DVB MHP (multimedia home platform)";
    case 0x11:
      return "Television MPEG2-HD";
    case 0x16:
      return "Advanced codec SD Television";
    case 0x17:
      return "Advanced codec SD NVOD Time shifted service";
    case 0x18:
      return "Advanced codec SD NVOD Reference service";
    case 0x19:
      return "Advanced codec HD Television";
    case 0x1a:
      return "Advanced codec HD NVOD Time shifted service";
    case 0x1b:
      return "Advanced codec HD NVOD Reference service";
    default:
      return "Please report : Unknown service type doc : EN 300 468 v1.10.1 table 81";
  }
}

/** @brief Display the service type according to EN 300 468 v1.10.1 table 81
 *
 * @param type the type to display
 * @param loglevel : the loglevel for displaying it
 */
void display_service_type(int type, int loglevel)
{
  log_message(loglevel, "Autoconf : service type: 0x%x : %s \n", type, service_type_to_str(type));
}

/** @brief Write the PID type into a string
 *
 * @param dest : the destination string
 * @param type the type to display
 */
char *pid_type_to_str(int type)
{
  switch(type)
  {
    case PID_PMT:
      return "PMT";
    case PID_PCR:
      return "PCR";
    case PID_VIDEO:
      return "Video";
    case PID_VIDEO_MPEG4:
      return "Video (MPEG4)";
    case PID_AUDIO:
      return "Audio";
    case PID_AUDIO_AAC:
      return "Audio (AAC)";
    case PID_AUDIO_AC3:
      return "Audio (AC3)";
    case PID_AUDIO_EAC3:
      return "Audio (E-AC3)";
    case PID_AUDIO_DTS:
      return "Audio (DTS)";
    case PID_SUBTITLE:
      return "Subtitle";
    case PID_TELETEXT:
      return "Teletext";
    case PID_UNKNOW:
    default:
      return "Unknown";
  }
}


/** @brief : display mumudvb info*/
void print_info ()
{
  fprintf (stderr, 
           "MuMuDVB Version "
               VERSION
               "\n --- Build information ---\n"
#ifdef ENABLE_CAM_SUPPORT
               "Built with CAM support.\n"
#else
               "Built without CAM support.\n"
#endif
#ifdef ENABLE_TRANSCODING
               "Built with transcoding support.\n"
#else
               "Built without transcoding support.\n"
#endif
#ifdef ATSC
               "Built with ATSC support.\n"
#ifdef HAVE_LIBUCSI
               "Built with ATSC long channel names support.\n"
#endif
#endif
#if DVB_API_VERSION >= 5
               "Built with support for DVB API Version 5 (DVB-S2).\n"
#endif
#ifdef HAVE_LIBPTHREAD
               "Built with pthread support (used for periodic signal strength display, cam support, transcoding, and threaded read).\n"
#else
               "Built without pthread support (NO periodic signal strength display, NO cam support, NO transcoding and NO threaded read).\n"
#endif
               "---------\n"
               "Originally based on dvbstream 0.6 by (C) Dave Chapman 2001-2004\n"
               "Released under the GPL.\n"
               "Latest version available from http://mumudvb.braice.net/\n"
               "Project from the cr@ns (http://www.crans.org)\n"
               "by Brice DUBOST (mumudvb@braice.net)\n\n");
}



/** @brief : display mumudvb usage*/
void usage (char *name)
{
  fprintf (stderr, "MuMuDVB is a program who can redistribute stream from DVB on a network, in multicast or in http unicast.\n"
      "It's main feature is to take a whole transponder and put each channel on a different multicast IP.\n\n"
          "Usage: %s [options] \n"
          "-c, --config : Config file\n"
          "-s, --signal : Display signal power\n"
          "-t, --traffic : Display channels traffic\n"
          "-l, --list-cards : List the DVB cards and exit\n"
          "-d, --debug  : Don't deamonize\n"
          "-v           : More verbose\n"
          "-q           : Less verbose\n"
          "-h, --help   : Help\n"
          "\n", name);
  print_info ();
}
