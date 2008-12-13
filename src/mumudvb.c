/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2008 Brice DUBOST
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

#include "mumudvb.h"
#include "tune.h"
#include "udp.h"
#include "dvb.h"
#include "cam.h"
#include "ts.h"
#include "errors.h"
#include "autoconf.h"
#include "sap.h"

extern uint32_t       crc32_table[256];

int multicast_ttl=DEFAULT_TTL;

/* Signal handling code shamelessly copied from VDR by Klaus Schmidinger 
   - see http://www.cadsoft.de/people/kls/vdr/index.htm */

// global variables used by SignalHandler
long now;
long time_no_diff;
long real_start_time;

int display_signal_strenght = 0; //do we periodically show the signal strenght ?

int no_daemon = 0; //do we deamonize mumudvb ?

int number_of_channels;

mumudvb_channel_t channels[MAX_CHANNELS]; //the channels...

int card = 0;
int card_tuned = 0;
int timeout_accord = ALARM_TIME_TIMEOUT;
int timeout_no_diff = ALARM_TIME_TIMEOUT_NO_DIFF;

int Interrupted = 0;
char nom_fich_chaines_diff[256];
char nom_fich_chaines_non_diff[256];
char nom_fich_pid[256];
int  write_streamed_channels=1;

//sap announces C99 initialisation
sap_parameters_t sap_vars={
  .sap_messages=NULL, 
  .sap=0, //No sap by default
  .sap_interval=SAP_DEFAULT_INTERVAL,
  .sap_sending_ip="0.0.0.0",
  .sap_default_group="",
  .sap_organisation="none",
};

//autoconfiguration. C99 initialisation
autoconf_parameters_t autoconf_vars={
  .autoconfiguration=0,
  .autoconf_ip_header="239.100",
  .time_start_autoconfiguration=0,
  .autoconf_temp_pmt=NULL,
  .autoconf_temp_pat=NULL,
  .autoconf_temp_sdt=NULL,
  .services=NULL,
  };

//CAM (Conditionnal Access Modules : for scrambled channels) C99 initialisation
cam_parameters_t cam_vars={
  .cam_support = 0,
  .cam_number=0,
  .cam_pmt_ptr=NULL,
#ifdef LIBDVBEN50221
  .tl=NULL,
  .sl=NULL,
  .stdcam=NULL,
  .ca_resource_connected=0,
  .seenpmt=0,
  .delay=0,
#else
#endif
};

//logging
int log_initialised=0;
int verbosity = 1;


// file descriptors
fds_t fds; // defined in dvb.h

// prototypes
static void SignalHandler (int signum);
int mumudvb_close(int Interrupted);


void
usage (char *name)
{
  fprintf (stderr, "mumudvb is a program who can redistribute stream from DVB on a network, in multicast.\n It's main feature is to take a whole transponder and put each channel on a different multicast IP.\n\n"
	   "Usage: %s [options] \n"
	   "-c, --config : Config file\n"
	   "-s, --signal : Display signal power\n"
	   "-d, --debug  : Don't deamonize\n"
	   "-v           : More verbose\n"
	   "-q           : Less verbose\n"
	   "-h, --help   : Help\n"
	   "\n"
	   "%s Version "
	   VERSION
	   "\n"
	   "Based on dvbstream 0.6 by (C) Dave Chapman 2001-2004\n"
	   "Released under the GPL.\n"
	   "Latest version available from http://mumudvb.braice.net/\n"
	   "Project from the cr@ns (www.crans.org)\n"
	   "by Brice DUBOST (mumudvb@braice.net)\n", name, name);
}


