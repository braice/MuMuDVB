/* 
mumudvb - UDP-ize a DVB transport stream.
Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.

Modified By Brice DUBOST

The latest version can be found at http://mumudvb.braice.net

Copyright notice:

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
    
*/


#define _GNU_SOURCE		// pour utiliser le program_invocation_short_name (extension gnu)

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
#include <errno.h>		// pour utiliser le program_invocation_short_name (extension gnu)

#include "mumudvb.h"
#include "tune.h"
#include "udp.h"
#include "dvb.h"
#include "errors.h"

#define VERSION "1.2"


/* Signal handling code shamelessly copied from VDR by Klaus Schmidinger 
   - see http://www.cadsoft.de/people/kls/vdr/index.htm */

// global variable user by SignalHandler
long now;
long time_no_diff;
long real_start_time;
int aff_force_signal = 0;
int no_daemon = 0;
int nb_flux;
int chaines_diffuses[MAX_CHAINES];
int chaines_diffuses_old[MAX_CHAINES];
char noms[MAX_CHAINES][MAX_LEN_NOM];
int carte_accordee = 0;
int timeout_accord = ALARM_TIME_TIMEOUT;
int timeout_no_diff = ALARM_TIME_TIMEOUT_NO_DIFF;
int Interrupted = 0;
char nom_fich_chaines_diff[256];
char nom_fich_chaines_non_diff[256];
int card = 0;


// file descriptors
fds_t fds; // defined in dvb.h


//Global for the moment
// CRC table for PAT rebuilding
unsigned long       crc32_table[256];

// prototypes
static void SignalHandler (int signum);
void gen_chaines_diff (int no_daemon, int *chaines_diffuses);
int pat_rewrite(unsigned char *buf,int num_pids, int *pids);


void
usage (char *name)
{
  fprintf (stderr, "Usage: %s [options] \n"
	   "-c, --config : Config file\n"
	   "-s, --signal : Display signal power\n"
	   "-d, --debug  : Don't deamonize\n"
	   "-h, --help   : Help\n"
	   "\n"
	   "%s Version "
	   VERSION
	   "\n"
	   "Based on dvbstream 0.6 by (C) Dave Chapman 2001-2004\n"
	   "Released under the GPL.\n"
	   "Latest version available from http://mumudvb.braice.net/\n"
	   "Modified for the cr@ns (www.crans.org)\n"
	   "by Brice DUBOST (mumudvb@braice.net)\n", name, name);
}


