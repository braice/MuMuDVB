/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) Brice DUBOST
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

#define VERSION "1.5.0"


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

//autoconfiguration
int autoconfiguration = 0;           //Do we use autoconfiguration ?
//Possible values for this variable
// 0 : none (or autoconf finished)
// 1 : we have the PMT pids and the channels, we search the audio and video
// 2 : we have only the tuning parameters, we search the channels and their pmt pids

long time_start_autoconfiguration=0; //When did we started autoconfiguration ?


//logging
int log_initialised=0;
int verbosity = 1;

//CAM (Conditionnal Access Modules : for scrambled channels)
int cam_support = 0;
access_sys_t *cam_sys_access;

// file descriptors
fds_t fds; // defined in dvb.h


//CRC table for PAT rebuilding and cam support
unsigned long       crc32_table[256];

// prototypes
static void SignalHandler (int signum);
int pat_rewrite(unsigned char *buf,int num_pids, int *pids);


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
  int k;
  int buf_pos;

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
  char nom_fich_pid[256];
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
  unsigned long crc32_table_temp_var[3];

  //do we support conditionnal access modules ?
  int i;
  mumudvb_ts_packet_t *cam_pmt_ptr=NULL;
  int cam_number=0;

  //for autoconfiguration
  mumudvb_ts_packet_t *autoconf_temp_pmt=NULL;
  mumudvb_ts_packet_t *autoconf_temp_pat=NULL;
  mumudvb_ts_packet_t *autoconf_temp_sdt=NULL;
  mumudvb_service_t   *services=NULL;


  //Getopt
  const char short_options[] = "c:sdhvq";
  const struct option long_options[] = {
    {"config", required_argument, NULL, 'c'},
    {"signal", no_argument, NULL, 's'},
    {"debug", no_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0}
  };
  int c, option_index = 0;


  // Initialise PID map
  for (k = 0; k < 8192; k++)
    {
      hi_mappids[k] = (k >> 8);
      lo_mappids[k] = (k & 0xff);
    }


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
  
  //end of command line options parsing
  
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
  while (fgets (current_line, CONF_LINELEN, conf_file)
	 && strlen (current_line) > 1)
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
	  cam_support = atoi (substring);
	  if(cam_support)
	    {
	      log_message( MSG_WARN,
			"!!! You have enabled the support for conditionnal acces modules (scrambled channels), this is a beta feature.Please report any bug/comment\n");
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
	  autoconfiguration = atoi (substring);
	  if(autoconfiguration)
	    {
	      log_message( MSG_WARN,
			"!!! You have enabled the support for autoconfiguration, this is a beta feature.Please report any bug/comment\n");
	    }
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
	  cam_number = atoi (substring);
	}
      else if (!strcmp (substring, "ip"))
	{
	  substring = strtok (NULL, delimiteurs);
	  sscanf (substring, "%s\n", channels[curr_channel].ipOut);
	  ip_ok = 1;
	}
      else if (!strcmp (substring, "common_port"))
	{
	  substring = strtok (NULL, delimiteurs);
	  common_port = atoi (substring);
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
      log_message( MSG_INFO,
		   "%s: %s\n",
		   nom_fich_chaines_diff, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }

  fclose (chaines_diff);

  chaines_non_diff = fopen (nom_fich_chaines_non_diff, "w");
  if (chaines_diff == NULL)
    {
      log_message( MSG_INFO,
		   "%s: %s\n",
		   nom_fich_chaines_non_diff, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }

  fclose (chaines_non_diff);

  
  log_message( MSG_INFO, "Streaming. Freq %lu pol %c srate %lu\n",
	       freq, pol, srate);


  /******************************************************/
  //we compute the crc32 tables for cam support,
  // autoconfiguration or pat rewriting
  /******************************************************/
  if(cam_support||rewrite_pat||autoconfiguration)
    {
      //CRC32 table initialisation (taken from the xine project), we can also use static tables if we want
      for( crc32_table_temp_var[0] = 0 ; crc32_table_temp_var[0] < 256 ; crc32_table_temp_var[0]++ ) {
	crc32_table_temp_var[2] = 0;
	for (crc32_table_temp_var[1] = (crc32_table_temp_var[0] << 24) | 0x800000 ; crc32_table_temp_var[1] != 0x80000000 ; crc32_table_temp_var[1] <<= 1) {
	  crc32_table_temp_var[2] = (crc32_table_temp_var[2] << 1) ^ (((crc32_table_temp_var[2] ^ crc32_table_temp_var[1]) & 0x80000000) ? 0x04c11db7 : 0);
	}
	crc32_table[crc32_table_temp_var[0]] = crc32_table_temp_var[2];
      }
    }


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

  if(cam_support){

    cam_pmt_ptr=malloc(sizeof(mumudvb_ts_packet_t));
    cam_sys_access=malloc(sizeof(access_sys_t));
    //cleaning
    for ( i = 0; i < MAX_PROGRAMS; i++ )
      cam_sys_access->pp_selected_programs[i]=NULL;
    CAMOpen(cam_sys_access, card, cam_number);
    cam_sys_access->cai->initialized=0;
    cam_sys_access->cai->ready=0;
    CAMPoll(cam_sys_access); //TODO : check why it sometimes fails --> look if the failure is associated with a non ready slot
  }
  
  /*****************************************************/
  //autoconfiguration
  //memory allocation for MPEG2-TS
  //packet structures
  /*****************************************************/

  //TODO : check the return values of malloc
  if(autoconfiguration)
    {
      autoconf_temp_pmt=malloc(sizeof(mumudvb_ts_packet_t));
      if(autoconf_temp_pmt==NULL)
	{
	  log_message( MSG_ERROR,"MALLOC\n");
	  return -1;
	}
      memset (autoconf_temp_pmt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
    }
  if(autoconfiguration==2)
    {
      autoconf_temp_pat=malloc(sizeof(mumudvb_ts_packet_t));
      if(autoconf_temp_pat==NULL)
	{
	  log_message( MSG_ERROR,"MALLOC\n");
	  return -1;
	}
      memset (autoconf_temp_pat, 0, sizeof( mumudvb_ts_packet_t));//we clear it
      autoconf_temp_sdt=malloc(sizeof(mumudvb_ts_packet_t));
      if(autoconf_temp_sdt==NULL)
	{
	  log_message( MSG_ERROR,"MALLOC\n");
	  return -1;
	}
      memset (autoconf_temp_sdt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
      services=malloc(sizeof(mumudvb_service_t));
      if(services==NULL)
	{
	  log_message( MSG_ERROR,"MALLOC\n");
	  return -1;
	}
      memset (services, 0, sizeof( mumudvb_service_t));//we clear it

    }

  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      channels[curr_channel].nb_bytes=0;
      //If there is more than one pid in one channel we mark it
      //For no autoconfiguration
      if(autoconfiguration==1 && channels[curr_channel].num_pids>1)
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
    return -1;

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
      channels[curr_channel].socketOut = makeclientsocket (channels[curr_channel].ipOut, channels[curr_channel].portOut, DEFAULT_TTL, &channels[curr_channel].sOut);
    }

  /*****************************************************/
  // Information about streamed channels
  /*****************************************************/

  if(autoconfiguration!=2)
    {
      log_message( MSG_INFO, "Diffusion %d channel%s\n", number_of_channels,
		   (number_of_channels == 1 ? "" : "s"));
      
      for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
	{
	  log_message( MSG_INFO, "Channel \"%s\" num %d ip %s:%d\n",
		       channels[curr_channel].name, curr_channel, channels[curr_channel].ipOut, channels[curr_channel].portOut);
	  log_message( MSG_DETAIL, "        pids : ");
	  for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
	    log_message( MSG_DETAIL, "%d ", channels[curr_channel].pids[curr_pid]);
	  log_message( MSG_DETAIL, "\n");
	}
    }


  if( autoconfiguration)
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
	  if(errno != EINTR) //TODO : comment why we ignore interrupted system call
	    {
	      poll_try++;
	      last_poll_error=errno;
	    }
	  //TODO : put a maximum number of interrupted system calls
	}

      if(poll_try==MAX_POLL_TRIES)
	{
	  log_message( MSG_ERROR, "Poll : We reach the maximum number of polling tries\n\tLast error when polling: %s\n", strerror (errno));
	  Interrupted=errno;
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
	      Interrupted=errno;
	      continue;
	    }

	  pid = ((temp_buffer_from_dvr[1] & 0x1f) << 8) | (temp_buffer_from_dvr[2]);


	  /*************************************************************************************/
	  /****              AUTOCONFIGURATION PART                                         ****/
	  /*************************************************************************************/
	  if( autoconfiguration==2)
	    {
	      if(pid==0)
		{
		  if(get_ts_packet(temp_buffer_from_dvr,autoconf_temp_pat))
		    {
		      //log_message(MSG_DEBUG,"Autoconf : New PAT pid\n");
		      if(autoconf_read_pat(autoconf_temp_pat,services))
			{
			  log_message(MSG_DEBUG,"It seems that we have finished ***************\n");
			  //Interrupted=1;
			  number_of_channels=services_to_channels(services, channels, cam_support,common_port, card); //Convert the list of services into channels
			  if (complete_card_fds(card, number_of_channels, channels, &fds,autoconfiguration) < 0)
			    {
			      log_message(MSG_ERROR,"Autoconf : ERROR : CANNOT Open the new descriptors\n");
			      Interrupted=666;
			      continue;
			    }

			  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
			    {
			      //if (create_card_fd (card, number_of_channels, channels, mandatory_pid, &fds) < 0)
			      //return -1;
			      //filters
			      set_ts_filt (fds.fd[curr_channel][0], channels[curr_channel].pids[0], DMX_PES_OTHER);
			      // Init udp
			      //TODO explain
			      channels[curr_channel].socketOut = makeclientsocket (channels[curr_channel].ipOut, channels[curr_channel].portOut, DEFAULT_TTL, &channels[curr_channel].sOut);
			    }

			  log_message(MSG_DEBUG,"Autoconf : Step TWO, we get the video ond audio PIDs\n");
			  free(autoconf_temp_sdt);
			  free(autoconf_temp_pat);
			  autoconfiguration=1; //Next step add video and audio pids
			}
		      memset (autoconf_temp_pat, 0, sizeof(mumudvb_ts_packet_t));//we clear it
		    }
		}
	      if(pid==17)
		{
		  if(get_ts_packet(temp_buffer_from_dvr,autoconf_temp_sdt))
		    {
		      //log_message(MSG_DEBUG,"Autoconf : New SDT pid\n");
		      autoconf_read_sdt(autoconf_temp_sdt->packet,autoconf_temp_sdt->len,services);
		      memset (autoconf_temp_sdt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
		    }
		}
	      continue;
	    }
	  if( autoconfiguration==1)
	    {
	      //here we call the autoconfiguration function
	      for(curr_channel=0;curr_channel<MAX_CHANNELS;curr_channel++)
		{
		  if((!channels[curr_channel].autoconfigurated) &&(channels[curr_channel].pids[0]==pid)&& pid)
		    {
		      if(get_ts_packet(temp_buffer_from_dvr,autoconf_temp_pmt))
			{
			  //Now we have the PMT, we parse it
			  log_message(MSG_DEBUG,"Autoconf : New PMT pid %d for channel %d\n",pid,curr_channel);
			  autoconf_read_pmt(autoconf_temp_pmt,&channels[curr_channel]);
			  log_message(MSG_DETAIL,"Autoconf : Final PIDs for channel %d \"%s\" : ",curr_channel, channels[curr_channel].name);
			  for(i=0;i<channels[curr_channel].num_pids;i++)
			    log_message(MSG_DETAIL," %d -",channels[curr_channel].pids[i]);
			  log_message(MSG_DETAIL,"\n");
			  channels[curr_channel].autoconfigurated=1;
			}
		    }
		}
	      continue;
	    }
	  /*************************************************************************************/
	  /****              AUTOCONFIGURATION PART FINISHED                                ****/
	  /*************************************************************************************/

	  //Pat rewrite only
	  //we save the full pat before otherwise only the first channel will be rewritten with a full PAT
	  if( (pid == 0) && //This is a PAT PID
	      rewrite_pat ) //AND we asked for rewrite
	    for(buf_pos=0;buf_pos<TS_PACKET_SIZE;buf_pos++)//TODO : use memcpy
	      saved_pat_buffer[buf_pos]=temp_buffer_from_dvr[buf_pos]; //We save the full pat
	  

	  //for each channel we'll look if we must send this PID
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

	      //cam support
	      if(cam_support && send_packet==1 &&cam_sys_access->cai->initialized&&cam_sys_access->cai->ready>=3)  //no need to check paquets we don't send
		{
		  if ((channels[curr_channel].cam_pmt_pid)&& (channels[curr_channel].cam_pmt_pid == pid))
		    {
		      if(get_ts_packet(temp_buffer_from_dvr,cam_pmt_ptr)) 
			{
			  //fprintf(stderr,"HOP\n");
     			  channels[curr_channel].cam_pmt_pid=0; //once we have asked the CAM for this PID, we clear it not to ask it again
			  cam_pmt_ptr->i_program_number=curr_channel;
			  en50221_SetCAPMT(cam_sys_access, cam_pmt_ptr);
			  cam_sys_access->cai->ready=0;
			  cam_pmt_ptr=malloc(sizeof(mumudvb_ts_packet_t)); //on alloue un nouveau //l'ancien est stocke dans la structure VLC
			  //TODO check if we have enough memory
			  memset (cam_pmt_ptr, 0, sizeof( mumudvb_ts_packet_t));//we clear it
			}
		    }
		}

	      //Rewrite PAT checking
	      if(send_packet==1)  //no need to check paquets we don't send
		if( (pid == 0) && //This is a PAT PID
		     rewrite_pat ) //AND we asked for rewrite
		  {
		    for(buf_pos=0;buf_pos<TS_PACKET_SIZE;buf_pos++)//TODO : use memcpy
		      temp_buffer_from_dvr[buf_pos]=saved_pat_buffer[buf_pos]; //We restore the full PAT
		    //and we try to rewrite it
		    if(pat_rewrite(temp_buffer_from_dvr,channels[curr_channel].num_pids,channels[curr_channel].pids)) //We try rewrite and if there's an error...
		      send_packet=0;//... we don't send it anyway
		  }

	      //Ok we must send it
	      if(send_packet==1)
		{
		  // we fill the channel buffer //TODO Make a memcpy
		  for (buf_pos = 0; buf_pos < bytes_read; buf_pos++)
		    channels[curr_channel].buf[channels[curr_channel].nb_bytes + buf_pos] = temp_buffer_from_dvr[buf_pos];

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


	      count_non_transmis = 0;
	      if (alarm_count == 1)
		{
		  alarm_count = 0;
		  log_message( MSG_INFO,
			       "Good, we receive back significant data\n");
		}
	    }
	  count_non_transmis++;
	  if (count_non_transmis > ALARM_COUNT_LIMIT)
	    {
	      log_message( MSG_INFO,
			   "Error : less than one paquet on %d sent\n",
			   ALARM_COUNT_LIMIT);
	      alarm_count = 1;
	    }
	}
    }
  /******************************************************/
  //End of main loop
  /******************************************************/


  if (Interrupted)
    {
      if(Interrupted< (1<<8)) //we check if it's a signal or a mumudvb error
	log_message( MSG_INFO, "\nCaught signal %d - closing cleanly.\n",
		     Interrupted);
      else
	log_message( MSG_INFO, "\nclosing cleanly.\n");
    }

  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    close (channels[curr_channel].socketOut);

  // we close the file descriptors
  close_card_fd (number_of_channels, channels, fds);

  if(cam_support)
    {
      CAMClose(cam_sys_access);
      free(cam_pmt_ptr);
      free(cam_sys_access);
    }

  if(autoconf_temp_pmt)
    free(autoconf_temp_pmt);


  if (remove (nom_fich_chaines_diff))
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   nom_fich_chaines_diff, strerror (errno));
      exit(ERROR_DEL_FILE);
    }

  if (remove (nom_fich_chaines_non_diff))
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
 *  It check for the end of autoconfiguration //TODO : put this in a function called when a new PMT arrives
 * 
 * This function also catches SIGPIPE and SIGUSR1
 * 
 ******************************************************/