int
main (int argc, char **argv)
{
  int k,i;

  //polling of the dvr device
  struct pollfd pfds[2];	//  DVR device
  int poll_try;
  int last_poll_error;


  // Tuning parapmeters
  //TODO : put all of them in a structure.
  unsigned long freq = 0;
  unsigned long srate = 0;
  char pol = 0;
  fe_spectral_inversion_t specInv = INVERSION_AUTO;
  int tone = -1;
  //DVB-T parameters
  fe_modulation_t modulation = CONSTELLATION_DEFAULT;
  fe_transmit_mode_t TransmissionMode = TRANSMISSION_MODE_DEFAULT;
  fe_bandwidth_t bandWidth = BANDWIDTH_DEFAULT;
  fe_guard_interval_t guardInterval = GUARD_INTERVAL_DEFAULT;
  fe_code_rate_t HP_CodeRate = HP_CODERATE_DEFAULT, LP_CodeRate =
    LP_CODERATE_DEFAULT;
  //TODO : check frontend capabilities
  fe_hierarchy_t hier = HIERARCHY_DEFAULT;
  uint8_t diseqc = 0; //satellite number 


  //MPEG2-TS reception and sort
  int pid;			// pid of the current mpeg2 packet
  int bytes_read;		// number of bytes actually read
  //temporary buffers
  unsigned char temp_buffer_from_dvr[TS_PACKET_SIZE];
  unsigned char saved_pat_buffer[TS_PACKET_SIZE];
  //Mandatory pids
  int mandatory_pid[MAX_MANDATORY_PID_NUMBER];

  struct timeval tv;

  //files
  char *conf_filename = NULL;
  FILE *conf_file;
  FILE *chaines_diff;
  FILE *chaines_non_diff;
  FILE *pidfile;

  // configuration file parsing
  int curr_channel = 0;
  int curr_pid = 0;
  int curr_pid_mandatory = 0;
  int send_packet=0;
  int port_ok = 0;
  int common_port = 0;
  int ip_ok = 0;
  char current_line[CONF_LINELEN];
  char *substring=NULL;
  char delimiteurs[] = " =";


  uint8_t hi_mappids[8192];
  uint8_t lo_mappids[8192];


  int alarm_count = 0;
  int count_non_transmis = 0;
  int tune_retval=0;


  //do we rewrite the pat pid ?
  int rewrite_pat = 0;

  // Initialise PID map
  for (k = 0; k < 8192; k++)
    {
      hi_mappids[k] = (k >> 8);
      lo_mappids[k] = (k & 0xff);
    }



  /******************************************************/
  //Getopt
  /******************************************************/
  const char short_options[] = "c:sdhvq";
  const struct option long_options[] = {
    {"config", required_argument, NULL, 'c'},
    {"signal", no_argument, NULL, 's'},
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
	      log_message( MSG_INFO, "malloc() failed: %s\n", strerror(errno));
	      exit(errno);
	    }
	  strncpy (conf_filename, optarg, strlen (optarg) + 1);
	  break;
	case 's':
	  display_signal_strenght = 1;
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
      daemon(42,0);

  //we open the descriptor for syslog
  if (!no_daemon)
    openlog ("MUMUDVB", LOG_PID, 0);
  log_initialised=1;

  /******************************************************/
  // config file reading
  /******************************************************/
  conf_file = fopen (conf_filename, "r");
  if (conf_file == NULL)
    {
      log_message( MSG_INFO, "%s: %s\n",
		conf_filename, strerror (errno));
      free(conf_filename);
      exit(ERROR_CONF_FILE);
    }
  free(conf_filename);

  //paranoya we clear all the content of all the channels
  for(curr_channel=0;curr_channel<MAX_CHANNELS;curr_channel++)
    memset (&channels[curr_channel], 0, sizeof (channels[curr_channel]));

  curr_channel=0;
  // we scan config file
  // see doc/README-conf for further information
  while (fgets (current_line, CONF_LINELEN, conf_file))
    {
      substring = strtok (current_line, delimiteurs);
      //commentary
      if (substring[0] == '#')
	continue; 

      if (!strcmp (substring, "timeout_accord"))
	{
	  substring = strtok (NULL, delimiteurs);	// on extrait la sous chaine
	  timeout_accord = atoi (substring);
	}
      else if (!strcmp (substring, "timeout_no_diff"))
	{
	  substring = strtok (NULL, delimiteurs);
	  timeout_no_diff= atoi (substring);
	}
      else if (!strcmp (substring, "rewrite_pat"))
	{
	  substring = strtok (NULL, delimiteurs);
	  rewrite_pat = atoi (substring);
	  if(rewrite_pat)
	    {
	      log_message( MSG_INFO,
			"You have enabled the Pat Rewriting, it has still some limitations please contact if you have some issues\n");
	    }
	}
      else if (!strcmp (substring, "cam_support"))
	{
	  substring = strtok (NULL, delimiteurs);
	  cam_vars.cam_support = atoi (substring);
	  if(cam_vars.cam_support)
	    {
	      log_message( MSG_WARN,
			"!!! You have enabled the support for conditionnal acces modules (scrambled channels), this is a beta feature.Please report any bug/comment\n");
#ifndef LIBDVBEN50221
	      log_message( MSG_DEBUG,
			"       You will use VLC code for cam support\n");
#else
	      log_message( MSG_DEBUG,
			"       You will use libdvben50221 for cam support\n");
#endif
	    }
	}
      else if (!strcmp (substring, "sat_number"))
	{
	  substring = strtok (NULL, delimiteurs);
	  diseqc = atoi (substring);
	}
      else if (!strcmp (substring, "autoconfiguration"))
	{
	  substring = strtok (NULL, delimiteurs);
	  autoconf_vars.autoconfiguration = atoi (substring);
	  if((autoconf_vars.autoconfiguration==1)||(autoconf_vars.autoconfiguration==2))
	    {
	      log_message( MSG_WARN,
			"!!! You have enabled the support for autoconfiguration, this is a beta feature.Please report any bug/comment\n");
	    }
	  else
	    autoconf_vars.autoconfiguration=0;
	  if(autoconf_vars.autoconfiguration==2)
	    {
	      log_message( MSG_INFO,
			"Full autoconfiguration, we activate SAP announces. if you want to desactivate them see the README.\n");
	      sap_vars.sap=1;
	    }
	}
      else if (!strcmp (substring, "autoconf_ip_header"))
	{
	  substring = strtok (NULL, delimiteurs);
	  if(strlen(substring)>7)
	    {
	      log_message( MSG_ERROR,
			   "The sap sending ip is too long\n");
	      exit(ERROR_CONF);
	    }
	  sscanf (substring, "%s\n", autoconf_vars.autoconf_ip_header);
	}
      else if (!strcmp (substring, "sap"))
	{
	  substring = strtok (NULL, delimiteurs);
	  sap_vars.sap = atoi (substring);
	  if(sap_vars.sap)
	    {
	      log_message( MSG_WARN,
			"!!! You have enabled the support for sap announces, this is a beta feature.Please report any bug/comment\n");
	    }
	}
      else if (!strcmp (substring, "sap_interval"))
	{
	  substring = strtok (NULL, delimiteurs);
	  sap_vars.sap_interval = atoi (substring);
	}
      else if (!strcmp (substring, "sap_organisation"))
	{
	  // other substring extraction method in order to keep spaces
	  substring = strtok (NULL, "=");
	  if (!(strlen (substring) >= 255 - 1))
	    strcpy(sap_vars.sap_organisation,strtok(substring,"\n"));	
	  else
	    {
		log_message( MSG_INFO,"Sap Organisation name too long\n");
	    }
	}
      else if (!strcmp (substring, "sap_sending_ip"))
	{
	  substring = strtok (NULL, delimiteurs);
	  if(strlen(substring)>19)
	    {
	      log_message( MSG_ERROR,
			   "The sap sending ip is too long\n");
	      exit(ERROR_CONF);
	    }
	  sscanf (substring, "%s\n", sap_vars.sap_sending_ip);
	}
      else if (!strcmp (substring, "freq"))
	{
	  substring = strtok (NULL, delimiteurs);
	  freq = atol (substring);
	  freq *= 1000UL;
	}
      else if (!strcmp (substring, "pol"))
	{
	  substring = strtok (NULL, delimiteurs);
	  if (tolower (substring[0]) == 'v')
	    {
	      pol = 'V';
	    }
	  else if (tolower (substring[0]) == 'h')
	    {
	      pol = 'H';
	    }
	  else
	    {
	      log_message( MSG_INFO,
			   "Config issue : %s polarisation\n",
			   conf_filename);
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (substring, "srate"))
	{
	  substring = strtok (NULL, delimiteurs);
	  srate = atol (substring);
	  srate *= 1000UL;
	}
      else if (!strcmp (substring, "card"))
	{
	  substring = strtok (NULL, delimiteurs);
	  card = atoi (substring);
	}
      else if (!strcmp (substring, "cam_number"))
	{
	  substring = strtok (NULL, delimiteurs);
	  cam_vars.cam_number = atoi (substring);
	}
      else if (!strcmp (substring, "ip"))
	{
	  substring = strtok (NULL, delimiteurs);
          if(strlen(substring)>19)
            {
              log_message( MSG_ERROR,
                           "The Ip address %s is too long.\n", substring);
              exit(ERROR_CONF);
            }
	  sscanf (substring, "%s\n", channels[curr_channel].ipOut);
	  ip_ok = 1;
	}
      else if (!strcmp (substring, "sap_group"))
	{
	  if (sap_vars.sap==0)
	    {
	      log_message( MSG_WARN,
			"Warning : you have not activated sap, the sap group will not be taken in account\n");

	    }
	  substring = strtok (NULL, "=");
	  if(strlen(substring)>19)
	    {
	      log_message( MSG_ERROR,
			   "The sap group is too long\n");
	      exit(ERROR_CONF);
	    }
	  sscanf (substring, "%s\n", channels[curr_channel].sap_group);
	}
      else if (!strcmp (substring, "sap_default_group"))
	{
	  if (sap_vars.sap==0)
	    {
	      log_message( MSG_WARN,
			"Warning : you have not activated sap, the sap group will not be taken in account\n");

	    }
	  substring = strtok (NULL, "=");
	  if(strlen(substring)>19)
	    {
	      log_message( MSG_ERROR,
			   "The sap default group is too long\n");
	      exit(ERROR_CONF);
	    }
	  sscanf (substring, "%s\n", sap_vars.sap_default_group);
	}
      else if (!strcmp (substring, "common_port"))
	{
	  substring = strtok (NULL, delimiteurs);
	  common_port = atoi (substring);
	}
      else if (!strcmp (substring, "multicast_ttl"))
	{
	  substring = strtok (NULL, delimiteurs);
	  multicast_ttl = atoi (substring);
	}
      else if (!strcmp (substring, "port"))
	{
	  substring = strtok (NULL, delimiteurs);
	  channels[curr_channel].portOut = atoi (substring);
	  port_ok = 1;
	}
      else if (!strcmp (substring, "cam_pmt_pid"))
	{
	  if ((port_ok == 0 && common_port==0)|| ip_ok == 0)
	    {
	      log_message( MSG_INFO,
			"You must precise ip and port before PIDs\n");
	      exit(ERROR_CONF);
	    }
	  substring = strtok (NULL, delimiteurs);
      	  channels[curr_channel].cam_pmt_pid = atoi (substring);
	  if (channels[curr_channel].cam_pmt_pid < 10 || channels[curr_channel].cam_pmt_pid > 8191){
	      log_message( MSG_INFO,
		      "Config issue : %s in pids, given pid : %d\n",
		      conf_filename, channels[curr_channel].cam_pmt_pid);
	    exit(ERROR_CONF);
	  }
	}
      else if (!strcmp (substring, "pids"))
	{
	  if ((port_ok == 0 && common_port==0)|| ip_ok == 0)
	    {
		log_message( MSG_INFO,
			"You must precise ip and port before PIDs\n");
	      exit(ERROR_CONF);
	    }
	  if (common_port!=0)
	    channels[curr_channel].portOut = common_port;
	  while ((substring = strtok (NULL, delimiteurs)) != NULL)
	    {
	      channels[curr_channel].pids[curr_pid] = atoi (substring);
	      // we see if the given pid is good
	      if (channels[curr_channel].pids[curr_pid] < 10 || channels[curr_channel].pids[curr_pid] > 8191)
		{
		  log_message( MSG_INFO,
			    "Config issue : %s in pids, given pid : %d\n",
			    conf_filename, channels[curr_channel].pids[curr_pid]);
		  exit(ERROR_CONF);
		}
	      curr_pid++;
	      if (curr_pid >= MAX_PIDS_PAR_CHAINE)
		{
		  log_message( MSG_INFO,
			       "Too many pids : %d channel : %d\n",
			       curr_pid, curr_channel);
		  exit(ERROR_CONF);
		}
	    }
	  channels[curr_channel].num_pids = curr_pid;
	  curr_pid = 0;
	  curr_channel++;

      	  channels[curr_channel].cam_pmt_pid = 0; //paranoya
	  port_ok = 0;
	  ip_ok = 0;
	}
      else if (!strcmp (substring, "name"))
	{
	  // other substring extraction method in order to keep spaces
	  substring = strtok (NULL, "=");
	  if (!(strlen (substring) >= MAX_NAME_LEN - 1))
	    strcpy(channels[curr_channel].name,strtok(substring,"\n"));	
	  else
	    {
		log_message( MSG_INFO,"Channel name too long\n");
	    }
	}
      else if (!strcmp (substring, "qam"))
	{
	  // DVB-T
	  substring = strtok (NULL, delimiteurs);
	  sscanf (substring, "%s\n", substring);
	  if (!strcmp (substring, "qpsk"))
	    modulation=QPSK;
	  else if (!strcmp (substring, "16"))
	    modulation=QAM_16;
	  else if (!strcmp (substring, "32"))
	    modulation=QAM_32;
	  else if (!strcmp (substring, "64"))
	    modulation=QAM_64;
	  else if (!strcmp (substring, "128"))
	    modulation=QAM_128;
	  else if (!strcmp (substring, "256"))
	    modulation=QAM_256;
	  else if (!strcmp (substring, "auto"))
	    modulation=QAM_AUTO;
	  else
	    {
		log_message( MSG_INFO,
			"Config issue : QAM\n");
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (substring, "trans_mode"))
	{
	  // DVB-T
	  substring = strtok (NULL, delimiteurs);
	  sscanf (substring, "%s\n", substring);
	  if (!strcmp (substring, "2k"))
	    TransmissionMode=TRANSMISSION_MODE_2K;
	  else if (!strcmp (substring, "8k"))
	    TransmissionMode=TRANSMISSION_MODE_8K;
	  else if (!strcmp (substring, "auto"))
	    TransmissionMode=TRANSMISSION_MODE_AUTO;
	  else
	    {
		log_message( MSG_INFO,
			"Config issue : trans_mode\n");
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (substring, "bandwidth"))
	{
	  // DVB-T
	  substring = strtok (NULL, delimiteurs);
	  sscanf (substring, "%s\n", substring);
	  if (!strcmp (substring, "8MHz"))
	    bandWidth=BANDWIDTH_8_MHZ;
	  else if (!strcmp (substring, "7MHz"))
	    bandWidth=BANDWIDTH_7_MHZ;
	  else if (!strcmp (substring, "6MHz"))
	    bandWidth=BANDWIDTH_6_MHZ;
	  else if (!strcmp (substring, "auto"))
	    bandWidth=BANDWIDTH_AUTO;
	  else
	    {
		log_message( MSG_INFO,
			"Config issue : bandwidth\n");
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (substring, "guardinterval"))
	{
	  // DVB-T
	  substring = strtok (NULL, delimiteurs);
	  sscanf (substring, "%s\n", substring);
	  if (!strcmp (substring, "1/32"))
	    guardInterval=GUARD_INTERVAL_1_32;
	  else if (!strcmp (substring, "1/16"))
	    guardInterval=GUARD_INTERVAL_1_16;
	  else if (!strcmp (substring, "1/8"))
	    guardInterval=GUARD_INTERVAL_1_8;
	  else if (!strcmp (substring, "1/4"))
	    guardInterval=GUARD_INTERVAL_1_4;
	  else if (!strcmp (substring, "auto"))
	    guardInterval=GUARD_INTERVAL_AUTO;
	  else
	    {
		log_message( MSG_INFO,
			"Config issue : guardinterval\n");
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (substring, "coderate"))
	{
	  // DVB-T
	  substring = strtok (NULL, delimiteurs);
	  sscanf (substring, "%s\n", substring);
	  if (!strcmp (substring, "none"))
	    HP_CodeRate=FEC_NONE;
	  else if (!strcmp (substring, "1/2"))
	    HP_CodeRate=FEC_1_2;
	  else if (!strcmp (substring, "2/3"))
	    HP_CodeRate=FEC_2_3;
	  else if (!strcmp (substring, "3/4"))
	    HP_CodeRate=FEC_3_4;
	  else if (!strcmp (substring, "4/5"))
	    HP_CodeRate=FEC_4_5;
	  else if (!strcmp (substring, "5/6"))
	    HP_CodeRate=FEC_5_6;
	  else if (!strcmp (substring, "6/7"))
	    HP_CodeRate=FEC_6_7;
	  else if (!strcmp (substring, "7/8"))
	    HP_CodeRate=FEC_7_8;
	  else if (!strcmp (substring, "8/9"))
	    HP_CodeRate=FEC_8_9;
	  else if (!strcmp (substring, "auto"))
	    HP_CodeRate=FEC_AUTO;
	  else
	    {
	      log_message( MSG_INFO,
			"Config issue : coderate\n");
	      exit(ERROR_CONF);
	    }
	  LP_CodeRate=HP_CodeRate; // I don't know what it exactly does, but it works like that
	}
      else
	{
	  if(strlen (current_line) > 1)
	    log_message( MSG_INFO,
			 "Config issue : unknow symbol : %s\n\n", substring);
	  continue;
	}
    }
  fclose (conf_file);
  /******************************************************/
  //end of config file reading
  /******************************************************/


  number_of_channels = curr_channel;
  if (curr_channel > MAX_CHANNELS)
    {
      log_message( MSG_INFO, "Too many channels : %d limit : %d\n",
		   curr_channel, MAX_CHANNELS);
      exit(ERROR_TOO_CHANNELS);
    }

  // we clear it by paranoia
  sprintf (nom_fich_chaines_diff, "/var/run/mumudvb/chaines_diffusees_carte%d",
	   card);
  sprintf (nom_fich_chaines_non_diff, "/var/run/mumudvb/chaines_non_diffusees_carte%d",
	   card);
  chaines_diff = fopen (nom_fich_chaines_diff, "w");
  if (chaines_diff == NULL)
    {
      write_streamed_channels=0;
      log_message( MSG_WARN,
		   "WARNING : Can't create %s: %s\n",
		   nom_fich_chaines_diff, strerror (errno));
      //exit(ERROR_CREATE_FILE);
    }
  else
    fclose (chaines_diff);

  chaines_non_diff = fopen (nom_fich_chaines_non_diff, "w");
  if (chaines_diff == NULL)
    {
      write_streamed_channels=0;
      log_message( MSG_WARN,
		   "WARNING : Can't create %s: %s\n",
		   nom_fich_chaines_non_diff, strerror (errno));
      //exit(ERROR_CREATE_FILE);
    }
  else
    fclose (chaines_non_diff);

  
  log_message( MSG_INFO, "Streaming. Freq %lu pol %c srate %lu\n",
	       freq, pol, srate);


  /******************************************************/
  // Card tuning
  /******************************************************/
  // alarm for tuning timeout
  if (signal (SIGALRM, SignalHandler) == SIG_IGN)
    signal (SIGALRM, SIG_IGN);
  if (signal (SIGUSR1, SignalHandler) == SIG_IGN)
    signal (SIGUSR1, SIG_IGN);
  alarm (timeout_accord);

  // We tune the card
  tune_retval =-1;

  if ((freq > 100000000))
    {
      if (open_fe (&fds.fd_frontend, card))
	{
	  tune_retval =
	    tune_it (fds.fd_frontend, freq, srate, 0, tone, specInv, diseqc,
		     modulation, HP_CodeRate, TransmissionMode, guardInterval,
		     bandWidth, LP_CodeRate, hier, display_signal_strenght);
	}
    }
  else if ((freq != 0) && (pol != 0) && (srate != 0))
    {
      if (open_fe (&fds.fd_frontend, card))
	{
	  tune_retval =
	    tune_it (fds.fd_frontend, freq, srate, pol, tone, specInv, diseqc,
		     modulation, HP_CodeRate, TransmissionMode, guardInterval,
		     bandWidth, LP_CodeRate, hier, display_signal_strenght);
	}
    }

  if (tune_retval < 0)
    {
      log_message( MSG_INFO, "Tunning issue, card %d\n", card);
      
      // we close the file descriptors
      close_card_fd (number_of_channels, channels, fds);
      exit(ERROR_TUNE);
    }

  log_message( MSG_INFO, "Card %d tuned\n", card);
  card_tuned = 1;
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
  alarm (ALARM_TIME);

  /*****************************************************/
  //cam_support
  /*****************************************************/

  if(cam_vars.cam_support){
    cam_vars.cam_pmt_ptr=malloc(sizeof(mumudvb_ts_packet_t));
#ifndef LIBDVBEN50221
    cam_vars.cam_sys_access=malloc(sizeof(access_sys_t));
    //cleaning
    for ( i = 0; i < MAX_PROGRAMS; i++ )
      cam_vars.cam_sys_access->pp_selected_programs[i]=NULL;
    CAMOpen(cam_vars.cam_sys_access, card, cam_vars.cam_number);
    cam_vars.cam_sys_access->cai->initialized=0;
    cam_vars.cam_sys_access->cai->ready=0;
    CAMPoll(cam_vars.cam_sys_access); //TODO : check why it sometimes fails --> look if the failure is associated with a non ready slot
#else
    //We initialise the cam. If fail, we remove cam support
    if(cam_start(&cam_vars,card))
      {
	log_message( MSG_ERROR,"Cannot initalise cam\n");
	cam_vars.cam_support=0;
      }

#endif
  }
  
  /*****************************************************/
  //autoconfiguration
  //memory allocation for MPEG2-TS
  //packet structures
  /*****************************************************/

  if(autoconf_vars.autoconfiguration)
    {
      autoconf_vars.autoconf_temp_pmt=malloc(sizeof(mumudvb_ts_packet_t));
      if(autoconf_vars.autoconf_temp_pmt==NULL)
	{
	  log_message( MSG_ERROR,"MALLOC\n");
	  return mumudvb_close(100<<8);
	}
      memset (autoconf_vars.autoconf_temp_pmt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
    }
  if(autoconf_vars.autoconfiguration==2)
    {
      if(common_port==0)
	common_port=1234;
      autoconf_vars.autoconf_temp_pat=malloc(sizeof(mumudvb_ts_packet_t));
      if(autoconf_vars.autoconf_temp_pat==NULL)
	{
	  log_message( MSG_ERROR,"MALLOC\n");
	  return mumudvb_close(100<<8);
	}
      memset (autoconf_vars.autoconf_temp_pat, 0, sizeof( mumudvb_ts_packet_t));//we clear it
      autoconf_vars.autoconf_temp_sdt=malloc(sizeof(mumudvb_ts_packet_t));
      if(autoconf_vars.autoconf_temp_sdt==NULL)
	{
	  log_message( MSG_ERROR,"MALLOC\n");
	  return mumudvb_close(100<<8);
	  
	}
      memset (autoconf_vars.autoconf_temp_sdt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
      autoconf_vars.services=malloc(sizeof(mumudvb_service_t));
      if(autoconf_vars.services==NULL)
	{
	  log_message( MSG_ERROR,"MALLOC\n");
	  return mumudvb_close(100<<8);
	}
      memset (autoconf_vars.services, 0, sizeof( mumudvb_service_t));//we clear it

    }

  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      channels[curr_channel].nb_bytes=0;
      //If there is more than one pid in one channel we mark it
      //For no autoconfiguration
      if(autoconf_vars.autoconfiguration==1 && channels[curr_channel].num_pids>1)
	{
	  log_message( MSG_DETAIL, "Autoconf : Autoconfiguration desactivated for channel \"%s\" \n", channels[curr_channel].name);
	  channels[curr_channel].autoconfigurated=1;
	}
    }


  /*****************************************************/
  //daemon part two, we write our PID
  /*****************************************************/

  // We write our pid in a file if we deamonize
  if (!no_daemon)
    {
      sprintf (nom_fich_pid, "/var/run/mumudvb/mumudvb_carte%d.pid", card);
      pidfile = fopen (nom_fich_pid, "w");
      if (pidfile == NULL)
	{
	  log_message( MSG_INFO,"%s: %s\n",
		  nom_fich_pid, strerror (errno));
	  exit(ERROR_CREATE_FILE);
	}
      fprintf (pidfile, "%d\n", getpid ());
      fclose (pidfile);
    }

  /*****************************************************/
  //Some initialisations
  /*****************************************************/

  // initialisation of active channels list
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      channels[curr_channel].streamed_channel = 0;
      channels[curr_channel].streamed_channel_old = 1;
    }

  //We initialise mantadory pid table
  for(curr_pid_mandatory=0;curr_pid_mandatory<MAX_MANDATORY_PID_NUMBER;curr_pid_mandatory++)
    {
      mandatory_pid[curr_pid_mandatory]=0;
    }

  //mandatory pids (always sent with all channels)
  //PAT : Program Association Table
  mandatory_pid[0]=1;
  //NIT : Network Information Table
  //It is intended to provide information about the physical network.
  mandatory_pid[16]=1;
  //SDT : Service Description Table
  //the SDT contains data describing the services in the system e.g. names of services, the service provider, etc.
  mandatory_pid[17]=1;
  //EIT : Event Information Table
  //the EIT contains data concerning events or programmes such as event name, start time, duration, etc.
  mandatory_pid[18]=1;
  //TDT : Time and Date Table
  //the TDT gives information relating to the present time and date.
  //This information is given in a separate table due to the frequent updating of this information.
  mandatory_pid[20]=1;

  /*****************************************************/
  //We open the file descriptors and
  //Set the filters
  /*****************************************************/


  // we open the file descriptors
  if (create_card_fd (card, number_of_channels, channels, mandatory_pid, &fds) < 0)
    return mumudvb_close(100<<8);

  //File descriptor for polling
  pfds[0].fd = fds.fd_dvr;
  //POLLIN : data available for read
  pfds[0].events = POLLIN | POLLPRI; 
  pfds[1].fd = 0;
  pfds[1].events = POLLIN | POLLPRI;

  for(curr_pid_mandatory=0;curr_pid_mandatory<MAX_MANDATORY_PID_NUMBER;curr_pid_mandatory++)
    if(mandatory_pid[curr_pid_mandatory]==1)
      set_ts_filt (fds.fd_mandatory[curr_pid_mandatory], curr_pid_mandatory, DMX_PES_OTHER);

  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
	set_ts_filt (fds.fd[curr_channel][curr_pid], channels[curr_channel].pids[curr_pid], DMX_PES_OTHER);
    }


  gettimeofday (&tv, (struct timezone *) NULL);
  real_start_time = tv.tv_sec;
  now = 0;


  /*****************************************************/
  // Init udp, we open the sockets
  /*****************************************************/
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      //we use makeclientsocket in order to join the multicast group associated with the channel
      //Some switches (like HP Procurve 26xx) broadcast multicast traffic when there is no client to the group
      channels[curr_channel].socketOut = makeclientsocket (channels[curr_channel].ipOut, channels[curr_channel].portOut, multicast_ttl, &channels[curr_channel].sOut);
    }


  /*****************************************************/
  // init sap
  /*****************************************************/

  if(sap_vars.sap)
    {
      sap_vars.sap_messages=malloc(sizeof(mumudvb_sap_message_t)*MAX_CHANNELS);
      if(sap_vars.sap_messages==NULL)
	{
	  log_message( MSG_ERROR,"MALLOC\n");
	  return mumudvb_close(100<<8);
	}
      memset (sap_vars.sap_messages, 0, sizeof( mumudvb_sap_message_t)*MAX_CHANNELS);//we clear it
      //For sap announces, we open the socket
      sap_vars.sap_socketOut =  makeclientsocket (SAP_IP, SAP_PORT, SAP_TTL, &sap_vars.sap_sOut);
      if(!strlen(sap_vars.sap_organisation))
	sprintf(sap_vars.sap_organisation,"mumudvb");
      sap_vars.sap_serial= 1 + (int) (424242.0 * (rand() / (RAND_MAX + 1.0)));
      sap_vars.sap_last_time_sent = 0;
      //todo : loop to create the version
    }


  /*****************************************************/
  // Information about streamed channels
  /*****************************************************/

  if(autoconf_vars.autoconfiguration!=2)
    {
      log_streamed_channels(number_of_channels, channels);
    }


  if(autoconf_vars.autoconfiguration)
    log_message(MSG_INFO,"Autoconfiguration Start\n");

  /******************************************************/
  //Main loop where we get the packets and send them
  /******************************************************/
  while (!Interrupted)
    {
      /* Poll the open file descriptors : we wait for data*/
      poll_try=0;
      last_poll_error=0;
      while((poll (pfds, 1, 500)<0)&&(poll_try<MAX_POLL_TRIES))
	{
	  if(errno != EINTR) //EINTR means Interrupted System Call, it normally shouldn't matter so much so we don't count it for our Poll tries
	    {
	      poll_try++;
	      last_poll_error=errno;
	    }
	  //TODO : put a maximum number of interrupted system calls
	}

      if(poll_try==MAX_POLL_TRIES)
	{
	  log_message( MSG_ERROR, "Poll : We reach the maximum number of polling tries\n\tLast error when polling: %s\n", strerror (errno));
	  Interrupted=errno<<8; //the <<8 is to make difference beetween signals and errors;
	  continue;
	}
      else if(poll_try)
	{
	  log_message( MSG_WARN, "Poll : Warning : error when polling: %s\n", strerror (last_poll_error));
	}

      /* Attempt to read 188 bytes from /dev/____/dvr */
      if ((bytes_read = read (fds.fd_dvr, temp_buffer_from_dvr, TS_PACKET_SIZE)) > 0)
	{
	  if (bytes_read != TS_PACKET_SIZE)
	    {
	      log_message( MSG_ERROR, "No bytes left to read - aborting\n");
	      Interrupted=errno<<8; //the <<8 is to make difference beetween signals and errors;
	      continue;
	    }

	  pid = ((temp_buffer_from_dvr[1] & 0x1f) << 8) | (temp_buffer_from_dvr[2]);


	  /*************************************************************************************/
	  /****              AUTOCONFIGURATION PART                                         ****/
	  /*************************************************************************************/
	  if( autoconf_vars.autoconfiguration==2) //Full autoconfiguration, we search the channels and their names
	    {
	      if(pid==0) //PAT : contains the services identifiers and the pmt pid for each service
		{
		  if(get_ts_packet(temp_buffer_from_dvr,autoconf_vars.autoconf_temp_pat))
		    {
		      //log_message(MSG_DEBUG,"Autoconf : New PAT pid\n");
		      if(autoconf_read_pat(autoconf_vars.autoconf_temp_pat,autoconf_vars.services))
			{
			  log_message(MSG_DEBUG,"Autoconf : It seems that we have finished *\n");
			  //Interrupted=1;
			  number_of_channels=services_to_channels(autoconf_vars.services, channels, cam_vars.cam_support,common_port, card); //Convert the list of services into channels
			  if(autoconf_vars.services)
			    {
			      autoconf_free_services(autoconf_vars.services);
			      autoconf_vars.services=NULL;
			    }
			  if (complete_card_fds(card, number_of_channels, channels, &fds,0) < 0)
			    {
			      log_message(MSG_ERROR,"Autoconf : ERROR : CANNOT Open the new descriptors\n");
			      Interrupted=666<<8; //the <<8 is to make difference beetween signals and errors;
			      continue;
			    }

			  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
			    {
			      //filters
			      set_ts_filt (fds.fd[curr_channel][0], channels[curr_channel].pids[0], DMX_PES_OTHER);
			      // Init udp
			      //TODO explain
			      channels[curr_channel].socketOut = makeclientsocket (channels[curr_channel].ipOut, channels[curr_channel].portOut, multicast_ttl, &channels[curr_channel].sOut);
			    }

			  log_message(MSG_DEBUG,"Autoconf : Step TWO, we get the video and audio PIDs\n");
			  free(autoconf_vars.autoconf_temp_sdt);
			  autoconf_vars.autoconf_temp_sdt=NULL;
			  free(autoconf_vars.autoconf_temp_pat);
			  autoconf_vars.autoconf_temp_pat=NULL;
			  autoconf_vars.autoconfiguration=1; //Next step add video and audio pids
			}
		      else
			memset (autoconf_vars.autoconf_temp_pat, 0, sizeof(mumudvb_ts_packet_t));//we clear it
		    }
		}
	      if(pid==17) //SDT : contains the names of the services
		{
		  if(get_ts_packet(temp_buffer_from_dvr,autoconf_vars.autoconf_temp_sdt))
		    {
		      //log_message(MSG_DEBUG,"Autoconf : New SDT pid\n");
		      autoconf_read_sdt(autoconf_vars.autoconf_temp_sdt->packet,autoconf_vars.autoconf_temp_sdt->len,autoconf_vars.services);
		      memset (autoconf_vars.autoconf_temp_sdt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
		    }
		}
	      continue;
	    }
	  if( autoconf_vars.autoconfiguration==1) //We have the channels and their PMT, we search the other pids
	    {
	      //here we call the autoconfiguration function
	      for(curr_channel=0;curr_channel<MAX_CHANNELS;curr_channel++)
		{
		  if((!channels[curr_channel].autoconfigurated) &&(channels[curr_channel].pids[0]==pid)&& pid)
		    {
		      if(get_ts_packet(temp_buffer_from_dvr,autoconf_vars.autoconf_temp_pmt))
			{
			  //Now we have the PMT, we parse it
			  log_message(MSG_DEBUG,"Autoconf : New PMT pid %d for channel %d\n",pid,curr_channel);
			  autoconf_read_pmt(autoconf_vars.autoconf_temp_pmt,&channels[curr_channel]);
			  log_message(MSG_DETAIL,"Autoconf : Final PIDs for channel %d \"%s\" : ",curr_channel, channels[curr_channel].name);
			  for(i=0;i<channels[curr_channel].num_pids;i++)
			    log_message(MSG_DETAIL," %d -",channels[curr_channel].pids[i]);
			  log_message(MSG_DETAIL,"\n");
			  channels[curr_channel].autoconfigurated=1;

			  //We check if autoconfiguration is finished
			  autoconf_vars.autoconfiguration=0;
			  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
			    if(!channels[curr_channel].autoconfigurated)
			      autoconf_vars.autoconfiguration=1;

			  //if it's finished, we open the new descriptors and add the new filters
			  if(autoconf_vars.autoconfiguration==0)
			    {
			      autoconf_end(card, number_of_channels, channels, &fds);
			      //We free autoconf memory
			      if(autoconf_vars.autoconf_temp_sdt)
				{
				  free(autoconf_vars.autoconf_temp_sdt);
				  autoconf_vars.autoconf_temp_sdt=NULL;
				}
			      if(autoconf_vars.autoconf_temp_pmt)
				{
				  free(autoconf_vars.autoconf_temp_pmt);
				  autoconf_vars.autoconf_temp_pmt=NULL;
				}
			      if(autoconf_vars.autoconf_temp_pat)
				{
				  free(autoconf_vars.autoconf_temp_pat);
				  autoconf_vars.autoconf_temp_pat=NULL;
				}
			      if(autoconf_vars.services)
				{
				  autoconf_free_services(autoconf_vars.services);
				  autoconf_vars.services=NULL;
				}
			    }
			}
		    }
		}
	      continue;
	    }
	  /*************************************************************************************/
	  /****              AUTOCONFIGURATION PART FINISHED                                ****/
	  /*************************************************************************************/

	  /******************************************************/
	  //Pat rewrite 
	  /******************************************************/
	  //we save the full pat before otherwise only the first channel will be rewritten with a full PAT
	  //in other words, we need a full pat for all the channels
	  if( (pid == 0) && //This is a PAT PID
	      rewrite_pat ) //AND we asked for rewrite
	    memcpy(saved_pat_buffer,temp_buffer_from_dvr,TS_PACKET_SIZE); //We save the full pat
	  

	  /******************************************************/
	  //for each channel we'll look if we must send this PID
	  /******************************************************/
	  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
	    {
	      //we'll see if we must send this pid for this channel
	      send_packet=0;
	      
	      //If it's a mandatory pid we send it
	      if((pid<MAX_MANDATORY_PID_NUMBER) && (mandatory_pid[pid]==1))
		send_packet=1;
	      
	      //if it isn't mandatory wee see if it is in the channel list
	      if(!send_packet)
		for (curr_pid = 0; (curr_pid < channels[curr_channel].num_pids)&& !send_packet; curr_pid++)
		  if ((channels[curr_channel].pids[curr_pid] == pid)) {
		    send_packet=1;
		    channels[curr_channel].streamed_channel++;
		  }

	      /******************************************************/
	      //cam support
	      // If we send the packet, we look if it's a cam pmt pid
	      /******************************************************/
	      if(cam_vars.cam_support && send_packet==1)  //no need to check paquets we don't send
#ifndef LIBDVBEN50221
		if(cam_vars.cam_sys_access->cai->initialized&&cam_vars.cam_sys_access->cai->ready>=3)  
#else
		if(cam_vars.ca_resource_connected && cam_vars.delay>=3 )
#endif
		{
		  if ((channels[curr_channel].cam_pmt_pid)&& (channels[curr_channel].cam_pmt_pid == pid))
		    {
		      if(get_ts_packet(temp_buffer_from_dvr,cam_vars.cam_pmt_ptr)) 
			{
#ifndef LIBDVBEN50221
     			  channels[curr_channel].cam_pmt_pid=0; //once we have asked the CAM for this PID, we clear it not to ask it again
			  cam_vars.cam_pmt_ptr->i_program_number=curr_channel;
			  en50221_SetCAPMT(cam_vars.cam_sys_access, cam_vars.cam_pmt_ptr, channels);
			  cam_vars.cam_sys_access->cai->ready=0;
			  cam_vars.cam_pmt_ptr=malloc(sizeof(mumudvb_ts_packet_t)); //We allocate a new one, the old one is stored in cam_sys_access
			  if(cam_vars.cam_pmt_ptr==NULL)
			    {
			      log_message( MSG_ERROR,"MALLOC\n");
			        return mumudvb_close(100<<8);
			    }
			  memset (cam_vars.cam_pmt_ptr, 0, sizeof( mumudvb_ts_packet_t));//we clear it
#else
			  cam_vars.delay=0;
			  mumudvb_cam_new_pmt(&cam_vars, cam_vars.cam_pmt_ptr);
     			  channels[curr_channel].cam_pmt_pid=0; //once we have asked the CAM for this PID, we clear it not to ask it again
#endif

			}
		    }
		}

	      /******************************************************/
	      //Rewrite PAT
	      /******************************************************/
	      if(send_packet==1)  //no need to check paquets we don't send
		if( (pid == 0) && //This is a PAT PID
		     rewrite_pat ) //AND we asked for rewrite
		  {
		    memcpy(temp_buffer_from_dvr,saved_pat_buffer,TS_PACKET_SIZE); //We restore the full PAT
		    //and we try to rewrite it
		    if(pat_rewrite(temp_buffer_from_dvr,channels[curr_channel].num_pids,channels[curr_channel].pids)) //We try rewrite and if there's an error...
		      send_packet=0;//... we don't send it anyway
		  }

	      /******************************************************/
	      //Ok we must send this packet,
	      // we add it to the channel buffer
	      /******************************************************/
	      if(send_packet==1)
		{
		  // we fill the channel buffer
		  memcpy(channels[curr_channel].buf + channels[curr_channel].nb_bytes, temp_buffer_from_dvr, bytes_read);

		  channels[curr_channel].buf[channels[curr_channel].nb_bytes + 1] =
		    (channels[curr_channel].buf[channels[curr_channel].nb_bytes + 1] & 0xe0) | hi_mappids[pid];
		  channels[curr_channel].buf[channels[curr_channel].nb_bytes + 2] = lo_mappids[pid];

		  channels[curr_channel].nb_bytes += bytes_read;
		  //The buffer is full, we send it
		  if ((channels[curr_channel].nb_bytes + TS_PACKET_SIZE) > MAX_UDP_SIZE)
		    {
		      sendudp (channels[curr_channel].socketOut, &channels[curr_channel].sOut, channels[curr_channel].buf,
			       channels[curr_channel].nb_bytes);
		      channels[curr_channel].nb_bytes = 0;
		    }
		}

	      //The idea is the following, when we send a packet we reinit count_non_transmis, wich count the number of packets
	      //which are not sent
	      //this number is increased for each new packet from the card
	      //Normally this number should never increase a lot since we asked the card to give us only interesting PIDs
	      //When it increases it means that the card give us crap, so we put a warning on the log.
	      count_non_transmis = 0;
	      if (alarm_count == 1)
		{
		  alarm_count = 0;
		  log_message( MSG_INFO,
			       "Good, we receive back significant data\n");
		}
	    }
	  //when we do autoconfiguration, we didn't set all the filters etc, so we don't care about count_non_transmis
	  if(!autoconf_vars.autoconfiguration)
	    {
	      count_non_transmis++;
	      if (count_non_transmis > ALARM_COUNT_LIMIT)
		if (alarm_count == 0)
		  {
		    log_message( MSG_INFO,
				 "Error : less than one paquet on %d sent\n",
				 ALARM_COUNT_LIMIT);
		    alarm_count = 1;
		  }
	    }
	}
    }
  /******************************************************/
  //End of main loop
  /******************************************************/
  
  return mumudvb_close(Interrupted);
  
}

/******************************************************/
//Clean closing and freeing
/******************************************************/
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


  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    close (channels[curr_channel].socketOut);

  // we close the file descriptors
  close_card_fd (number_of_channels, channels, fds);

  if(cam_vars.cam_support)
    {
      if(cam_vars.cam_pmt_ptr)
	free(cam_vars.cam_pmt_ptr);
#ifdef LIBDVBEN50221
      cam_stop(&cam_vars);
#else
      CAMClose(cam_vars.cam_sys_access);
      if(cam_vars.cam_sys_access)
	free(cam_vars.cam_sys_access);
#endif
    }

  //autoconf variables freeing
  if(autoconf_vars.services)
    autoconf_free_services(autoconf_vars.services);
  if(autoconf_vars.autoconf_temp_sdt)
    free(autoconf_vars.autoconf_temp_sdt);
  if(autoconf_vars.autoconf_temp_pmt)
    free(autoconf_vars.autoconf_temp_pmt);
  if(autoconf_vars.autoconf_temp_pat)
    free(autoconf_vars.autoconf_temp_pat);

  //sap variables freeing
  if(sap_vars.sap_messages)
    free(sap_vars.sap_messages);
  
  if ((write_streamed_channels)&&remove (nom_fich_chaines_diff)) 
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   nom_fich_chaines_diff, strerror (errno));
      exit(ERROR_DEL_FILE);
    }

  if ((write_streamed_channels)&&remove (nom_fich_chaines_non_diff))
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   nom_fich_chaines_non_diff, strerror (errno));
      exit(ERROR_DEL_FILE);
    }


  if (!no_daemon)
    {
      if (remove (nom_fich_pid))
	{
	  log_message( MSG_INFO, "%s: %s\n",
		       nom_fich_pid, strerror (errno));
	  exit(ERROR_DEL_FILE);
	}
    }

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
 * This function also catches SIGPIPE and SIGUSR1
 * 
 ******************************************************/
