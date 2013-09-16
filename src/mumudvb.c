/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2011 Brice DUBOST
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
 * @brief This file is the main file of MuMuDVB
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

#define _GNU_SOURCE		//in order to use program_invocation_short_name and recursive mutexes (GNU extensions)


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
#include <time.h>
#include <linux/dvb/version.h>
#include <sys/mman.h>
#include <pthread.h>

#include "mumudvb.h"
#include "tune.h"
#include "network.h"
#include "dvb.h"
#ifdef ENABLE_CAM_SUPPORT
#include "cam.h"
#endif
#ifdef ENABLE_SCAM_SUPPORT
#include "scam_capmt.h"
#include "scam_common.h"
#include "scam_getcw.h"
#include "scam_decsa.h"
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

#ifdef __UCLIBC__
#define program_invocation_short_name "mumudvb"
#else
extern char *program_invocation_short_name;
#endif

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
int  write_streamed_channels=1;
pthread_t signalpowerthread;
pthread_t cardthread;
pthread_t monitorthread;
card_thread_parameters_t cardthreadparams;


mumudvb_chan_and_pids_t chan_and_pids={
  .lock=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
  .number_of_channels=0,
  .dont_send_scrambled=0,
  .filter_transport_error=0,
  .psi_tables_filtering=PSI_TABLES_FILTERING_NONE,
  .check_cc=0,
};


//multicast parameters
multicast_parameters_t multicast_vars={
  .multicast=1,
  .multicast_ipv6=0,
  .multicast_ipv4=1,
  .ttl=DEFAULT_TTL,
  .common_port = 1234,
  .auto_join=0,
  .rtp_header = 0,
  .iface4="\0",
  .iface6="\0",
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
	.flush_on_eagain=0,
};




