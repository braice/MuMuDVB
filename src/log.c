/* 
 * mumudvb - Stream a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
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
	    case MSG_FLOOD:
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
	  log_message( MSG_INFO, "Channel number : %3d, name : \"%s\"  service id %d \n", curr_channel, channels[curr_channel].name, channels[curr_channel].service_id);
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
      else if (!strncmp (substring, "autoconf_", strlen("autoconf_")))
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
      else if (!strcmp (substring, "service_id"))
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
      if (channels[curr_channel].service_id)
        fprintf ( config_file, "service_id=%d\n", channels[curr_channel].service_id);
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


typedef struct ca_sys_id_t
{
  int beginning;
  int end; //if == 0 equivalent to have end=beginning
  char descr[128];
}ca_sys_id_t;

//updated 2009 12 02
  ca_sys_id_t casysids[]={
  {0x01,0, "IPDC SPP (TS 102 474) Annex A "},
  {0x02,0, "IPDC SPP (TS 102 474) Annex B"},
  {0x04,0, "OMA DRM Content Format"},
  {0x05,0, "OMA BCAST 1.0"},
  {0x06,0, "OMA BCAST 1.0 (U)SIM"},
  {0x07,0, "Reserved for Open IPTV Forum"},
  {0x00,0xFF, "Standardized systems"},
  {0x0100,0x01ff, "Canal Plus"},
  {0x0200,0x02ff, "CCETT"},
  {0x0300,0x03ff, "Kabel Deutschland"},
  {0x0400,0x04ff, "Eurodec"},
  {0x0500,0x05ff, "France Telecom"},
  {0x0600,0x06ff, "Irdeto"},
  {0x0700,0x07ff, "Jerrold/GI/Motorola"},
  {0x0800,0x08ff, "Matra Communication"},
  {0x0900,0x09ff, "News Datacom"},
  {0x0A00,0x0Aff, "Nokia"},
  {0x0B00,0x0Bff, "Norwegian Telekom"},
  {0x0C00,0x0Cff, "NTL"},
  {0x0D00,0x0Dff, "CrytoWorks (Irdeto)"},
  {0x0E00,0x0Eff, "Scientific Atlanta"},
  {0x0F00,0x0Fff, "Sony"},
  {0x1000,0x10ff, "Tandberg Television"},
  {0x1100,0x11ff, "Thomson"},
  {0x1200,0x12ff, "TV/Com"},
  {0x1300,0x13ff, "HPT - Croatian Post and Telecommunications"},
  {0x1400,0x14ff, "HRT - Croatian Radio and Television"},
  {0x1500,0x15ff, "IBM"},
  {0x1600,0x16ff, "Nera"},
  {0x1700,0x17ff, "BetaTechnik"},
  {0x1800,0x18ff, "Kudelski SA"},
  {0x1900,0x19ff, "Titan Information Systems"},
  {0x2000,0x20ff, "Telefonica Servicios Audiovisuales"},
  {0x2100,0x21ff, "STENTOR (France Telecom, CNES and DGA)"},
  {0x2200,0x22ff, "Scopus Network Technologies"},
  {0x2300,0x23ff, "BARCO AS"},
  {0x2400,0x24ff, "StarGuide Digital Networks"},
  {0x2500,0x25ff, "Mentor Data System, Inc."},
  {0x2600,0x26ff, "European Broadcasting Union"},
  {0x2700 ,0x270F , "PolyCipher (NGNA, LLC)"},
  {0x4347 ,0 , "Crypton"},
  {0x4700 ,0x47FF , "General Instrument (Motorola)"},
  {0x4800 ,0x48FF , "Telemann"},
  {0x4900 ,0x49FF , "CrytoWorks (China) (Irdeto)"},
  {0x4A10 ,0x4A1F , "Easycas"},
  {0x4A20 ,0x4A2F , "AlphaCrypt"},
  {0x4A30 ,0x4A3F , "DVN Holdings"},
  {0x4A40 ,0x4A4F , "Shanghai Advanced Digital Technology Co. Ltd. (ADT)"},
  {0x4A50 ,0x4A5F , "Shenzhen Kingsky Company (China) Ltd."},
  {0x4A60 ,0x4A6F , "@Sky"},
  {0x4A70 ,0x4A7F , "Dreamcrypt"},
  {0x4A80 ,0x4A8F , "THALESCrypt"},
  {0x4A90 ,0x4A9F , "Runcom Technologies"},
  {0x4AA0 ,0x4AAF , "SIDSA"},
  {0x4AB0 ,0x4ABF , "Beijing Compunicate Technology Inc."},
  {0x4AC0 ,0x4ACF , "Latens Systems Ltd"},
  {0x4AD0 ,0x4AD1 , "XCrypt Inc."},
  {0x4AD2 ,0x4AD3 , "Beijing Digital Video Technology Co., Ltd."},
  {0x4AD4 ,0x4AD5 , "Widevine Technologies, Inc."},
  {0x4AD6 ,0x4AD7 , "SK Telecom Co., Ltd."},
  {0x4AD8 ,0x4AD9 , "Enigma Systems"},
  {0x4ADA ,0 , "Wyplay SAS"},
  {0x4ADB ,0 , "Jinan Taixin Electronics, Co., Ltd."},
  {0x4ADC ,0 , "LogiWays"},
  {0x4ADD ,0 , "ATSC System Renewability Message (SRM)"},
  {0x4ADE ,0 , "CerberCrypt"},
  {0x4ADF ,0 , "Caston Co., Ltd."},
  {0x4AE0 ,0x4AE1 , "Digi Raum Electronics Co. Ltd."},
  {0x4AE2 ,0x4AE3 , "Microsoft Corp."},
  {0x4AE4 ,0 , "Coretrust, Inc."},
  {0x4AE5 ,0 , "IK SATPROF"},
  {0x4AE6 ,0 , "SypherMedia International"},
  {0x4AE7 ,0 , "Guangzhou Ewider Technology Corporation Limited"},
  {0x4AE8 ,0 , "FG DIGITAL Ltd."},
  {0x4AE9 ,0 , "Dreamer-i Co., Ltd."},
  {0x4AEA ,0 , "Cryptoguard AB"},
  {0x4AEB ,0 , "Abel DRM Systems AS"},
  {0x4AEC ,0 , "FTS DVL SRL"},
  {0x4AED ,0 , "Unitend Technologies, Inc."},
  {0x4AEE ,0 , "Deltacom Electronics OOD"},
  {0x4AEF ,0 , "NetUP Inc."},
  {0x4AF0 ,0 , "Beijing Alliance Broadcast Vision Technology Co., Ltd."},
  {0x4AF1 ,0 , "China DTV Media Inc., Ltd. #1"},
  {0x4AF2 ,0 , "China DTV Media Inc., Ltd. #2"},
  {0x4AF3 ,0 , "Baustem Information Technologies, Ltd."},
  {0x4AF4 ,0 , "Marlin Developer Community, LLC"},
  {0x4AF5 ,0 , "SecureMedia"},
  {0x4AF6 ,0 , "Tongfang CAS"},
  {0x4AF7 ,0 , "MSA"},
  {0x4AF8 ,0 , "Griffin CAS"},
  {0x4AF9 ,0x4AFA , "Beijing Topreal Technologies Co., Ltd"},
  {0x4AFB ,0 , "NST"},
  {0x4AFC ,0 , "Panaccess Systems GmbH"},
  {0x4B00 ,0x4B02 , "Tongfang CAS"},
  {0x4B03 ,0 , "DuoCrypt"},
  {0x4B04 ,0 , "Great Wall CAS"},
  {0x4B05 ,0x4B06 , "DIGICAP"},
  {0x5347 ,0 , "GkWare e.K."},
  {0x5601 ,0 , "Verimatrix, Inc. #1"},
  {0x5602 ,0 , "Verimatrix, Inc. #2"},
  {0x5603 ,0 , "Verimatrix, Inc. #3"},
  {0x5604 ,0 , "Verimatrix, Inc. #4"},
  {0x5605 ,0x5606 , "Sichuan Juizhou Electronic Co. Ltd"},
  {0x5607 ,0x5608 , "Viewscenes"},
  {0xAA00 ,0 , "Best CAS Ltd"},
  };
  int num_casysids=105;


/** @brief Display the ca system id according to ETR 162 
 *
 * @param id the id to display
 */

