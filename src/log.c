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
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>


#include "mumudvb.h"
#include "errors.h"
#include "log.h"
#include "tune.h"


#ifdef ENABLE_CAM_SUPPORT
#include <libdvben50221/en50221_errno.h>
#endif

#define LOG_HEAD_LEN 6
extern int no_daemon;
extern int Interrupted;

log_params_t log_params={
  .verbosity = MSG_INFO+1,
  .log_type=LOGGING_UNDEFINED,
  .rotating_log_file=0,
  .syslog_initialised=0,
  .log_header=NULL,
  .log_file=NULL,
  .log_flush_interval = -1,
};

static char *log_module="Logs: ";


/** @brief Read a line of the configuration file to check if there is a logging parameter
 *
 * @param stats_infos the stats infos parameters
 * @param log_params the logging parameters
 * @param substring The currrent line
 */
int read_logging_configuration(stats_infos_t *stats_infos, char *substring)
{

  char delimiteurs[] = CONFIG_FILE_SEPARATOR;
  if (!strcmp (substring, "show_traffic_interval"))
  {
    substring = strtok (NULL, delimiteurs);
    stats_infos->show_traffic_interval= atoi (substring);
    if(stats_infos->show_traffic_interval<1)
    {
      stats_infos->show_traffic_interval=1;
      log_message( log_module, MSG_WARN,"Sorry the minimum interval for showing the traffic is 1s\n");
    }
  }
  else if (!strcmp (substring, "compute_traffic_interval"))
  {
    substring = strtok (NULL, delimiteurs);
    stats_infos->compute_traffic_interval= atoi (substring);
    if(stats_infos->compute_traffic_interval<1)
    {
      stats_infos->compute_traffic_interval=1;
      log_message( log_module, MSG_WARN,"Sorry the minimum interval for computing the traffic is 1s\n");
    }
  }
  else if (!strcmp (substring, "up_threshold"))
  {
    substring = strtok (NULL, delimiteurs);
    stats_infos->up_threshold= atoi (substring);
  }
  else if (!strcmp (substring, "down_threshold"))
  {
    substring = strtok (NULL, delimiteurs);
    stats_infos->down_threshold= atoi (substring);
  }
  else if (!strcmp (substring, "debug_updown"))
  {
    substring = strtok (NULL, delimiteurs);
    stats_infos->debug_updown= atoi (substring);
  }
  else if (!strcmp (substring, "log_type"))
  {
    substring = strtok (NULL, delimiteurs);
    if (!strcmp (substring, "console"))
      log_params.log_type |= LOGGING_CONSOLE;
    else if (!strcmp (substring, "syslog"))
    {
      openlog ("MUMUDVB", LOG_PID, 0);
      log_params.log_type |= LOGGING_SYSLOG;
      log_params.syslog_initialised=1;
    }
    else
      log_message(log_module,MSG_WARN,"Invalid value for log_type\n");
  }
  else if (!strcmp (substring, "log_file"))
  {
    substring = strtok (NULL, delimiteurs);
    log_params.log_file_path=malloc((strlen(substring)+1)*sizeof(char));
    strncpy(log_params.log_file_path,substring,strlen(substring)+1);
    if(log_params.log_file_path==NULL)
    {
      log_message(log_module,MSG_WARN,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      return -1;
    }
  }
  else if (!strcmp (substring, "log_header"))
  {
    substring = strtok (NULL,"=" );
    if(log_params.log_header!=NULL)
      free(log_params.log_header);
    log_params.log_header=malloc((strlen(substring)+1)*sizeof(char));
    if(log_params.log_header==NULL)
    {
      log_message(log_module,MSG_WARN,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      return -1;
    }
    sprintf(log_params.log_header,"%s",substring);
  }
  else if (!strcmp (substring, "log_flush_interval"))
  {
    substring = strtok (NULL, delimiteurs);
    log_params.log_flush_interval = atof (substring);
  }
  else
    return 0;
  return 1;
}

/**
 * @brief Return a string description of the log priorities
*/
char *priorities(int type)
{
  switch(type)
  {
    case MSG_ERROR:
      return "ERRO";
    case MSG_WARN:
      return "WARN";
    case MSG_INFO:
      return "Info";
    case MSG_DETAIL:
      return "Deb0";
    case MSG_DEBUG:
      return "Deb1";
    case MSG_FLOOD:
      return "Deb2";
    default:
      return "";
  }
}


/**
 * @brief Sync_log for logrotate
 * This function is called when a sighup is received. This function flushes the log and reopen the logfile
 *
 *
*/
void sync_logs()
{
  if (log_params.log_type |= LOGGING_FILE && log_params.log_file)
  {
    fflush(log_params.log_file);
    log_params.log_file=freopen(log_params.log_file_path,"a",log_params.log_file);
  }
}


/**
 * @brief Print a log message
 *
 * @param log_module : the name of the part of MuMuDVB which send the message
 * @param type : message type MSG_*
 * @param psz_format : the message in the printf format
*/
void log_message( char* log_module, int type,
                    const char *psz_format, ... )
{
  va_list args;
  int priority=0;
  char *tempchar;
  int message_size;
  mumu_string_t log_string;
  log_string.string=NULL;
  log_string.length=0;

  /*****************************************/
  //if the log header is not initialised
  // we do it
  /*****************************************/

  if(log_params.log_header==NULL)
  {
    log_params.log_header=malloc((strlen(DEFAULT_LOG_HEADER)+1)*sizeof(char));
    if(log_params.log_header==NULL)
    {
      if (log_params.log_type == LOGGING_FILE)
        fprintf( log_params.log_file,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      else if (log_params.log_type == LOGGING_SYSLOG)
        syslog (MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      else
        fprintf( stderr,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      va_end( args );
      Interrupted=ERROR_MEMORY<<8;
      return;
    }
    sprintf(log_params.log_header,"%s",DEFAULT_LOG_HEADER);

  }
  /*****************************************/
  //We apply the templates to the header
  /*****************************************/
  mumu_string_append(&log_string, "%s",log_params.log_header);
  log_string.string=mumu_string_replace(log_string.string,&log_string.length,1,"%priority",priorities(type));
  if(log_module!=NULL)
    log_string.string=mumu_string_replace(log_string.string,&log_string.length,1,"%module",log_module);
  else
    log_string.string=mumu_string_replace(log_string.string,&log_string.length,1,"%module","");

  char timestring[40];
  time_t actual_time;
  actual_time=time(NULL);
  sprintf(timestring,"%jd", (intmax_t)actual_time);
  log_string.string=mumu_string_replace(log_string.string,&log_string.length,1,"%timeepoch",timestring);
  asctime_r(localtime(&actual_time),timestring);
  timestring[strlen(timestring)-1]='\0'; //In order to remove the final '\n' but by asctime
  log_string.string=mumu_string_replace(log_string.string,&log_string.length,1,"%date",timestring);

  char pidstring[10];
  sprintf (pidstring, "%d", getpid ());
  log_string.string=mumu_string_replace(log_string.string,&log_string.length,1,"%pid",pidstring);


  /*****************************************/
  //We append the log message
  /*****************************************/
  //The length returned by mumu_string_replace is the allocated length not the string length
  //If we want mumu_string_append to work we need to update the length to the string length
  log_string.length=strlen(log_string.string);
  va_start( args, psz_format );
  message_size=vsnprintf(NULL, 0, psz_format, args);
  va_end( args );
  tempchar=malloc((message_size+1)*sizeof(char));
  if(tempchar==NULL)
  {
    if (log_params.log_type == LOGGING_FILE)
      fprintf( log_params.log_file,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    else if (log_params.log_type == LOGGING_SYSLOG)
      syslog (MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    else
      fprintf( stderr,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    va_end( args );
    Interrupted=ERROR_MEMORY<<8;
    return;
  }

  va_start( args, psz_format );
  vsprintf(tempchar, psz_format, args );
  va_end( args );
  //If there is no \n at the end of the message we add it
  char terminator='\0';
  if(tempchar[strlen(tempchar)-1] != '\n')
    terminator='\n';
  mumu_string_append(&log_string,"%s%c",tempchar,terminator);
  free(tempchar);

  /*****************************************/
  //We "display" the log message
  /*****************************************/
  if(type<log_params.verbosity)
    {
      if ( log_params.log_type & LOGGING_FILE)
	fprintf(log_params.log_file,"%s",log_string.string);
      if((log_params.log_type & LOGGING_SYSLOG) && (log_params.syslog_initialised))
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
            default:
              priority=LOG_USER;
	    }
	  syslog (priority,"%s",log_string.string);
	}
	if((log_params.log_type == LOGGING_UNDEFINED) ||
          (log_params.log_type & LOGGING_CONSOLE) ||
          ((log_params.log_type & LOGGING_SYSLOG) && (log_params.syslog_initialised==0)))
          fprintf(stderr,"%s",log_string.string);
    }
  mumu_free_string(&log_string);

}

/**
 * @brief Display the list of the streamed channels
 *
 * @param number_of_channels the number of channels
 * @param channels : the channels array
 */
void log_streamed_channels(char *log_module,int number_of_channels, mumudvb_channel_t *channels, int multicast_ipv4,int multicast_ipv6, int unicast, int unicast_master_port, char *unicastipOut)
{
  int curr_channel;
  int curr_pid;

  log_message( log_module,  MSG_INFO, "Diffusion %d channel%s\n", number_of_channels,
	       (number_of_channels <= 1 ? "" : "s"));
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
  {
    log_message( log_module,  MSG_INFO, "Channel number : %3d, name : \"%s\"  service id %d \n", curr_channel, channels[curr_channel].name, channels[curr_channel].service_id);
    if(multicast_ipv4)
      {
	log_message( log_module,  MSG_INFO, "\tMulticast4 ip : %s:%d\n", channels[curr_channel].ip4Out, channels[curr_channel].portOut);
      }
    if(multicast_ipv6)
      {
	log_message( log_module,  MSG_INFO, "\tMulticast6 ip : [%s]:%d\n", channels[curr_channel].ip6Out, channels[curr_channel].portOut);
      }
    if(unicast)
    {
      log_message( log_module,  MSG_INFO, "\tUnicast : Channel accessible via the master connection, %s:%d\n",unicastipOut, unicast_master_port);
      if(channels[curr_channel].unicast_port)
        log_message( log_module,  MSG_INFO, "\tUnicast : Channel accessible directly via %s:%d\n",unicastipOut, channels[curr_channel].unicast_port);
    }
    mumu_string_t string=EMPTY_STRING;
    char lang[5];
    if((Interrupted=mumu_string_append(&string, "        pids : ")))return;
    for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
    {
      strncpy(lang+1,channels[curr_channel].pids_language[curr_pid],4);
      lang[0]=(lang[1]=='-') ? '\0': ' ';
      if((Interrupted=mumu_string_append(&string, "%d (%s%s), ", channels[curr_channel].pids[curr_pid], pid_type_to_str(channels[curr_channel].pids_type[curr_pid]), lang)))
        return;
    }
    log_message( log_module, MSG_DETAIL,"%s\n",string.string);
    mumu_free_string(&string);
  }
}

/**
 * @brief Display the PIDs of a channel
 *
 */
void log_pids(char *log_module, mumudvb_channel_t *channel, int curr_channel)
{
  /******** display the pids **********/

  mumu_string_t string=EMPTY_STRING;
  if((Interrupted=mumu_string_append(&string, "PIDs for channel %d \"%s\" : ",curr_channel, channel->name)))return;
  for (int curr_pid = 0; curr_pid < channel->num_pids; curr_pid++)
  {
    if((Interrupted=mumu_string_append(&string, " %d",channel->pids[curr_pid])))return;
  }
  log_message( log_module, MSG_DETAIL,"%s\n",string.string);
  mumu_free_string(&string);
  /********  end of display the pids **********/
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
      log_message( log_module, MSG_WARN,
		   "Error file_streamed_channels %s: %s\n",
		   file_streamed_channels_filename, strerror (errno));
      return;
    }

  file_not_streamed_channels = fopen (file_not_streamed_channels_filename, "w");
  if (file_not_streamed_channels == NULL)
    {
      log_message( log_module,  MSG_WARN,
		   "Error file_not_streamed_channels %s: %s\n",
		   file_not_streamed_channels_filename, strerror (errno));
      return;
    }

  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    //We store the old to be sure that we store only channels over the minimum packets limit
    if (channels[curr_channel].streamed_channel)
      {
	fprintf (file_streamed_channels, "%s:%d:%s", channels[curr_channel].ip4Out, channels[curr_channel].portOut, channels[curr_channel].name);
	if (channels[curr_channel].scrambled_channel == FULLY_UNSCRAMBLED)
	  fprintf (file_streamed_channels, ":FullyUnscrambled\n");
 	else if (channels[curr_channel].scrambled_channel == PARTIALLY_UNSCRAMBLED)
	  fprintf (file_streamed_channels, ":PartiallyUnscrambled\n");
	else //HIGHLY_SCRAMBLED
	  fprintf (file_streamed_channels, ":HighlyScrambled\n");
      }
    else
      fprintf (file_not_streamed_channels, "%s:%d:%s\n", channels[curr_channel].ip4Out, channels[curr_channel].portOut, channels[curr_channel].name);
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
      log_message( log_module,  MSG_WARN, "Strange error %s: %s\n",
		   orig_conf_filename, strerror (errno));
      return;
    }


  config_file = fopen (saving_filename, "w");
  if (config_file == NULL)
    {
      log_message( log_module,  MSG_WARN,
		   "saving_filename %s: %s\n",
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
      else if (!strcmp (substring, "oscam"))
	continue;
      else if (!strcmp (substring, "ring_buffer_size"))
	continue;
      else if (!strcmp (substring, "decsa_delay"))
	continue;
      else if (!strcmp (substring, "send_delay"))
	continue;
      else if (!strcmp (substring, "decsa_wait"))
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
      log_message( log_module,  MSG_WARN,
		   "Error config_file %s: %s\n",
		   saving_filename, strerror (errno));
      return;
    }

  fprintf ( config_file, "#Configuration for %d channel%s\n", number_of_channels,
	       (number_of_channels <= 1 ? "" : "s"));
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      fprintf ( config_file, "#Channel number : %3d\nip=%s\nport=%d\nname=%s\n",
		curr_channel,
		channels[curr_channel].ip4Out,
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
  #ifdef ENABLE_SCAM_SUPPORT
      if (channels[curr_channel].scam_support) {
        fprintf ( config_file, "oscam=%d\n", channels[curr_channel].scam_support);
        fprintf ( config_file, "ring_buffer_size=%" PRIu64 "\n", channels[curr_channel].ring_buffer_size);
		fprintf ( config_file, "decsa_delay=%" PRIu64 "\n", channels[curr_channel].decsa_delay);
		fprintf ( config_file, "send_delay=%" PRIu64 "\n", channels[curr_channel].send_delay);
		fprintf ( config_file, "decsa_wait=%" PRIu64 "\n", channels[curr_channel].decsa_wait);
	  }
      fprintf ( config_file, "#End of config file\n");
  #endif
	}


  fclose (config_file);

}


typedef struct ca_sys_id_t
{
  int beginning;
  int end; //if == 0 equivalent to have end=beginning
  char descr[128];
}ca_sys_id_t;

//updated 2010 06 12
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
  {0x4B07 ,0 , "Wuhan Reikost Technology Co., Ltd."},
  {0x4B08 ,0 , "Philips"},
  {0x4B09 ,0 , "Ambernetas"},
  {0x4B0A ,0x4B0B , "Beijing Sumavision Technologies CO. LTD."},
  {0x4B0C ,0x4B0F , "Sichuan changhong electric co.,ltd."},
  {0x5347 ,0 , "GkWare e.K."},
  {0x5601 ,0 , "Verimatrix, Inc. #1"},
  {0x5602 ,0 , "Verimatrix, Inc. #2"},
  {0x5603 ,0 , "Verimatrix, Inc. #3"},
  {0x5604 ,0 , "Verimatrix, Inc. #4"},
  {0x5605 ,0x5606 , "Sichuan Juizhou Electronic Co. Ltd"},
  {0x5607 ,0x5608 , "Viewscenes"},
  {0x7BE0 ,0x7BE1 , "OOO"},
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
void display_service_type(int type, int loglevel, char *log_module)
{
  log_message( log_module, loglevel, "service type: 0x%x : %s \n", type, service_type_to_str(type));
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
    case PID_VIDEO_MPEG1:
      return "Video (MPEG1)";
    case PID_VIDEO_MPEG2:
      return "Video (MPEG2)";
    case PID_VIDEO_MPEG4_ASP:
      return "Video (MPEG4-ASP)";
    case PID_VIDEO_MPEG4_AVC:
      return "Video (MPEG4-AVC)";
    case PID_AUDIO_MPEG1:
      return "Audio (MPEG1)";
    case PID_AUDIO_MPEG2:
      return "Audio (MPEG2)";
    case PID_AUDIO_AAC_LATM:
      return "Audio (AAC-LATM)";
    case PID_AUDIO_AAC_ADTS:
      return "Audio (AAC-ADTS)";
    case PID_AUDIO_ATSC:
      return "Audio (ATSC A/53B)";
    case PID_AUDIO_AC3:
      return "Audio (AC3)";
    case PID_AUDIO_EAC3:
      return "Audio (E-AC3)";
    case PID_AUDIO_DTS:
      return "Audio (DTS)";
    case PID_AUDIO_AAC:
      return "Audio (AAC)";
    case PID_EXTRA_VBIDATA:
      return "VBI Data";
    case PID_EXTRA_VBITELETEXT:
      return "VBI Teletext";
    case PID_EXTRA_TELETEXT:
      return "Teletext";
    case PID_EXTRA_SUBTITLE:
      return "Subtitling";
    case PID_UNKNOW:
    default:
      return "Unknown";
  }
}

#ifdef ENABLE_CAM_SUPPORT
/** @brief Write the error from the libdvben50221  into a string
 *
 * @param dest : the destination string
 * @param error the error to display
 */
char *liben50221_error_to_str(int error)
{
  switch(error)
  {
    case 0:
      return "EN50221ERR_NONE";
    case EN50221ERR_CAREAD:
      return "EN50221ERR_CAREAD";
    case EN50221ERR_CAWRITE:
      return "EN50221ERR_CAWRITE";
    case EN50221ERR_TIMEOUT:
      return "EN50221ERR_TIMEOUT";
    case EN50221ERR_BADSLOTID:
      return "EN50221ERR_BADSLOTID";
    case EN50221ERR_BADCONNECTIONID:
      return "EN50221ERR_BADCONNECTIONID";
    case EN50221ERR_BADSTATE:
      return "EN50221ERR_BADSTATE";
    case EN50221ERR_BADCAMDATA:
      return "EN50221ERR_BADCAMDATA";
    case EN50221ERR_OUTOFMEMORY:
      return "EN50221ERR_OUTOFMEMORY";
    case EN50221ERR_ASNENCODE:
      return "EN50221ERR_ASNENCODE";
    case EN50221ERR_OUTOFCONNECTIONS:
      return "EN50221ERR_OUTOFCONNECTIONS";
    case EN50221ERR_OUTOFSLOTS:
      return "EN50221ERR_OUTOFSLOTS";
    case EN50221ERR_IOVLIMIT:
      return "EN50221ERR_IOVLIMIT";
    case EN50221ERR_BADSESSIONNUMBER:
      return "EN50221ERR_BADSESSIONNUMBER";
    case EN50221ERR_OUTOFSESSIONS:
      return "EN50221ERR_OUTOFSESSIONS";
    default:
      return "UNKNOWN";
  }
}

/** @brief Write the error from the libdvben50221  into a string containing the description of the error
 *
 * @param dest : the destination string
 * @param error the error to display
 */
char *liben50221_error_to_str_descr(int error)
{
  switch(error)
  {
    case 0:
      return "No Error.";
    case EN50221ERR_CAREAD:
      return "error during read from CA device.";
    case EN50221ERR_CAWRITE:
      return "error during write to CA device.";
    case EN50221ERR_TIMEOUT:
      return "timeout occured waiting for a response from a device.";
    case EN50221ERR_BADSLOTID:
      return "bad slot ID supplied by user - the offending slot_id will not be set.";
    case EN50221ERR_BADCONNECTIONID:
      return "bad connection ID supplied by user.";
    case EN50221ERR_BADSTATE:
      return "slot/connection in the wrong state.";
    case EN50221ERR_BADCAMDATA:
      return "CAM supplied an invalid request.";
    case EN50221ERR_OUTOFMEMORY:
      return "memory allocation failed.";
    case EN50221ERR_ASNENCODE:
      return "ASN.1 encode failure - indicates library bug.";
    case EN50221ERR_OUTOFCONNECTIONS:
      return "no more connections available.";
    case EN50221ERR_OUTOFSLOTS:
      return "no more slots available - the offending slot_id will not be set.";
    case EN50221ERR_IOVLIMIT:
      return "Too many struct iovecs were used.";
    case EN50221ERR_BADSESSIONNUMBER:
      return "Bad session number suppplied by user.";
    case EN50221ERR_OUTOFSESSIONS:
      return "no more sessions available.";
    default:
      return "Unknown error, please contact";
  }
}

#endif

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
#ifdef ENABLE_SCAM_SUPPORT
               "Built with SCAM support.\n"
#else
               "Built without SCAM support.\n"
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
               "Built with support for DVB API Version 5.\n"
#ifdef SYS_DVBT2
               "\tBuilt with support for DVB-T2.\n"
#endif
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
          "--server_id  : The server id (for autoconfiguration, overrided by the configuration file)\n"
          "-d, --debug  : Don't deamonize\n"
          "-v           : More verbose\n"
          "-q           : Less verbose\n"
          "--dumpfile   : Debug option : Dump the stream into the specified file\n"
          "-h, --help   : Help\n"
          "\n", name);
  print_info ();
}

void show_traffic( char *log_module, double now, int show_traffic_interval, mumudvb_chan_and_pids_t *chan_and_pids)
{
  static long show_traffic_time=0;

  if(!show_traffic_time)
    show_traffic_time=now;
  if((now-show_traffic_time)>=show_traffic_interval)
    {
      show_traffic_time=now;
      for (int curr_channel = 0; curr_channel < chan_and_pids->number_of_channels; curr_channel++)
        {
          log_message( log_module,  MSG_INFO, "Traffic :  %.2f kb/s \t  for channel \"%s\"\n",
                       chan_and_pids->channels[curr_channel].traffic*8,
                       chan_and_pids->channels[curr_channel].name);
        }
    }
}