//autoconfiguration
autoconf_parameters_t autoconf_vars={
  .lock=PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
  .autoconfiguration=0,
  .autoconf_radios=0,
  .autoconf_scrambled=0,
  .autoconf_pid_update=1,
  .autoconf_ip4="239.100.%card.%number",
  .autoconf_ip6="FF15:4242::%server:%card:%number",
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
  .rewrite_eit=OPTION_UNDEFINED,
  .eit_version=-1,
  .full_eit=NULL,
  .eit_needs_update=0,
  .sdt_force_eit=OPTION_UNDEFINED,
  .eit_packets=NULL,
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
int mumudvb_close(monitor_parameters_t* monitor_thread_params, unicast_parameters_t* unicast_vars, volatile int* strengththreadshutdown, void *cam_vars_v, void *scam_vars_v, char* filename_channels_not_streamed,char *filename_channels_streamed, char *filename_pid, int Interrupted);



int
    main (int argc, char **argv)
{
  //sap announces
  sap_parameters_t sap_vars={
    .sap_messages4=NULL,
    .sap_messages6=NULL,
    .sap=OPTION_UNDEFINED, //No sap by default
    .sap_interval=SAP_DEFAULT_INTERVAL,
    .sap_sending_ip4="0.0.0.0",
    .sap_sending_ip6="::",
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


  //tuning parameters
  tuning_parameters_t tuneparams={
    .card = 0,
    .tuner = 0,
    .card_dev_path=DVB_DEV_PATH,
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
#if STREAM_ID
    .stream_id=0,
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
    .cam_mmi_autoresponse=1,
    .cam_pmt_follow=1,
    .cam_menulist_str = EMPTY_STRING,
    .cam_menu_string = EMPTY_STRING,
  };
  mumu_string_append(&cam_vars.cam_menu_string,"Not retrieved");
  cam_parameters_t *cam_vars_ptr=&cam_vars;
  #else
  void *cam_vars_ptr=NULL;
  #endif


  #ifdef ENABLE_SCAM_SUPPORT
  //SCAM (software conditionnal Access Modules : for scrambled channels)
  scam_parameters_t scam_vars={
	  .scam_support = 0,
	  .getcwthread_shutdown=0,
  };
  scam_parameters_t *scam_vars_ptr=&scam_vars;
  int scam_threads_started=0;
  #else
  void *scam_vars_ptr=NULL;
  #endif

  char filename_channels_not_streamed[DEFAULT_PATH_LEN];
  char filename_channels_streamed[DEFAULT_PATH_LEN];
  char filename_pid[DEFAULT_PATH_LEN]=PIDFILE_PATH;

  int server_id = 0; /** The server id for the template %server */

  int iRet,cmdlinecard;
  cmdlinecard=-1;

  //MPEG2-TS reception and sort
  int pid;			/** pid of the current mpeg2 packet */
  int ScramblingControl;
  int continuity_counter;

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
  char *dump_filename = NULL;
  FILE *dump_file;

  // configuration file parsing
  int curr_channel = 0;
  int curr_pid = 0;
  int send_packet=0;
  int channel_start = 0;
  char current_line[CONF_LINELEN];
  char *substring=NULL;
  char delimiteurs[] = CONFIG_FILE_SEPARATOR;

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
    {"dumpfile", required_argument, NULL, 'z'},
    {0, 0, 0, 0}
  };
  int c, option_index = 0;
  int listingcards=0;
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
	listingcards=1;
        break;
      case 'z':
        dump_filename = (char *) malloc (strlen (optarg) + 1);
        if (!dump_filename)
        {
          log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
          exit(ERROR_MEMORY);
        }
        strncpy (dump_filename, optarg, strlen (optarg) + 1);
        log_message( log_module, MSG_WARN,"You've decided to dump the received stream into %s. Be warned, it can grow quite fast", dump_filename);
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

  if(listingcards)
    {
      print_info ();
      list_dvb_cards ();
      exit(0);
    }

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
#ifdef ENABLE_SCAM_SUPPORT
  for (int i = 0; i < MAX_CHANNELS; ++i)
    pthread_mutex_init(&chan_and_pids.channels[i].cw_lock, NULL);
#endif


  /******************************************************/
  // config file displaying
  /******************************************************/
  conf_file = fopen (conf_filename, "r");
  if (conf_file == NULL)
  {
    log_message( log_module,  MSG_ERROR, "%s: %s\n",
                 conf_filename, strerror (errno));
    free(conf_filename);
    exit(ERROR_CONF_FILE);
  }
  log_message( log_module, MSG_FLOOD,"==== Configuration file ====");
  int line_num=1;
   while (fgets (current_line, CONF_LINELEN, conf_file))
  {
    int line_len;
    //We suppress the end of line
    line_len=strlen(current_line);
    if(current_line[line_len-1]=='\r' ||current_line[line_len-1]=='\n')
      current_line[line_len-1]=0;
    log_message( log_module, MSG_FLOOD,"%03d %s\n",line_num,current_line);
    line_num++;
  }
  log_message( log_module, MSG_FLOOD,"============ done ===========\n");
  fclose (conf_file);
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
    //Thanks to Pierre Gronlier pierre.gronlier at gmail.com for finding that bug
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
    else if((iRet=read_cam_configuration(cam_vars_ptr, &chan_and_pids.channels[curr_channel], channel_start, substring))) //Read the line concerning the cam parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
#endif
#ifdef ENABLE_SCAM_SUPPORT
    else if((iRet=read_scam_configuration(scam_vars_ptr, &chan_and_pids.channels[curr_channel], channel_start, substring))) //Read the line concerning the cam parameters
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
    else if (!strcmp (substring, "filter_transport_error"))
    {
      substring = strtok (NULL, delimiteurs);
      chan_and_pids.filter_transport_error = atoi (substring);
    }
    else if (!strcmp (substring, "psi_tables_filtering"))
    {
      substring = strtok (NULL, delimiteurs);
      if (!strcmp (substring, "pat"))
        chan_and_pids.psi_tables_filtering = PSI_TABLES_FILTERING_PAT_ONLY;
      else if (!strcmp (substring, "pat_cat"))
        chan_and_pids.psi_tables_filtering = PSI_TABLES_FILTERING_PAT_CAT_ONLY;
      else if (!strcmp (substring, "none"))
        chan_and_pids.psi_tables_filtering = PSI_TABLES_FILTERING_NONE;
      if (chan_and_pids.psi_tables_filtering == PSI_TABLES_FILTERING_PAT_ONLY)
        log_message( log_module,  MSG_INFO, "You have enabled PSI tables filtering, only PAT will be send\n");
      if (chan_and_pids.psi_tables_filtering == PSI_TABLES_FILTERING_PAT_CAT_ONLY)
        log_message( log_module,  MSG_INFO, "You have enabled PSI tables filtering, only PAT and CAT will be send\n");
    }
    else if (!strcmp (substring, "dvr_buffer_size"))
    {
      substring = strtok (NULL, delimiteurs);
      card_buffer.dvr_buffer_size = atoi (substring);
      if(card_buffer.dvr_buffer_size<=0)
      {
        log_message( log_module,  MSG_WARN,
                     "The buffer size MUST be >0, forced to 1 packet\n");
        card_buffer.dvr_buffer_size = 1;
      }
      stats_infos.show_buffer_stats=1;
    }
    else if (!strcmp (substring, "dvr_thread"))
    {
      substring = strtok (NULL, delimiteurs);
      card_buffer.threaded_read = atoi (substring);
      if(card_buffer.threaded_read)
      {
        log_message( log_module,  MSG_WARN,
                     "You want to use a thread for reading the card, please report bugs/problems\n");
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
        log_message( log_module,  MSG_WARN, "The option ts_id is depreciated, use service_id instead.\n");
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
   else if (!strcmp (substring, "check_cc"))
    {
      substring = strtok (NULL, delimiteurs);
      chan_and_pids.check_cc = atoi (substring);
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
                   "Full autoconfiguration, we activate SAP announces. if you want to deactivate them see the README.\n");
      sap_vars.sap=OPTION_ON;
    }
    if(rewrite_vars.rewrite_pat == OPTION_UNDEFINED)
    {
      rewrite_vars.rewrite_pat=OPTION_ON;
      log_message( log_module,  MSG_INFO,
                   "Full autoconfiguration, we activate PAT rewriting. if you want to disable it see the README.\n");
    }
    if(rewrite_vars.rewrite_sdt == OPTION_UNDEFINED)
    {
      rewrite_vars.rewrite_sdt=OPTION_ON;
      log_message( log_module,  MSG_INFO,
                   "Full autoconfiguration, we activate SDT rewriting. if you want to disable it see the README.\n");
    }
    if(rewrite_vars.rewrite_eit == OPTION_UNDEFINED)
    {
      rewrite_vars.rewrite_eit=OPTION_ON;
      log_message( log_module,  MSG_INFO,
                   "Full autoconfiguration, we activate EIT rewriting. if you want to disable it see the README.\n");
    }
  }
  if(card_buffer.max_thread_buffer_size<card_buffer.dvr_buffer_size)
  {
    log_message( log_module,  MSG_WARN,
		 "Warning : You set a thread buffer size lower than your DVR buffer size, it's not possible to use such values. I increase your dvr_thread_buffer_size ...\n");
		 card_buffer.max_thread_buffer_size=card_buffer.dvr_buffer_size;
  }

  //If we specified a card number on the command line, it overrides the config file
  if(cmdlinecard!=-1)
    tuneparams.card=cmdlinecard;

  //Template for the card dev path
  char number[10];
  sprintf(number,"%d",tuneparams.card);
  int l=sizeof(tuneparams.card_dev_path);
  mumu_string_replace(tuneparams.card_dev_path,&l,0,"%card",number);

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

  // Show in log that we are starting
  log_message( log_module,  MSG_INFO,"========== End of configuration, MuMuDVB version %s is starting ==========",VERSION);

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

  //We deactivate things depending on multicast if multicast is suppressed
  if(!multicast_vars.ttl)
  {
    log_message( log_module,  MSG_INFO, "The multicast TTL is set to 0, multicast will be disabled.\n");
    multicast_vars.multicast=0;
  }
  if(!multicast_vars.multicast)
  {
#ifdef ENABLE_TRANSCODING
    for (curr_channel = 0; curr_channel < MAX_CHANNELS; curr_channel++)
    {
      if(chan_and_pids.channels[curr_channel].transcode_options.enable)
      {
	log_message( log_module,  MSG_INFO, "NO Multicast, transcoding disabled for channel \"%s\".\n", chan_and_pids.channels[curr_channel].name);
	chan_and_pids.channels[curr_channel].transcode_options.enable=0;
      }
    }
#endif
      if(multicast_vars.rtp_header)
      {
	multicast_vars.rtp_header=0;
	log_message( log_module,  MSG_INFO, "NO Multicast, RTP Header is disabled.\n");
      }
      if(sap_vars.sap==OPTION_ON)
      {
	log_message( log_module,  MSG_INFO, "NO Multicast, SAP announces are disabled.\n");
	sap_vars.sap=OPTION_OFF;
      }
  }
  free(conf_filename);
  if(!multicast_vars.multicast && !unicast_vars.unicast)
  {
    log_message( log_module,  MSG_ERROR, "NO Multicast AND NO unicast. No data can be send :(, Exciting ....\n");
    set_interrupted(ERROR_CONF<<8);
    goto mumudvb_close_goto;
  }



  // we clear them by paranoia
  sprintf (filename_channels_streamed, STREAMED_LIST_PATH,
           tuneparams.card, tuneparams.tuner);
  sprintf (filename_channels_not_streamed, NOT_STREAMED_LIST_PATH,
           tuneparams.card, tuneparams.tuner);
#ifdef ENABLE_CAM_SUPPORT
  sprintf (cam_vars.filename_cam_info, CAM_INFO_LIST_PATH,
           tuneparams.card, tuneparams.tuner);
#endif
  channels_diff = fopen (filename_channels_streamed, "w");
  if (channels_diff == NULL)
  {
    write_streamed_channels=0;
    log_message( log_module,  MSG_WARN,
                 "Can't create %s: %s\n",
                 filename_channels_streamed, strerror (errno));
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

  if (open_fe (&fds.fd_frontend, tuneparams.card_dev_path, tuneparams.tuner,1))
  {

  /*****************************************************/
  //daemon part two, we write our PID as we are tuned
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
    log_message( log_module, MSG_INFO, "The pid will be written in %s", filename_pid);
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


    iRet =
        tune_it (fds.fd_frontend, &tuneparams);
  }

  if (iRet < 0)
  {
    log_message( log_module,  MSG_INFO, "Tunning issue, card %d\n", tuneparams.card);
    // we close the file descriptors
    close_card_fd (fds);
    set_interrupted(ERROR_TUNE<<8);
    goto mumudvb_close_goto;
  }
  log_message( log_module,  MSG_INFO, "Card %d, tuner %d tuned\n", tuneparams.card, tuneparams.tuner);
  tuneparams.card_tuned = 1;

  //Thread for showing the strength
  strength_parameters_t strengthparams;
  strengthparams.fds = &fds;
  strengthparams.tuneparams = &tuneparams;
  pthread_create(&(signalpowerthread), NULL, show_power_func, &strengthparams);
  //Thread for reading from the DVB card initialization
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
#ifdef ENABLE_SCAM_SUPPORT
	.scam_vars_v=scam_vars_ptr,
#endif
    .server_id=server_id,
    .filename_channels_not_streamed=filename_channels_not_streamed,
    .filename_channels_streamed=filename_channels_streamed,
  };

  pthread_create(&(monitorthread), NULL, monitor_func, &monitor_thread_params);

  /*****************************************************/
  //scam_support
  /*****************************************************/

#ifdef ENABLE_SCAM_SUPPORT
   if(scam_vars.scam_support){
	if(scam_getcw_start(scam_vars_ptr,tuneparams.card))
    {
      log_message("SCAM_GETCW: ", MSG_ERROR,"Cannot initalise scam\n");
      scam_vars.scam_support=0;
    }
    else
    {
      //If the scam is properly initialised, we autoconfigure scrambled channels
      autoconf_vars.autoconf_scrambled=1;
    }
   }
#endif

  /*****************************************************/
  //cam_support
  /*****************************************************/

#ifdef ENABLE_CAM_SUPPORT
  if(cam_vars.cam_support){
    //We initialise the cam. If fail, we remove cam support
    if(cam_start(cam_vars_ptr,tuneparams.card))
    {
      log_message("CAM: ", MSG_ERROR,"Cannot initalise cam\n");
      cam_vars.cam_support=0;
    }
    else
    {
      //If the cam is properly initialised, we autoconfigure scrambled channels
      autoconf_vars.autoconf_scrambled=1;
    }
    for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
    {
      //We allocate the packet for storing the PMT for CAM purposes
      if(chan_and_pids.channels[curr_channel].cam_pmt_packet==NULL)
      {
        chan_and_pids.channels[curr_channel].cam_pmt_packet=malloc(sizeof(mumudvb_ts_packet_t));
        if(chan_and_pids.channels[curr_channel].cam_pmt_packet==NULL)
        {
          log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
          set_interrupted(ERROR_MEMORY<<8);
          goto mumudvb_close_goto;
        }
        memset (chan_and_pids.channels[curr_channel].cam_pmt_packet, 0, sizeof( mumudvb_ts_packet_t));//we clear it
        pthread_mutex_init(&chan_and_pids.channels[curr_channel].cam_pmt_packet->packetmutex,NULL);
      }
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
    set_interrupted(ERROR_GENERIC<<8);
    goto mumudvb_close_goto;
  }

#ifdef ENABLE_SCAM_SUPPORT
  /*****************************************************/
  //scam
  /*****************************************************/
  iRet=scam_init_no_autoconf(&autoconf_vars, scam_vars_ptr, chan_and_pids.channels,chan_and_pids.number_of_channels);
  if(iRet)
  {
    set_interrupted(ERROR_GENERIC<<8);
    goto mumudvb_close_goto;
  }

#endif
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
      set_interrupted(ERROR_MEMORY<<8);
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
      set_interrupted(ERROR_MEMORY<<8);
      goto mumudvb_close_goto;
    }
    memset (rewrite_vars.full_sdt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
    pthread_mutex_init(&rewrite_vars.full_sdt->packetmutex,NULL);
  }

  /*****************************************************/
  //EIT rewriting
  //memory allocation for MPEG2-TS
  //packet structures
  /*****************************************************/

  if(rewrite_vars.rewrite_eit == OPTION_ON)
  {
    rewrite_vars.full_eit=malloc(sizeof(mumudvb_ts_packet_t));
    if(rewrite_vars.full_eit==NULL)
    {
      log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      set_interrupted(ERROR_MEMORY<<8);
      goto mumudvb_close_goto;
    }
    memset (rewrite_vars.full_eit, 0, sizeof( mumudvb_ts_packet_t));//we clear it
    pthread_mutex_init(&rewrite_vars.full_eit->packetmutex,NULL);
  }

  /*****************************************************/
  //Some initialisations
  /*****************************************************/
  if(multicast_vars.rtp_header)
	multicast_vars.num_pack=(MAX_UDP_SIZE-TS_PACKET_SIZE)/TS_PACKET_SIZE;
  else
	multicast_vars.num_pack=(MAX_UDP_SIZE)/TS_PACKET_SIZE;

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
    /** @todo : allocate only if autoconf */
    if(chan_and_pids.channels[curr_channel].pmt_packet==NULL)
    {
      chan_and_pids.channels[curr_channel].pmt_packet=malloc(sizeof(mumudvb_ts_packet_t));
      if(chan_and_pids.channels[curr_channel].pmt_packet==NULL)
      {
        log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      set_interrupted(ERROR_MEMORY<<8);
      goto mumudvb_close_goto;
      }
      memset (chan_and_pids.channels[curr_channel].pmt_packet, 0, sizeof( mumudvb_ts_packet_t));//we clear it
      pthread_mutex_init(&chan_and_pids.channels[curr_channel].pmt_packet->packetmutex,NULL);

    }

  }

  //We initialise asked pid table
  memset (chan_and_pids.asked_pid, 0, sizeof( uint8_t)*8193);//we clear it
  memset (chan_and_pids.number_chan_asked_pid, 0, sizeof( uint8_t)*8193);//we clear it

  // We initialize the table for checking the TS discontinuities
  for (curr_pid = 0; curr_pid < 8193; curr_pid++)
    chan_and_pids.continuity_counter_pid[curr_pid]=-1;

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
    set_interrupted(ERROR_GENERIC<<8);
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
    set_interrupted(ERROR_MEMORY<<8);
    goto mumudvb_close_goto;
  }

  //We fill the file descriptor information structure. the first one is irrelevant
  //(file descriptor associated to the DVB card) but we allocate it for consistency
  unicast_vars.fd_info=NULL;
  unicast_vars.fd_info=realloc(unicast_vars.fd_info,(fds.pfdsnum)*sizeof(unicast_fd_info_t));
  if (unicast_vars.fd_info==NULL)
  {
    log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    set_interrupted(ERROR_MEMORY<<8);
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
      if(multicast_vars.multicast_ipv4)
	{
	  //See the README for the reason of this option
	  if(multicast_vars.auto_join)
	    chan_and_pids.channels[curr_channel].socketOut4 = makeclientsocket (chan_and_pids.channels[curr_channel].ip4Out, chan_and_pids.channels[curr_channel].portOut, multicast_vars.ttl, multicast_vars.iface4, &chan_and_pids.channels[curr_channel].sOut4);
	  else
	    chan_and_pids.channels[curr_channel].socketOut4 = makesocket (chan_and_pids.channels[curr_channel].ip4Out, chan_and_pids.channels[curr_channel].portOut, multicast_vars.ttl, multicast_vars.iface4, &chan_and_pids.channels[curr_channel].sOut4);
	}
      if(multicast_vars.multicast_ipv6)
	{
	  //See the README for the reason of this option
	  if(multicast_vars.auto_join)
	    chan_and_pids.channels[curr_channel].socketOut6 = makeclientsocket6 (chan_and_pids.channels[curr_channel].ip6Out, chan_and_pids.channels[curr_channel].portOut, multicast_vars.ttl, multicast_vars.iface6, &chan_and_pids.channels[curr_channel].sOut6);
	  else
	    chan_and_pids.channels[curr_channel].socketOut6 = makesocket6 (chan_and_pids.channels[curr_channel].ip6Out, chan_and_pids.channels[curr_channel].portOut, multicast_vars.ttl, multicast_vars.iface6, &chan_and_pids.channels[curr_channel].sOut6);
	}
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
    set_interrupted(ERROR_GENERIC<<8);
    goto mumudvb_close_goto;
  }

  /*****************************************************/
  // Information about streamed channels
  /*****************************************************/

  if(autoconf_vars.autoconfiguration!=AUTOCONF_MODE_FULL)
    log_streamed_channels(log_module,
			  chan_and_pids.number_of_channels,
			  chan_and_pids.channels,
			  multicast_vars.multicast_ipv4,
			  multicast_vars.multicast_ipv6,
			  unicast_vars.unicast,
			  unicast_vars.portOut,
			  unicast_vars.ipOut);

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
  //We open the dump file if any
  /******************************************************/
  dump_file = NULL;
  if(dump_filename)
  {
    dump_file = fopen (dump_filename, "w");
    if (dump_file == NULL)
    {
      log_message( log_module,  MSG_ERROR, "%s: %s\n",
                  dump_filename, strerror (errno));
      free(dump_filename);
    }
  }
  mlockall(MCL_CURRENT | MCL_FUTURE);
  /******************************************************/
  //Main loop where we get the packets and send them
  /******************************************************/
  int poll_ret;
  /**Buffer containing one packet*/
  unsigned char *actual_ts_packet;
  while (!get_interrupted())
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
	iRet=unicast_handle_fd_event(&unicast_vars, &fds, chan_and_pids.channels, chan_and_pids.number_of_channels, &strengthparams, &autoconf_vars, cam_vars_ptr, scam_vars_ptr);
	if(iRet)
	{
	  set_interrupted(iRet);
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
	set_interrupted(poll_ret);
	continue;
      }
      /**************************************************************/
      /* UNICAST HTTP                                               */
      /**************************************************************/
      if((!(fds.pfds[0].revents&POLLIN)) && (!(fds.pfds[0].revents&POLLPRI))) //Priority to the DVB packets so if there is dvb packets and something else, we look first to dvb packets
      {
	iRet=unicast_handle_fd_event(&unicast_vars, &fds, chan_and_pids.channels, chan_and_pids.number_of_channels, &strengthparams, &autoconf_vars, cam_vars_ptr, scam_vars_ptr);
	if(iRet)
	  set_interrupted(iRet);
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
      stats_infos.stats_num_packets_received+=(int) card_buffer.bytes_read/TS_PACKET_SIZE;
      stats_infos.stats_num_reads++;
    }

    for(card_buffer.read_buff_pos=0;
	(card_buffer.read_buff_pos+TS_PACKET_SIZE)<=card_buffer.bytes_read;
	card_buffer.read_buff_pos+=TS_PACKET_SIZE)//we loop on the subpackets
    {
      actual_ts_packet=card_buffer.reading_buffer+card_buffer.read_buff_pos;

      //If the user asked to dump the streams it's here tath it should be done
      if(dump_file)
        if(fwrite(actual_ts_packet,sizeof(unsigned char),TS_PACKET_SIZE,dump_file)<TS_PACKET_SIZE)
          log_message( log_module,MSG_WARN,"Error while writing the dump : %s", strerror(errno));

      // Test if the error bit is set in the TS packet received
      if ((actual_ts_packet[1] & 0x80) == 0x80)
      {
            log_message( log_module, MSG_FLOOD,"Error bit set in TS packet!\n");
            // Test if we discard the packets with error bit set
            if (chan_and_pids.filter_transport_error>0) continue;
      }

      // Get the PID of the received TS packet
      pid = ((actual_ts_packet[1] & 0x1f) << 8) | (actual_ts_packet[2]);

      // Check the continuity
      if(chan_and_pids.check_cc)
      {
        continuity_counter=actual_ts_packet[3] & 0x0f;
        if (chan_and_pids.continuity_counter_pid[pid]!=-1 && chan_and_pids.continuity_counter_pid[pid]!=continuity_counter && ((chan_and_pids.continuity_counter_pid[pid]+1) & 0x0f)!=continuity_counter)
          strengthparams.ts_discontinuities++;
        chan_and_pids.continuity_counter_pid[pid]=continuity_counter;
      }

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
        iRet = autoconf_new_packet(pid, actual_ts_packet, &autoconf_vars,  &fds, &chan_and_pids, &tuneparams, &multicast_vars, &unicast_vars, server_id, scam_vars_ptr);
        if(iRet)
          set_interrupted(iRet);
      }
      if(autoconf_vars.autoconfiguration)
        continue;

      /******************************************************/
      //   AUTOCONFIGURATION PART FINISHED
      /******************************************************/
