/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2010 Brice DUBOST
 * 
 * Code for dealing with libdvben50221 inspired from zap_ca
 * Copyright (C) 2004, 2005 Manu Abraham <abraham.manu@gmail.com>
 * Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)
 * 
 * Transcoding written by Utelisys Communications B.V.
 * Copyright (C) 2009 Utelisys Communications B.V.
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
 * @brief This file is the main file of mumudvb
 */


/** @mainpage Documentation for the mumudvb project
 * @section introduction
 * Mumudvb is a program that can redistribute streams from DVB on a network using
 * multicasting or HTTP unicast. It is able to multicast a whole DVB transponder by assigning
 * each channel to a different multicast IP.
 *
 * @section Main features

 * Stream channels from a transponder on different multicast IPs

 * The program can rewrite the PAT Pid in order to announce only present channels (useful for some set-top boxes)

 * Support for scrambled channels (if you don't have a CAM you can use sasc-ng, but check if it's allowed in you country)

 * Support for autoconfiguration

 * Generation of SAP announces

 *@section files
 * mumudvb.h header containing global information 
 *
 * autoconf.c autoconf.h code related to autoconfiguration
 *
 * cam.c cam.h : code related to the support of scrambled channels
 *
 * crc32.c : the crc32 table
 *
 * dvb.c dvb.h functions related to the DVB card : oppening filters, file descriptors etc
 *
 * log.c logging functions
 *
 * pat_rewrite.c rewrite.h : the functions associated with the rewrite of the PAT pid
 *
 * sdt_rewrite.c rewrite.h : the functions associated with the rewrite of the SDT pid
 *
 * sap.c sap.h : sap announces
 *
 * ts.c ts.h : function related to the MPEG-TS parsing
 *
 * tune.c tune.h : tuning of the dvb card
 *
 * network.c network.h : networking ie openning sockets, sending packets
 *
 * unicast_http.c unicast_http.h : HTTP unicast
 */

#define _GNU_SOURCE		//in order to use program_invocation_short_name (extension gnu)

#include "config.h"

// Linux includes:
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <stdint.h>
#include <resolv.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <values.h>
#include <string.h>
#include <syslog.h>
#include <getopt.h>
#include <errno.h>
#include <linux/dvb/version.h>

#include "mumudvb.h"
#include "tune.h"
#include "network.h"
#include "dvb.h"
#ifdef ENABLE_CAM_SUPPORT
#include "cam.h"
#endif
#include "ts.h"
#include "errors.h"
#include "autoconf.h"
#include "sap.h"
#include "rewrite.h"
#include "unicast_http.h"
#include "rtp.h"
#include "log.h"
#ifdef ENABLE_TRANSCODING
#include "transcode.h"
#endif

/** the table for crc32 claculations */
extern uint32_t       crc32_table[256];

static char *log_module="Main: ";

/* Signal handling code shamelessly copied from VDR by Klaus Schmidinger 
   - see http://www.cadsoft.de/people/kls/vdr/index.htm */

// global variables used by SignalHandler
long now;
long real_start_time;
int *card_tuned;
int received_signal = 0;

int timeout_no_diff = ALARM_TIME_TIMEOUT_NO_DIFF;
// file descriptors
fds_t fds; /** File descriptors associated with the card */
int no_daemon = 0;
int Interrupted = 0;
int  write_streamed_channels=1;


pthread_t signalpowerthread;
pthread_t cardthread;
pthread_t monitorthread;
card_thread_parameters_t cardthreadparams;


mumudvb_chan_and_pids_t chan_and_pids={
  .number_of_channels=0,
  .dont_send_scrambled=0,
};


//multicast parameters
multicast_parameters_t multicast_vars={
  .multicast=1,
  .ttl=DEFAULT_TTL,
  .common_port = 1234,
  .auto_join=0,
  .rtp_header = 0,
};




//autoconfiguration
autoconf_parameters_t autoconf_vars={
  .autoconfiguration=0,
  .autoconf_radios=0,
  .autoconf_scrambled=0,
  .autoconf_pid_update=1,
  .autoconf_lcn=0,
  .autoconf_ip="239.100.%card.%number",
  .time_start_autoconfiguration=0,
  .transport_stream_id=-1,
  .autoconf_temp_pat=NULL,
  .autoconf_temp_sdt=NULL,
  .autoconf_temp_psip=NULL,
  .services=NULL,
  .autoconf_unicast_port="\0",
  .autoconf_multicast_port="\0",
  .num_service_id=0,
  .name_template="\0",
};



//Parameters for rewriting
rewrite_parameters_t rewrite_vars={
  .rewrite_pat = OPTION_UNDEFINED,
  .pat_version=-1,
  .full_pat=NULL,
  .pat_needs_update=1,
  .full_pat_ok=0,
  .pat_continuity_counter=0,
  .rewrite_sdt = OPTION_UNDEFINED,
  .sdt_version=-1,
  .full_sdt=NULL,
  .sdt_needs_update=1,
  .full_sdt_ok=0,
  .sdt_continuity_counter=0,
  .eit_sort=OPTION_UNDEFINED,
};



#ifdef ENABLE_TRANSCODING
/** The transcode options defined for all the channels */
transcode_options_t global_transcode_opt;
#endif

//logging
extern log_params_t log_params;

// prototypes
static void SignalHandler (int signum);//below
int read_multicast_configuration(multicast_parameters_t *, mumudvb_channel_t *, int, int *, char *); //in multicast.c
void *monitor_func(void* arg);
int mumudvb_close(monitor_parameters_t* monitor_thread_params, unicast_parameters_t* unicast_vars, int* strengththreadshutdown, cam_parameters_t* cam_vars, char* filename_channels_not_streamed,char *filename_channels_diff, char *filename_pid, int Interrupted);

