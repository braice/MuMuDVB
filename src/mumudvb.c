/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2009 Brice DUBOST
 * 
 * Code for dealing with libdvben50221 inspired from zap_ca
 * Copyright (C) 2004, 2005 Manu Abraham <abraham.manu@gmail.com>
 * Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)
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
 * pat_rewrite.c pat_rewrite.h : the functions associated with the rewrite of the PAT pid
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
#include <errno.h>		// in order to use program_invocation_short_name (gnu extension)
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
#include "pat_rewrite.h"
#include "unicast_http.h"
#include "rtp.h"
#include "log.h"

/** the table for crc32 claculations */
extern uint32_t       crc32_table[256];

/* Signal handling code shamelessly copied from VDR by Klaus Schmidinger 
   - see http://www.cadsoft.de/people/kls/vdr/index.htm */

// global variables used by SignalHandler
long now;
long time_no_diff;
long real_start_time;

//statistics for the big buffer
int stats_num_packets_received=0;
int stats_num_reads=0;
int show_buffer_stats=0;
long show_buffer_stats_time = 0; /** */
int show_buffer_stats_interval = 120; /** How often we how the statistics about the DVR buffer*/
//statistics for the traffic
int show_traffic = 0; /** do we periodically show the traffic ?*/
long show_traffic_time = 0; /** */
int show_traffic_time_usec = 0; /** */
long compute_traffic_time = 0; /** */
int compute_traffic_time_usec = 0; /** */
int show_traffic_interval = 10; /** The interval for the traffic display */
int compute_traffic_interval = 10; /** The interval for the traffic calculation */

int no_daemon = 0; /** do we deamonize mumudvb ? */

int timeout_no_diff = ALARM_TIME_TIMEOUT_NO_DIFF;
// file descriptors
fds_t fds; /** File descriptors associated with the card */


int Interrupted = 0;
char filename_channels_diff[256];
char filename_channels_not_streamed[256];
char filename_cam_info[256];
char filename_pid[256];
char filename_gen_conf[256];
int  write_streamed_channels=1;

mumudvb_chan_and_pids_t chan_and_pids={
  .number_of_channels=0,
};

//multicast parameters
multicast_parameters_t multicast_vars={
  .ttl=DEFAULT_TTL,
  .common_port = 1234,
  .auto_join=0,
//  .rtp_header = 0,
};

int rtp_header = 0;

//tuning parameters C99 initialisation
tuning_parameters_t tuneparams={
  .card = 0,
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
  .modulation_set = 0,
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


//sap announces C99 initialisation
sap_parameters_t sap_vars={
  .sap_messages=NULL, 
  .sap=SAP_UNDEFINED, //No sap by default
  .sap_interval=SAP_DEFAULT_INTERVAL,
  .sap_sending_ip="0.0.0.0",
  .sap_default_group="",
  .sap_organisation="MuMuDVB",
  .sap_uri="\0",
  .sap_ttl=SAP_DEFAULT_TTL,
};

//autoconfiguration. C99 initialisation
autoconf_parameters_t autoconf_vars={
  .autoconfiguration=0,
  .autoconf_radios=0,
  .autoconf_scrambled=0,
  .autoconf_pid_update=1,
  .autoconf_ip_header="239.100",
  .time_start_autoconfiguration=0,
  .transport_stream_id=-1,
  .autoconf_temp_pat=NULL,
  .autoconf_temp_sdt=NULL,
  .autoconf_temp_psip=NULL,
  .services=NULL,
  .autoconf_unicast_start_port=0,
};

#ifdef ENABLE_CAM_SUPPORT
//CAM (Conditionnal Access Modules : for scrambled channels) C99 initialisation
cam_parameters_t cam_vars={
  .cam_support = 0,
  .cam_number=0,
  .need_reset=0,
  .reset_counts=0,
  .reset_interval=CAM_DEFAULT_RESET_INTERVAL,
  .timeout_no_cam_init=CAM_DEFAULT_RESET_INTERVAL,
  .max_reset_number=CAM_DEFAULT_MAX_RESET_NUM,
  .tl=NULL,
  .sl=NULL,
  .stdcam=NULL,
  .ca_resource_connected=0,
  .delay=0,
  .mmi_state = MMI_STATE_CLOSED,
};
#endif

//Parameters for PAT rewriting
pat_rewrite_parameters_t rewrite_vars={
  .rewrite_pat = 0,
  .pat_version=-1,
  .full_pat=NULL,
  .needs_update=1,
  .full_pat_ok=0,
  .continuity_counter=0,
};

//Parameters for HTTP unicast
unicast_parameters_t unicast_vars={
  .ipOut="\0",
  .portOut=4242,
  .consecutive_errors_timeout=UNICAST_CONSECUTIVE_ERROR_TIMEOUT,
  .max_clients=-1,
};

//logging
int log_initialised=0; /**say if we opened the syslog resource*/
int verbosity = MSG_INFO+1; /** the verbosity level for log messages */






// prototypes
static void SignalHandler (int signum);//below
int read_multicast_configuration(multicast_parameters_t *multicast_vars, mumudvb_channel_t *current_channel, int *ip_ok, char *substring); //in multicast.c
int mumudvb_poll(fds_t *fds); //below



int
    main (int argc, char **argv)
{
  int k,iRet;


  //MPEG2-TS reception and sort
  int pid;			/** pid of the current mpeg2 packet */
  int ScramblingControl;
  int bytes_read;		/** number of bytes actually read */
  //temporary buffers
  /**Buffer containing one packet*/
  unsigned char actual_ts_packet[TS_PACKET_SIZE];
  /**The buffer from the DVR wich can contain several TS packets*/
  unsigned char *temp_buffer_from_dvr;
  /** The maximum number of packets in the buffer from DVR*/
  int dvr_buffer_size=DEFAULT_TS_BUFFER_SIZE;
  int buffpos;
  int poll_ret;

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
  int ip_ok = 0;
  char current_line[CONF_LINELEN];
  char *substring=NULL;
  char delimiteurs[] = CONFIG_FILE_SEPARATOR;


  uint8_t hi_mappids[8192];
  uint8_t lo_mappids[8192];


  /**The number of partial packets received*/
  int partial_packet_number=0;
  /**Do we avoid sending the SDT pid (for VLC)*/
  int dont_send_sdt =0;
  /**Do we avoid sending scrambled packets ?*/
  int dont_send_scrambled=0;


  // Initialise PID map
  for (k = 0; k < 8192; k++)
  {
    hi_mappids[k] = (k >> 8);
    lo_mappids[k] = (k & 0xff);
  }

  /******************************************************/
  //Getopt
  /******************************************************/
  const char short_options[] = "c:sdthvq";
  const struct option long_options[] = {
    {"config", required_argument, NULL, 'c'},
    {"signal", no_argument, NULL, 's'},
    {"traffic", no_argument, NULL, 't'},
    {"debug", no_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0}
  };
  int c, option_index = 0;



  if (argc == 1)
  {
    usage (argv[0]);
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
          log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
          exit(errno);
        }
        strncpy (conf_filename, optarg, strlen (optarg) + 1);
        break;
      case 's':
        tuneparams.display_strenght = 1;
        break;
      case 't':
        show_traffic = 1;
        break;
      case 'd':
        no_daemon = 1;
        break;
      case 'v':
        verbosity++;
        break;
      case 'q':
        verbosity--;
        break;
      case 'h':
        usage (program_invocation_short_name);
        exit(ERROR_ARGS);
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
    log_message( MSG_WARN, "Cannot daemonize: %s\n",
                 strerror (errno));
    exit(666); //FIXME : use an error
  }

  //we open the descriptor for syslog
  if (!no_daemon)
    openlog ("MUMUDVB", LOG_PID, 0);
  log_initialised=1;

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
    log_message( MSG_ERROR, "%s: %s\n",
                 conf_filename, strerror (errno));
    free(conf_filename);
    exit(ERROR_CONF_FILE);
  }

  curr_channel=0;
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
      continue;
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
    else if((iRet=read_sap_configuration(&sap_vars, &chan_and_pids.channels[curr_channel], ip_ok, substring))) //Read the line concerning the sap parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