#ifdef ENABLE_SCAM_SUPPORT
      /******************************************************/
      //   SCAM PMT GET PART in case of no autoconf
      /******************************************************/
      if(!ScramblingControl &&  scam_vars.need_pmt_get)
      {
        scam_new_packet(pid, actual_ts_packet, &scam_vars, chan_and_pids.channels);
      }
      if(scam_vars.need_pmt_get)
        continue;

      /******************************************************/
      //   SCAM PMT GET PART FINISHED
      /******************************************************/
      if(!scam_threads_started) {
        for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++) {
          if (chan_and_pids.channels[curr_channel].scam_support && scam_vars.scam_support)
            set_interrupted(scam_channel_start(&chan_and_pids.channels[curr_channel]));
        }
        scam_threads_started=1;
      }
#endif
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
	       pthread_mutex_lock(&chan_and_pids.lock);
	       for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
	         chan_and_pids.channels[curr_channel].sdt_rewrite_skip=0;
	       pthread_mutex_unlock(&chan_and_pids.lock);
	     }
	   }
      /******************************************************/
      //EIT rewrite
      /******************************************************/
		if( (pid == 18) && //This is an EIT PID
			 rewrite_vars.rewrite_eit == OPTION_ON ) //AND we asked for rewrite
	   {
	     eit_rewrite_new_global_packet(actual_ts_packet, &rewrite_vars);
	   }


      /******************************************************/
      //for each channel we'll look if we must send this PID
      /******************************************************/
      pthread_mutex_lock(&chan_and_pids.lock);
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
#ifdef ENABLE_SCAM_SUPPORT
          if(chan_and_pids.dont_send_scrambled && (ScramblingControl>0)&& (pid != chan_and_pids.channels[curr_channel].pmt_pid)&& (!chan_and_pids.channels[curr_channel].scam_support) && (!scam_vars.scam_support))
            send_packet=0;