int
    main (int argc, char **argv)
{


  //sap announces
  sap_parameters_t sap_vars={
    .sap_messages=NULL,
    .sap=OPTION_UNDEFINED, //No sap by default
    .sap_interval=SAP_DEFAULT_INTERVAL,
    .sap_sending_ip="0.0.0.0",
    .sap_default_group="",
    .sap_organisation="MuMuDVB",
    .sap_uri="\0",
    .sap_ttl=SAP_DEFAULT_TTL,
  };

  //Statistics
  stats_infos_t stats_infos={
  .stats_num_packets_received=0,
  .stats_num_reads=0,
  .show_buffer_stats=0,
  .show_buffer_stats_time = 0,
  .show_buffer_stats_interval = 120,
  .show_traffic = 0,
  .show_traffic_time = 0,
  .compute_traffic_time = 0,
  .show_traffic_interval = 10,
  .compute_traffic_interval = 10,
  .up_threshold = 80,
  .down_threshold = 30,
  .debug_updown = 0,
  };

  //Parameters for HTTP unicast
  unicast_parameters_t unicast_vars={
    .unicast=0,
    .ipOut="0.0.0.0",
    .portOut=4242,
    .portOut_str=NULL,
    .consecutive_errors_timeout=UNICAST_CONSECUTIVE_ERROR_TIMEOUT,
    .max_clients=-1,
    .queue_max_size=UNICAST_DEFAULT_QUEUE_MAX,
    .socket_sendbuf_size=0,
    .drop_on_eagain=0,
  };


  //tuning parameters
  tuning_parameters_t tuneparams={
    .card = 0,
    .tuner = 0,
    .card_dev_path="",
    .card_tuned = 0,
    .tuning_timeout = ALARM_TIME_TIMEOUT,
    .freq = 0,
    .srate = 0,
    .pol = 0,
    .lnb_voltage_off=0,
    .lnb_type=LNB_UNIVERSAL,
    .lnb_lof_standard=DEFAULT_LOF_STANDARD,
    .lnb_slof=DEFAULT_SLOF,
    .lnb_lof_low=DEFAULT_LOF1_UNIVERSAL,
    .lnb_lof_high=DEFAULT_LOF2_UNIVERSAL,
    .sat_number = 0,
    .switch_type = 'C',
    .modulation_set = 0,
    .display_strenght = 0,
    .check_status = 1,
    .strengththreadshutdown = 0,
    .HP_CodeRate = HP_CODERATE_DEFAULT,//cf tune.h
    .LP_CodeRate = LP_CODERATE_DEFAULT,
    .TransmissionMode = TRANSMISSION_MODE_DEFAULT,
    .guardInterval = GUARD_INTERVAL_DEFAULT,
    .bandwidth = BANDWIDTH_DEFAULT,
    .hier = HIERARCHY_DEFAULT,
    .fe_type=FE_QPSK, //sat by default
  #if DVB_API_VERSION >= 5
    .delivery_system=SYS_UNDEFINED,
    .rolloff=ROLLOFF_35,
  #endif
  };
  card_tuned=&tuneparams.card_tuned;


  #ifdef ENABLE_CAM_SUPPORT
  //CAM (Conditionnal Access Modules : for scrambled channels)
  cam_parameters_t cam_vars={
    .cam_support = 0,
    .cam_number=0,
    .cam_reask_interval=0,
    .need_reset=0,
    .reset_counts=0,
    .reset_interval=CAM_DEFAULT_RESET_INTERVAL,
    .timeout_no_cam_init=CAM_DEFAULT_RESET_INTERVAL,
    .max_reset_number=CAM_DEFAULT_MAX_RESET_NUM,
    .tl=NULL,
    .sl=NULL,
    .stdcam=NULL,
    .ca_resource_connected=0,
    .mmi_state = MMI_STATE_CLOSED,
    .ca_info_ok_time=0,
    .cam_delay_pmt_send=0,
    .cam_interval_pmt_send=3,
    .cam_pmt_send_time=0,
  };
  #endif

  char filename_channels_not_streamed[DEFAULT_PATH_LEN];
  char filename_channels_diff[DEFAULT_PATH_LEN];
  char filename_pid[DEFAULT_PATH_LEN]=PIDFILE_PATH;
  char filename_gen_conf[DEFAULT_PATH_LEN];

  int server_id = 0; /** The server id for the template %server */

  int k,iRet,cmdlinecard;
  cmdlinecard=-1;

  //MPEG2-TS reception and sort
  int pid;			/** pid of the current mpeg2 packet */
  int ScramblingControl;

  /** The buffer for the card */
  card_buffer_t card_buffer;
  memset (&card_buffer, 0, sizeof (card_buffer_t));
  card_buffer.dvr_buffer_size=DEFAULT_TS_BUFFER_SIZE;
  card_buffer.max_thread_buffer_size=DEFAULT_THREAD_BUFFER_SIZE;
  /** List of mandatory pids */
  uint8_t mandatory_pid[MAX_MANDATORY_PID_NUMBER];

  struct timeval tv;

  //files
  char *conf_filename = NULL;
  FILE *conf_file;
  FILE *channels_diff;
  FILE *channels_not_streamed;
#ifdef ENABLE_CAM_SUPPORT
  FILE *cam_info;
#endif
  FILE *pidfile;

  // configuration file parsing
  int curr_channel = 0;
  int curr_pid = 0;
  int send_packet=0;
  int channel_start = 0;
  char current_line[CONF_LINELEN];
  char *substring=NULL;
  char delimiteurs[] = CONFIG_FILE_SEPARATOR;


  uint8_t hi_mappids[8193];
  uint8_t lo_mappids[8193];


  // Initialise PID map
  for (k = 0; k < 8193; k++)
  {
    hi_mappids[k] = (k >> 8);
    lo_mappids[k] = (k & 0xff);
  }

  /******************************************************/
  //Getopt
  /******************************************************/
  const char short_options[] = "c:sdthvql";
  const struct option long_options[] = {
    {"config", required_argument, NULL, 'c'},
    {"signal", no_argument, NULL, 's'},
    {"traffic", no_argument, NULL, 't'},
    {"server_id", required_argument, NULL, 'i'},
    {"debug", no_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {"list-cards", no_argument, NULL, 'l'},
    {"card", required_argument, NULL, 'a'},
    {0, 0, 0, 0}
  };
  int c, option_index = 0;

  if (argc == 1)
  {
    usage (program_invocation_short_name);
    exit(ERROR_ARGS);
  }

  while (1)
  {
    c = getopt_long (argc, argv, short_options,
                     long_options, &option_index);

    if (c == -1)
    {
      break;
    }
    switch (c)
    {
      case 'c':
        conf_filename = (char *) malloc (strlen (optarg) + 1);
        if (!conf_filename)
        {
          log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
          exit(ERROR_MEMORY);
        }
        strncpy (conf_filename, optarg, strlen (optarg) + 1);
        break;
      case 'a':
        cmdlinecard=atoi(optarg);
        break;
      case 's':
        tuneparams.display_strenght = 1;
        break;
      case 'i':
        server_id = atoi(optarg);
        break;
      case 't':
        stats_infos.show_traffic = 1;
        break;
      case 'd':
        no_daemon = 1;
        break;
      case 'v':
        log_params.verbosity++;
        break;
      case 'q':
        log_params.verbosity--;
        break;
      case 'h':
        usage (program_invocation_short_name);
        exit(ERROR_ARGS);
        break;
      case 'l':
        print_info ();
        list_dvb_cards ();
        exit(0);
        break;
    }
  }
  if (optind < argc)
  {
    usage (program_invocation_short_name);
    exit(ERROR_ARGS);
  }

  /******************************************************/
  //end of command line options parsing
  /******************************************************/

  // DO NOT REMOVE (make mumudvb a deamon)
  if(!no_daemon)
    if(daemon(42,0))
  {
    log_message( log_module,  MSG_WARN, "Cannot daemonize: %s\n",
                 strerror (errno));
    exit(666); //FIXME : use an error
  }

  //If the user didn't defined a prefered logging way, and we daemonise, we set to syslog
  if (!no_daemon)
  {
    if(log_params.log_type==LOGGING_UNDEFINED)
    {
      openlog ("MUMUDVB", LOG_PID, 0);
      log_params.log_type=LOGGING_SYSLOG;
      log_params.syslog_initialised=1;
    }
  }

  //Display general information
  print_info ();

  //paranoya we clear all the content of all the channels
  memset (&chan_and_pids.channels, 0, sizeof (mumudvb_channel_t)*MAX_CHANNELS);

  /******************************************************/
  // config file reading
  /******************************************************/
  conf_file = fopen (conf_filename, "r");
  if (conf_file == NULL)
  {
    log_message( log_module,  MSG_ERROR, "%s: %s\n",
                 conf_filename, strerror (errno));
    free(conf_filename);
    exit(ERROR_CONF_FILE);
  }

  curr_channel=-1;
  int curr_channel_old=-1;
  // we scan config file
  // see doc/README_CONF* for further information
  int line_len;
  while (fgets (current_line, CONF_LINELEN, conf_file))
  {
      //We suppress the end of line (this can disturb atoi if there is spaces at the end of the line)
      //Thanks to pierre gronlier pierre.gronlier at gmail.com for finding that bug
    line_len=strlen(current_line);
    if(current_line[line_len-1]=='\r' ||current_line[line_len-1]=='\n')
      current_line[line_len-1]=0;

    //Line without "=" we continue
    if(strstr(current_line,"=")==NULL)
    {
      //We check if it's not a channel_next line
      substring = strtok (current_line, delimiteurs);
      //If nothing in the substring we avoid the segfault in the next line
      if(substring == NULL)
        continue;
      if(strcmp (substring, "channel_next") )
        continue;
    }
      //commentary
    if (current_line[0] == '#')
      continue;
      //We split the line
    substring = strtok (current_line, delimiteurs);

      //If nothing in the substring we avoid the segfault in the next line
    if(substring == NULL)
      continue;

      //commentary
    if (substring[0] == '#')
      continue;

    if(curr_channel<0)
      channel_start=0;
    else
      channel_start=1;
    if((iRet=read_tuning_configuration(&tuneparams, substring))) //Read the line concerning the tuning parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
    else if((iRet=read_autoconfiguration_configuration(&autoconf_vars, substring))) //Read the line concerning the autoconfiguration parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
    else if((iRet=read_sap_configuration(&sap_vars, &chan_and_pids.channels[curr_channel], channel_start, substring))) //Read the line concerning the sap parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
#ifdef ENABLE_CAM_SUPPORT
    else if((iRet=read_cam_configuration(&cam_vars, &chan_and_pids.channels[curr_channel], channel_start, substring))) //Read the line concerning the cam parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
#endif
    else if((iRet=read_unicast_configuration(&unicast_vars, &chan_and_pids.channels[curr_channel], channel_start, substring))) //Read the line concerning the unicast parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
    else if((iRet=read_multicast_configuration(&multicast_vars, chan_and_pids.channels, channel_start, &curr_channel, substring))) //Read the line concerning the multicast parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
    else if((iRet=read_rewrite_configuration(&rewrite_vars, substring))) //Read the line concerning the rewrite parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
#ifdef ENABLE_TRANSCODING
    else if ((transcode_read_option((curr_channel>=0)?&chan_and_pids.channels[curr_channel].transcode_options : &global_transcode_opt, delimiteurs, &substring)))
    {
      continue;
    }
#endif
    else if((iRet=read_logging_configuration(&stats_infos, substring))) //Read the line concerning the logging parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
    else if (!strcmp (substring, "channel_next"))
    {
      curr_channel++;
      log_message( log_module, MSG_INFO,"channel next\n");
    }
    else if (!strcmp (substring, "timeout_no_diff"))
    {
      substring = strtok (NULL, delimiteurs);
      timeout_no_diff= atoi (substring);
    }
    else if (!strcmp (substring, "dont_send_scrambled"))
    {
      substring = strtok (NULL, delimiteurs);
      chan_and_pids.dont_send_scrambled = atoi (substring);
    }
    else if (!strcmp (substring, "dvr_buffer_size"))
    {
      substring = strtok (NULL, delimiteurs);
      card_buffer.dvr_buffer_size = atoi (substring);
      if(card_buffer.dvr_buffer_size<=0)
      {
        log_message( log_module,  MSG_WARN,
                     "Warning : the buffer size MUST be >0, forced to 1 packet\n");
        card_buffer.dvr_buffer_size = 1;
      }
      if(card_buffer.dvr_buffer_size>1)
        log_message( log_module,  MSG_WARN,
                     "Warning : You set a buffer size > 1, this feature is experimental, please report bugs/problems or results\n");
      stats_infos.show_buffer_stats=1;
    }
    else if (!strcmp (substring, "dvr_thread"))
    {
      substring = strtok (NULL, delimiteurs);
      card_buffer.threaded_read = atoi (substring);
      if(card_buffer.threaded_read)
      {
        log_message( log_module,  MSG_WARN,
                     "Warning : You want to use a thread for reading the card, this feature is experimental, please report bugs/problems or results\n");
      }
    }
    else if (!strcmp (substring, "dvr_thread_buffer_size"))
    {
      substring = strtok (NULL, delimiteurs);
      card_buffer.max_thread_buffer_size = atoi (substring);
    }
    else if ((!strcmp (substring, "service_id")) || (!strcmp (substring, "ts_id")))
    {
      if(!strcmp (substring, "ts_id"))
        log_message( log_module,  MSG_WARN, "Warning : the option ts_id is deprecated, use service_id instead.\n");
      if ( channel_start == 0)
      {
        log_message( log_module,  MSG_ERROR,
                     "service_id : You have to start a channel first (using ip= or channel_next)\n");
        exit(ERROR_CONF);
      }
      substring = strtok (NULL, delimiteurs);
      chan_and_pids.channels[curr_channel].service_id = atoi (substring);
    }
    else if (!strcmp (substring, "pids"))
    {
      curr_pid = 0;
      if ( channel_start == 0)
      {
        log_message( log_module,  MSG_ERROR,
                     "pids : You have to start a channel first (using ip= or channel_next)\n");
        exit(ERROR_CONF);
      }
      if (multicast_vars.common_port!=0 && chan_and_pids.channels[curr_channel].portOut == 0)
        chan_and_pids.channels[curr_channel].portOut = multicast_vars.common_port;
      while ((substring = strtok (NULL, delimiteurs)) != NULL)
      {
        chan_and_pids.channels[curr_channel].pids[curr_pid] = atoi (substring);
	 // we see if the given pid is good
        if (chan_and_pids.channels[curr_channel].pids[curr_pid] < 10 || chan_and_pids.channels[curr_channel].pids[curr_pid] >= 8193)
        {
          log_message( log_module,  MSG_ERROR,
                       "Config issue : %s in pids, given pid : %d\n",
                       conf_filename, chan_and_pids.channels[curr_channel].pids[curr_pid]);
          exit(ERROR_CONF);
        }
        curr_pid++;
        if (curr_pid >= MAX_PIDS_PAR_CHAINE)
        {
          log_message( log_module,  MSG_ERROR,
                       "Too many pids : %d channel : %d\n",
                       curr_pid, curr_channel);
          exit(ERROR_CONF);
        }
      }
      chan_and_pids.channels[curr_channel].num_pids = curr_pid;
    }
    else if (!strcmp (substring, "name"))
    {
      if ( channel_start == 0)
      {
        log_message( log_module,  MSG_ERROR,
                     "name : You have to start a channel first (using ip= or channel_next)\n");
        exit(ERROR_CONF);
      }
	  // other substring extraction method in order to keep spaces
      substring = strtok (NULL, "=");
      if (!(strlen (substring) >= MAX_NAME_LEN - 1))
        strcpy(chan_and_pids.channels[curr_channel].name,strtok(substring,"\n"));	
      else
      {
        log_message( log_module,  MSG_WARN,"Channel name too long\n");
        strncpy(chan_and_pids.channels[curr_channel].name,strtok(substring,"\n"),MAX_NAME_LEN-1);
        chan_and_pids.channels[curr_channel].name[MAX_NAME_LEN-1]='\0';
      }
    }
    else if (!strcmp (substring, "server_id"))
    {
      substring = strtok (NULL, delimiteurs);
      server_id = atoi (substring);
    }
    else if (!strcmp (substring, "filename_pid"))
    {
      substring = strtok (NULL, delimiteurs);
      if(strlen(substring)>=DEFAULT_PATH_LEN)
      {
        log_message(log_module,MSG_WARN,"filename_pid too long \n");
      }
      else
        strcpy(filename_pid,substring);
    }
    else
    {
      if(strlen (current_line) > 1)
        log_message( log_module,  MSG_WARN,
                     "Config issue : unknow symbol : %s\n\n", substring);
      continue;
    }

    if (curr_channel > MAX_CHANNELS)
    {
      log_message( log_module,  MSG_ERROR, "Too many channels : %d limit : %d\n",
                   curr_channel, MAX_CHANNELS);
      exit(ERROR_TOO_CHANNELS);
    }

    //A new channel have been defined
    if(curr_channel_old != curr_channel)
    {
      curr_channel_old = curr_channel;
      #ifdef ENABLE_TRANSCODING
      //We copy the common transcode options to the new channel
      transcode_copy_options(&global_transcode_opt,&chan_and_pids.channels[curr_channel].transcode_options);
      #endif
    }
  }
  fclose (conf_file);


  //Autoconfiguration full is the simple mode for autoconfiguration, we set other option by default
  if(autoconf_vars.autoconfiguration==AUTOCONF_MODE_FULL)
  {
    if((sap_vars.sap == OPTION_UNDEFINED) && (multicast_vars.multicast))
    {
      log_message( log_module,  MSG_INFO,
                   "Full autoconfiguration, we activate SAP announces. if you want to desactivate them see the README.\n");
      sap_vars.sap=OPTION_ON;
    }
    if(rewrite_vars.rewrite_pat == OPTION_UNDEFINED)
    {
      rewrite_vars.rewrite_pat=OPTION_ON;
      log_message( log_module,  MSG_INFO,
                   "Full autoconfiguration, we activate PAT rewritting. if you want to desactivate it see the README.\n");
    }
    if(rewrite_vars.rewrite_sdt == OPTION_UNDEFINED)
    {
      rewrite_vars.rewrite_sdt=OPTION_ON;
      log_message( log_module,  MSG_INFO,
                   "Full autoconfiguration, we activate SDT rewritting. if you want to desactivate it see the README.\n");
    }
    if(rewrite_vars.eit_sort == OPTION_UNDEFINED)
    {
      rewrite_vars.eit_sort=OPTION_ON;
      log_message( log_module,  MSG_INFO,
                   "Full autoconfiguration, we activate sorting of the EIT PID. if you want to desactivate it see the README.\n");
    }
  }
  if(card_buffer.max_thread_buffer_size<card_buffer.dvr_buffer_size)
  {
    log_message( log_module,  MSG_WARN,
		 "Warning : You set a thread buffer size lower than your dvr buffer size, it's not possible to use such values. I increase your dvr_thread_buffer_size ...\n");
		 card_buffer.max_thread_buffer_size=card_buffer.dvr_buffer_size;
  }

  //If we specified a card number on the command line, it overrides the config file
  if(cmdlinecard!=-1)
    tuneparams.card=cmdlinecard;
  //if no specific path for the DVB device, we use the default one
  if((!strlen(tuneparams.card_dev_path))||(cmdlinecard!=-1))
    sprintf(tuneparams.card_dev_path,DVB_DEV_PATH,tuneparams.card);


  //If we specified a string for the unicast port out, we parse it
  if(unicast_vars.portOut_str!=NULL)
  {
    int len;
    len=strlen(unicast_vars.portOut_str)+1;
    char number[10];
    sprintf(number,"%d",tuneparams.card);
    unicast_vars.portOut_str=mumu_string_replace(unicast_vars.portOut_str,&len,1,"%card",number);
    sprintf(number,"%d",tuneparams.tuner);
    unicast_vars.portOut_str=mumu_string_replace(unicast_vars.portOut_str,&len,1,"%tuner",number);
    sprintf(number,"%d",server_id);
    unicast_vars.portOut_str=mumu_string_replace(unicast_vars.portOut_str,&len,1,"%server",number);
    unicast_vars.portOut=string_comput(unicast_vars.portOut_str);
    log_message( "Unicast: ", MSG_DEBUG, "computed unicast master port : %d\n",unicast_vars.portOut);
  }

  if(log_params.log_file_path!=NULL)
  {
    int len;
    len=strlen(log_params.log_file_path)+1;
    char number[10];
    sprintf(number,"%d",tuneparams.card);
    log_params.log_file_path=mumu_string_replace(log_params.log_file_path,&len,1,"%card",number);
    sprintf(number,"%d",tuneparams.tuner);
    log_params.log_file_path=mumu_string_replace(log_params.log_file_path,&len,1,"%tuner",number);
    sprintf(number,"%d",server_id);
    log_params.log_file_path=mumu_string_replace(log_params.log_file_path,&len,1,"%server",number);
    log_params.log_file = fopen (log_params.log_file_path, "a");
    if (log_params.log_file)
      log_params.log_type |= LOGGING_FILE;
    else
      log_message(log_module,MSG_WARN,"Cannot open log file %s: %s\n", substring, strerror (errno));
  }
  /******************************************************/
  //end of config file reading
  /******************************************************/

  /*****************************************************/
  //daemon part two, we write our PID as we know the card number
  /*****************************************************/

  // We write our pid in a file if we deamonize
  if (!no_daemon)
  {
    int len;
    len=DEFAULT_PATH_LEN;
    char number[10];
    sprintf(number,"%d",tuneparams.card);
    mumu_string_replace(filename_pid,&len,0,"%card",number);
    sprintf(number,"%d",tuneparams.tuner);
    mumu_string_replace(filename_pid,&len,0,"%tuner",number);
    sprintf(number,"%d",server_id);
    mumu_string_replace(filename_pid,&len,0,"%server",number);;
    pidfile = fopen (filename_pid, "w");
    if (pidfile == NULL)
    {
      log_message( log_module,  MSG_INFO,"%s: %s\n",
                   filename_pid, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }
    fprintf (pidfile, "%d\n", getpid ());
    fclose (pidfile);
  }

  // + 1 Because of the new syntax
  chan_and_pids.number_of_channels = curr_channel+1;
  /*****************************************************/
  //Autoconfiguration init
  /*****************************************************/

  if(autoconf_vars.autoconfiguration)
  {
    if(autoconf_vars.autoconf_pid_update)
    {
      log_message( "Autoconf: ", MSG_INFO,
                   "The autoconfiguration auto update is enabled. If you want to disable it put \"autoconf_pid_update=0\" in your config file.\n");
    }
    //In case of autoconfiguration, we generate a config file with the channels discovered
    //Here we generate the header, ie we take the actual config file and copy it removing the channels
    sprintf (filename_gen_conf, GEN_CONF_PATH,
             tuneparams.card, tuneparams.tuner);
    gen_config_file_header(conf_filename, filename_gen_conf);
  }
  else
    autoconf_vars.autoconf_pid_update=0;
  /*****************************************************/
  //End of Autoconfiguration init
  /*****************************************************/

  //Transcoding, we apply the templates
#ifdef ENABLE_TRANSCODING
  for (curr_channel = 0; curr_channel < MAX_CHANNELS; curr_channel++)
  {
    transcode_options_apply_templates(&chan_and_pids.channels[curr_channel].transcode_options,tuneparams.card,tuneparams.tuner,server_id,curr_channel);
  }
#endif

  //We desactivate things depending on multicast if multicast is suppressed
  if(!multicast_vars.ttl)
  {
    log_message( log_module,  MSG_INFO, "The multicast TTL is set to 0, multicast will be desactivated.\n");
    multicast_vars.multicast=0;
  }
  if(!multicast_vars.multicast)
  {
#ifdef ENABLE_TRANSCODING
    for (curr_channel = 0; curr_channel < MAX_CHANNELS; curr_channel++)
    {
      if(chan_and_pids.channels[curr_channel].transcode_options.enable)
      {
	log_message( log_module,  MSG_INFO, "NO Multicast, transcoding desactivated for channel \"%s\".\n", chan_and_pids.channels[curr_channel].name);
	chan_and_pids.channels[curr_channel].transcode_options.enable=0;
      }
    }
#endif
      if(multicast_vars.rtp_header)
      {
	multicast_vars.rtp_header=0;
	log_message( log_module,  MSG_INFO, "NO Multicast, RTP Header is desactivated.\n");
      }
      if(sap_vars.sap==OPTION_ON)
      {
	log_message( log_module,  MSG_INFO, "NO Multicast, SAP announces are desactivated.\n");
	sap_vars.sap=OPTION_OFF;
      }
  }
  free(conf_filename);
  if(!multicast_vars.multicast && !unicast_vars.unicast)
  {
    log_message( log_module,  MSG_ERROR, "NO Multicast AND NO unicast. No data can be send :(, Exciting ....\n");
    Interrupted=ERROR_CONF<<8;
    goto mumudvb_close_goto;
  }



  // we clear them by paranoia
  sprintf (filename_channels_diff, STREAMED_LIST_PATH,
           tuneparams.card, tuneparams.tuner);
  sprintf (filename_channels_not_streamed, NOT_STREAMED_LIST_PATH,
           tuneparams.card, tuneparams.tuner);
  sprintf (cam_vars.filename_cam_info, CAM_INFO_LIST_PATH,
           tuneparams.card, tuneparams.tuner);
  channels_diff = fopen (filename_channels_diff, "w");
  if (channels_diff == NULL)
  {
    write_streamed_channels=0;
    log_message( log_module,  MSG_WARN,
                 "Can't create %s: %s\n",
                 filename_channels_diff, strerror (errno));
  }
  else
    fclose (channels_diff);

  channels_not_streamed = fopen (filename_channels_not_streamed, "w");
  if (channels_diff == NULL)
  {
    write_streamed_channels=0;
    log_message( log_module,  MSG_WARN,
                 "Can't create %s: %s\n",
                 filename_channels_not_streamed, strerror (errno));
  }
  else
    fclose (channels_not_streamed);


#ifdef ENABLE_CAM_SUPPORT
  if(cam_vars.cam_support)
  {
    cam_info = fopen (cam_vars.filename_cam_info, "w");
    if (cam_info == NULL)
    {
      log_message( log_module,  MSG_WARN,
                   "Can't create %s: %s\n",
                   cam_vars.filename_cam_info, strerror (errno));
    }
    else
      fclose (cam_info);
  }
#endif


  log_message( log_module,  MSG_INFO, "Streaming. Freq %d\n",
               tuneparams.freq);


  /******************************************************/
  // Card tuning
  /******************************************************/
  if (signal (SIGALRM, SignalHandler) == SIG_IGN)
    signal (SIGALRM, SIG_IGN);
  if (signal (SIGUSR1, SignalHandler) == SIG_IGN)
    signal (SIGUSR1, SIG_IGN);
  if (signal (SIGUSR2, SignalHandler) == SIG_IGN)
    signal (SIGUSR2, SIG_IGN);
  if (signal (SIGHUP, SignalHandler) == SIG_IGN)
    signal (SIGHUP, SIG_IGN);
  // alarm for tuning timeout
  if(tuneparams.tuning_timeout)
  {
    alarm (tuneparams.tuning_timeout);
  }


  // We tune the card
  iRet =-1;

  if (open_fe (&fds.fd_frontend, tuneparams.card_dev_path, tuneparams.tuner))
  {
    iRet = 
        tune_it (fds.fd_frontend, &tuneparams);
  }

  if (iRet < 0)
  {
    log_message( log_module,  MSG_INFO, "Tunning issue, card %d\n", tuneparams.card);
    // we close the file descriptors
    close_card_fd (fds);
    Interrupted=ERROR_TUNE<<8;
    goto mumudvb_close_goto;
  }
  log_message( log_module,  MSG_INFO, "Card %d tuned\n", tuneparams.card);
  tuneparams.card_tuned = 1;

  //Thread for showing the strength
  strength_parameters_t strengthparams;
  strengthparams.fds = &fds;
  strengthparams.tuneparams = &tuneparams;
  pthread_create(&(signalpowerthread), NULL, show_power_func, &strengthparams);
  //Thread for reading from the DVB card initialisation
  if(card_buffer.threaded_read)
  {
    cardthreadparams.thread_running=1;
    cardthreadparams.fds = &fds;
    cardthreadparams.card_buffer=&card_buffer;
    pthread_mutex_init(&cardthreadparams.carddatamutex,NULL);
    pthread_cond_init(&cardthreadparams.threadcond,NULL);
    cardthreadparams.threadshutdown=0;
  }
  else
    cardthreadparams.thread_running=0;



  /******************************************************/
  //card tuned
  /******************************************************/

  // the card is tuned, we catch signals to close cleanly
  if (signal (SIGHUP, SignalHandler) == SIG_IGN)
    signal (SIGHUP, SIG_IGN);
  if (signal (SIGINT, SignalHandler) == SIG_IGN)
    signal (SIGINT, SIG_IGN);
  if (signal (SIGTERM, SignalHandler) == SIG_IGN)
    signal (SIGTERM, SIG_IGN);
  struct sigaction act;
  act.sa_handler = SIG_IGN;
  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;
  if(sigaction (SIGPIPE, &act, NULL)<0)
    log_message( log_module,  MSG_ERROR,"ErrorSigaction\n");


  //We record the starting time
  gettimeofday (&tv, (struct timezone *) NULL);
  real_start_time = tv.tv_sec;
  now = 0;

  alarm (ALARM_TIME);

  if(stats_infos.show_traffic)
    log_message( log_module, MSG_INFO,"The traffic will be shown every %d second%c\n",stats_infos.show_traffic_interval, stats_infos.show_traffic_interval > 1? 's':' ');



  /******************************************************/
  // Monitor Thread
  /******************************************************/
  monitor_parameters_t monitor_thread_params ={
    .threadshutdown=0,
    .wait_time=10,
    .autoconf_vars=&autoconf_vars,
    .sap_vars=&sap_vars,
    .chan_and_pids=&chan_and_pids,
    .multicast_vars=&multicast_vars,
    .unicast_vars=&unicast_vars,
    .tuneparams=&tuneparams,
    .stats_infos=&stats_infos,
    .server_id=server_id,
    .filename_channels_not_streamed=filename_channels_not_streamed,
    .filename_channels_diff=filename_channels_diff,
  };

  pthread_create(&(monitorthread), NULL, monitor_func, &monitor_thread_params);

  /*****************************************************/
  //cam_support
  /*****************************************************/

#ifdef ENABLE_CAM_SUPPORT
  if(cam_vars.cam_support){
    //We initialise the cam. If fail, we remove cam support
    if(cam_start(&cam_vars,tuneparams.card))
    {
      log_message("CAM: ", MSG_ERROR,"Cannot initalise cam\n");
      cam_vars.cam_support=0;
    }
    else
    {
      //If the cam is properly initialised, we autoconfigure scrambled channels
      autoconf_vars.autoconf_scrambled=1;
    }
  }
#endif

  /*****************************************************/
  //autoconfiguration
  //memory allocation for MPEG2-TS
  //packet structures
  /*****************************************************/
  iRet=autoconf_init(&autoconf_vars, chan_and_pids.channels,chan_and_pids.number_of_channels);
  if(iRet)
  {
    Interrupted=ERROR_GENERIC<<8;
    goto mumudvb_close_goto;
  }

  /*****************************************************/
  //Pat rewriting
  //memory allocation for MPEG2-TS
  //packet structures
  /*****************************************************/

  if(rewrite_vars.rewrite_pat == OPTION_ON)
  {
    for (curr_channel = 0; curr_channel < MAX_CHANNELS; curr_channel++)
      chan_and_pids.channels[curr_channel].generated_pat_version=-1;

    rewrite_vars.full_pat=malloc(sizeof(mumudvb_ts_packet_t));
    if(rewrite_vars.full_pat==NULL)
    {
      log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      Interrupted=ERROR_MEMORY<<8;
      goto mumudvb_close_goto;
    }
    memset (rewrite_vars.full_pat, 0, sizeof( mumudvb_ts_packet_t));//we clear it
    pthread_mutex_init(&rewrite_vars.full_pat->packetmutex,NULL);
  }

  /*****************************************************/
  //SDT rewriting
  //memory allocation for MPEG2-TS
  //packet structures
  /*****************************************************/

  if(rewrite_vars.rewrite_sdt == OPTION_ON)
  {
    for (curr_channel = 0; curr_channel < MAX_CHANNELS; curr_channel++)
      chan_and_pids.channels[curr_channel].generated_sdt_version=-1;

    rewrite_vars.full_sdt=malloc(sizeof(mumudvb_ts_packet_t));
    if(rewrite_vars.full_sdt==NULL)
    {
      log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      Interrupted=ERROR_MEMORY<<8;
      goto mumudvb_close_goto;
    }
    memset (rewrite_vars.full_sdt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
    pthread_mutex_init(&rewrite_vars.full_sdt->packetmutex,NULL);
  }

  /*****************************************************/
  //Some initialisations
  /*****************************************************/

  //Initialisation of the channels for RTP
  if(multicast_vars.rtp_header)
    for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
      init_rtp_header(&chan_and_pids.channels[curr_channel]);

  // initialisation of active channels list
  for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
  {
    chan_and_pids.channels[curr_channel].num_packet = 0;
    chan_and_pids.channels[curr_channel].streamed_channel = 1;
    chan_and_pids.channels[curr_channel].num_scrambled_packets = 0;
    chan_and_pids.channels[curr_channel].scrambled_channel = 0;

    //We alloc the channel pmt_packet (useful for autoconf and cam)
    /**@todo : allocate only if autoconf or cam*/
    if(chan_and_pids.channels[curr_channel].pmt_packet==NULL)
    {
      chan_and_pids.channels[curr_channel].pmt_packet=malloc(sizeof(mumudvb_ts_packet_t));
      if(chan_and_pids.channels[curr_channel].pmt_packet==NULL)
      {
        log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      Interrupted=ERROR_MEMORY<<8;
      goto mumudvb_close_goto;
      }
      memset (chan_and_pids.channels[curr_channel].pmt_packet, 0, sizeof( mumudvb_ts_packet_t));//we clear it
      pthread_mutex_init(&chan_and_pids.channels[curr_channel].pmt_packet->packetmutex,NULL);
    }

  }

  //We initialise asked pid table
  memset (chan_and_pids.asked_pid, 0, sizeof( uint8_t)*8193);//we clear it
  memset (chan_and_pids.number_chan_asked_pid, 0, sizeof( uint8_t)*8193);//we clear it

  //We initialise mandatory pid table
  memset (mandatory_pid, 0, sizeof( uint8_t)*MAX_MANDATORY_PID_NUMBER);//we clear it

  //mandatory pids (always sent with all channels)
  //PAT : Program Association Table
  mandatory_pid[0]=1;
  chan_and_pids.asked_pid[0]=PID_ASKED;
  //CAT : Conditional Access Table 
  mandatory_pid[1]=1;
  chan_and_pids.asked_pid[1]=PID_ASKED;
  //NIT : Network Information Table
  //It is intended to provide information about the physical network.
  mandatory_pid[16]=1;
  chan_and_pids.asked_pid[16]=PID_ASKED;
  //SDT : Service Description Table
  //the SDT contains data describing the services in the system e.g. names of services, the service provider, etc.
  mandatory_pid[17]=1;
  chan_and_pids.asked_pid[17]=PID_ASKED;
  //EIT : Event Information Table
  //the EIT contains data concerning events or programmes such as event name, start time, duration, etc.
  mandatory_pid[18]=1;
  chan_and_pids.asked_pid[18]=PID_ASKED;
  //TDT : Time and Date Table
  //the TDT gives information relating to the present time and date.
  //This information is given in a separate table due to the frequent updating of this information.
  mandatory_pid[20]=1;
  chan_and_pids.asked_pid[20]=PID_ASKED;

  //PSIP : Program and System Information Protocol
  //Specific to ATSC, this is more or less the equivalent of sdt plus other stuff
  if(tuneparams.fe_type==FE_ATSC)
    chan_and_pids.asked_pid[PSIP_PID]=PID_ASKED;

  /*****************************************************/
  //We open the file descriptors and
  //Set the filters
  /*****************************************************/

  //We fill the asked_pid array
  for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
  {
    for (curr_pid = 0; curr_pid < chan_and_pids.channels[curr_channel].num_pids; curr_pid++)
    {
      if(chan_and_pids.asked_pid[chan_and_pids.channels[curr_channel].pids[curr_pid]]==PID_NOT_ASKED)
        chan_and_pids.asked_pid[chan_and_pids.channels[curr_channel].pids[curr_pid]]=PID_ASKED;
      chan_and_pids.number_chan_asked_pid[chan_and_pids.channels[curr_channel].pids[curr_pid]]++;
    }
  }

  // we open the file descriptors
  if (create_card_fd (tuneparams.card_dev_path, tuneparams.tuner, chan_and_pids.asked_pid, &fds) < 0)
  {
    Interrupted=ERROR_GENERIC<<8;
    goto mumudvb_close_goto;
  }

  set_filters(chan_and_pids.asked_pid, &fds);
  fds.pfds=NULL;
  fds.pfdsnum=1;
  //+1 for closing the pfd list, see man poll
  fds.pfds=realloc(fds.pfds,(fds.pfdsnum+1)*sizeof(struct pollfd));
  if (fds.pfds==NULL)
  {
    log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    Interrupted=ERROR_MEMORY<<8;
    goto mumudvb_close_goto;
  }

  //We fill the file descriptor information structure. the first one is irrelevant
  //(file descriptor associated to the DVB card) but we allocate it for consistency
  unicast_vars.fd_info=NULL;
  unicast_vars.fd_info=realloc(unicast_vars.fd_info,(fds.pfdsnum)*sizeof(unicast_fd_info_t));
  if (unicast_vars.fd_info==NULL)
  {
    log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    Interrupted=ERROR_MEMORY<<8;
    goto mumudvb_close_goto;
  }

  //File descriptor for polling the DVB card
  fds.pfds[0].fd = fds.fd_dvr;
  //POLLIN : data available for read
  fds.pfds[0].events = POLLIN | POLLPRI; 
  fds.pfds[0].revents = 0;
  fds.pfds[1].fd = 0;
  fds.pfds[1].events = POLLIN | POLLPRI;
  fds.pfds[1].revents = 0;



  /*****************************************************/
  // Init network, we open the sockets
  /*****************************************************/
  if(multicast_vars.multicast)
    for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
    {
      //See the README for the reason of this option
      if(multicast_vars.auto_join)
        chan_and_pids.channels[curr_channel].socketOut = makeclientsocket (chan_and_pids.channels[curr_channel].ipOut, chan_and_pids.channels[curr_channel].portOut, multicast_vars.ttl, &chan_and_pids.channels[curr_channel].sOut);
      else
        chan_and_pids.channels[curr_channel].socketOut = makesocket (chan_and_pids.channels[curr_channel].ipOut, chan_and_pids.channels[curr_channel].portOut, multicast_vars.ttl, &chan_and_pids.channels[curr_channel].sOut);
  }


  //We open the socket for the http unicast if needed and we update the poll structure
  if(unicast_vars.unicast)
  {
    log_message("Unicast: ", MSG_INFO,"We open the Master http socket for address %s:%d\n",unicast_vars.ipOut, unicast_vars.portOut);
    unicast_create_listening_socket(UNICAST_MASTER, -1, unicast_vars.ipOut, unicast_vars.portOut, &unicast_vars.sIn, &unicast_vars.socketIn, &fds, &unicast_vars);
    /** open the unicast listening connections fo the channels */
    for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
      if(chan_and_pids.channels[curr_channel].unicast_port)
    {
      log_message("Unicast: ", MSG_INFO,"We open the channel %d http socket address %s:%d\n",curr_channel, unicast_vars.ipOut, chan_and_pids.channels[curr_channel].unicast_port);
      unicast_create_listening_socket(UNICAST_LISTEN_CHANNEL, curr_channel, unicast_vars.ipOut,chan_and_pids.channels[curr_channel].unicast_port , &chan_and_pids.channels[curr_channel].sIn, &chan_and_pids.channels[curr_channel].socketIn, &fds, &unicast_vars);
    }
  }


  /*****************************************************/
  // init sap
  /*****************************************************/

  iRet=init_sap(&sap_vars, multicast_vars);
  if(iRet)
  {
    Interrupted=ERROR_GENERIC<<8;
    goto mumudvb_close_goto;
  }

  /*****************************************************/
  // Information about streamed channels
  /*****************************************************/

  if(autoconf_vars.autoconfiguration!=AUTOCONF_MODE_FULL)
    log_streamed_channels(log_module,chan_and_pids.number_of_channels, chan_and_pids.channels, multicast_vars.multicast, unicast_vars.unicast, unicast_vars.portOut, unicast_vars.ipOut);

  if(autoconf_vars.autoconfiguration)
    log_message("Autoconf: ",MSG_INFO,"Autoconfiguration Start\n");


  //Thread for reading from the DVB card RUNNING
  if(card_buffer.threaded_read)
  {
    pthread_create(&(cardthread), NULL, read_card_thread_func, &cardthreadparams);
    //We alloc the buffers
    card_buffer.write_buffer_size=card_buffer.max_thread_buffer_size*TS_PACKET_SIZE;
    card_buffer.buffer1=malloc(sizeof(unsigned char)*card_buffer.write_buffer_size);
    card_buffer.buffer2=malloc(sizeof(unsigned char)*card_buffer.write_buffer_size);
    card_buffer.actual_read_buffer=1;
    card_buffer.reading_buffer=card_buffer.buffer1;
    card_buffer.writing_buffer=card_buffer.buffer2;
    cardthreadparams.main_waiting=0;
  }else
  {
    //We alloc the buffer
    card_buffer.reading_buffer=malloc(sizeof(unsigned char)*TS_PACKET_SIZE*card_buffer.dvr_buffer_size);
  }
  /******************************************************/
  //Main loop where we get the packets and send them
  /******************************************************/
  int poll_ret;
  /**Buffer containing one packet*/
  unsigned char *actual_ts_packet;
  while (!Interrupted)
  {
    if(card_buffer.threaded_read)
    {
      if(!card_buffer.bytes_in_write_buffer && !cardthreadparams.unicast_data)
      {
	pthread_mutex_lock(&cardthreadparams.carddatamutex);
        cardthreadparams.main_waiting=1;
        pthread_cond_wait(&cardthreadparams.threadcond,&cardthreadparams.carddatamutex);
	//pthread_mutex_lock(&cardthreadparams.carddatamutex);
        cardthreadparams.main_waiting=0;
      }
      else
	pthread_mutex_lock(&cardthreadparams.carddatamutex);

      if(card_buffer.bytes_in_write_buffer)
      {
	if(card_buffer.actual_read_buffer==1)
	{
	  card_buffer.reading_buffer=card_buffer.buffer2;
	  card_buffer.writing_buffer=card_buffer.buffer1;
	  card_buffer.actual_read_buffer=2;
	  
	}
	else
	{
	  card_buffer.reading_buffer=card_buffer.buffer1;
	  card_buffer.writing_buffer=card_buffer.buffer2;
	  card_buffer.actual_read_buffer=1;
	  
	}
	card_buffer.bytes_read=card_buffer.bytes_in_write_buffer;
	card_buffer.bytes_in_write_buffer=0;
      }
      pthread_mutex_unlock(&cardthreadparams.carddatamutex);
      if(cardthreadparams.unicast_data)
      {
	iRet=unicast_handle_fd_event(&unicast_vars, &fds, chan_and_pids.channels, chan_and_pids.number_of_channels);
	if(iRet)
	{
	  Interrupted=iRet;
	  continue;
	}
	pthread_mutex_lock(&cardthreadparams.carddatamutex);
	cardthreadparams.unicast_data=0;
	pthread_mutex_unlock(&cardthreadparams.carddatamutex);
	
      }
    }
    else
    {
      /* Poll the open file descriptors : we wait for data*/
      poll_ret=mumudvb_poll(&fds);
      if(poll_ret)
      {
	Interrupted=poll_ret;
	continue;
      }
      /**************************************************************/
      /* UNICAST HTTP                                               */
      /**************************************************************/ 
      if((!(fds.pfds[0].revents&POLLIN)) && (!(fds.pfds[0].revents&POLLPRI))) //Priority to the DVB packets so if there is dvb packets and something else, we look first to dvb packets
      {
	iRet=unicast_handle_fd_event(&unicast_vars, &fds, chan_and_pids.channels, chan_and_pids.number_of_channels);
	if(iRet)
	  Interrupted=iRet;
	//no DVB packet, we continue
	continue;
      }
      /**************************************************************/
      /* END OF UNICAST HTTP                                        */
      /**************************************************************/ 

      if((card_buffer.bytes_read=card_read(fds.fd_dvr,  card_buffer.reading_buffer, &card_buffer))==0)
	continue;
    }

    if(card_buffer.dvr_buffer_size!=1 && stats_infos.show_buffer_stats)
    {
      stats_infos.stats_num_packets_received+=(int) card_buffer.bytes_read/188;
      stats_infos.stats_num_reads++;
    }
 
    for(card_buffer.read_buff_pos=0;
	(card_buffer.read_buff_pos+TS_PACKET_SIZE)<=card_buffer.bytes_read;
	card_buffer.read_buff_pos+=TS_PACKET_SIZE)//we loop on the subpackets
    {
      actual_ts_packet=card_buffer.reading_buffer+card_buffer.read_buff_pos;

      pid = ((actual_ts_packet[1] & 0x1f) << 8) | (actual_ts_packet[2]);

      //Software filtering in case the card doesn't have hardware filtering
      if(chan_and_pids.asked_pid[8192]==PID_NOT_ASKED && chan_and_pids.asked_pid[pid]==PID_NOT_ASKED)
        continue;

      ScramblingControl = (actual_ts_packet[3] & 0xc0) >> 6;
/* 0 = Not scrambled
   1 = Reserved for future use
   2 = Scrambled with even key
   3 = Scrambled with odd key*/
	  

      /******************************************************/
      //   AUTOCONFIGURATION PART
      /******************************************************/
      if(!ScramblingControl &&  autoconf_vars.autoconfiguration)
      {
        iRet = autoconf_new_packet(pid, actual_ts_packet, &autoconf_vars,  &fds, &chan_and_pids, &tuneparams, &multicast_vars, &unicast_vars, server_id);
        if(iRet)
          Interrupted = iRet;
      }
      if(autoconf_vars.autoconfiguration)
        continue;
 
      /******************************************************/
      //   AUTOCONFIGURATION PART FINISHED
      /******************************************************/

      /******************************************************/
      //Pat rewrite 
      /******************************************************/
      if( (pid == 0) && //This is a PAT PID
           rewrite_vars.rewrite_pat == OPTION_ON ) //AND we asked for rewrite
      {
        pat_rewrite_new_global_packet(actual_ts_packet, &rewrite_vars);
      }
      /******************************************************/
      //SDT rewrite 
      /******************************************************/
      if( (pid == 17) && //This is a SDT PID
           rewrite_vars.rewrite_sdt == OPTION_ON ) //AND we asked for rewrite
	   {
	     //we check the new packet and if it's fully updated we set the skip to 0
	     if(sdt_rewrite_new_global_packet(actual_ts_packet, &rewrite_vars)==1)
	     {
	       log_message( log_module, MSG_DETAIL,"The SDT version changed, we force the update of all the channels.\n");
	       for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
	        chan_and_pids.channels[curr_channel].sdt_rewrite_skip=0;
	     }
	   }


      /******************************************************/
      //for each channel we'll look if we must send this PID
      /******************************************************/
      for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
      {
	//we'll see if we must send this pid for this channel
        send_packet=0;

	//If it's a mandatory pid we send it
        if((pid<MAX_MANDATORY_PID_NUMBER) && (mandatory_pid[pid]==1))
          send_packet=1;
        if ((pid == PSIP_PID) && (tuneparams.fe_type==FE_ATSC))
          send_packet=1;


	//if it isn't mandatory wee see if it is in the channel list
        if(!send_packet)
          for (curr_pid = 0; (curr_pid < chan_and_pids.channels[curr_channel].num_pids)&& !send_packet; curr_pid++)
            if ((chan_and_pids.channels[curr_channel].pids[curr_pid] == pid) || (chan_and_pids.channels[curr_channel].pids[curr_pid] == 8192)) //We can stream whole transponder using 8192
        {
          send_packet=1;
          //avoid sending of scrambled channels if we asked to
          if(chan_and_pids.dont_send_scrambled && (ScramblingControl>0)&& (pid != chan_and_pids.channels[curr_channel].pmt_pid) )
            send_packet=0;
          if ((ScramblingControl>0) && (pid != chan_and_pids.channels[curr_channel].pmt_pid) )
            chan_and_pids.channels[curr_channel].num_scrambled_packets++;

          //we don't count the PMT pid for up channels
          if (pid != chan_and_pids.channels[curr_channel].pmt_pid)
            chan_and_pids.channels[curr_channel].num_packet++;
        }

        /******************************************************/
	//cam support
	// If we send the packet, we look if it's a cam pmt pid
        /******************************************************/
#ifdef ENABLE_CAM_SUPPORT
        if((cam_vars.cam_support && send_packet==1) &&  //no need to check paquets we don't send
            cam_vars.ca_resource_connected &&
            ((now-cam_vars.cam_pmt_send_time)>=cam_vars.cam_interval_pmt_send ))
        {
          if(cam_new_packet(pid, curr_channel, actual_ts_packet, &autoconf_vars, &cam_vars, &chan_and_pids.channels[curr_channel]))
            cam_vars.cam_pmt_send_time=now; //A packet was sent to the CAM
        }
#endif

        /******************************************************/
	//PMT follow (ie we check if the pids announced in the PMT changed)
        /******************************************************/
        if( (autoconf_vars.autoconf_pid_update) && 
             (send_packet==1) && //no need to check paquets we don't send
             (chan_and_pids.channels[curr_channel].autoconfigurated) && //only channels whose pids where detected by autoconfiguration (we don't erase "manual" channels)
             (chan_and_pids.channels[curr_channel].pmt_pid==pid) &&     //And we see the PMT
             pid)
        {
          autoconf_pmt_follow( actual_ts_packet, &fds, &chan_and_pids.channels[curr_channel], tuneparams.card_dev_path, tuneparams.tuner, &chan_and_pids );
        }
	  
        /******************************************************/
	//Rewrite PAT
        /******************************************************/
        if((send_packet==1) && //no need to check paquets we don't send
            (pid == 0) && //This is a PAT PID
            rewrite_vars.rewrite_pat == OPTION_ON )  //AND we asked for rewrite
          send_packet=pat_rewrite_new_channel_packet(actual_ts_packet, &rewrite_vars, &chan_and_pids.channels[curr_channel], curr_channel);

        /******************************************************/
	//Rewrite SDT
        /******************************************************/
        if((send_packet==1) && //no need to check paquets we don't send
            (pid == 17) && //This is a SDT PID
            rewrite_vars.rewrite_sdt == OPTION_ON &&  //AND we asked for rewrite
            !chan_and_pids.channels[curr_channel].sdt_rewrite_skip ) //AND the generation was successful
          send_packet=sdt_rewrite_new_channel_packet(actual_ts_packet, &rewrite_vars, &chan_and_pids.channels[curr_channel], curr_channel);

        /******************************************************/
        //EIT SORT
        /******************************************************/
        if((send_packet==1) &&//no need to check paquets we don't send
           (pid == 18) && //This is a EIT PID
            (chan_and_pids.channels[curr_channel].service_id) && //we have the service_id
            rewrite_vars.eit_sort ) //AND we asked for EIT sorting
        {
          send_packet=eit_sort_new_packet(actual_ts_packet, &chan_and_pids.channels[curr_channel]);
        }

        /******************************************************/
	//Ok we must send this packet,
	// we add it to the channel buffer
        /******************************************************/
        if(send_packet==1)
        {
          // we fill the channel buffer
          memcpy(chan_and_pids.channels[curr_channel].buf + chan_and_pids.channels[curr_channel].nb_bytes, actual_ts_packet, TS_PACKET_SIZE);

          chan_and_pids.channels[curr_channel].buf[chan_and_pids.channels[curr_channel].nb_bytes + 1] =
              (chan_and_pids.channels[curr_channel].buf[chan_and_pids.channels[curr_channel].nb_bytes + 1] & 0xe0) | hi_mappids[pid];
          chan_and_pids.channels[curr_channel].buf[chan_and_pids.channels[curr_channel].nb_bytes + 2] = lo_mappids[pid];

          chan_and_pids.channels[curr_channel].nb_bytes += TS_PACKET_SIZE;
          //The buffer is full, we send it
          if ((!multicast_vars.rtp_header && ((chan_and_pids.channels[curr_channel].nb_bytes + TS_PACKET_SIZE) > MAX_UDP_SIZE))
	    ||(multicast_vars.rtp_header && ((chan_and_pids.channels[curr_channel].nb_bytes + RTP_HEADER_LEN + TS_PACKET_SIZE) > MAX_UDP_SIZE)))
          {
            //For bandwith measurement (traffic)
            chan_and_pids.channels[curr_channel].sent_data+=chan_and_pids.channels[curr_channel].nb_bytes+20+8; // IP=20 bytes header and UDP=8 bytes header
            if (multicast_vars.rtp_header) chan_and_pids.channels[curr_channel].sent_data+=RTP_HEADER_LEN;

            /********* TRANSCODE **********/
#ifdef ENABLE_TRANSCODING
	    if (NULL != chan_and_pids.channels[curr_channel].transcode_options.enable &&
		    1 == *chan_and_pids.channels[curr_channel].transcode_options.enable) {

		if (NULL == chan_and_pids.channels[curr_channel].transcode_handle) {

		    strcpy(chan_and_pids.channels[curr_channel].transcode_options.ip, chan_and_pids.channels[curr_channel].ipOut);

		    chan_and_pids.channels[curr_channel].transcode_handle = transcode_start_thread(chan_and_pids.channels[curr_channel].socketOut,
			&chan_and_pids.channels[curr_channel].sOut, &chan_and_pids.channels[curr_channel].transcode_options);
		}

		if (NULL != chan_and_pids.channels[curr_channel].transcode_handle) {
		    transcode_enqueue_data(chan_and_pids.channels[curr_channel].transcode_handle,
			chan_and_pids.channels[curr_channel].buf,
					   chan_and_pids.channels[curr_channel].nb_bytes);
		}
	    }

	    if (NULL == chan_and_pids.channels[curr_channel].transcode_options.enable ||
		    1 != *chan_and_pids.channels[curr_channel].transcode_options.enable ||
		    ((NULL != chan_and_pids.channels[curr_channel].transcode_options.streaming_type &&
		    STREAMING_TYPE_MPEGTS != *chan_and_pids.channels[curr_channel].transcode_options.streaming_type)&&
		    (NULL == chan_and_pids.channels[curr_channel].transcode_options.send_transcoded_only ||
		     1 != *chan_and_pids.channels[curr_channel].transcode_options.send_transcoded_only)))
#endif
            /********** MULTICAST *************/
             //if the multicast TTL is set to 0 we don't send the multicast packets
            if(multicast_vars.multicast)
	    {
	      if(multicast_vars.rtp_header)
	      {
		/****** RTP *******/
		rtp_update_sequence_number(&chan_and_pids.channels[curr_channel]);
                sendudp (chan_and_pids.channels[curr_channel].socketOut,
		         &chan_and_pids.channels[curr_channel].sOut,
		         chan_and_pids.channels[curr_channel].buf_with_rtp_header,
                         chan_and_pids.channels[curr_channel].nb_bytes+RTP_HEADER_LEN);
	      }
	      else
		sendudp (chan_and_pids.channels[curr_channel].socketOut,
			 &chan_and_pids.channels[curr_channel].sOut,
			 chan_and_pids.channels[curr_channel].buf,
                         chan_and_pids.channels[curr_channel].nb_bytes);
	    }
            /*********** UNICAST **************/
	    unicast_data_send(&chan_and_pids.channels[curr_channel], chan_and_pids.channels, &fds, &unicast_vars);
            /********* END of UNICAST **********/
	    chan_and_pids.channels[curr_channel].nb_bytes = 0;
          }
        }
      }
    }
  }
  /******************************************************/
  //End of main loop
  /******************************************************/

  gettimeofday (&tv, (struct timezone *) NULL);
  log_message( log_module,  MSG_INFO,
               "End of streaming. We streamed during %dd %d:%02d:%02d\n",(tv.tv_sec - real_start_time )/86400,((tv.tv_sec - real_start_time) % 86400 )/3600,((tv.tv_sec - real_start_time) % 3600)/60,(tv.tv_sec - real_start_time) %60 );

  if(card_buffer.partial_packet_number)
    log_message( log_module,  MSG_INFO,
                 "We received %d partial packets :-( \n",card_buffer.partial_packet_number );
  if(card_buffer.partial_packet_number)
    log_message( log_module,  MSG_INFO,
                 "We have got %d overflow errors\n",card_buffer.overflow_number );
mumudvb_close_goto:
  //If the thread is not started, we don't send the unexisting address of monitor_thread_params
  return mumudvb_close(monitorthread == 0 ? NULL:&monitor_thread_params , &unicast_vars, &tuneparams.strengththreadshutdown, &cam_vars, filename_channels_not_streamed, filename_channels_diff, filename_pid, Interrupted);

}

/** @brief Clean closing and freeing
 *
 *
 */
int mumudvb_close(monitor_parameters_t *monitor_thread_params, unicast_parameters_t *unicast_vars, int *strengththreadshutdown, cam_parameters_t *cam_vars, char *filename_channels_not_streamed, char *filename_channels_diff, char *filename_pid, int Interrupted)
{

  int curr_channel;


  if (Interrupted)
  {
    if(Interrupted< (1<<8)) //we check if it's a signal or a mumudvb error
      log_message( log_module,  MSG_INFO, "Caught signal %d - closing cleanly.\n",
                   Interrupted);
    else
      log_message( log_module,  MSG_INFO, "Closing cleanly. Error %d\n",Interrupted>>8);
  }

  if(signalpowerthread)
  {
    log_message(log_module,MSG_FLOOD,"Signal/power Thread closing\n");
    *strengththreadshutdown=1;
    pthread_join(signalpowerthread, NULL);
  }
  if(cardthreadparams.thread_running)
  {
    log_message(log_module,MSG_FLOOD,"Card reading Thread closing\n");
    cardthreadparams.threadshutdown=1;
    pthread_mutex_destroy(&cardthreadparams.carddatamutex);
    pthread_cond_destroy(&cardthreadparams.threadcond);
  }
  //We shutdown the monitoring thread
  if(monitorthread)
  {
    log_message(log_module,MSG_FLOOD,"Monitor Thread closing\n");
    monitor_thread_params->threadshutdown=1;
    pthread_join(monitorthread, NULL);
  }

  for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
  {
#ifdef ENABLE_TRANSCODING
    transcode_request_thread_end(chan_and_pids.channels[curr_channel].transcode_handle);
#endif
    if(chan_and_pids.channels[curr_channel].socketOut>0)
      close (chan_and_pids.channels[curr_channel].socketOut);
    if(chan_and_pids.channels[curr_channel].socketIn>0)
      close (chan_and_pids.channels[curr_channel].socketIn); 
      //Free the channel structures
    if(chan_and_pids.channels[curr_channel].pmt_packet)
      free(chan_and_pids.channels[curr_channel].pmt_packet);
    chan_and_pids.channels[curr_channel].pmt_packet=NULL;
  }

#ifdef ENABLE_TRANSCODING
    /* End transcoding and clear transcode options */
    for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
    {
        transcode_wait_thread_end(chan_and_pids.channels[curr_channel].transcode_handle);
        free_transcode_options(&chan_and_pids.channels[curr_channel].transcode_options);
    }
    free_transcode_options(&global_transcode_opt);
#endif
  // we close the file descriptors
  close_card_fd (fds);

  //We close the unicast connections and free the clients
  unicast_freeing(unicast_vars, chan_and_pids.channels);

#ifdef ENABLE_CAM_SUPPORT
  if(cam_vars->cam_support)
  {
    // stop CAM operation
    cam_stop(cam_vars);
    // delete cam_info file
    if (remove (cam_vars->filename_cam_info))
    {
      log_message( log_module,  MSG_WARN,
                   "%s: %s\n",
                   cam_vars->filename_cam_info, strerror (errno));
    }
  }
#endif

  //autoconf variables freeing
  autoconf_freeing(&autoconf_vars);

  //sap variables freeing
  if(monitor_thread_params && monitor_thread_params->sap_vars->sap_messages)
    free(monitor_thread_params->sap_vars->sap_messages);

  //Pat rewrite freeing
  if(rewrite_vars.full_pat)
    free(rewrite_vars.full_pat);

  //SDT rewrite freeing
  if(rewrite_vars.full_sdt)
    free(rewrite_vars.full_sdt);

  if (strlen(filename_channels_diff) && (write_streamed_channels)&&remove (filename_channels_diff)) 
  {
    log_message( log_module,  MSG_WARN,
                 "%s: %s\n",
                 filename_channels_diff, strerror (errno));
    exit(ERROR_DEL_FILE);
  }

  if (strlen(filename_channels_not_streamed) && (write_streamed_channels)&&remove (filename_channels_not_streamed))
  {
    log_message( log_module,  MSG_WARN,
                 "%s: %s\n",
                 filename_channels_not_streamed, strerror (errno));
    exit(ERROR_DEL_FILE);
  }


  if (!no_daemon)
  {
    if (remove (filename_pid))
    {
      log_message( log_module,  MSG_INFO, "%s: %s\n",
                   filename_pid, strerror (errno));
      exit(ERROR_DEL_FILE);
    }
  }

  if(log_params.log_file)
  {
    fclose(log_params.log_file);
    free(log_params.log_file_path);
  }

  /*free the file descriptors*/
  if(fds.pfds)
    free(fds.pfds);
  fds.pfds=NULL;
  if(unicast_vars->fd_info)
    free(unicast_vars->fd_info);
  unicast_vars->fd_info=NULL;

  if(log_params.log_header!=NULL)
      free(log_params.log_header);
//   plop if(temp_buffer_from_dvr)
//       free(temp_buffer_from_dvr);

  if(Interrupted<(1<<8))
    return (0);
  else
    return(Interrupted>>8);
}

/******************************************************
 * Signal Handler Function
 *
 * This function is called periodically 
 *  It checks for the tuning timeouts
 *
 * This function also catches SIGPIPE, SIGUSR1, SIGUSR2 and SIGHUP
 *
 ******************************************************/
static void SignalHandler (int signum)
{
  if (signum == SIGALRM && !Interrupted)
  {
    struct timeval tv;
    gettimeofday (&tv, (struct timezone *) NULL);
    now = tv.tv_sec - real_start_time;
    if (card_tuned && !*card_tuned)
    {
      log_message( log_module,  MSG_INFO,
                   "Card not tuned after timeout - exiting\n");
      exit(ERROR_TUNE);
    }
    alarm (ALARM_TIME);
  }
  else if (signum == SIGUSR1)
  {
    received_signal=SIGUSR1;
  }
  else if (signum == SIGUSR2)
  {
    received_signal=SIGUSR2;
  }
  else if (signum == SIGHUP)
  {
    received_signal=signum;
  }
  else if (signum != SIGPIPE)
  {
    Interrupted = signum;
  }
  signal (signum, SignalHandler);
}





void *monitor_func(void* arg)
{
  monitor_parameters_t  *params;
  params= (monitor_parameters_t  *) arg;
  int i,curr_channel;
  struct timeval tv;
  double monitor_now;
  double monitor_start;
  double last_updown_check=0;
  double last_flush_time = 0;
  double time_no_diff=0;
  int num_big_buffer_show=0;

  gettimeofday (&tv, (struct timezone *) NULL);
  monitor_start = tv.tv_sec + tv.tv_usec/1000000;

  while(!params->threadshutdown)
  {
    gettimeofday (&tv, (struct timezone *) NULL);
    monitor_now =  tv.tv_sec + tv.tv_usec/1000000 -monitor_start;
    now = tv.tv_sec - real_start_time;

    /*******************************************/
    /* We deal with the received signals       */
    /*******************************************/
    if (received_signal == SIGUSR1) //Display signal strength
    {
      params->tuneparams->display_strenght = params->tuneparams->display_strenght ? 0 : 1;
      received_signal = 0;
    }
    else if (received_signal == SIGUSR2) //Display traffic
    {
      params->stats_infos->show_traffic = params->stats_infos->show_traffic ? 0 : 1;
      if(params->stats_infos->show_traffic)
        log_message( log_module, MSG_INFO,"The traffic will be shown every %d seconds\n",params->stats_infos->show_traffic_interval);
      else
        log_message( log_module, MSG_INFO,"The traffic will not be shown anymore\n");
      received_signal = 0;
    }
    else if (received_signal == SIGHUP) //Sync logs
    {
      log_message( log_module, MSG_DEBUG,"Syncing logs\n");
      sync_logs();
      received_signal = 0;
    }

    /*autoconfiguration*/
    /*We check if we reached the autoconfiguration timeout*/
    if(params->autoconf_vars->autoconfiguration)
    {
      int iRet;
      iRet = autoconf_poll(now, params->autoconf_vars, params->chan_and_pids, params->tuneparams, params->multicast_vars, &fds, params->unicast_vars, params->server_id);
      if(iRet)
        Interrupted = iRet;
    }

    if(!params->autoconf_vars->autoconfiguration)
    {
      /*we are not doing autoconfiguration we can do something else*/
      /*sap announces*/
      sap_poll(params->sap_vars,params->chan_and_pids->number_of_channels,params->chan_and_pids->channels,*params->multicast_vars, (long)monitor_now);



    /*******************************************/
    /* compute the bandwith occupied by        */
    /* each channel                            */
    /*******************************************/
    float time_interval;
    if(!params->stats_infos->compute_traffic_time)
      params->stats_infos->compute_traffic_time=monitor_now;
    if((monitor_now-params->stats_infos->compute_traffic_time)>=params->stats_infos->compute_traffic_interval)
    {
      time_interval=monitor_now-params->stats_infos->compute_traffic_time;
      params->stats_infos->compute_traffic_time=monitor_now;
      for (curr_channel = 0; curr_channel < params->chan_and_pids->number_of_channels; curr_channel++)
      {
        params->chan_and_pids->channels[curr_channel].traffic=((float)params->chan_and_pids->channels[curr_channel].sent_data)/time_interval*1/1000;
        params->chan_and_pids->channels[curr_channel].sent_data=0;
      }
    }

    /*******************************************/
    /*show the bandwith measurement            */
    /*******************************************/
    if(params->stats_infos->show_traffic)
    {
      show_traffic(log_module,monitor_now, params->stats_infos->show_traffic_interval, params->chan_and_pids);
    }


    /*******************************************/
    /* Show the statistics for the big buffer  */
    /*******************************************/
    if(params->stats_infos->show_buffer_stats)
    {
      if(!params->stats_infos->show_buffer_stats_time)
        params->stats_infos->show_buffer_stats_time=monitor_now;
      if((monitor_now-params->stats_infos->show_buffer_stats_time)>=params->stats_infos->show_buffer_stats_interval)
      {
        params->stats_infos->show_buffer_stats_time=monitor_now;
        log_message( log_module,  MSG_DETAIL, "Average packets in the buffer %d\n", params->stats_infos->stats_num_packets_received/params->stats_infos->stats_num_reads);
        params->stats_infos->stats_num_packets_received=0;
        params->stats_infos->stats_num_reads=0;
        num_big_buffer_show++;
        if(num_big_buffer_show==10)
          params->stats_infos->show_buffer_stats=0;
      }
    }

    /*******************************************/
    /* Periodically flush the logs if asked  */
    /*******************************************/
    if((log_params.log_file) && (log_params.log_flush_interval !=-1))
    {
      if(!last_flush_time)
      {
        last_flush_time=monitor_now;
        fflush(log_params.log_file);
      }
      if((monitor_now-last_flush_time)>=log_params.log_flush_interval)
      {
        log_message( log_module,  MSG_FLOOD, "Flushing logs\n");
        fflush(log_params.log_file);
        last_flush_time=monitor_now;
      }
    }

    /*******************************************/
    /* Check if the chanel scrambling state    */
    /* has changed                             */
    /*******************************************/
    // Current thresholds for calculation
    // (<2%) FULLY_UNSCRAMBLED
    // (5%<=ratio<=75%) PARTIALLY_UNSCRAMBLED
    // (>80%) HIGHLY_SCRAMBLED
    // The gap is an hysteresis to avoid excessive jumping between states
    for (curr_channel = 0; curr_channel < params->chan_and_pids->number_of_channels; curr_channel++)
    {
      mumudvb_channel_t *current;
      current=&params->chan_and_pids->channels[curr_channel];
      /* Calcultation of the ratio (percentage) of scrambled packets received*/
      if (current->num_packet >0 && current->num_scrambled_packets>10)
        current->ratio_scrambled = (int)(current->num_scrambled_packets*100/(current->num_packet));
      else
        current->ratio_scrambled = 0;

      /* Test if we have only unscrambled packets (<2%) - scrambled_channel=FULLY_UNSCRAMBLED : fully unscrambled*/
      if ((current->ratio_scrambled < 2) && (current->scrambled_channel != FULLY_UNSCRAMBLED))
      {
        log_message( log_module,  MSG_INFO,
                      "Channel \"%s\" is now fully unscrambled (%d%% of scrambled packets). Card %d\n",
                      current->name, current->ratio_scrambled, params->tuneparams->card);
        current->scrambled_channel = FULLY_UNSCRAMBLED;// update
      }
      /* Test if we have partiallay unscrambled packets (5%<=ratio<=75%) - scrambled_channel=PARTIALLY_UNSCRAMBLED : partially unscrambled*/
      if ((current->ratio_scrambled >= 5) && (current->ratio_scrambled <= 75) && (current->scrambled_channel != PARTIALLY_UNSCRAMBLED))
      {
        log_message( log_module,  MSG_INFO,
                      "Channel \"%s\" is now partially unscrambled (%d%% of scrambled packets). Card %d\n",
                      current->name, current->ratio_scrambled, params->tuneparams->card);
        current->scrambled_channel = PARTIALLY_UNSCRAMBLED;// update
      }
      /* Test if we have nearly only scrambled packets (>80%) - scrambled_channel=HIGHLY_SCRAMBLED : highly scrambled*/
      if ((current->ratio_scrambled > 80) && current->scrambled_channel != HIGHLY_SCRAMBLED)
      {
        log_message( log_module,  MSG_INFO,
                      "Channel \"%s\" is now higly scrambled (%d%% of scrambled packets). Card %d\n",
                      current->name, current->ratio_scrambled, params->tuneparams->card);
        current->scrambled_channel = HIGHLY_SCRAMBLED;// update
      }
    }







    /*******************************************/
    /* Check if the channel stream state       */
    /* has changed                             */
    /*******************************************/
    if(last_updown_check)
    {
      /* Check if the channel stream state has changed*/
      for (curr_channel = 0; curr_channel < params->chan_and_pids->number_of_channels; curr_channel++)
      {
        mumudvb_channel_t *current;
        current=&params->chan_and_pids->channels[curr_channel];
        double packets_per_sec;
        int num_scrambled;
        if(params->chan_and_pids->dont_send_scrambled)
          num_scrambled=current->num_scrambled_packets;
        else
          num_scrambled=0;
        packets_per_sec=((double)current->num_packet-num_scrambled)/(monitor_now-last_updown_check);
        if( params->stats_infos->debug_updown)
        {
          log_message( log_module,  MSG_FLOOD,
                      "Channel \"%s\" streamed_channel %f packets/s\n",
                      current->name,packets_per_sec);
        }
        if ((packets_per_sec >= params->stats_infos->up_threshold) && (!current->streamed_channel))
        {
          log_message( log_module,  MSG_INFO,
                      "Channel \"%s\" back.Card %d\n",
                      current->name, params->tuneparams->card);
          current->streamed_channel = 1;  // update
          if(params->sap_vars->sap == OPTION_ON)
            sap_update(&params->chan_and_pids->channels[curr_channel], params->sap_vars, curr_channel, *params->multicast_vars); //Channel status changed, we update the sap announces
        }
        else if ((current->streamed_channel) && (packets_per_sec < params->stats_infos->down_threshold))
        {
          log_message( log_module,  MSG_INFO,
                      "Channel \"%s\" down.Card %d\n",
                      current->name, params->tuneparams->card);
          current->streamed_channel = 0;  // update
          if(params->sap_vars->sap == OPTION_ON)
            sap_update(&params->chan_and_pids->channels[curr_channel], params->sap_vars, curr_channel, *params->multicast_vars); //Channel status changed, we update the sap announces
        }
      }
    }
    /* reinit */
    for (curr_channel = 0; curr_channel < params->chan_and_pids->number_of_channels; curr_channel++)
    {
      params->chan_and_pids->channels[curr_channel].num_packet = 0;
      params->chan_and_pids->channels[curr_channel].num_scrambled_packets = 0;
    }
    last_updown_check=monitor_now;





    /*******************************************/
    /* we count active channels                */
    /*******************************************/
    int count_of_active_channels=0;
    for (curr_channel = 0; curr_channel < params->chan_and_pids->number_of_channels; curr_channel++)
      if (params->chan_and_pids->channels[curr_channel].streamed_channel)
        count_of_active_channels++;

    /*Time no diff is the time when we got 0 active channels*/
    /*if we have active channels, we reinit this counter*/
    if(count_of_active_channels)
      time_no_diff=0;
    /*If we don't have active channels and this is the first time, we store the time*/
    else if(!time_no_diff)
      time_no_diff=(long)monitor_now;


    /*******************************************/
    /* If we don't stream data for             */
    /* a too long time, we exit                */
    /*******************************************/
    if((timeout_no_diff)&& (time_no_diff&&((monitor_now-time_no_diff)>timeout_no_diff)))
    {
      log_message( log_module,  MSG_ERROR,
                  "No data from card %d in %fs, exiting.\n",
                  params->tuneparams->card, timeout_no_diff);
      Interrupted=ERROR_NO_DIFF<<8; //the <<8 is to make difference beetween signals and errors
    }





    /*******************************************/
    /* generation of the file which says       */
    /* the streamed channels                   */
    /*******************************************/
    if (write_streamed_channels)
      gen_file_streamed_channels(params->filename_channels_diff, params->filename_channels_not_streamed, params->chan_and_pids->number_of_channels, params->chan_and_pids->channels);


    }
    for(i=0;i<params->wait_time && !params->threadshutdown;i++)
      usleep(100000);
  }

  log_message(log_module,MSG_DEBUG, "Monitor thread stopping, it lasted %f seconds\n", monitor_now);
  return 0;

}