char *ca_sys_id_to_str(int id)
{
  //cf ETR 162 and http://www.dvbservices.com/identifiers/ca_system_id



  for(int i=0;i<num_casysids;i++)
  {
    if((casysids[i].end == 0 && casysids[i].beginning == id) || ( casysids[i].beginning <= id && casysids[i].end >= id))
      return casysids[i].descr;
  }
  return "UNKNOWN, please report";

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

/** @brief Convert the service type to str according to EN 300 468 v1.10.1 table 81
 *
 * @param type the type to display
 * @param dest : the destination string
 */
char *simple_service_type_to_str(int type)
{
  if(type>=0x80 && type<=0xFE)
    return "User defined";

  switch(type)
  {
    case 0x01:
    case 0x11:
    case 0x16:
    case 0x19:
      return "Television";
    case 0x02:
    case 0x0a:
      return "Radio";
    default:
      return "";
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
          "--card       : The DVB card to use (overrided by the configuration file)\n"
          "-d, --debug  : Don't deamonize\n"
          "-v           : More verbose\n"
          "-q           : Less verbose\n"
          "-h, --help   : Help\n"
          "\n", name);
  print_info ();
}

void show_traffic(long now, int show_traffic_interval, mumudvb_chan_and_pids_t *chan_and_pids)
{
  static long show_traffic_time=0;

  if(!show_traffic_time)
    show_traffic_time=now;
  if((now-show_traffic_time)>=show_traffic_interval)
    {
      show_traffic_time=now;
      for (int curr_channel = 0; curr_channel < chan_and_pids->number_of_channels; curr_channel++)
        {
          log_message( MSG_INFO, "Traffic :  %.2f kB/s \t  for channel \"%s\"\n",
                       chan_and_pids->channels[curr_channel].traffic,
                       chan_and_pids->channels[curr_channel].name);
        }
    }
}