static void SignalHandler (int signum)
{

  struct timeval tv;
  int curr_channel = 0;
  int count_of_active_channels=0;

  if (signum == SIGALRM && !Interrupted)
    {

      gettimeofday (&tv, (struct timezone *) NULL);
      now = tv.tv_sec - real_start_time;

      if (display_signal_strenght && card_tuned)
	affiche_puissance (fds);

      if (!card_tuned)
	{
	  log_message( MSG_INFO,
		       "Card not tuned after %ds - exiting\n",
		       timeout_accord);
	  exit(ERROR_TUNE);
	}

      //autoconfiguration
      //We check if we reached the autoconfiguration timeout
      //if it's finished, we open the new descriptors and add the new filters
      if(autoconf_vars.autoconfiguration)
	{
	  if(!autoconf_vars.time_start_autoconfiguration)
	    autoconf_vars.time_start_autoconfiguration=now;
	  else if (now-autoconf_vars.time_start_autoconfiguration>AUTOCONFIGURE_TIME)
	    {
	      log_message(MSG_WARN,"Autoconf : Warning : Not all the channels were configured before timeout\n");
	      autoconf_vars.autoconfiguration=0;
	      autoconf_end(card, number_of_channels, channels, &fds);
	      //We free autoconf memory
	      if(autoconf_vars.autoconf_temp_sdt)
		{
		  free(autoconf_vars.autoconf_temp_sdt);
		  autoconf_vars.autoconf_temp_sdt=NULL;
		}
	      if(autoconf_vars.autoconf_temp_pmt)
		{
		  free(autoconf_vars.autoconf_temp_pmt);
		  autoconf_vars.autoconf_temp_pmt=NULL;
		}
	      if(autoconf_vars.autoconf_temp_pat)
		{
		  free(autoconf_vars.autoconf_temp_pat);
		  autoconf_vars.autoconf_temp_pat=NULL;
		}
	      if(autoconf_vars.services)
		{
		  autoconf_free_services(autoconf_vars.services);
 		  autoconf_vars.services=NULL;
		}
	    }
	}
      //end of autoconfiguration
      else //we are not doing autoconfiguration we can do something else
	{
	  //sap announces
	  if(sap_vars.sap)
	    {
	      if(!sap_vars.sap_last_time_sent)
		{
		  // it's the first time we are here, we initialize all the channels
		  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
		    {
		      sap_update(channels[curr_channel], &sap_vars.sap_messages[curr_channel]);
		    }
		  sap_vars.sap_last_time_sent=now-sap_vars.sap_interval-1;
		}
	      if((now-sap_vars.sap_last_time_sent)>=sap_vars.sap_interval)
		{
		  sap_send(sap_vars.sap_messages, number_of_channels);
		  sap_vars.sap_last_time_sent=now;
		}
	    }
	  //end of sap announces

	  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
	    if ((channels[curr_channel].streamed_channel >= 100) && (!channels[curr_channel].streamed_channel_old))
	      {
		log_message( MSG_INFO,
			     "Channel \"%s\" back.Card %d\n",
			     channels[curr_channel].name, card);
		channels[curr_channel].streamed_channel_old = 1;	// update
		if(sap_vars.sap)
		  sap_update(channels[curr_channel], &sap_vars.sap_messages[curr_channel]); //Channel status changed, we update the sap announces
	      }
	    else if ((channels[curr_channel].streamed_channel_old) && (channels[curr_channel].streamed_channel < 30))
	      {
		log_message( MSG_INFO,
			     "Channel \"%s\" down.Card %d\n",
			     channels[curr_channel].name, card);
		channels[curr_channel].streamed_channel_old = 0;	// update
		if(sap_vars.sap)
		  sap_update(channels[curr_channel], &sap_vars.sap_messages[curr_channel]); //Channel status changed, we update the sap announces
	      }

	  /*******************************************/
	  // we count active channels
	  /*******************************************/
	  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
	    if (channels[curr_channel].streamed_channel_old)
	      count_of_active_channels++;

	  //Time no diff is the time when we got 0 active channels
	  //if we have active channels, we reinit this counter
	  if(count_of_active_channels)
	    time_no_diff=0;
	  //If we don't have active channels and this is the first time, we store the time
	  else if(!time_no_diff)
	    time_no_diff=now;

	  //If we don't stream data for a too long time, we exit
	  if(time_no_diff&&((now-time_no_diff)>timeout_no_diff))
	    {
	      log_message( MSG_INFO,
			   "No data from card %d in %ds, exiting.\n",
			   card, timeout_no_diff);
	      Interrupted=ERROR_NO_DIFF<<8; //the <<8 is to make difference beetween signals and errors
	    }

	  //generation of the files wich says the streamed channels
	  if (write_streamed_channels)
	    gen_chaines_diff(nom_fich_chaines_diff, nom_fich_chaines_non_diff, number_of_channels, channels);

	  // reinit
	  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
	    channels[curr_channel].streamed_channel = 0;
      
	  if(cam_vars.cam_support)
	    {
#ifndef LIBDVBEN50221
	      CAMPoll(cam_vars.cam_sys_access);
	      cam_vars.cam_sys_access->cai->ready++;
#else
	      cam_vars.delay++;
#endif
	    }
	}
      alarm (ALARM_TIME);
    }
  else if (signum == SIGUSR1)
    {
      display_signal_strenght = display_signal_strenght ? 0 : 1;
    }
  else if (signum != SIGPIPE)
    {
      Interrupted = signum;
    }
  signal (signum, SignalHandler);
}