int
main (int argc, char **argv)
{
  int k;
  int buf_pos;
  unsigned char buf[MAX_CHAINES][MAX_UDP_SIZE];
  struct pollfd pfds[2];	//  DVR device


  // Tuning parapmeters
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
  unsigned char diseqc = 0;

  // DVB reception and sort
  int pid;			// pid of the current mpeg2 packet
  int bytes_read;		// number of bytes actually read
  unsigned char temp_buf[TS_PACKET_SIZE];
  unsigned char temp_buf2[TS_PACKET_SIZE];
  int nb_bytes[MAX_CHAINES];
  //Mandatory pids
  int mandatory_pid[MAX_MANDATORY];

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
  int ip_ok = 0;
  char ligne_courante[CONF_LINELEN];
  char *sous_chaine;
  char delimiteurs[] = " =";


  unsigned char hi_mappids[8192];
  unsigned char lo_mappids[8192];
  int pids[MAX_CHAINES][MAX_PIDS_PAR_CHAINE];
  int num_pids[MAX_CHAINES];
  int alarm_count = 0;
  int count_non_transmis = 0;
  int tune_retval=0;


  //does we rewrite the pat pid ?
  int rewrite_pat = 0;
  unsigned long crc32_table_temp_var[3];

  const char short_options[] = "c:sdh";
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
	      if (!no_daemon)
		syslog (LOG_USER, "malloc() failed: %s\n", strerror(errno));
	      else
		fprintf(stderr, "malloc() failed: %s\n", strerror(errno));
	      exit(errno);
	    }
	  strncpy (conf_filename, optarg, strlen (optarg) + 1);
	  break;
	case 's':
	  aff_force_signal = 1;
	  break;
	case 'd':
	  no_daemon = 1;
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

  // DO NOT REMOVE
  if(!no_daemon)
    {
      daemon(42,0);
    }

  if (!no_daemon)
    openlog ("MUMUDVB", LOG_PID, 0);

  // config file
  conf_file = fopen (conf_filename, "r");
  if (conf_file == NULL)
    {
      if (!no_daemon)
	syslog (LOG_USER, "%s: %s\n",
		conf_filename, strerror (errno));
      else
	fprintf (stderr,
		 "%s: %s\n",
		 conf_filename, strerror (errno));
      exit(ERROR_CONF_FILE);
    }

  memset (pids, 0, sizeof (pids));



  // we scan config file
  // see doc/README-conf for further information
  while (fgets (ligne_courante, CONF_LINELEN, conf_file)
	 && strlen (ligne_courante) > 1)
    {
      sous_chaine = strtok (ligne_courante, delimiteurs);
      //commentary
      if (sous_chaine[0] == '#')
	continue; 
      
      if (!strcmp (sous_chaine, "timeout_accord"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);	// on extrait la sous chaine
	  timeout_accord = atoi (sous_chaine);
	}
      else if (!strcmp (sous_chaine, "timeout_no_diff"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);
	  timeout_no_diff= atoi (sous_chaine);
	}
      else if (!strcmp (sous_chaine, "rewrite_pat"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);
	  rewrite_pat = atoi (sous_chaine);
	  if(rewrite_pat)
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"!!! You have enabled the Pat Rewriting, this is an experimental feature, you have been warned\n");
	      else
		fprintf (stderr,
			"!!! You have enabled the Pat Rewriting, this is an experimental feature, you have been warned\n");

	      //we compute the crc32 tables
	      //CRC32 table initialisation (taken from the xine project)
	      for( crc32_table_temp_var[0] = 0 ; crc32_table_temp_var[0] < 256 ; crc32_table_temp_var[0]++ ) {
		crc32_table_temp_var[2] = 0;
		for (crc32_table_temp_var[1] = (crc32_table_temp_var[0] << 24) | 0x800000 ; crc32_table_temp_var[1] != 0x80000000 ; crc32_table_temp_var[1] <<= 1) {
		  crc32_table_temp_var[2] = (crc32_table_temp_var[2] << 1) ^ (((crc32_table_temp_var[2] ^ crc32_table_temp_var[1]) & 0x80000000) ? 0x04c11db7 : 0);
		}
		crc32_table[crc32_table_temp_var[0]] = crc32_table_temp_var[2];
	      }
	    }
	}
      else if (!strcmp (sous_chaine, "freq"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);
	  freq = atol (sous_chaine);
	  freq *= 1000UL;
	}
      else if (!strcmp (sous_chaine, "pol"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);
	  if (tolower (sous_chaine[0]) == 'v')
	    {
	      pol = 'V';
	    }
	  else if (tolower (sous_chaine[0]) == 'h')
	    {
	      pol = 'H';
	    }
	  else
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"Config issue : %s polarisation\n",
			conf_filename);
	      else
		fprintf (stderr,
			 "Config issue : %s polarisation\n",
			 conf_filename);
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (sous_chaine, "srate"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);
	  srate = atol (sous_chaine);
	  srate *= 1000UL;
	}
      else if (!strcmp (sous_chaine, "card"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);
	  card = atoi (sous_chaine);
	  if (card > 5)
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"Six cards MAX\n");
	      else
		fprintf (stderr,
			"Six cards MAX\n");
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (sous_chaine, "ip"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);
	  sscanf (sous_chaine, "%s\n", ipOut[curr_channel]);
	  ip_ok = 1;
	}
      else if (!strcmp (sous_chaine, "port"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);
	  portOut[curr_channel] = atoi (sous_chaine);
	  port_ok = 1;
	}
      else if (!strcmp (sous_chaine, "pids"))
	{
	  if (port_ok == 0 || ip_ok == 0)
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"You must precise ip and port before PIDs\n");
	      else
		fprintf (stderr,
			"You must precise ip and port before PIDs\n");
	      exit(ERROR_CONF);
	    }
	  while ((sous_chaine = strtok (NULL, delimiteurs)) != NULL)
	    {
	      pids[curr_channel][curr_pid] = atoi (sous_chaine);
	      // we see if the given pid is good
	      if (pids[curr_channel][curr_pid] < 10 || pids[curr_channel][curr_pid] > 8191)
		{
		  if (!no_daemon)
		    syslog (LOG_USER,
			    "Config issue : %s in pids, given pid : %d\n",
			    conf_filename, pids[curr_channel][curr_pid]);
		  else
		    fprintf (stderr,
			    "Config issue : %s in pids, given pid : %d\n",
			     conf_filename, pids[curr_channel][curr_pid]);
		  exit(ERROR_CONF);
		}
	      curr_pid++;
	      if (curr_pid >= MAX_PIDS_PAR_CHAINE)
		{
		  if (!no_daemon)
		    syslog (LOG_USER,
			    "Too many pids : %d channel : %d\n",
			    curr_pid, curr_channel);
		  else
		    fprintf (stderr,
			    "Too many pids : %d channel : %d\n",
			     curr_pid, curr_channel);
		  exit(ERROR_CONF);
		}
	    }
	  num_pids[curr_channel] = curr_pid;
	  curr_pid = 0;
	  curr_channel++;
	  port_ok = 0;
	  ip_ok = 0;
	}
      else if (!strcmp (sous_chaine, "name"))
	{
	  // other substring extraction method in order to keep spaces
	  sous_chaine = strtok (NULL, "=");
	  if (!(strlen (sous_chaine) >= MAX_LEN_NOM - 1))
	    strcpy(noms[curr_channel],strtok(sous_chaine,"\n"));	
	  else
	    {
	      if (!no_daemon)
		syslog (LOG_USER,"Channel name too long\n");
	      else
		fprintf (stderr, "Channel name too long\n");
	    }
	}
      else if (!strcmp (sous_chaine, "qam"))
	{
	  // TNT
	  sous_chaine = strtok (NULL, delimiteurs);
	  sscanf (sous_chaine, "%s\n", sous_chaine);
	  if (!strcmp (sous_chaine, "qpsk"))
	    modulation=QPSK;
	  else if (!strcmp (sous_chaine, "16"))
	    modulation=QAM_16;
	  else if (!strcmp (sous_chaine, "32"))
	    modulation=QAM_32;
	  else if (!strcmp (sous_chaine, "64"))
	    modulation=QAM_64;
	  else if (!strcmp (sous_chaine, "128"))
	    modulation=QAM_128;
	  else if (!strcmp (sous_chaine, "256"))
	    modulation=QAM_256;
	  else if (!strcmp (sous_chaine, "auto"))
	    modulation=QAM_AUTO;
	  else
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"Config issue : QAM\n");
	      else
		fprintf (stderr,
			"Config issue : QAM\n");
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (sous_chaine, "trans_mode"))
	{
	  // TNT
	  sous_chaine = strtok (NULL, delimiteurs);
	  sscanf (sous_chaine, "%s\n", sous_chaine);
	  if (!strcmp (sous_chaine, "2k"))
	    TransmissionMode=TRANSMISSION_MODE_2K;
	  else if (!strcmp (sous_chaine, "8k"))
	    TransmissionMode=TRANSMISSION_MODE_8K;
	  else if (!strcmp (sous_chaine, "auto"))
	    TransmissionMode=TRANSMISSION_MODE_AUTO;
	  else
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"Config issue : trans_mode\n");
	      else
		fprintf (stderr,
			"Config issue : trans_mode\n");
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (sous_chaine, "bandwidth"))
	{
	  // TNT
	  sous_chaine = strtok (NULL, delimiteurs);
	  sscanf (sous_chaine, "%s\n", sous_chaine);
	  if (!strcmp (sous_chaine, "8MHz"))
	    bandWidth=BANDWIDTH_8_MHZ;
	  else if (!strcmp (sous_chaine, "7MHz"))
	    bandWidth=BANDWIDTH_7_MHZ;
	  else if (!strcmp (sous_chaine, "6MHz"))
	    bandWidth=BANDWIDTH_6_MHZ;
	  else if (!strcmp (sous_chaine, "auto"))
	    bandWidth=BANDWIDTH_AUTO;
	  else
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"Config issue : bandwidth\n");
	      else
		fprintf (stderr,
			"Config issue : bandwidth\n");
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (sous_chaine, "guardinterval"))
	{
	  // TNT
	  sous_chaine = strtok (NULL, delimiteurs);
	  sscanf (sous_chaine, "%s\n", sous_chaine);
	  if (!strcmp (sous_chaine, "1/32"))
	    guardInterval=GUARD_INTERVAL_1_32;
	  else if (!strcmp (sous_chaine, "1/16"))
	    guardInterval=GUARD_INTERVAL_1_16;
	  else if (!strcmp (sous_chaine, "1/8"))
	    guardInterval=GUARD_INTERVAL_1_8;
	  else if (!strcmp (sous_chaine, "1/4"))
	    guardInterval=GUARD_INTERVAL_1_4;
	  else if (!strcmp (sous_chaine, "auto"))
	    guardInterval=GUARD_INTERVAL_AUTO;
	  else
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"Config issue : guardinterval\n");
	      else
		fprintf (stderr,
			 "Config issue : guardinterval\n");
	      exit(ERROR_CONF);
	    }
	}
      else if (!strcmp (sous_chaine, "coderate"))
	{
	  // TNT
	  sous_chaine = strtok (NULL, delimiteurs);
	  sscanf (sous_chaine, "%s\n", sous_chaine);
	  if (!strcmp (sous_chaine, "none"))
	    HP_CodeRate=FEC_NONE;
	  else if (!strcmp (sous_chaine, "1/2"))
	    HP_CodeRate=FEC_1_2;
	  else if (!strcmp (sous_chaine, "2/3"))
	    HP_CodeRate=FEC_2_3;
	  else if (!strcmp (sous_chaine, "3/4"))
	    HP_CodeRate=FEC_3_4;
	  else if (!strcmp (sous_chaine, "4/5"))
	    HP_CodeRate=FEC_4_5;
	  else if (!strcmp (sous_chaine, "5/6"))
	    HP_CodeRate=FEC_5_6;
	  else if (!strcmp (sous_chaine, "6/7"))
	    HP_CodeRate=FEC_6_7;
	  else if (!strcmp (sous_chaine, "7/8"))
	    HP_CodeRate=FEC_7_8;
	  else if (!strcmp (sous_chaine, "8/9"))
	    HP_CodeRate=FEC_8_9;
	  else if (!strcmp (sous_chaine, "auto"))
	    HP_CodeRate=FEC_AUTO;
	  else
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"Config issue : coderate\n");
	      else
		fprintf (stderr,
			"Config issue : coderate\n");
	      exit(ERROR_CONF);
	    }
	  LP_CodeRate=HP_CodeRate; // je ne sais pas tres bien ce que cela change mais je les met egales
	}
      else
	{
	  /*probleme */
	  if (!no_daemon)
	    syslog (LOG_USER,
		    "Config issue : unknow symbol : %s\n\n", sous_chaine);
	  else
	    fprintf (stderr,
		    "Config issue : unknow symbol : %s\n\n", sous_chaine);
	  continue;
	}
    }
  nb_flux = curr_channel;
  if (curr_channel > MAX_CHAINES)
    {
      if (!no_daemon)
	syslog (LOG_USER, "Too many channels : %d limit : %d\n",
		curr_channel, MAX_CHAINES);
      else
	fprintf (stderr, "Too many channels : %d limit : %d\n",
		 curr_channel, MAX_CHAINES);
      exit(ERROR_TOO_CHANNELS);
    }

  // we clear it by precaution
  sprintf (nom_fich_chaines_diff, "/var/run/mumudvb/chaines_diffusees_carte%d",
	   card);
  sprintf (nom_fich_chaines_non_diff, "/var/run/mumudvb/chaines_non_diffusees_carte%d",
	   card);
  chaines_diff = fopen (nom_fich_chaines_diff, "w");
  if (chaines_diff == NULL)
    {
      if (!no_daemon)
	syslog (LOG_USER,
		"%s: %s\n",
		nom_fich_chaines_diff, strerror (errno));
      else
	fprintf (stderr,
		 "%s: %s\n",
		 nom_fich_chaines_diff, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }

  fclose (chaines_diff);

  chaines_non_diff = fopen (nom_fich_chaines_non_diff, "w");
  if (chaines_diff == NULL)
    {
      if (!no_daemon)
	syslog (LOG_USER,
		"%s: %s\n",
		nom_fich_chaines_non_diff, strerror (errno));
      else
	fprintf (stderr,
		 "%s: %s\n",
		 nom_fich_chaines_non_diff, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }

  fclose (chaines_non_diff);

  if (!no_daemon)
    syslog (LOG_USER, "Diffusion. Freq %lu pol %c srate %lu\n",
	    freq, pol, srate);

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
		     bandWidth, LP_CodeRate, hier, aff_force_signal);
	}
    }
  else if ((freq != 0) && (pol != 0) && (srate != 0))
    {
      if (open_fe (&fds.fd_frontend, card))
	{
	  if (!no_daemon)
	    syslog (LOG_USER, "Tuning to %ld Hz\n", freq);
	  else
	    fprintf (stderr, "Tuning to %ld Hz\n", freq);
	  tune_retval =
	    tune_it (fds.fd_frontend, freq, srate, pol, tone, specInv, diseqc,
		     modulation, HP_CodeRate, TransmissionMode, guardInterval,
		     bandWidth, LP_CodeRate, hier, aff_force_signal);
	}
    }

  if (tune_retval < 0)
    {
      if (!no_daemon)
	syslog (LOG_USER, "Tunning issue, card %d\n", card);
      else
	fprintf (stderr, "Tunning issue, card %d\n", card);

      // we close the file descriptors
      close_card_fd (card, nb_flux, num_pids, mandatory_pid, fds);
      exit(ERROR_TUNE);
    }

  carte_accordee = 1;
  // the card is tuned, we catch signals to close cleanly
  if (signal (SIGHUP, SignalHandler) == SIG_IGN)
    signal (SIGHUP, SIG_IGN);
  if (signal (SIGINT, SignalHandler) == SIG_IGN)
    signal (SIGINT, SIG_IGN);
  if (signal (SIGTERM, SignalHandler) == SIG_IGN)
    signal (SIGTERM, SIG_IGN);
  alarm (ALARM_TIME);

  // We write our pid in a file if deamon
  if (!no_daemon)
    {
      sprintf (nom_fich_pid, "/var/run/mumudvb/mumudvb_carte%d.pid", card);
      pidfile = fopen (nom_fich_pid, "w");
      if (pidfile == NULL)
	{
	  syslog (LOG_USER, "%s: %s\n",
		  nom_fich_pid, strerror (errno));
	  exit(ERROR_CREATE_FILE);
	}
      fprintf (pidfile, "%d\n", getpid ());
      fclose (pidfile);
    }


  // init of active channels list
  for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
    {
      chaines_diffuses[curr_channel] = 0;
      chaines_diffuses_old[curr_channel] = 1;
    }

  //We initialise mantadory table
  for(curr_pid_mandatory=0;curr_pid_mandatory<MAX_MANDATORY;curr_pid_mandatory++)
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

  // we open the file descriptors
  if (create_card_fd (card, nb_flux, num_pids, mandatory_pid, &fds) < 0)
    return -1;

  for(curr_pid_mandatory=0;curr_pid_mandatory<MAX_MANDATORY;curr_pid_mandatory++)
    if(mandatory_pid[curr_pid_mandatory]==1)
      set_ts_filt (fds.fd_mandatory[curr_pid_mandatory], curr_pid_mandatory, DMX_PES_OTHER);

  for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
    {
      for (curr_pid = 0; curr_pid < num_pids[curr_channel]; curr_pid++)
	set_ts_filt (fds.fd[curr_channel][curr_pid], pids[curr_channel][curr_pid], DMX_PES_OTHER);
    }

  gettimeofday (&tv, (struct timezone *) NULL);
  real_start_time = tv.tv_sec;
  now = 0;

  ttl = 2;
  if (!no_daemon)
    syslog (LOG_USER, "Card %d tuned\n", card);
  else
    fprintf (stderr, "Card %d tuned\n", card);

  // Init udp
  for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
    {
      // le makeclientsocket est pour joindre automatiquement l'ip de multicast créée
      // les swiths HP broadcastent les ip de multicast sans clients
      socketOut[curr_channel] = makeclientsocket (ipOut[curr_channel], portOut[curr_channel], ttl, &sOut[curr_channel], no_daemon);
    }

  if (!no_daemon)
    syslog (LOG_USER, "Diffusion %d channel%s\n", nb_flux,
	    (nb_flux == 1 ? "" : "s"));
  else
    fprintf (stderr, "Diffusion %d channel%s\n", nb_flux,
	     (nb_flux == 1 ? "" : "s"));

  for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
    {
      if (!no_daemon)
	{
	  syslog (LOG_USER, "Channel \"%s\" num %d ip %s:%d\n",
		  noms[curr_channel], curr_channel, ipOut[curr_channel], portOut[curr_channel]);
	}
      else
	{
	  fprintf (stderr,
		   "Channel \"%s\" num %d ip %s:%d\n",
		   noms[curr_channel], curr_channel, ipOut[curr_channel], portOut[curr_channel]);
	  fprintf (stderr, "        pids : ");
	  for (curr_pid = 0; curr_pid < num_pids[curr_channel]; curr_pid++)
	    fprintf (stderr, "%d ", pids[curr_channel][curr_pid]);
	  fprintf (stderr, "\n");
	}
    }


  // Stream reading and sending

  pfds[0].fd = fds.fd_dvr;
  pfds[0].events = POLLIN | POLLPRI;
  pfds[1].events = POLLIN | POLLPRI;

  for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
    nb_bytes[curr_channel]=0;

  while (!Interrupted)
    {
      /* Poll the open file descriptors */
      poll (pfds, 1, 500);

      /* Attempt to read 188 bytes from /dev/ost/dvr */
      if ((bytes_read = read (fds.fd_dvr, temp_buf, TS_PACKET_SIZE)) > 0)
	{
	  if (bytes_read != TS_PACKET_SIZE)
	    {
		if (!no_daemon)
		  syslog (LOG_USER, "No bytes left to read - aborting\n");
		else
		  fprintf (stderr, "No bytes left to read - aborting\n");
		break;
	    }

	  pid = ((temp_buf[1] & 0x1f) << 8) | (temp_buf[2]);

	  //Pat rewrite only
	  //we save the full pat before otherwise only the first channel will be rewritten with a full PAT
	  if( (pid == 0) && //This is a PAT PID
	      rewrite_pat ) //AND we asked for rewrite
	    for(buf_pos=0;buf_pos<TS_PACKET_SIZE;buf_pos++)
	      temp_buf2[buf_pos]=temp_buf[buf_pos]; //We save the full pat
	  

	  //for each channel we'll look if we must send this PID
	  for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
	    {
	      //we'll see if we must send this pid for this channel
	      send_packet=0;
	      
	      //If it's a mandatory pid we send it
	      if((pid<MAX_MANDATORY) && (mandatory_pid[pid]==1))
		send_packet=1;
	      
	      //if it isn't mandatory wee see if it is in the channel list
	      if(!send_packet)
		for (curr_pid = 0; (curr_pid < num_pids[curr_channel])&& !send_packet; curr_pid++)
		  if ((pids[curr_channel][curr_pid] == pid)) {
		    send_packet=1;
		    chaines_diffuses[curr_channel]++;
		  }

	      //Rewrite PAT checking
	      if(send_packet==1)
		if( (pid == 0) && //This is a PAT PID
		     rewrite_pat ) //AND we asked for rewrite
		  {
		    for(buf_pos=0;buf_pos<TS_PACKET_SIZE;buf_pos++)
		      temp_buf[buf_pos]=temp_buf2[buf_pos]; //We restore the full PAT
		    //and we try to rewrite it
		    if(pat_rewrite(temp_buf,num_pids[curr_channel],pids[curr_channel])) //We try rewrite and if there's an error...
		      send_packet=0;//... we don't send it anyway
		  }

	      //Ok we must send it
	      if(send_packet==1)
		{
		  // we fill the channel buffer
		  for (buf_pos = 0; buf_pos < bytes_read; buf_pos++)
		    buf[curr_channel][nb_bytes[curr_channel] + buf_pos] = temp_buf[buf_pos];

		  buf[curr_channel][nb_bytes[curr_channel] + 1] =
		    (buf[curr_channel][nb_bytes[curr_channel] + 1] & 0xe0) | hi_mappids[pid];
		  buf[curr_channel][nb_bytes[curr_channel] + 2] = lo_mappids[pid];

		  nb_bytes[curr_channel] += bytes_read;
		  //The buffer is full, we send it
		  if ((nb_bytes[curr_channel] + TS_PACKET_SIZE) > MAX_UDP_SIZE)
		    {
		      sendudp (socketOut[curr_channel], &sOut[curr_channel], buf[curr_channel],
			       nb_bytes[curr_channel]);
		      nb_bytes[curr_channel] = 0;
		    }
		}


	      count_non_transmis = 0;
	      if (alarm_count == 1)
		{
		  alarm_count = 0;
		  if (!no_daemon)
		    syslog (LOG_USER,
			   "Good, we receive back significant data\n");
		  else
		    fprintf (stderr,
			   "Good, we receive back significant data\n");
		}
	    }
	  count_non_transmis++;
	  if (count_non_transmis > ALARM_COUNT_LIMIT)
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"Error : less than one paquet on %d sent\n",
			ALARM_COUNT_LIMIT);
	      else
		fprintf (stderr,
			 "Error : less than one paquet on %d sent\n",
			 ALARM_COUNT_LIMIT);
	      alarm_count = 1;
	    }
	}
    }

  if (Interrupted)
    {
      if (!no_daemon)
	{
	  if(Interrupted< (1<<8)) //we check if it's a signal or a mumudvb error
	    syslog (LOG_USER, "\nCaught signal %d - closing cleanly.\n",
		    Interrupted);
	  else
	    syslog (LOG_USER, "\nclosing cleanly.\n");
	}
      else
	{
	  if(Interrupted< (1<<8)) //we check if it's a signal or a mumudvb error
	    fprintf (stderr, "\nCaught signal %d - closing cleanly.\n",
		     Interrupted);
	  else
	    fprintf (stderr, "\nclosing cleanly.\n");

	}
    }

  for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
    close (socketOut[curr_channel]);

  // we close the file descriptors
  close_card_fd (card, nb_flux, num_pids, mandatory_pid, fds);

  if (remove (nom_fich_chaines_diff))
    {
      if (!no_daemon)
	syslog (LOG_USER,
		"%s: %s\n",
		nom_fich_chaines_diff, strerror (errno));
      else
	fprintf (stderr,
		 "%s: %s\n",
		 nom_fich_chaines_diff, strerror (errno));
      exit(ERROR_DEL_FILE);
    }

  if (remove (nom_fich_chaines_non_diff))
    {
      if (!no_daemon)
	syslog (LOG_USER,
		"%s: %s\n",
		nom_fich_chaines_non_diff, strerror (errno));
      else
	fprintf (stderr,
		 "%s: %s\n",
		 nom_fich_chaines_non_diff, strerror (errno));
      exit(ERROR_DEL_FILE);
    }


  if (!no_daemon)
    {
      if (remove (nom_fich_pid))
	{
	  syslog (LOG_USER, "%s: %s\n",
		  nom_fich_pid, strerror (errno));
	  exit(ERROR_DEL_FILE);
	}
    }

  if(Interrupted<(1<<8))
    return (0);
  else
    return(Interrupted>>8);
}