#else
          if(chan_and_pids.dont_send_scrambled && (ScramblingControl>0)&& (pid != chan_and_pids.channels[curr_channel].pmt_pid) )
            send_packet=0;
#endif
          break;
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
          if(cam_new_packet(pid, curr_channel, actual_ts_packet, cam_vars_ptr, &chan_and_pids.channels[curr_channel]))
            cam_vars.cam_pmt_send_time=now; //A packet was sent to the CAM
        }
#endif

        /******************************************************/
	//scam support
	// sending capmt to oscam
        /******************************************************/
#ifdef ENABLE_SCAM_SUPPORT
		if (scam_vars.scam_support &&(chan_and_pids.channels[curr_channel].need_scam_ask==CAM_NEED_ASK))
		{ 
				if (chan_and_pids.channels[curr_channel].scam_support && chan_and_pids.channels[curr_channel].pmt_packet->len_full != 0 ) {								
					  iRet=scam_send_capmt(&chan_and_pids.channels[curr_channel],tuneparams.card);
					  if(iRet)
					  {
						set_interrupted(ERROR_GENERIC<<8);
						goto mumudvb_close_goto;
					  }
				}
				chan_and_pids.channels[curr_channel].need_scam_ask=CAM_ASKED;
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
        //PMT follow for the cam for  non autoconfigurated channels.
        // This is a PMT update forced for the CAM in case of no autoconfiguration
        /******************************************************/
#ifdef ENABLE_CAM_SUPPORT
        if((cam_vars.cam_pmt_follow) &&
           (chan_and_pids.channels[curr_channel].need_cam_ask==CAM_ASKED) &&
           (send_packet==1) && //no need to check paquets we don't send
           (!chan_and_pids.channels[curr_channel].autoconfigurated) && //the check is for the non autoconfigurated channels
           (chan_and_pids.channels[curr_channel].pmt_pid==pid) &&     //And we see the PMT
            pid)
        {
          cam_pmt_follow( actual_ts_packet, &chan_and_pids.channels[curr_channel] );
        }
#endif
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
        //Rewrite EIT
        /******************************************************/
        if((send_packet==1) &&//no need to check paquets we don't send
           (pid == 18) && //This is a EIT PID
            (chan_and_pids.channels[curr_channel].service_id) && //we have the service_id
            rewrite_vars.rewrite_eit == OPTION_ON) //AND we asked for EIT sorting
        {
          eit_rewrite_new_channel_packet(actual_ts_packet, &rewrite_vars, &chan_and_pids.channels[curr_channel],
        		  &multicast_vars, &unicast_vars, scam_vars_ptr ,&chan_and_pids,&fds);
          send_packet=0; //for EIT it is sent by the rewrite function itself
        }

        /******************************************************/
        // Test if PSI tables filtering is activated
        /******************************************************/
        if (send_packet==1 && chan_and_pids.psi_tables_filtering>0 && pid<32)
        {
           // Keep only PAT and CAT
           if (chan_and_pids.psi_tables_filtering==1 && pid>1) send_packet=0;
           // Keep only PAT
           if (chan_and_pids.psi_tables_filtering==2 && pid>0) send_packet=0;
        }
        mumudvb_channel_t *channel = &chan_and_pids.channels[curr_channel];
        /******************************************************/
        //Ok we must send this packet,
        // we add it to the channel buffer
        /******************************************************/
        if(send_packet==1)
        {
           buffer_func(channel, actual_ts_packet, &unicast_vars, &multicast_vars, scam_vars_ptr, &chan_and_pids, &fds);
        }

      }
      pthread_mutex_unlock(&chan_and_pids.lock);
    }
  }
  /******************************************************/
  //End of main loop
  /******************************************************/
  if(dump_file)
    fclose(dump_file);
  gettimeofday (&tv, (struct timezone *) NULL);
  log_message( log_module,  MSG_INFO,
               "End of streaming. We streamed during %ldd %ld:%02ld:%02ld\n",(tv.tv_sec - real_start_time )/86400,((tv.tv_sec - real_start_time) % 86400 )/3600,((tv.tv_sec - real_start_time) % 3600)/60,(tv.tv_sec - real_start_time) %60 );

  if(card_buffer.partial_packet_number)
    log_message( log_module,  MSG_INFO,
                 "We received %d partial packets :-( \n",card_buffer.partial_packet_number );
  if(card_buffer.partial_packet_number)
    log_message( log_module,  MSG_INFO,
                 "We have got %d overflow errors\n",card_buffer.overflow_number );