static void
SignalHandler (int signum)
{

  struct timeval tv;
  int curr_channel = 0;
  int curr_pid = 0;
  int compteur_chaines_diff=0;
  int oldautoconfiguration;

  if (signum == SIGALRM)
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
      //here we check if it's finished
      //if it's finished, we open the new descriptors and add the new filters
      if(autoconfiguration)
	{
	  oldautoconfiguration=autoconfiguration;
	  //We check if the autoconfiguration is finished
	  //  if there is still a channel wich is not configurated we 
	  //  keep autoconfiguration continue
	  if(autoconfiguration==1)
	    {
	      autoconfiguration=0;
	      for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
		if(!channels[curr_channel].autoconfigurated)
		  autoconfiguration=oldautoconfiguration;
	    }

	  if(time_start_autoconfiguration)
	    time_start_autoconfiguration=now;
	  else if (now-time_start_autoconfiguration>AUTOCONFIGURE_TIME)
	    {
	      log_message(MSG_WARN,"Autoconf : Warning : Not all the channels were configured before timeout\n");
	      autoconfiguration=0;
	    }

	  if(!autoconfiguration)
	    {
	      log_message(MSG_DETAIL,"Autoconfiguration almost done\n");
	      log_message(MSG_DETAIL,"Autoconf : Open the new descriptors\n");
	      if (complete_card_fds(card, number_of_channels, channels, &fds,autoconfiguration) < 0)
		{
		  log_message(MSG_ERROR,"Autoconf : ERROR : CANNOT Open the new descriptors\n");
		//return -1; //TODO !!!!!!!!! ADD AN ERROR
		}
	      log_message(MSG_DETAIL,"Autoconf : Add the new filters\n");
	      for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
		{
		  for (curr_pid = 1; curr_pid < channels[curr_channel].num_pids; curr_pid++)
		    set_ts_filt (fds.fd[curr_channel][curr_pid], channels[curr_channel].pids[curr_pid], DMX_PES_OTHER);
		}

	      log_message(MSG_INFO,"Autoconfiguration done\n");
	      //TODO put this in a function
	      log_message( MSG_INFO, "Diffusion %d channel%s\n", number_of_channels,
			   (number_of_channels == 1 ? "" : "s"));
	      for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
		{
		  log_message( MSG_INFO, "Channel \"%s\" num %d ip %s:%d\n",
			       channels[curr_channel].name, curr_channel, channels[curr_channel].ipOut, channels[curr_channel].portOut);
		  log_message( MSG_DETAIL, "        pids : ");
		  for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
		    log_message( MSG_DETAIL, "%d ", channels[curr_channel].pids[curr_pid]);
		  log_message( MSG_DETAIL, "\n");
		}
	    }


	}
      //end of autoconfiguration
      else //we are not doing autoconfiguration we can do something else
	{
	  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
	    if ((channels[curr_channel].streamed_channel >= 100) && (!channels[curr_channel].streamed_channel_old))
	      {
		log_message( MSG_INFO,
			     "Channel \"%s\" back.Card %d\n",
			     channels[curr_channel].name, card);
		channels[curr_channel].streamed_channel_old = 1;	// update
	      }
	    else if ((channels[curr_channel].streamed_channel_old) && (channels[curr_channel].streamed_channel < 30))
	      {
		log_message( MSG_INFO,
			     "Channel \"%s\" down.Card %d\n",
			     channels[curr_channel].name, card);
		channels[curr_channel].streamed_channel_old = 0;	// update
	      }

	  // we count active channels
	  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
	    if (channels[curr_channel].streamed_channel_old)
	      compteur_chaines_diff++;

	  // reinit si on diffuse
	  if(compteur_chaines_diff)
	    time_no_diff=0;
	  // sinon si c le moment ou on arrete on stoque l'heure
	  else if(!time_no_diff)
	    time_no_diff=now;

	  // on ne diffuse plus depuis trop longtemps
	  if(time_no_diff&&((now-time_no_diff)>timeout_no_diff))
	    {
	      log_message( MSG_INFO,
			   "No data from card %d in %ds, exiting.\n",
			   card, timeout_no_diff);
	      Interrupted=ERROR_NO_DIFF<<8; //the <<8 is to make difference beetween signals and errors
	    }

	  gen_chaines_diff(nom_fich_chaines_diff, nom_fich_chaines_non_diff, number_of_channels, channels);

	  // reinit
	  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
	    channels[curr_channel].streamed_channel = 0;
      
	  if(cam_support)
	    {
	      CAMPoll(cam_sys_access);
	      cam_sys_access->cai->ready++;
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


int
pat_rewrite(unsigned char *buf,int num_pids, int *pids)
{
  int i,pos_buf,buf_pos;
  

  //destination buffer
  unsigned char buf_dest[188];
  int buf_dest_pos=0;

  pat_t       *pat=(pat_t*)(buf+TS_HEADER_LEN);
  pat_prog_t  *prog;
  int delta=PAT_LEN+TS_HEADER_LEN;
  int section_length=0;
  int new_section_length;
  unsigned long crc32;
  unsigned long calc_crc32;


  //PAT reading
  section_length=HILO(pat->section_length);
  if((section_length>(TS_PACKET_SIZE-TS_HEADER_LEN)) && section_length)
    {
      if (section_length)
	{
	  log_message( MSG_INFO,"PAT too big : %d, don't know how rewrite, sent as is\n", section_length);
	}
      else //empty PAT
	{
	  return 1;
	}
      return 0; //we sent as is
    }
  //CRC32
  //CRC32 calculation taken from the xine project
  //Test of the crc32
  calc_crc32=0xffffffff;
  //we compute the CRC32
  for(i = 0; i < section_length-1; i++) {
    calc_crc32 = (calc_crc32 << 8) ^ crc32_table[(calc_crc32 >> 24) ^ buf[i+TS_HEADER_LEN]];
  }
 
  crc32=0x00000000;

  crc32|=buf[TS_HEADER_LEN+section_length+3-4]<<24;
  crc32|=buf[TS_HEADER_LEN+section_length+3-4+1]<<16;
  crc32|=buf[TS_HEADER_LEN+section_length+3-4+2]<<8;
  crc32|=buf[TS_HEADER_LEN+section_length+3-4+3];
  
  if((calc_crc32-crc32)!=0)
    {
      //Bad CRC32
      return 1; //We don't send this PAT
    }

/*   fprintf (stderr, "table_id %x ",pat->table_id); */
/*   fprintf (stderr, "dummy %x ",pat->dummy); */
/*   fprintf (stderr, "ts_id 0x%04x ",HILO(pat->transport_stream_id)); */
/*   fprintf (stderr, "section_length %d ",HILO(pat->section_length)); */
/*   fprintf (stderr, "version %i ",pat->version_number); */
/*   fprintf (stderr, "last_section_number %x ",pat->last_section_number); */
/*   fprintf (stderr, "\n"); */


  //sounds good, lets start the copy
  //we copy the ts header
  for(i=0;i<TS_HEADER_LEN;i++)
    buf_dest[i]=buf[i];
  //we copy the PAT header
  for(i=TS_HEADER_LEN;i<TS_HEADER_LEN+PAT_LEN;i++)
    buf_dest[i]=buf[i];

  buf_dest_pos=TS_HEADER_LEN+PAT_LEN;

  //We copy what we need : EIT announce and present PMT announce
  //strict comparaison due to the calc of section len cf down
  while((delta+PAT_PROG_LEN)<(section_length+TS_HEADER_LEN))
    {
      prog=(pat_prog_t*)((char*)buf+delta);
      if(HILO(prog->program_number)==0)
	{
	  //we found the announce for the EIT pid
	  for(pos_buf=0;pos_buf<PAT_PROG_LEN;pos_buf++)
	    buf_dest[buf_dest_pos+pos_buf]=buf[pos_buf+delta];
	  buf_dest_pos+=PAT_PROG_LEN;
	}
      else
	{
	  for(i=0;i<num_pids;i++)
	    if(pids[i]==HILO(prog->network_pid))
	      {
		//we found a announce for a PMT pid in our stream, we keep it
		for(pos_buf=0;pos_buf<PAT_PROG_LEN;pos_buf++)
		  buf_dest[buf_dest_pos+pos_buf]=buf[pos_buf+delta];
		buf_dest_pos+=PAT_PROG_LEN;
	      }
	}
      delta+=PAT_PROG_LEN;
    }


  //we compute the new section length
  //section lenght is the size of the section after section_length (crc32 included : 4 bytes)
  //so it's size of the crc32 + size of the pat prog + size of the pat header - 3 first bytes (the pat header until section length included)
  //Finally it's total_pat_data_size + 1
  new_section_length=buf_dest_pos-TS_HEADER_LEN + 1;

  //We write the new section length
  buf_dest[1+TS_HEADER_LEN]=(((new_section_length)&0x0f00)>>8)  | (0xf0 & buf_dest[1+TS_HEADER_LEN]);
  buf_dest[2+TS_HEADER_LEN]=new_section_length & 0xff;


  //CRC32 calculation taken from the xine project
  //Now we must adjust the CRC32
  //we compute the CRC32
  crc32=0xffffffff;
  for(i = 0; i < new_section_length-1; i++) {
    crc32 = (crc32 << 8) ^ crc32_table[(crc32 >> 24) ^ buf_dest[i+TS_HEADER_LEN]];
  }


  //We write the CRC32 to the buffer
  buf_dest[buf_dest_pos]=(crc32>>24) & 0xff;
  buf_dest_pos+=1;
  buf_dest[buf_dest_pos]=(crc32>>16) & 0xff;
  buf_dest_pos+=1;
  buf_dest[buf_dest_pos]=(crc32>>8) & 0xff;
  buf_dest_pos+=1;
  buf_dest[buf_dest_pos]=crc32 & 0xff;
  buf_dest_pos+=1;


  //Padding with 0xFF 
  memset(buf_dest+buf_dest_pos,0xFF,TS_PACKET_SIZE-buf_dest_pos);


  //We copy the result to the original buffer
  for(buf_pos=0;buf_pos<TS_PACKET_SIZE;buf_pos++)
    buf[buf_pos]=buf_dest[buf_pos];

  //Everything is Ok ....
  return 0;

}