static void
SignalHandler (int signum)
{
  struct timeval tv;
  int curr_channel = 0;
  int compteur_chaines_diff=0;

  if (signum == SIGALRM)
    {
      gettimeofday (&tv, (struct timezone *) NULL);
      now = tv.tv_sec - real_start_time;
      if (aff_force_signal && carte_accordee)
	affiche_puissance (fds, no_daemon);
      if (!carte_accordee)
	{
	  if (!no_daemon)
	    syslog (LOG_USER,
		    "Card not tuned after %ds - exiting\n",
		    timeout_accord);
	  else
	    fprintf (stderr,
		    "Card not tuned after %ds - exiting\n",
		     timeout_accord);
	  exit(ERROR_TUNE);
	}	
      for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
	if ((chaines_diffuses[curr_channel] >= 100) && (!chaines_diffuses_old[curr_channel]))
	  {
	    if (!no_daemon)
	      syslog (LOG_USER,
		      "Channel \"%s\" back.Card %d\n",
		      noms[curr_channel], card);
	    else
	      fprintf (stderr,
		      "Channel \"%s\" back.Card %d\n",
		       noms[curr_channel], card);
	    chaines_diffuses_old[curr_channel] = 1;	// update
	  }
	else if ((chaines_diffuses_old[curr_channel]) && (chaines_diffuses[curr_channel] < 100))
	  {
	    if (!no_daemon)
	      syslog (LOG_USER,
		      "Channel \"%s\" down.Card %d\n",
		      noms[curr_channel], card);
	    else
	      fprintf (stderr,
		      "Channel \"%s\" down.Card %d\n",
		      noms[curr_channel], card);
	    chaines_diffuses_old[curr_channel] = 0;	// update
	  }

      // we count active channels
      for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
	if (chaines_diffuses_old[curr_channel])
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
	  if (!no_daemon)
	    syslog (LOG_USER,
		    "No data from card %d in %ds, exiting.\n",
		      card, timeout_no_diff);
	    else
	      fprintf (stderr,
		    "No data from card %d in %ds, exiting.\n",
		      card, timeout_no_diff);
	  Interrupted=ERROR_NO_DIFF<<8; //the <<8 is to make difference beetween signals and errors
	}

      // on envoie le old pour annoncer que les chaines
      // qui diffusent au dessus du quota de pauqets
      gen_chaines_diff (no_daemon, chaines_diffuses_old);

      // reinit
      for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
	chaines_diffuses[curr_channel] = 0;
      alarm (ALARM_TIME);
    }
  else if (signum == SIGUSR1)
    {
      aff_force_signal = aff_force_signal ? 0 : 1;
    }
  else if (signum != SIGPIPE)
    {
      Interrupted = signum;
    }
  signal (signum, SignalHandler);
}