mumudvb_close_goto:
  //If the thread is not started, we don't send the unexisting address of monitor_thread_params
  return mumudvb_close(monitorthread == 0 ? NULL:&monitor_thread_params , &unicast_vars, &tuneparams.strengththreadshutdown, cam_vars_ptr, scam_vars_ptr, filename_channels_not_streamed, filename_channels_streamed, filename_pid, get_interrupted());

}

/** @brief Clean closing and freeing
 *
 *
 */
int mumudvb_close(monitor_parameters_t *monitor_thread_params, unicast_parameters_t *unicast_vars, volatile int *strengththreadshutdown, void *cam_vars_v, void *scam_vars_v, char *filename_channels_not_streamed, char *filename_channels_streamed, char *filename_pid, int Interrupted)
{

  int curr_channel;
  int iRet;

  #ifndef ENABLE_CAM_SUPPORT
   (void) cam_vars_v; //to make compiler happy
  #else
  cam_parameters_t *cam_vars=(cam_parameters_t *)cam_vars_v;
  #endif

  #ifndef ENABLE_SCAM_SUPPORT
   (void) scam_vars_v; //to make compiler happy
  #else
  scam_parameters_t *scam_vars=(scam_parameters_t *)scam_vars_v;
  #endif
  if (Interrupted)
  {
    if(Interrupted< (1<<8)) //we check if it's a signal or a mumudvb error
      log_message( log_module,  MSG_INFO, "Caught signal %d - closing cleanly.\n",
                   Interrupted);
    else
      log_message( log_module,  MSG_INFO, "Closing cleanly. Error %d\n",Interrupted>>8);
  }
  struct timespec ts;

  if(signalpowerthread)
  {
    log_message(log_module,MSG_DEBUG,"Signal/power Thread closing\n");
    *strengththreadshutdown=1;
#ifndef __UCLIBC__
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;
    iRet=pthread_timedjoin_np(signalpowerthread, NULL, &ts);
#else
    iRet=pthread_join(signalpowerthread, NULL);
#endif
    if(iRet)
      log_message(log_module,MSG_WARN,"Signal/power Thread badly closed: %s\n", strerror(iRet));

  }
  if(cardthreadparams.thread_running)
  {
    log_message(log_module,MSG_DEBUG,"Card reading Thread closing\n");
    cardthreadparams.threadshutdown=1;
    pthread_mutex_destroy(&cardthreadparams.carddatamutex);
    pthread_cond_destroy(&cardthreadparams.threadcond);
  }
  //We shutdown the monitoring thread
  if(monitorthread)
  {
    log_message(log_module,MSG_DEBUG,"Monitor Thread closing\n");
    monitor_thread_params->threadshutdown=1;
#ifndef __UCLIBC__
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5;
    iRet=pthread_timedjoin_np(monitorthread, NULL, &ts);
#else
    iRet=pthread_join(monitorthread, NULL);
#endif
    if(iRet)
      log_message(log_module,MSG_WARN,"Monitor Thread badly closed: %s\n", strerror(iRet));
  }

  for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
  {
#ifdef ENABLE_TRANSCODING
    transcode_request_thread_end(chan_and_pids.channels[curr_channel].transcode_handle);
#endif
    if(chan_and_pids.channels[curr_channel].socketOut4>0)
      close (chan_and_pids.channels[curr_channel].socketOut4);
    if(chan_and_pids.channels[curr_channel].socketOut6>0)
      close (chan_and_pids.channels[curr_channel].socketOut6);
    if(chan_and_pids.channels[curr_channel].socketIn>0)
      close (chan_and_pids.channels[curr_channel].socketIn);
      //Free the channel structures
    if(chan_and_pids.channels[curr_channel].pmt_packet)
      free(chan_and_pids.channels[curr_channel].pmt_packet);
    chan_and_pids.channels[curr_channel].pmt_packet=NULL;


#ifdef ENABLE_SCAM_SUPPORT
	if (chan_and_pids.channels[curr_channel].scam_support && scam_vars->scam_support) {
		scam_channel_stop(&chan_and_pids.channels[curr_channel]);
	}
#endif



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
    mumu_free_string(&cam_vars->cam_menulist_str);
    mumu_free_string(&cam_vars->cam_menu_string);
  }
#endif
#ifdef ENABLE_SCAM_SUPPORT
  if(scam_vars->scam_support)
  {
	scam_getcw_stop(scam_vars);
  }
#endif

  //autoconf variables freeing
  autoconf_freeing(&autoconf_vars);

  //sap variables freeing
  if(monitor_thread_params && monitor_thread_params->sap_vars->sap_messages4)
    free(monitor_thread_params->sap_vars->sap_messages4);
  if(monitor_thread_params && monitor_thread_params->sap_vars->sap_messages6)
    free(monitor_thread_params->sap_vars->sap_messages6);

  //Pat rewrite freeing
  if(rewrite_vars.full_pat)
    free(rewrite_vars.full_pat);

  //SDT rewrite freeing
  if(rewrite_vars.full_sdt)
    free(rewrite_vars.full_sdt);

  if (strlen(filename_channels_streamed) && (write_streamed_channels)&&remove (filename_channels_streamed))
  {
    log_message( log_module,  MSG_WARN,
                 "%s: %s\n",
                 filename_channels_streamed, strerror (errno));
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


  /*free the file descriptors*/
  if(fds.pfds)
    free(fds.pfds);
  fds.pfds=NULL;
  if(unicast_vars->fd_info)
    free(unicast_vars->fd_info);
  unicast_vars->fd_info=NULL;

//   plop if(temp_buffer_from_dvr)
//       free(temp_buffer_from_dvr);

  // Format ExitCode (normal exit)
  int ExitCode;
  if(Interrupted<(1<<8))
    ExitCode=0;
  else
    ExitCode=Interrupted>>8;

  // Show in log that we are stopping
  log_message( log_module,  MSG_INFO,"========== MuMuDVB version %s is stopping with ExitCode %d ==========",VERSION,ExitCode);

  // Freeing log ressources
  if(log_params.log_file)
  {
    fclose(log_params.log_file);
    free(log_params.log_file_path);
  }
  if(log_params.log_header!=NULL)
      free(log_params.log_header);
  munlockall();

  // End
  return(ExitCode);

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
  if (signum == SIGALRM && !get_interrupted())
  {
    if (card_tuned && !*card_tuned)
    {
      log_message( log_module,  MSG_INFO,
                   "Card not tuned after timeout - exiting\n");
      exit(ERROR_TUNE);
    }
  }
  else if (signum == SIGUSR1 || signum == SIGUSR2 || signum == SIGHUP)
  {
    received_signal=signum;
  }
  else if (signum != SIGPIPE)
  {
    set_interrupted(signum);
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
  int autoconf;

  gettimeofday (&tv, (struct timezone *) NULL);
  monitor_start = tv.tv_sec + tv.tv_usec/1000000;
  monitor_now = monitor_start;
#ifdef ENABLE_SCAM_SUPPORT	
  struct scam_parameters_t *scam_vars;
  scam_vars=(struct scam_parameters_t *) params->scam_vars_v;
#endif	
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
    pthread_mutex_lock(&params->autoconf_vars->lock);
    pthread_mutex_lock(&params->chan_and_pids->lock);

    /*We check if we reached the autoconfiguration timeout*/
    if(params->autoconf_vars->autoconfiguration)
    {
      int iRet;
      iRet = autoconf_poll(now, params->autoconf_vars, params->chan_and_pids, params->tuneparams, params->multicast_vars, &fds, params->unicast_vars, params->server_id, params->scam_vars_v);

      if(iRet)
        set_interrupted(iRet);
    }
    autoconf=params->autoconf_vars->autoconfiguration;//to reduce the lock range
    //this value is not going from null values to non zero values due to the sequencial implementation of autoconfiguration

    pthread_mutex_unlock(&params->autoconf_vars->lock);

    if(!autoconf)
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
        if (time_interval!=0)
          params->chan_and_pids->channels[curr_channel].traffic=((float)params->chan_and_pids->channels[curr_channel].sent_data)/time_interval*1/1000;
        else
          params->chan_and_pids->channels[curr_channel].traffic=0;
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
        if (params->stats_infos->stats_num_reads!=0)
          log_message( log_module,  MSG_DETAIL, "Average packets in the buffer %d\n", params->stats_infos->stats_num_packets_received/params->stats_infos->stats_num_reads);
        else
          log_message( log_module,  MSG_DETAIL, "Average packets in the buffer cannot be calculated - No packets read!\n");
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
      /* Check the PID scrambling state */
	  int curr_pid;
      for (curr_pid = 0; curr_pid < current->num_pids; curr_pid++)
      {
        if (current->pids_num_scrambled_packets[curr_pid]>0)
            current->pids_scrambled[curr_pid]=1;
        else
            current->pids_scrambled[curr_pid]=0;
        current->pids_num_scrambled_packets[curr_pid]=0;
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
        if (monitor_now>last_updown_check)
            packets_per_sec=((double)current->num_packet-num_scrambled)/(monitor_now-last_updown_check);
        else
          packets_per_sec=0;
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
    pthread_mutex_lock(&chan_and_pids.lock);
    for (curr_channel = 0; curr_channel < params->chan_and_pids->number_of_channels; curr_channel++)
    {
      params->chan_and_pids->channels[curr_channel].num_packet = 0;
      params->chan_and_pids->channels[curr_channel].num_scrambled_packets = 0;
    }
    pthread_mutex_unlock(&chan_and_pids.lock);
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
                  "No data from card %d in %ds, exiting.\n",
                  params->tuneparams->card, timeout_no_diff);
      set_interrupted(ERROR_NO_DIFF<<8); //the <<8 is to make difference beetween signals and errors
    }


#ifdef ENABLE_SCAM_SUPPORT
	if (scam_vars->scam_support) {
		/*******************************************/
		/* we check num of packets in ring buffer                */
		/*******************************************/
		for (curr_channel = 0; curr_channel < params->chan_and_pids->number_of_channels; curr_channel++) {
		  mumudvb_channel_t *channel = &params->chan_and_pids->channels[curr_channel];
		  if (channel->scam_support) {
			  unsigned int ring_buffer_num_packets = 0;
			  unsigned int to_descramble = 0;
			  unsigned int to_send = 0;

			  if (channel->ring_buf) {
				pthread_mutex_lock(&channel->ring_buf->lock);
				to_descramble = channel->ring_buf->to_descramble;
				to_send = channel->ring_buf->to_send;
				ring_buffer_num_packets = to_descramble + to_send;
				pthread_mutex_unlock(&channel->ring_buf->lock);
			  }
			  if (ring_buffer_num_packets>=channel->ring_buffer_size)
				log_message( log_module,  MSG_ERROR, "%s: ring buffer overflow, packets in ring buffer %u, ring buffer size %llu\n",channel->name, ring_buffer_num_packets, channel->ring_buffer_size);
			  else
				log_message( log_module,  MSG_DEBUG, "%s: packets in ring buffer %u, ring buffer size %llu, to descramble %u, to send %u\n",channel->name, ring_buffer_num_packets, channel->ring_buffer_size, to_descramble, to_send);
		  }
		}
	}

#endif



    /*******************************************/
    /* generation of the file which says       */
    /* the streamed channels                   */
    /*******************************************/
    if (write_streamed_channels)
      gen_file_streamed_channels(params->filename_channels_streamed, params->filename_channels_not_streamed, params->chan_and_pids->number_of_channels, params->chan_and_pids->channels);


    }
    pthread_mutex_unlock(&params->chan_and_pids->lock);

    for(i=0;i<params->wait_time && !params->threadshutdown;i++)
      usleep(100000);
  }

  log_message(log_module,MSG_DEBUG, "Monitor thread stopping, it lasted %f seconds\n", monitor_now);
  return 0;

}