#ifdef ENABLE_CAM_SUPPORT
    else if((iRet=read_cam_configuration(&cam_vars, &chan_and_pids.channels[curr_channel], ip_ok, substring))) //Read the line concerning the cam parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
#endif
    else if((iRet=read_unicast_configuration(&unicast_vars, &chan_and_pids.channels[curr_channel], ip_ok, substring))) //Read the line concerning the unicast parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
    else if((iRet=read_multicast_configuration(&multicast_vars, &chan_and_pids.channels[curr_channel], &ip_ok, substring))) //Read the line concerning the multicast parameters
    {
      if(iRet==-1)
        exit(ERROR_CONF);
    }
    else if (!strcmp (substring, "timeout_no_diff"))
    {
      substring = strtok (NULL, delimiteurs);
      timeout_no_diff= atoi (substring);
    }
    else if (!strcmp (substring, "show_traffic_interval"))
    {
      substring = strtok (NULL, delimiteurs);
      show_traffic_interval= atoi (substring);
      if(show_traffic_interval<ALARM_TIME)
      {
        show_traffic_interval=ALARM_TIME;
        log_message(MSG_WARN,"Sorry the minimum interval for showing the traffic is %ds\n",ALARM_TIME);
      }
    }
    else if (!strcmp (substring, "compute_traffic_interval"))
    {
      substring = strtok (NULL, delimiteurs);
      compute_traffic_interval= atoi (substring);
      if(compute_traffic_interval<ALARM_TIME)
      {
        compute_traffic_interval=ALARM_TIME;
        log_message(MSG_WARN,"Sorry the minimum interval for computing the traffic is %ds\n",ALARM_TIME);
      }
    }
    else if (!strcmp (substring, "rewrite_pat"))
    {
      substring = strtok (NULL, delimiteurs);
      rewrite_vars.rewrite_pat = atoi (substring);
      if(rewrite_vars.rewrite_pat)
      {
        log_message( MSG_INFO,
                     "You have enabled the Pat Rewriting\n");
      }
    }
    else if (!strcmp (substring, "dont_send_scrambled"))
    {
      substring = strtok (NULL, delimiteurs);
      dont_send_scrambled = atoi (substring);
    }
    else if (!strcmp (substring, "dont_send_sdt"))
    {
      substring = strtok (NULL, delimiteurs);
      dont_send_sdt = atoi (substring);
      if(dont_send_sdt)
        log_message( MSG_INFO, "You decided not to send the SDT pid. This is a VLC workaround.\n");
    }
    else if (!strcmp (substring, "rtp_header"))
    {
      substring = strtok (NULL, delimiteurs);
      rtp_header = atoi (substring);
      if (rtp_header==1)
        log_message( MSG_INFO, "You decided to send the RTP header.\n");
    }

    else if (!strcmp (substring, "dvr_buffer_size"))
    {
      substring = strtok (NULL, delimiteurs);
      dvr_buffer_size = atoi (substring);
      if(dvr_buffer_size<=0)
      {
        log_message( MSG_WARN,
                     "Warning : the buffer size MUST be >0, forced to 1 packet\n");
        dvr_buffer_size = 1;
      }
      if(dvr_buffer_size>1)
        log_message( MSG_WARN,
                     "Warning : You set a buffer size > 1, this feature is experimental, please report bugs/problems or results\n");
      show_buffer_stats=1;
    }

    else if (!strcmp (substring, "ts_id"))
    {
      if ( ip_ok == 0)
      {
        log_message( MSG_ERROR,
                     "ts_id : You must precise ip first\n");
        exit(ERROR_CONF);
      }
      substring = strtok (NULL, delimiteurs);
      chan_and_pids.channels[curr_channel].ts_id = atoi (substring);
    }
    else if (!strcmp (substring, "pids"))
    {
      if ( ip_ok == 0)
      {
        log_message( MSG_ERROR,
                     "pids : You must precise ip first\n");
        exit(ERROR_CONF);
      }
      if (multicast_vars.common_port!=0 && chan_and_pids.channels[curr_channel].portOut == 0)
        chan_and_pids.channels[curr_channel].portOut = multicast_vars.common_port;
      while ((substring = strtok (NULL, delimiteurs)) != NULL)
      {
        chan_and_pids.channels[curr_channel].pids[curr_pid] = atoi (substring);
	      // we see if the given pid is good
        if (chan_and_pids.channels[curr_channel].pids[curr_pid] < 10 || chan_and_pids.channels[curr_channel].pids[curr_pid] > 8191)
        {
          log_message( MSG_ERROR,
                       "Config issue : %s in pids, given pid : %d\n",
                       conf_filename, chan_and_pids.channels[curr_channel].pids[curr_pid]);
          exit(ERROR_CONF);
        }
        curr_pid++;
        if (curr_pid >= MAX_PIDS_PAR_CHAINE)
        {
          log_message( MSG_ERROR,
                       "Too many pids : %d channel : %d\n",
                       curr_pid, curr_channel);
          exit(ERROR_CONF);
        }
      }
      chan_and_pids.channels[curr_channel].num_pids = curr_pid;
      curr_pid = 0;
      curr_channel++;
      ip_ok = 0;
    }
    else if (!strcmp (substring, "name"))
    {
      if ( ip_ok == 0)
      {
        log_message( MSG_ERROR,
                     "name : You must precise ip first\n");
        exit(ERROR_CONF);
      }
	  // other substring extraction method in order to keep spaces
      substring = strtok (NULL, "=");
      if (!(strlen (substring) >= MAX_NAME_LEN - 1))
        strcpy(chan_and_pids.channels[curr_channel].name,strtok(substring,"\n"));	
      else
      {
        log_message( MSG_WARN,"Channel name too long\n");
        strncpy(chan_and_pids.channels[curr_channel].name,strtok(substring,"\n"),MAX_NAME_LEN-1);
      }
    }
    else
    {
      if(strlen (current_line) > 1)
        log_message( MSG_WARN,
                     "Config issue : unknow symbol : %s\n\n", substring);
      continue;
    }

    if (curr_channel > MAX_CHANNELS)
    {
      log_message( MSG_ERROR, "Too many channels : %d limit : %d\n",
                   curr_channel, MAX_CHANNELS);
      exit(ERROR_TOO_CHANNELS);
    }

  }
  fclose (conf_file);

  if((autoconf_vars.autoconfiguration==AUTOCONF_MODE_FULL)&&(sap_vars.sap == SAP_UNDEFINED))
  {
    log_message( MSG_INFO,
                 "Full autoconfiguration, we activate SAP announces. if you want to desactivate them see the README.\n");
    sap_vars.sap=SAP_ON;
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
    sprintf (filename_pid, "/var/run/mumudvb/mumudvb_carte%d.pid", tuneparams.card);
    pidfile = fopen (filename_pid, "w");
    if (pidfile == NULL)
    {
      log_message( MSG_INFO,"%s: %s\n",
                   filename_pid, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }
    fprintf (pidfile, "%d\n", getpid ());
    fclose (pidfile);
  }

  
  /*****************************************************/
  //Autoconfiguration init
  /*****************************************************/
  
  if(autoconf_vars.autoconfiguration)
  {
    if(autoconf_vars.autoconf_pid_update)
    {
      log_message( MSG_INFO,
                   "The autoconfiguration auto update is enabled. If you want to disable it put \"autoconf_pid_update=0\" in your config file.\n");
    }
      //In case of autoconfiguration, we generate a config file with the channels discovered
      //Here we generate the header, ie we take the actual config file and copy it removing the channels
    sprintf (filename_gen_conf, GEN_CONF_PATH,
             tuneparams.card);
    gen_config_file_header(conf_filename, filename_gen_conf);
  }
  else 
    autoconf_vars.autoconf_pid_update=0;

  free(conf_filename);

  chan_and_pids.number_of_channels = curr_channel;

  // we clear them by paranoia
  sprintf (filename_channels_diff, STREAMED_LIST_PATH,
           tuneparams.card);
  sprintf (filename_channels_not_streamed, NOT_STREAMED_LIST_PATH,
           tuneparams.card);
  sprintf (filename_cam_info, CAM_INFO_LIST_PATH,
           tuneparams.card);
  channels_diff = fopen (filename_channels_diff, "w");
  if (channels_diff == NULL)
  {
    write_streamed_channels=0;
    log_message( MSG_WARN,
                 "WARNING : Can't create %s: %s\n",
                 filename_channels_diff, strerror (errno));
  }
  else
    fclose (channels_diff);

  channels_not_streamed = fopen (filename_channels_not_streamed, "w");
  if (channels_diff == NULL)
  {
    write_streamed_channels=0;
    log_message( MSG_WARN,
                 "WARNING : Can't create %s: %s\n",
                 filename_channels_not_streamed, strerror (errno));
  }
  else
    fclose (channels_not_streamed);


#ifdef ENABLE_CAM_SUPPORT
  if(cam_vars.cam_support)
  {
    cam_info = fopen (filename_cam_info, "w");
    if (cam_info == NULL)
    {
      log_message( MSG_WARN,
                   "WARNING : Can't create %s: %s\n",
                   filename_cam_info, strerror (errno));
    }
    else
      fclose (cam_info);
  }
#endif


  log_message( MSG_INFO, "Streaming. Freq %d\n",
               tuneparams.freq);


  /******************************************************/
  // Card tuning
  /******************************************************/
  // alarm for tuning timeout
  if(tuneparams.tuning_timeout)
  {
    if (signal (SIGALRM, SignalHandler) == SIG_IGN)
      signal (SIGALRM, SIG_IGN);
    if (signal (SIGUSR1, SignalHandler) == SIG_IGN)
      signal (SIGUSR1, SIG_IGN);
    if (signal (SIGUSR2, SignalHandler) == SIG_IGN)
      signal (SIGUSR2, SIG_IGN);
    alarm (tuneparams.tuning_timeout);
  }


  // We tune the card
  iRet =-1;

  if (open_fe (&fds.fd_frontend, tuneparams.card))
  {
    iRet = 
        tune_it (fds.fd_frontend, &tuneparams);
  }

  if (iRet < 0)
  {
    log_message( MSG_INFO, "Tunning issue, card %d\n", tuneparams.card);
    // we close the file descriptors
    close_card_fd (fds);
    return mumudvb_close(ERROR_TUNE<<8);
  }
  log_message( MSG_INFO, "Card %d tuned\n", tuneparams.card);
  tuneparams.card_tuned = 1;

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
    log_message( MSG_ERROR,"ErrorSigaction\n");

  alarm (ALARM_TIME);

  if(show_traffic)
    log_message(MSG_INFO,"The traffic will be shown every %d seconds\n",show_traffic_interval);


  /*****************************************************/
  //cam_support
  /*****************************************************/

#ifdef ENABLE_CAM_SUPPORT
  if(cam_vars.cam_support){
    //We initialise the cam. If fail, we remove cam support
    if(cam_start(&cam_vars,tuneparams.card,filename_cam_info))
    {
      log_message( MSG_ERROR,"Cannot initalise cam\n");
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
    return mumudvb_close(ERROR_GENERIC);

  /*****************************************************/
  //Pat rewriting
  //memory allocation for MPEG2-TS
  //packet structures
  /*****************************************************/

  if(rewrite_vars.rewrite_pat)
  {
    for (curr_channel = 0; curr_channel < MAX_CHANNELS; curr_channel++)
      chan_and_pids.channels[curr_channel].generated_pat_version=-1;

    rewrite_vars.full_pat=malloc(sizeof(mumudvb_ts_packet_t));
    if(rewrite_vars.full_pat==NULL)
    {
      log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      return mumudvb_close(100<<8);
    }
    memset (rewrite_vars.full_pat, 0, sizeof( mumudvb_ts_packet_t));//we clear it
      
  }


  /*****************************************************/
  //Some initialisations
  /*****************************************************/

  //Initialisation of the channels for RTP
  if(rtp_header)
    for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
      init_rtp_header(&chan_and_pids.channels[curr_channel]);

  // initialisation of active channels list
  for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
  {
    chan_and_pids.channels[curr_channel].streamed_channel = 0;
    chan_and_pids.channels[curr_channel].num_packet = 0;
    chan_and_pids.channels[curr_channel].streamed_channel_old = 1;
    chan_and_pids.channels[curr_channel].scrambled_channel = 0;
    chan_and_pids.channels[curr_channel].scrambled_channel_old = 0;
      
      //We alloc the channel pmt_packet (useful for autoconf and cam
    /**@todo : allocate only if autoconf or cam*/
    if(chan_and_pids.channels[curr_channel].pmt_packet==NULL)
    {
      chan_and_pids.channels[curr_channel].pmt_packet=malloc(sizeof(mumudvb_ts_packet_t));
      if(chan_and_pids.channels[curr_channel].pmt_packet==NULL)
      {
        log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
        return mumudvb_close(ERROR_MEMORY<<8);
      }
      memset (chan_and_pids.channels[curr_channel].pmt_packet, 0, sizeof( mumudvb_ts_packet_t));//we clear it
    }

  }

  //We initialise asked pid table
  memset (chan_and_pids.asked_pid, 0, sizeof( uint8_t)*8192);//we clear it
  memset (chan_and_pids.number_chan_asked_pid, 0, sizeof( uint8_t)*8192);//we clear it

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

  //We alloc the buffer
  temp_buffer_from_dvr=malloc(sizeof(unsigned char)*TS_PACKET_SIZE*dvr_buffer_size);

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
  if (create_card_fd (tuneparams.card, chan_and_pids.asked_pid, &fds) < 0)
    return mumudvb_close(100<<8);

  set_filters(chan_and_pids.asked_pid, &fds);
  fds.pfds=NULL;
  fds.pfdsnum=1;
  //+1 for closing the pfd list, see man poll
  fds.pfds=realloc(fds.pfds,(fds.pfdsnum+1)*sizeof(struct pollfd));
  if (fds.pfds==NULL)
  {
    log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return mumudvb_close(100<<8);
  }
    
  //We fill the file descriptor information structure. the first one is irrelevant
  //(file descriptor associated to the DVB card) but we allocate it for consistency
  unicast_vars.fd_info=NULL;
  unicast_vars.fd_info=realloc(unicast_vars.fd_info,(fds.pfdsnum)*sizeof(unicast_fd_info_t));
  if (unicast_vars.fd_info==NULL)
  {
    log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return mumudvb_close(100<<8);
  }

  //File descriptor for polling the DVB card
  fds.pfds[0].fd = fds.fd_dvr;
  //POLLIN : data available for read
  fds.pfds[0].events = POLLIN | POLLPRI; 
  fds.pfds[1].fd = 0;
  fds.pfds[1].events = POLLIN | POLLPRI;

  //We record the starting time
  gettimeofday (&tv, (struct timezone *) NULL);
  real_start_time = tv.tv_sec;
  now = 0;

  /*****************************************************/
  // Init network, we open the sockets
  /*****************************************************/
  for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
  {
      //See the README for the reason of this option
    if(multicast_vars.auto_join)
      chan_and_pids.channels[curr_channel].socketOut = makeclientsocket (chan_and_pids.channels[curr_channel].ipOut, chan_and_pids.channels[curr_channel].portOut, multicast_vars.ttl, &chan_and_pids.channels[curr_channel].sOut);
    else
      chan_and_pids.channels[curr_channel].socketOut = makesocket (chan_and_pids.channels[curr_channel].ipOut, chan_and_pids.channels[curr_channel].portOut, multicast_vars.ttl, &chan_and_pids.channels[curr_channel].sOut);
  }
  //We open the socket for the http unicast if needed and we update the poll structure
  if(strlen(unicast_vars.ipOut))
  {
    log_message(MSG_INFO,"Unicast : We open the Master http socket for address %s:%d\n",unicast_vars.ipOut, unicast_vars.portOut);
    unicast_create_listening_socket(UNICAST_MASTER, -1, unicast_vars.ipOut, unicast_vars.portOut, &unicast_vars.sIn, &unicast_vars.socketIn, &fds, &unicast_vars);
    /** open the unicast listening connections fo the channels */
    for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
      if(chan_and_pids.channels[curr_channel].unicast_port)
    {
      log_message(MSG_INFO,"Unicast : We open the channel %d http socket address %s:%d\n",curr_channel, unicast_vars.ipOut, chan_and_pids.channels[curr_channel].unicast_port);
      unicast_create_listening_socket(UNICAST_LISTEN_CHANNEL, curr_channel, unicast_vars.ipOut,chan_and_pids.channels[curr_channel].unicast_port , &chan_and_pids.channels[curr_channel].sIn, &chan_and_pids.channels[curr_channel].socketIn, &fds, &unicast_vars);
    }
  }


  /*****************************************************/
  // init sap
  /*****************************************************/

  iRet=init_sap(&sap_vars, multicast_vars);
  if(iRet)
    return mumudvb_close(ERROR_GENERIC);

  /*****************************************************/
  // Information about streamed channels
  /*****************************************************/

  if(autoconf_vars.autoconfiguration!=AUTOCONF_MODE_FULL)
    log_streamed_channels(chan_and_pids.number_of_channels, chan_and_pids.channels);

  if(autoconf_vars.autoconfiguration)
    log_message(MSG_INFO,"Autoconfiguration Start\n");

  /******************************************************/
  //Main loop where we get the packets and send them
  /******************************************************/
  while (!Interrupted)
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

    /* Attempt to read 188 bytes from /dev/____/dvr */
    if ((bytes_read = read (fds.fd_dvr, temp_buffer_from_dvr, TS_PACKET_SIZE*dvr_buffer_size)) > 0)
    {

      if((bytes_read>0 )&& (bytes_read % TS_PACKET_SIZE))
      {
        log_message( MSG_WARN, "Warning : partial packet received len %d\n", bytes_read);
        partial_packet_number++;
        bytes_read-=bytes_read % TS_PACKET_SIZE;
        if(bytes_read<=0)
          continue;
      }
    }

    if(bytes_read<0)
    {
      if(errno!=EAGAIN)
        log_message( MSG_WARN,"Error : DVR Read error : %s \n",strerror(errno));
      continue;
    }

    if(dvr_buffer_size!=1)
    {
      stats_num_packets_received+=(int) bytes_read/188;
      stats_num_reads++;
    }
 
    for(buffpos=0;(buffpos+TS_PACKET_SIZE)<=bytes_read;buffpos+=TS_PACKET_SIZE)//we loop on the subpackets
    {
      memcpy( actual_ts_packet,temp_buffer_from_dvr+buffpos,TS_PACKET_SIZE);

      pid = ((actual_ts_packet[1] & 0x1f) << 8) | (actual_ts_packet[2]);

      //Software filtering in case the card doesn't have hardware filtering
      if(chan_and_pids.asked_pid[pid]==PID_NOT_ASKED)
        continue;

      ScramblingControl = (actual_ts_packet[3] & 0xc0) >> 6;
	  // 0 = Not scrambled
	  // 1 = Reserved for future use
	  // 2 = Scrambled with even key
	  // 3 = Scrambled with odd key
	  

      /*************************************************************************************/
      /****              AUTOCONFIGURATION PART                                         ****/
      /*************************************************************************************/
      if(!ScramblingControl &&  autoconf_vars.autoconfiguration)
      {
        Interrupted = autoconf_new_packet(pid, actual_ts_packet, &autoconf_vars, &chan_and_pids, tuneparams, multicast_vars, &fds, &unicast_vars);
        continue;
      }
 
      /*************************************************************************************/
      /****              AUTOCONFIGURATION PART FINISHED                                ****/
      /*************************************************************************************/

      /******************************************************/
      //Pat rewrite 
      /******************************************************/
      //we save the full pat wich will be the source pat for all the channels
      if( (pid == 0) && //This is a PAT PID
           rewrite_vars.rewrite_pat ) //AND we asked for rewrite
      {
        /*Check the version before getting the full packet*/
        if(!rewrite_vars.needs_update)
        {
          rewrite_vars.needs_update=pat_need_update(&rewrite_vars,actual_ts_packet);
          if(rewrite_vars.needs_update) //It needs update we mark the packet as empty
            rewrite_vars.full_pat->empty=1;
        }
        /*We need to update the full packet, we download it*/
        if(rewrite_vars.needs_update)
        {
          if(get_ts_packet(actual_ts_packet,rewrite_vars.full_pat))
          {
            log_message(MSG_DEBUG,"Pat rewrite : Full pat updated\n");
            /*We've got the FULL PAT packet*/
            update_version(&rewrite_vars);
            rewrite_vars.needs_update=0;
            rewrite_vars.full_pat_ok=1;
          }
        }
	      //To avoid the duplicates, we have to update the continuity counter
        rewrite_vars.continuity_counter++;
        rewrite_vars.continuity_counter= rewrite_vars.continuity_counter % 32;
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
	      
	      //VLC workaround
        if(dont_send_sdt && pid==17)
          send_packet=0;
	      
	      //if it isn't mandatory wee see if it is in the channel list
        if(!send_packet)
          for (curr_pid = 0; (curr_pid < chan_and_pids.channels[curr_channel].num_pids)&& !send_packet; curr_pid++)
            if ((chan_and_pids.channels[curr_channel].pids[curr_pid] == pid))
        {
          send_packet=1;
		      //avoid sending of scrambled channels if we asked to
          if(dont_send_scrambled && (ScramblingControl>0)&& (pid != chan_and_pids.channels[curr_channel].pmt_pid) )
            send_packet=0;
          if ((ScramblingControl>0) && (pid != chan_and_pids.channels[curr_channel].pmt_pid) )
            chan_and_pids.channels[curr_channel].scrambled_channel++;
		      //we don't count the PMT pid for up channels
          if(send_packet && (pid != chan_and_pids.channels[curr_channel].pmt_pid))
            chan_and_pids.channels[curr_channel].streamed_channel++;
          if (pid != chan_and_pids.channels[curr_channel].pmt_pid)
            chan_and_pids.channels[curr_channel].num_packet++;
        }

        /******************************************************/
	      //cam support
	      // If we send the packet, we look if it's a cam pmt pid
        /******************************************************/
#ifdef ENABLE_CAM_SUPPORT
	      //We don't ask the cam before the end of autoconfiguration
        if(!autoconf_vars.autoconfiguration && cam_vars.cam_support && send_packet==1)  //no need to check paquets we don't send
          if(cam_vars.ca_resource_connected && cam_vars.delay>=1 )
        {
          if ((chan_and_pids.channels[curr_channel].need_cam_ask==CAM_NEED_ASK)&& (chan_and_pids.channels[curr_channel].pmt_pid == pid))
          {
			//if the packet is already ok, we don't get it (it can be updated by pmt_follow)
            if((autoconf_vars.autoconf_pid_update && !chan_and_pids.channels[curr_channel].pmt_packet->empty && chan_and_pids.channels[curr_channel].pmt_packet->packet_ok)||
                (!autoconf_vars.autoconf_pid_update && get_ts_packet(actual_ts_packet,chan_and_pids.channels[curr_channel].pmt_packet))) 
            {
                            //We check the transport stream id of the packet
              if(check_pmt_ts_id(chan_and_pids.channels[curr_channel].pmt_packet, &chan_and_pids.channels[curr_channel]))
              {
                cam_vars.delay=0;
                iRet=mumudvb_cam_new_pmt(&cam_vars, chan_and_pids.channels[curr_channel].pmt_packet);
                if(iRet==1)
                {
                  log_message( MSG_INFO,"CAM : CA PMT sent for channel %d : \"%s\"\n", curr_channel, chan_and_pids.channels[curr_channel].name );
                  chan_and_pids.channels[curr_channel].need_cam_ask=CAM_ASKED; //once we have asked the CAM for this PID, we don't have to ask anymore
                }
                else if(iRet==-1)
                {
                  log_message( MSG_DETAIL,"CAM : Problem sending CA PMT for channel %d : \"%s\"\n", curr_channel, chan_and_pids.channels[curr_channel].name );
                  chan_and_pids.channels[curr_channel].pmt_packet->empty=1;//if there was a problem, we reset the packet
                }
              }
              else
              {
                chan_and_pids.channels[curr_channel].pmt_packet->empty=1;//The ts_id is bad, we will try to get another PMT packet
              }
            }
          }
        }
#endif

        /******************************************************/
	      //PMT follow (ie we check if the pids announced in the pmt changed)
        /******************************************************/
        /**@todo : put this in a function*/
        if( (autoconf_vars.autoconf_pid_update) && (send_packet==1) && (chan_and_pids.channels[curr_channel].autoconfigurated) &&(chan_and_pids.channels[curr_channel].pmt_pid==pid) && pid)  //no need to check paquets we don't send
        {
          /*Note : the pmt version is initialised during autoconfiguration*/
          /*Check the version stored in the channel*/
          if(!chan_and_pids.channels[curr_channel].pmt_needs_update)
          {
		      //Checking without crc32, it there is a change we get the full packet for crc32 checking
            chan_and_pids.channels[curr_channel].pmt_needs_update=pmt_need_update(&chan_and_pids.channels[curr_channel],actual_ts_packet,1);

            if(chan_and_pids.channels[curr_channel].pmt_needs_update&&chan_and_pids.channels[curr_channel].pmt_packet) //It needs update we mark the packet as empty
              chan_and_pids.channels[curr_channel].pmt_packet->empty=1;
          }
          /*We need to update the full packet, we download it*/
          if(chan_and_pids.channels[curr_channel].pmt_needs_update)
          {
            if(get_ts_packet(actual_ts_packet,chan_and_pids.channels[curr_channel].pmt_packet))
            {
              if(pmt_need_update(&chan_and_pids.channels[curr_channel],chan_and_pids.channels[curr_channel].pmt_packet->packet,0))
              {
                log_message(MSG_DETAIL,"Autoconfiguration : PMT packet updated, we have now to check if there is new things\n");
                /*We've got the FULL PMT packet*/
                if(autoconf_read_pmt(chan_and_pids.channels[curr_channel].pmt_packet, &chan_and_pids.channels[curr_channel], tuneparams.card, chan_and_pids.asked_pid, chan_and_pids.number_chan_asked_pid, &fds)==0)
                {
                  if(chan_and_pids.channels[curr_channel].need_cam_ask==CAM_ASKED)
                    chan_and_pids.channels[curr_channel].need_cam_ask=CAM_NEED_ASK;
                  update_pmt_version(&chan_and_pids.channels[curr_channel]);
                  chan_and_pids.channels[curr_channel].pmt_needs_update=0;
                }
                else
                  chan_and_pids.channels[curr_channel].pmt_packet->empty=1;
              }
              else
              {
                log_message(MSG_DEBUG,"Autoconfiguration : False alert, nothing to do\n");
                chan_and_pids.channels[curr_channel].pmt_needs_update=0;
              }
            }
          }
        }
	  
        /******************************************************/
	      //Rewrite PAT
        /******************************************************/
        if(send_packet==1)  //no need to check paquets we don't send
          if( (pid == 0) && //This is a PAT PID
               rewrite_vars.rewrite_pat)  //AND we asked for rewrite
        {
          if(rewrite_vars.full_pat_ok ) //AND the global full pat is ok
          {
            /*We check if it's the first pat packet ? or we send it each time ?*/
            /*We check if the versions corresponds*/
            if(!rewrite_vars.needs_update && chan_and_pids.channels[curr_channel].generated_pat_version!=rewrite_vars.pat_version)//We check the version only if the PAT is not currently updated
            {
              log_message(MSG_DEBUG,"Pat rewrite : We need to rewrite the PAT for the channel %d : \"%s\"\n", curr_channel, chan_and_pids.channels[curr_channel].name);
              /*They mismatch*/
              /*We generate the rewritten packet*/
              if(pat_channel_rewrite(&rewrite_vars, chan_and_pids.channels, curr_channel,actual_ts_packet))
              {
                /*We update the version*/
                chan_and_pids.channels[curr_channel].generated_pat_version=rewrite_vars.pat_version;
              }
              else
                log_message(MSG_DEBUG,"Pat rewrite : ERROR with the pat for the channel %d : \"%s\"\n", curr_channel, chan_and_pids.channels[curr_channel].name);
			      			    
            }
            if(chan_and_pids.channels[curr_channel].generated_pat_version==rewrite_vars.pat_version)
            {
              /*We send the rewrited PAT from chan_and_pids.channels[curr_channel].generated_pat*/
              memcpy(actual_ts_packet,chan_and_pids.channels[curr_channel].generated_pat,TS_PACKET_SIZE);
			    //To avoid the duplicates, we have to update the continuity counter
              pat_rewrite_set_continuity_counter(actual_ts_packet,rewrite_vars.continuity_counter);
            }
            else
            {
              send_packet=0;
              log_message(MSG_DEBUG,"Pat rewrite : Bad pat channel version, we don't send the pat for the channel %d : \"%s\"\n", curr_channel, chan_and_pids.channels[curr_channel].name);
            }
          }
          else
          {
            send_packet=0;
            log_message(MSG_DEBUG,"Pat rewrite : We need a global pat update, we don't send the pat for the channel %d : \"%s\"\n", curr_channel, chan_and_pids.channels[curr_channel].name);
          }
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
          if ((chan_and_pids.channels[curr_channel].nb_bytes + TS_PACKET_SIZE) > MAX_UDP_SIZE)
          {
		      //For bandwith measurement (traffic)
            chan_and_pids.channels[curr_channel].sent_data+=chan_and_pids.channels[curr_channel].nb_bytes;

            /****** RTP *******/
            if(rtp_header)
              rtp_update_sequence_number(&chan_and_pids.channels[curr_channel]);

            /********** MULTICAST *************/
             //if the multicast TTL is set to 0 we don't send the multicast packets
            if(multicast_vars.ttl)
              sendudp (chan_and_pids.channels[curr_channel].socketOut, &chan_and_pids.channels[curr_channel].sOut, chan_and_pids.channels[curr_channel].buf,
                       chan_and_pids.channels[curr_channel].nb_bytes);
            /*********** UNICAST **************/
            if(chan_and_pids.channels[curr_channel].clients)
            {
              unicast_client_t *actual_client;
              unicast_client_t *temp_client;
              int written_len;
              actual_client=chan_and_pids.channels[curr_channel].clients;
              while(actual_client!=NULL)
              {
			      //NO RTP over http waiting for the RTSP implementation
                if(rtp_header)
                  written_len=write(actual_client->Socket,chan_and_pids.channels[curr_channel].buf+RTP_HEADER_LEN, chan_and_pids.channels[curr_channel].nb_bytes-RTP_HEADER_LEN)+RTP_HEADER_LEN; //+RTP_HEADER_LEN to avoid changing the next lines
                else
                  written_len=write(actual_client->Socket,chan_and_pids.channels[curr_channel].buf, chan_and_pids.channels[curr_channel].nb_bytes);
			      //We check if all the data was successfully written
                if(written_len<chan_and_pids.channels[curr_channel].nb_bytes)
                {
				  //No ! 
                  if(written_len==-1)
                    log_message(MSG_DEBUG,"Error when writing to client %s:%d : %s\n",
                                inet_ntoa(actual_client->SocketAddr.sin_addr),
                                          actual_client->SocketAddr.sin_port,
                                          strerror(errno));
                  else
                    log_message(MSG_DEBUG,"Not all the data was written to %s:%d. Asked len : %d, written len %d\n",
                                inet_ntoa(actual_client->SocketAddr.sin_addr),
                                          actual_client->SocketAddr.sin_port,
                                          chan_and_pids.channels[curr_channel].nb_bytes,
                                          written_len);
                  if(!actual_client->consecutive_errors)
                  {
                    log_message(MSG_DETAIL,"Unicast : Error when writing to client %s:%d : %s\n",
                                inet_ntoa(actual_client->SocketAddr.sin_addr),
                                          actual_client->SocketAddr.sin_port,
                                          strerror(errno));
                    gettimeofday (&tv, (struct timezone *) NULL);
                    actual_client->first_error_time = tv.tv_sec;
                    actual_client->consecutive_errors=1;
                  }
                  else 
                  {
				      //We have actually errors, we check if we reached the timeout
                    gettimeofday (&tv, (struct timezone *) NULL);
                    if((unicast_vars.consecutive_errors_timeout > 0) && (tv.tv_sec - actual_client->first_error_time) > unicast_vars.consecutive_errors_timeout)
                    {
                      log_message(MSG_INFO,"Consecutive errors when writing to client %s:%d during too much time, we disconnect\n",
                                  inet_ntoa(actual_client->SocketAddr.sin_addr),
                                            actual_client->SocketAddr.sin_port);
                      temp_client=actual_client->chan_next;
                      unicast_close_connection(&unicast_vars,&fds,actual_client->Socket,chan_and_pids.channels);
                      actual_client=temp_client;
                    }
                  }
                }
                else if (actual_client->consecutive_errors)
                {
                  log_message(MSG_DETAIL,"We can write again to client %s:%d\n",
                              inet_ntoa(actual_client->SocketAddr.sin_addr),
                                        actual_client->SocketAddr.sin_port);
                  actual_client->consecutive_errors=0;
                }

                if(actual_client) //Can be null if the client was destroyed
                  actual_client=actual_client->chan_next;
              }
            }
            /********* END of UNICAST **********/
		      //If there is a rtp header we don't overwrite it
            if(rtp_header)
              chan_and_pids.channels[curr_channel].nb_bytes = RTP_HEADER_LEN;
            else
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
  log_message( MSG_INFO,
               "\nEnd of streaming. We streamed during %d:%02d:%02d\n",(tv.tv_sec - real_start_time)/3600,((tv.tv_sec - real_start_time) % 3600)/60,(tv.tv_sec - real_start_time) %60 );

  if(partial_packet_number)
    log_message( MSG_INFO,
                 "We received %d partial packets :-( \n",partial_packet_number );

  return mumudvb_close(Interrupted);
  
}

/** @brief Clean closing and freeing
 *
 *
 */
int mumudvb_close(int Interrupted)
{

  int curr_channel;

  if (Interrupted)
  {
    if(Interrupted< (1<<8)) //we check if it's a signal or a mumudvb error
      log_message( MSG_INFO, "\nCaught signal %d - closing cleanly.\n",
                   Interrupted);
    else
      log_message( MSG_INFO, "\nclosing cleanly. Error %d\n",Interrupted>>8);
  }

  for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
  {
    close (chan_and_pids.channels[curr_channel].socketOut);
    if(chan_and_pids.channels[curr_channel].socketIn>0)
      close (chan_and_pids.channels[curr_channel].socketIn); 
      //Free the channel structures
    if(chan_and_pids.channels[curr_channel].pmt_packet)
      free(chan_and_pids.channels[curr_channel].pmt_packet);
    chan_and_pids.channels[curr_channel].pmt_packet=NULL;
  }

  // we close the file descriptors
  close_card_fd (fds);

  //We close the unicast connections and free the clients
  unicast_freeing(&unicast_vars, chan_and_pids.channels);

#ifdef ENABLE_CAM_SUPPORT
  if(cam_vars.cam_support)
  {
      // stop CAM operation
    cam_stop(&cam_vars);
      // delete cam_info file
    if (remove (filename_cam_info))
    {
      log_message( MSG_WARN,
                   "%s: %s\n",
                   filename_cam_info, strerror (errno));
    }
  }
#endif

  //autoconf variables freeing
  autoconf_freeing(&autoconf_vars);

  //sap variables freeing
  if(sap_vars.sap_messages)
    free(sap_vars.sap_messages);
  
  //Pat rewrite freeing
  if(rewrite_vars.full_pat)
    free(rewrite_vars.full_pat);

  if ((write_streamed_channels)&&remove (filename_channels_diff)) 
  {
    log_message( MSG_WARN,
                 "%s: %s\n",
                 filename_channels_diff, strerror (errno));
    exit(ERROR_DEL_FILE);
  }

  if ((write_streamed_channels)&&remove (filename_channels_not_streamed))
  {
    log_message( MSG_WARN,
                 "%s: %s\n",
                 filename_channels_not_streamed, strerror (errno));
    exit(ERROR_DEL_FILE);
  }


  if (!no_daemon)
  {
    if (remove (filename_pid))
    {
      log_message( MSG_INFO, "%s: %s\n",
                   filename_pid, strerror (errno));
      exit(ERROR_DEL_FILE);
    }
  }

  /*free the file descriptors*/
  if(fds.pfds)
    free(fds.pfds);
  fds.pfds=NULL;
  if(unicast_vars.fd_info)
    free(unicast_vars.fd_info);
  unicast_vars.fd_info=NULL;

//    if(temp_buffer_from_dvr)
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
 *  It checks for the different timeouts : 
 *   Tuning, autoconfiguration ..
 *
 *  It updates the status of the channels
 *
 *  It shows the signal strenght
 *
 *  It check for the end of autoconfiguration
 * 
 * This function also catches SIGPIPE, SIGUSR1 and SIGUSR2
 * 
 ******************************************************/
static void SignalHandler (int signum)
{
  /** @todo : refactor */
  struct timeval tv;
  int curr_channel = 0;
  int count_of_active_channels=0;

  if (signum == SIGALRM && !Interrupted)
  {

    gettimeofday (&tv, (struct timezone *) NULL);
    now = tv.tv_sec - real_start_time;

    if (tuneparams.display_strenght && tuneparams.card_tuned)
      show_power (fds);

    if (!tuneparams.card_tuned)
    {
      log_message( MSG_INFO,
                   "Card not tuned after %ds - exiting\n",
                   tuneparams.tuning_timeout);
      exit(ERROR_TUNE);
    }

      //autoconfiguration
      //We check if we reached the autoconfiguration timeout
    if(autoconf_vars.autoconfiguration)
      Interrupted = autoconf_poll(now, &autoconf_vars, &chan_and_pids, tuneparams, multicast_vars, &fds, &unicast_vars);
    else //we are not doing autoconfiguration we can do something else
    {
      //sap announces
      sap_poll(&sap_vars,chan_and_pids.number_of_channels,chan_and_pids.channels,multicast_vars, now);


	  // Check if the chanel stream state has changed
      for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
      {
        if ((chan_and_pids.channels[curr_channel].streamed_channel/ALARM_TIME >= 80) && (!chan_and_pids.channels[curr_channel].streamed_channel_old))
        {
          log_message( MSG_INFO,
                       "Channel \"%s\" back.Card %d\n",
                       chan_and_pids.channels[curr_channel].name, tuneparams.card);
          chan_and_pids.channels[curr_channel].streamed_channel_old = 1;	// update
          if(sap_vars.sap == SAP_ON)
            sap_update(chan_and_pids.channels[curr_channel], &sap_vars, curr_channel, multicast_vars); //Channel status changed, we update the sap announces
        }
        else if ((chan_and_pids.channels[curr_channel].streamed_channel_old) && (chan_and_pids.channels[curr_channel].streamed_channel/ALARM_TIME < 30))
        {
          log_message( MSG_INFO,
                       "Channel \"%s\" down.Card %d\n",
                       chan_and_pids.channels[curr_channel].name, tuneparams.card);
          chan_and_pids.channels[curr_channel].streamed_channel_old = 0;	// update
          if(sap_vars.sap == SAP_ON)
            sap_update(chan_and_pids.channels[curr_channel], &sap_vars, curr_channel, multicast_vars); //Channel status changed, we update the sap announces
        }
      }


	  //compute the bandwith occupied by each channel
      float time_interval;
      if(!compute_traffic_time)
        compute_traffic_time=now;
      if((now-compute_traffic_time)>=compute_traffic_interval)
      {
        time_interval=now+tv.tv_usec/1000000-compute_traffic_time-compute_traffic_time_usec/1000000;
        compute_traffic_time=now;
        compute_traffic_time_usec=tv.tv_usec;
        for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
        {
          chan_and_pids.channels[curr_channel].traffic=((float)chan_and_pids.channels[curr_channel].sent_data)/time_interval*1/1204;
          chan_and_pids.channels[curr_channel].sent_data=0;
        }
      }

            //show the bandwith measurement
      if(show_traffic)
      {
        if(!show_traffic_time)
          show_traffic_time=now;
        if((now-show_traffic_time)>=show_traffic_interval)
        {
          show_traffic_time=now;
          for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
          {
            log_message( MSG_INFO, "Traffic :  %.2f kB/s \t  for channel \"%s\"\n",
                         chan_and_pids.channels[curr_channel].traffic,
                         chan_and_pids.channels[curr_channel].name);
          }
        }
      }


      /*Show the statistics for the big buffer*/
      if(show_buffer_stats)
      {
        if(!show_buffer_stats_time)
          show_buffer_stats_time=now;
        if((now-show_buffer_stats_time)>=show_buffer_stats_interval)
        {
          show_buffer_stats_time=now;
          log_message( MSG_DETAIL, "DETAIL : Average packets in the buffer %d\n", stats_num_packets_received/stats_num_reads);
          stats_num_packets_received=0;
          stats_num_reads=0;
        }
      }



      // Check if the chanel scrambling state has changed
      for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
      {
	      // Calcultation of the ratio (percentage) of scrambled packets received
        if (chan_and_pids.channels[curr_channel].num_packet >0 && chan_and_pids.channels[curr_channel].scrambled_channel>10)
          chan_and_pids.channels[curr_channel].ratio_scrambled = (int)(chan_and_pids.channels[curr_channel].scrambled_channel*100/(chan_and_pids.channels[curr_channel].num_packet));
        else
          chan_and_pids.channels[curr_channel].ratio_scrambled = 0;

	      // Test if we have only unscrambled packets (<2%) - scrambled_channel_old=FULLY_UNSCRAMBLED : fully unscrambled
        if ((chan_and_pids.channels[curr_channel].ratio_scrambled < 2) && (chan_and_pids.channels[curr_channel].scrambled_channel_old != FULLY_UNSCRAMBLED))
        {
          log_message( MSG_INFO,
                       "Channel \"%s\" is now fully unscrambled (%d%% of scrambled packets). Card %d\n",
                       chan_and_pids.channels[curr_channel].name, chan_and_pids.channels[curr_channel].ratio_scrambled, tuneparams.card);
          chan_and_pids.channels[curr_channel].scrambled_channel_old = FULLY_UNSCRAMBLED;// update
        }
	      // Test if we have partiallay unscrambled packets (5%<=ratio<=80%) - scrambled_channel_old=PARTIALLY_UNSCRAMBLED : partially unscrambled
        if ((chan_and_pids.channels[curr_channel].ratio_scrambled >= 5) && (chan_and_pids.channels[curr_channel].ratio_scrambled <= 80) && (chan_and_pids.channels[curr_channel].scrambled_channel_old != PARTIALLY_UNSCRAMBLED))
        {
          log_message( MSG_INFO,
                       "Channel \"%s\" is now partially unscrambled (%d%% of scrambled packets). Card %d\n",
                       chan_and_pids.channels[curr_channel].name, chan_and_pids.channels[curr_channel].ratio_scrambled, tuneparams.card);
          chan_and_pids.channels[curr_channel].scrambled_channel_old = PARTIALLY_UNSCRAMBLED;// update
        }
	      // Test if we have nearly only scrambled packets (>90%) - scrambled_channel_old=HIGHLY_SCRAMBLED : highly scrambled
        if ((chan_and_pids.channels[curr_channel].ratio_scrambled > 90) && chan_and_pids.channels[curr_channel].scrambled_channel_old != HIGHLY_SCRAMBLED)
        {
          log_message( MSG_INFO,
                       "Channel \"%s\" is now higly scrambled (%d%% of scrambled packets). Card %d\n",
                       chan_and_pids.channels[curr_channel].name, chan_and_pids.channels[curr_channel].ratio_scrambled, tuneparams.card);
          chan_and_pids.channels[curr_channel].scrambled_channel_old = HIGHLY_SCRAMBLED;// update
        }
      }

      /*******************************************/
	  // we count active channels
      /*******************************************/
      for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
        if (chan_and_pids.channels[curr_channel].streamed_channel_old)
          count_of_active_channels++;

	  //Time no diff is the time when we got 0 active channels
	  //if we have active channels, we reinit this counter
      if(count_of_active_channels)
        time_no_diff=0;
	  //If we don't have active channels and this is the first time, we store the time
      else if(!time_no_diff)
        time_no_diff=now;

	  //If we don't stream data for a too long time, we exit
      if((timeout_no_diff)&& (time_no_diff&&((now-time_no_diff)>timeout_no_diff)))
      {
        log_message( MSG_INFO,
                     "No data from card %d in %ds, exiting.\n",
                     tuneparams.card, timeout_no_diff);
        Interrupted=ERROR_NO_DIFF<<8; //the <<8 is to make difference beetween signals and errors
      }

	  //generation of the files wich says the streamed channels
      if (write_streamed_channels)
        gen_file_streamed_channels(filename_channels_diff, filename_channels_not_streamed, chan_and_pids.number_of_channels, chan_and_pids.channels);

	  // reinit
      for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
      {
        chan_and_pids.channels[curr_channel].streamed_channel = 0;
        chan_and_pids.channels[curr_channel].num_packet = 0;
        chan_and_pids.channels[curr_channel].scrambled_channel = 0;
      }

#ifdef ENABLE_CAM_SUPPORT
      if(cam_vars.cam_support)
        cam_vars.delay++;
#endif
    }
    alarm (ALARM_TIME);
  }
  else if (signum == SIGUSR1)
  {
    tuneparams.display_strenght = tuneparams.display_strenght ? 0 : 1;
  }
  else if (signum == SIGUSR2)
  {
    show_traffic = show_traffic ? 0 : 1;
    if(show_traffic)
      log_message(MSG_INFO,"The traffic will be shown every %d seconds\n",show_traffic_interval);
    else
      log_message(MSG_INFO,"The traffic will not be shown anymore\n");
  }
  else if (signum != SIGPIPE)
  {
    Interrupted = signum;
  }
  signal (signum, SignalHandler);
}

/** @brief : poll the file descriptors fds with a limit in the number of errors
 *
 * @param fds : the file descriptors
 */
int mumudvb_poll(fds_t *fds)
{
  int poll_try;
  int poll_eintr=0;
  int last_poll_error;
  int Interrupted;

  poll_try=0;
  poll_eintr=0;
  last_poll_error=0;
  while((poll (fds->pfds, fds->pfdsnum, 500)<0)&&(poll_try<MAX_POLL_TRIES))
  {
    if(errno != EINTR) //EINTR means Interrupted System Call, it normally shouldn't matter so much so we don't count it for our Poll tries
    {
      poll_try++;
      last_poll_error=errno;
    }
    else
    {
      poll_eintr++;
      if(poll_eintr==10)
      {
        log_message( MSG_DEBUG, "Poll : 10 successive EINTR\n");
        poll_eintr=0;
      }
    }
    /**@todo : put a maximum number of interrupted system calls per unit time*/
  }
  
  if(poll_try==MAX_POLL_TRIES)
  {
    log_message( MSG_ERROR, "Poll : We reach the maximum number of polling tries\n\tLast error when polling: %s\n", strerror (errno));
    Interrupted=errno<<8; //the <<8 is to make difference beetween signals and errors;
    return Interrupted;
  }
  else if(poll_try)
  {
    log_message( MSG_WARN, "Poll : Warning : error when polling: %s\n", strerror (last_poll_error));
  }
  return 0;
}