void
gen_chaines_diff (int no_daemon, int *chaines_diffuses)
{
  FILE *chaines_diff;
  FILE *chaines_non_diff;
  int curr_channel;

  chaines_diff = fopen (nom_fich_chaines_diff, "w");
  if (chaines_diff == NULL)
    {
      if (!no_daemon)
	syslog (LOG_USER,
		"%s: %s\n",
		nom_fich_chaines_diff, strerror (errno));
      else
	fprintf (stderr,
		 "%s: %s\n",
		 nom_fich_chaines_diff, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }

  chaines_non_diff = fopen (nom_fich_chaines_non_diff, "w");
  if (chaines_non_diff == NULL)
    {
      if (!no_daemon)
	syslog (LOG_USER,
		"%s: %s\n",
		nom_fich_chaines_non_diff, strerror (errno));
      else
	fprintf (stderr,
		 "%s: %s\n",
		 nom_fich_chaines_non_diff, strerror (errno));
      exit(ERROR_CREATE_FILE);
    }

  for (curr_channel = 0; curr_channel < nb_flux; curr_channel++)
    if (chaines_diffuses[curr_channel])
      fprintf (chaines_diff, "%s:%d:%s\n", ipOut[curr_channel], portOut[curr_channel], noms[curr_channel]);
    else
      fprintf (chaines_non_diff, "%s:%d:%s\n", ipOut[curr_channel], portOut[curr_channel], noms[curr_channel]);
  fclose (chaines_diff);
  fclose (chaines_non_diff);

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
	  if (!no_daemon)
	    syslog (LOG_USER,"PAT too big : %d, don't know how rewrite, sent as is\n", section_length);
	  else
	    fprintf (stderr, "PAT too big : %d, don't know how rewrite, sent as is\n", section_length);
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

  //All is Ok ....
  return 0;

}

