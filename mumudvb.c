/* 
mumudvb - UDP-ize a DVB transport stream.
(C) Dave Chapman <dave@dchapman.com> 2001, 2002.
Modified By Brice DUBOST
 * 
The latest version can be found at http://www.crans.org

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
  /* ATTENTION
     Version modifiee par braice (sigfault@netcourrier.com) pour le Crans (www.crans.org)
     me demander pour toute documentation)
   */


#define _GNU_SOURCE		// pour utiliser le program_invocation_short_name (extention gnu)

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
#include <errno.h>		// pour utiliser le program_invocation_short_name (extention gnu)

#include "mumudvb.h"
#include "tune.h"
#include "udp.h"
#include "dvb.h"

#define VERSION "1.0.20050421"


/* Signal handling code shamelessly copied from VDR by Klaus Schmidinger 
   - see http://www.cadsoft.de/people/kls/vdr/index.htm */

//global variable user by SignalHandler
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
int card = 0;


//file descriptors
fds_t fds;			//defined in dvb.h


//prototypes
static void SignalHandler (int signum);
void gen_chaines_diff (int no_daemon, int *chaines_diffuses);
//fin des prototypes

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
	   "Latest version available from http://www.crans.org/\n"
	   "Version modifiee pour le CRANS (www.crans.org)\n"
	   "par Brice DUBOST (brice.dubost@crans.org)\n", name, name);
}


int
main (int argc, char **argv)
{
  int i, j;
  unsigned char buf[MAX_FLUX][MTU];
  struct pollfd pfds[2];	/*  DVR device and Telnet connection */


  //reception
  unsigned long freq = 0;
  unsigned long srate = 0;
  char pol = 0;

  //DVB reception and sort
  int pid;			//pid of the current mpeg2 packet
  int bytes_read;		//nuber of bytes actually read
  unsigned char temp_buf[MTU];
  int nb_bytes[MAX_CHAINES];

  struct timeval tv;

  //files
  char *conf_filename = NULL;
  FILE *conf_file;
  char nom_fich_pid[256];
  //cf fin
  FILE *chaines_diff;
  FILE *pidfile;

  //configuration file parsing
  int curr_chaine = 0;
  int curr_pid = 0;
  int port_ok = 0;
  int ip_ok = 0;
  char ligne_courante[CONF_LINELEN];
  char *sous_chaine;
  char delimiteurs[] = " =";


  fe_spectral_inversion_t specInv = INVERSION_AUTO;
  int tone = -1;
  //debut des parametres TNT a changer
  fe_modulation_t modulation = CONSTELLATION_DEFAULT;
  fe_transmit_mode_t TransmissionMode = TRANSMISSION_MODE_DEFAULT;
  fe_bandwidth_t bandWidth = BANDWIDTH_DEFAULT;
  fe_guard_interval_t guardInterval = GUARD_INTERVAL_DEFAULT;
  fe_code_rate_t HP_CodeRate = HP_CODERATE_DEFAULT, LP_CodeRate =
    LP_CODERATE_DEFAULT;
  //fin des params TNT
  fe_hierarchy_t hier = HIERARCHY_DEFAULT;
  unsigned char diseqc = 0;
  unsigned char hi_mappids[8192];
  unsigned char lo_mappids[8192];
  int pids[MAX_CHAINES][MAX_PIDS_PAR_CHAINE];
  int num_pids[MAX_CHAINES];
  int alarm_count = 0;
  int count_non_transmis = 0;


  const char short_options[] = "c:sdh";
  const struct option long_options[] = {
    {"config", required_argument, NULL, 'c'},
    {"signal", no_argument, NULL, 's'},
    {"debug", no_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0}
  };
  int c, option_index = 0;
  /* Initialise PID map */
  for (i = 0; i < 8192; i++)
    {
      hi_mappids[i] = (i >> 8);
      lo_mappids[i] = (i & 0xff);
    }

  if (argc == 1)
    {
      usage (argv[0]);
      exit (1);
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
	  conf_filename = malloc (strlen (optarg) + 1);
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
	  exit (1);
	  break;
	}
    }
  if (optind < argc)
    {
      usage (program_invocation_short_name);
      exit (1);
    }

  //DO NOT REMOVE
  if(!no_daemon)
    {
      daemon(42,0);
    }

  if (!no_daemon)
    openlog ("MUMUDVB", LOG_PID, 0);

  /* on ouvre le fichier de conf */
  conf_file = fopen (conf_filename, "r");
  if (conf_file == NULL)
    {
      if (!no_daemon)
	syslog (LOG_USER, "N'as pas pu ouvrir le fichier de config %s\n",
		conf_filename);
      else
	fprintf (stderr,
		 "N'as pas pu ouvrir le fichier de config %s\n",
		 conf_filename);
      exit (2);
    }

  memset (pids, 0, sizeof (pids));	//init du tableau de pids



  //on scanne le fichier de conf
  // la syntaxe de celui ci est dans syntaxe_conf.txt
  while (fgets (ligne_courante, CONF_LINELEN, conf_file)
	 && strlen (ligne_courante) > 1)
    {
      sous_chaine = strtok (ligne_courante, delimiteurs);	//on extrait la sous chaine avant le =
      if (sous_chaine[0] == '#')
	continue; 
      if (!strcmp (sous_chaine, "timeout_accord"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  timeout_accord = atoi (sous_chaine);
	}
      else if (!strcmp (sous_chaine, "timeout_no_diff"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  timeout_no_diff= atoi (sous_chaine);
	}
      else if (!strcmp (sous_chaine, "freq"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  freq = atol (sous_chaine);
	  freq *= 1000UL;
	  /*insérer un controle de validité de la fréquence */
	}
      else if (!strcmp (sous_chaine, "pol"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
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
			"Probleme avec le fichier de configuration %s au niveau de la polarisation\n",
			conf_filename);
	      else
		fprintf (stderr,
			 "Pb configuration %s polarisation\n",
			 conf_filename);
	      exit (3);
	    }
	}
      else if (!strcmp (sous_chaine, "srate"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  srate = atol (sous_chaine);
	  srate *= 1000UL;
	  /*insérer un controle de validité du srate */
	}
      else if (!strcmp (sous_chaine, "card"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  card = atoi (sous_chaine);
	  if (card > 5)		//on verifie la validité du numero de carte
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"il n'y a que 6 cartes au maximum sur une machine !!!!\n");
	      else
		fprintf (stderr,
			 "6 cartes Maximum\n");
	      exit (30);
	    }
	}
      else if (!strcmp (sous_chaine, "ip"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  sscanf (sous_chaine, "%s\n", ipOut[curr_chaine]);	//pour enlever le \n a la fin et les espaces
	  ip_ok = 1;
	}
      else if (!strcmp (sous_chaine, "port"))
	{
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  portOut[curr_chaine] = atoi (sous_chaine);
	  port_ok = 1;
	}
      else if (!strcmp (sous_chaine, "pids"))
	{
	  if (port_ok == 0 || ip_ok == 0)
	    {
	      if (!no_daemon)
		syslog (LOG_USER,
			"Il faut préciser le port et l'ip avant les pids\n");
	      else
		fprintf (stderr,
			 "Il faut préciser le port et l'ip avant les pids\n");
	      exit (100);
	    }
	  while ((sous_chaine = strtok (NULL, delimiteurs)) != NULL)	//on extrait la sous chaine
	    {
	      pids[curr_chaine][curr_pid] = atoi (sous_chaine);
	      if (pids[curr_chaine][curr_pid] < 10 || pids[curr_chaine][curr_pid] > 8191)	//on verifie la validité du pid donné
		{
		  if (!no_daemon)
		    syslog (LOG_USER,
			    "Probleme avec le fichier de configuration %s au niveau des pids, le pid donné etait %d\n",
			    conf_filename, pids[curr_chaine][curr_pid]);
		  else
		    fprintf (stderr,
			     "Pb configuration %s pids, pid donné : %d\n",
			     conf_filename, pids[curr_chaine][curr_pid]);
		  exit (20);
		}
	      curr_pid++;
	      if (curr_pid >= MAX_PIDS_PAR_CHAINE)
		{
		  if (!no_daemon)
		    syslog (LOG_USER,
			    "Trop de pids :  %d pour la chaine %d\n",
			    curr_pid, curr_chaine);
		  else
		    fprintf (stderr, "Trop de pids :  %d chaine %d\n",
			     curr_pid, curr_chaine);
		  exit (10);
		}
	    }
	  num_pids[curr_chaine] = curr_pid;
	  curr_pid = 0;
	  curr_chaine++;	//chaine suivante
	  port_ok = 0;
	  ip_ok = 0;
	}
      else if (!strcmp (sous_chaine, "name"))
	{
	  //      sous_chaine=strtok(NULL,delimiteurs);//on extrait la sous chaine
	  sous_chaine = strtok (NULL, "=");
	  if (!(strlen (sous_chaine) >= MAX_LEN_NOM - 1))
	    strcpy(noms[curr_chaine],strtok(sous_chaine,"\n"));	//pour enlever le \n a la fin
	  else
	    fprintf (stderr, "Nom de chaine trop long\n");
	}
      else if (!strcmp (sous_chaine, "qam"))
	{
	  //modulation
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  sscanf (sous_chaine, "%s\n", sous_chaine);	//pour enlever le \n a la fin et les espaces
/* 	  QPSK, */
/* 	    QAM_16, */
/* 	    QAM_32, */
/* 	    QAM_64, */
/* 	    QAM_128, */
/* 	    QAM_256, */
/* 	    QAM_AUTO */
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
		syslog (LOG_USER, "Probleme de configuration champ QAM\n");
	      else
		fprintf (stderr, "Probleme au niveau du fichier de configuration champ qam invalide\n");
	      exit (20);
	    }
	}
      else if (!strcmp (sous_chaine, "trans_mode"))
	{
	  //TransmissionMode
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  sscanf (sous_chaine, "%s\n", sous_chaine);	//pour enlever le \n a la fin et les espaces
/* 	  TRANSMISSION_MODE_2K, */
/* 	    TRANSMISSION_MODE_8K, */
/* 	    TRANSMISSION_MODE_AUTO */
	  if (!strcmp (sous_chaine, "2k"))
	    TransmissionMode=TRANSMISSION_MODE_2K;
	  else if (!strcmp (sous_chaine, "8k"))
	    TransmissionMode=TRANSMISSION_MODE_8K;
	  else if (!strcmp (sous_chaine, "auto"))
	    TransmissionMode=TRANSMISSION_MODE_AUTO;
	  else
	    {
	      if (!no_daemon)
		syslog (LOG_USER, "Probleme de configuration champ trans_mode\n");
	      else
		fprintf (stderr, "Probleme au niveau du fichier de configuration champ trans_mode invalide\n");
	      exit (20);
	    }
	}
      else if (!strcmp (sous_chaine, "bandwidth"))
	{
	  //bandWidth
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  sscanf (sous_chaine, "%s\n", sous_chaine);	//pour enlever le \n a la fin et les espaces
/* 	  BANDWIDTH_8_MHZ, */
/* 	    BANDWIDTH_7_MHZ, */
/* 	    BANDWIDTH_6_MHZ, */
/* 	    BANDWIDTH_AUTO */
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
		syslog (LOG_USER, "Probleme de configuration champ bandwidth\n");
	      else
		fprintf (stderr, "Probleme au niveau du fichier de configuration champ bandwidth invalide\n");
	      exit (20);
	    }
	}
      else if (!strcmp (sous_chaine, "guardinterval"))
	{
	  //guardInterval
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  sscanf (sous_chaine, "%s\n", sous_chaine);	//pour enlever le \n a la fin et les espaces
/* 	  GUARD_INTERVAL_1_32, */
/* 	    GUARD_INTERVAL_1_16, */
/* 	    GUARD_INTERVAL_1_8, */
/* 	    GUARD_INTERVAL_1_4, */
/*         GUARD_INTERVAL_AUTO */
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
		syslog (LOG_USER, "Probleme de configuration champ guardinterval\n");
	      else
		fprintf (stderr, "Probleme au niveau du fichier de configuration champ guardinterval invalide\n");
	      exit (20);
	    }
	}
      else if (!strcmp (sous_chaine, "coderate"))
	{
	  //HP_CodeRate ou LP_CodeRate
	  sous_chaine = strtok (NULL, delimiteurs);	//on extrait la sous chaine
	  sscanf (sous_chaine, "%s\n", sous_chaine);	//pour enlever le \n a la fin et les espaces
/* 	  FEC_NONE = 0, */
/* 	    FEC_1_2, */
/* 	    FEC_2_3, */
/* 	    FEC_3_4, */
/* 	    FEC_4_5, */
/* 	    FEC_5_6, */
/* 	    FEC_6_7, */
/* 	    FEC_7_8, */
/* 	    FEC_8_9, */
/*         FEC_AUTO */
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
		syslog (LOG_USER, "Probleme de configuration champ coderate\n");
	      else
		fprintf (stderr, "Probleme au niveau du fichier de configuration champ coderate invalide\n");
	      exit (20);
	    }
	  LP_CodeRate=HP_CodeRate; //je ne sais pas tres bien ce que cela change mais je les met egales
	}
      else
	{
	  /*probleme */
	  continue;
	}
    }
  nb_flux = curr_chaine;
  if (curr_chaine > MAX_CHAINES)
    {
      if (!no_daemon)
	syslog (LOG_USER, "Trop de chaines : %d la limite est : %d\n",
		curr_chaine, MAX_CHAINES);
      else
	fprintf (stderr, "Trop de chaines : %d limite : %d\n",
		 curr_chaine, MAX_CHAINES);
      exit (21);
    }

  /*on le vide */
  sprintf (nom_fich_chaines_diff, "/var/run/tv/chaines_diffusees_carte%d",
	   card);
  chaines_diff = fopen (nom_fich_chaines_diff, "w");
  if (chaines_diff == NULL)
    {
      if (!no_daemon)
	syslog (LOG_USER,
		"N'as pas pu creer le fichier des chaines diffusees %s\n",
		nom_fich_chaines_diff);
      else
	fprintf (stderr,
		 "N'as pas pu creer le fichier %s\n",
		 nom_fich_chaines_diff);
      exit (2);
    }

  fclose (chaines_diff);

  if (!no_daemon)
    syslog (LOG_USER, "Diffusion. Frequence %lu polarisation %c srate %lu\n",
	    freq, pol, srate);

  //alarm for tuning timeout
  if (signal (SIGALRM, SignalHandler) == SIG_IGN)
    signal (SIGALRM, SIG_IGN);
  if (signal (SIGUSR1, SignalHandler) == SIG_IGN)
    signal (SIGUSR1, SIG_IGN);
  alarm (timeout_accord);




  //we tune the card
  if ((freq > 100000000))
    {
      if (open_fe (&fds.fd_frontend, card))
	{
	  i =
	    tune_it (fds.fd_frontend, freq, srate, 0, tone, specInv, diseqc,
		     modulation, HP_CodeRate, TransmissionMode, guardInterval,
		     bandWidth, LP_CodeRate, hier, aff_force_signal);
	}
    }
  else if ((freq != 0) && (pol != 0) && (srate != 0))
    {
      if (open_fe (&fds.fd_frontend, card))
	{
	  fprintf (stderr, "Tuning to %ld Hz\n", freq);
	  i =
	    tune_it (fds.fd_frontend, freq, srate, pol, tone, specInv, diseqc,
		     modulation, HP_CodeRate, TransmissionMode, guardInterval,
		     bandWidth, LP_CodeRate, hier, aff_force_signal);
	}
    }

  if (i < 0)
    {
      if (!no_daemon)
	syslog (LOG_USER, "Probleme pour accorder la carte %d\n", card);
      else
	fprintf (stderr, "Probleme pour accorder la carte %d\n", card);

      exit (111);
    }

  carte_accordee = 1;
  //the card is tuned, so we can catch signals in order to leave cleanly
  if (signal (SIGHUP, SignalHandler) == SIG_IGN)
    signal (SIGHUP, SIG_IGN);
  if (signal (SIGINT, SignalHandler) == SIG_IGN)
    signal (SIGINT, SIG_IGN);
  if (signal (SIGTERM, SignalHandler) == SIG_IGN)
    signal (SIGTERM, SIG_IGN);
  alarm (ALARM_TIME);



  if (!no_daemon)
    {
      sprintf (nom_fich_pid, "/var/run/tv/mumudvb_carte%d.pid", card);
      pidfile = fopen (nom_fich_pid, "w");
      if (pidfile == NULL)
	{
	  syslog (LOG_USER, "N'as pas pu creer le fichier %s\n",
		  nom_fich_pid);
	  exit (2);
	}
      fprintf (pidfile, "%d\n", getpid ());
      fclose (pidfile);
    }

  //we open the file descriptors
  if (create_card_fd (card, nb_flux, num_pids, &fds) < 0)
    return -1;


  //init of the list of diffused streams
  for (i = 0; i < nb_flux; i++)
    {
      chaines_diffuses[i] = 0;
      chaines_diffuses_old[i] = 1;
    }

  //Now we set the filters
  set_ts_filt (fds.fd_zero, 0, DMX_PES_OTHER);	//le pid PAT

  for (i = 0; i < nb_flux; i++)
    {
      for (j = 0; j < num_pids[i]; j++)
	set_ts_filt (fds.fd[i][j], pids[i][j], DMX_PES_OTHER);
    }

  gettimeofday (&tv, (struct timezone *) NULL);
  real_start_time = tv.tv_sec;
  now = 0;

  ttl = 2;
  if (!no_daemon)
    syslog (LOG_USER, "Carte %d accordée\n", card);
  else
    fprintf (stderr, "Carte %d accordée\n", card);

  /* Init udp */
  for (i = 0; i < nb_flux; i++)
    {
      socketOut[i] = makeclientsocket (ipOut[i], portOut[i], ttl, &sOut[i], no_daemon);	//le makeclientsocket est pour joindre automatiquement le flux
    }

  if (!no_daemon)
    syslog (LOG_USER, "Diffusion de %d chaine%s\n", nb_flux,
	    (nb_flux == 1 ? "" : "s"));
  else
    fprintf (stderr, "Diffusion de %d chaine%s\n", nb_flux,
	     (nb_flux == 1 ? "" : "s"));

  for (i = 0; i < nb_flux; i++)
    {
      if (!no_daemon)
	{
	  syslog (LOG_USER, "Chaine %s %d  ip %s, port %d\n",
		  noms[i], i, ipOut[i], portOut[i]);
	}
      else
	{
	  fprintf (stderr,
		   "Chaine %s %d\n        L'ip est %s, le port est %d\n",
		   noms[i], i, ipOut[i], portOut[i]);
	  fprintf (stderr, "        pids : ");
	  for (j = 0; j < num_pids[i]; j++)
	    fprintf (stderr, "%d ", pids[i][j]);
	  fprintf (stderr, "\n");
	}
    }


  /* Read packets */

  pfds[0].fd = fds.fd_dvr;
  pfds[0].events = POLLIN | POLLPRI;
  pfds[1].events = POLLIN | POLLPRI;

  while (!Interrupted)
    {
      /* Poll the open file descriptors */
      poll (pfds, 1, 500);

      {
	/* Attempt to read 188 bytes from /dev/ost/dvr */
	if ((bytes_read = read (fds.fd_dvr, temp_buf, PACKET_SIZE)) > 0)
	  {
	    if (bytes_read != PACKET_SIZE)
	      {
		if (!no_daemon)
		  syslog (LOG_USER, "No bytes left to read - aborting\n");
		else
		  fprintf (stderr, "No bytes left to read - aborting\n");
		break;
	      }

	    pid = ((temp_buf[1] & 0x1f) << 8) | (temp_buf[2]);
	    for (i = 0; i < nb_flux; i++)
	      {
		for (j = 0; j < num_pids[i]; j++)
		  {
		    if ((pids[i][j] == pid) || pid == 0)
		      {
			if (pid != 0)	//si  c pas le pid 0
			  chaines_diffuses[i]++;

			for (j = 0; j < bytes_read; j++)
			  buf[i][nb_bytes[i] + j] = temp_buf[j];
			buf[i][nb_bytes[i] + 1] =
			  (buf[i][nb_bytes[i] + 1] & 0xe0) | hi_mappids[pid];
			buf[i][nb_bytes[i] + 2] = lo_mappids[pid];
			nb_bytes[i] += bytes_read;
			// If there isn't enough room for 1 more packet, then send it.
			if ((nb_bytes[i] + PACKET_SIZE) > MAX_UDP_SIZE)
			  {
			    sendudp (socketOut[i], &sOut[i], buf[i],
				     nb_bytes[i]);
			    nb_bytes[i] = 0;
			  }
		      }
		  }
		count_non_transmis = 0;
		if (alarm_count == 1)
		  {
		    alarm_count = 0;
		    fprintf (stderr,
			     "Retour a la normale, on retransmet des paquets\n");
		  }
	      }
	    count_non_transmis++;
	    if (count_non_transmis > ALARM_COUNT_LIMIT)
	      {
		if (!no_daemon)
		  syslog (LOG_USER,
			  "Probleme : Moins d'un paquet sur %d a été transmis\n",
			  ALARM_COUNT_LIMIT);
		else
		  fprintf (stderr,
			   "Probleme : Moins d'un paquet sur %d a été transmis\n",
			   ALARM_COUNT_LIMIT);
		alarm_count = 1;
	      }
	  }

      }
    }

  if (Interrupted)
    {
      if (!no_daemon)
	syslog (LOG_USER, "\nCaught signal %d - closing cleanly.\n",
		Interrupted);
      else
	fprintf (stderr, "\nCaught signal %d - closing cleanly.\n",
		 Interrupted);
    }

  for (i = 0; i < nb_flux; i++)
    close (socketOut[i]);

  //we close the file descriptors
  close_card_fd (card, nb_flux, num_pids, fds);
  close (fds.fd_frontend);

  if (remove (nom_fich_chaines_diff))
    {
      if (!no_daemon)
	syslog (LOG_USER,
		"N'as pas pu supprimer le fichier %s\n",
		nom_fich_chaines_diff);
      else
	fprintf (stderr,
		 "N'as pas pu supprimer le fichier des chaines diffusees %s\n",
		 nom_fich_chaines_diff);
      exit (2);
    }


  if (!no_daemon)
    {
      if (remove (nom_fich_pid))
	syslog (LOG_USER, "N'as pas pu supprimer le fichier %s\n",
		nom_fich_pid);
    }

  return (0);
}


static void
SignalHandler (int signum)
{
  struct timeval tv;
  int i = 0;
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
		    "Carte non accordée apres %d s, on stoppe\n",
		    timeout_accord);
	  else
	    fprintf (stderr,
		     "La carte n'a pas été accordée au bout de %d secondes, on arrête de s'obstiner\n",
		     timeout_accord);
	  exit (100);
	}	
      for (i = 0; i < nb_flux; i++)
	if ((chaines_diffuses[i] >= 100) && (!chaines_diffuses_old[i]))
	  {
	    if (!no_daemon)
	      syslog (LOG_USER,
		      "Reprise de la diffusion de la chaine %s %d.\n",
		      noms[i], i);
	    else
	      fprintf (stderr,
		       "Reprise de la diffusion de la chaine %s %d. Il est (en secondes depuis le jour 0) %ld\n\tOn diffuse Depuis %ld secondes\n",
		       noms[i], i, tv.tv_sec, now);
	    chaines_diffuses_old[i] = 1;	//mise à jour
	  }
	else if ((chaines_diffuses_old[i]) && (chaines_diffuses[i] < 100))
	  {
	    if (!no_daemon)
	      syslog (LOG_USER,
		      "Arrêt de la diffusion de la chaine %s %d.On diffuse depuis %ld secondes\n",
		      noms[i], i, now);
	    else
	      fprintf (stderr,
		       "Arrêt de la diffusion de la chaine %s %d. Il est (en secondes depuis le jour 0) %ld\n\tOn diffuse Depuis %ld secondes\n",
		       noms[i], i, tv.tv_sec, now);
	    chaines_diffuses_old[i] = 0;	//mise à jour
	  }

      for (i = 0; i < nb_flux; i++)// on compte les chaines diffuses
	if (chaines_diffuses_old[i])
	  compteur_chaines_diff++;

      if(compteur_chaines_diff)//reinit si on diffuse
	time_no_diff=0;
      else if(!time_no_diff)//sinon si c le moment ou on arrete on stoque l'heure
	time_no_diff=now;

      if(time_no_diff&&((now-time_no_diff)>timeout_no_diff))//on ne diffuse plus depuis trop longtemps
	{
	  if (!no_daemon)
	    syslog (LOG_USER,
		    "Carte %d ne diffuse rien depuis %d s, on quitte.On a commencé a diffuser il y a %ld secondes\n",
		      card, timeout_no_diff, now);
	    else
	      fprintf (stderr,
		    "Carte %d On ne diffuse rien depuis %d secondes, on quitte. Il est (en secondes depuis le jour 0) %ld\n\tOna commencer a diffuser il y a %ld secondes\n",
		      card, timeout_no_diff, tv.tv_sec, now);
	  Interrupted=100;
	}
			

      gen_chaines_diff (no_daemon, chaines_diffuses_old);//on envoie le old pour pas annoncer une chaine qui diffuse troi paquets

      //reinit
      for (i = 0; i < nb_flux; i++)
	chaines_diffuses[i] = 0;
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
  int i;

  chaines_diff = fopen (nom_fich_chaines_diff, "w");
  if (chaines_diff == NULL)
    {
      if (!no_daemon)
	syslog (LOG_USER,
		"N'as pas pu creer le fichier %s\n",
		nom_fich_chaines_diff);
      else
	fprintf (stderr,
		 "N'as pas pu creer le fichier des chaines diffusees %s\n",
		 nom_fich_chaines_diff);
      exit (2);
    }

  for (i = 0; i < nb_flux; i++)
    if (chaines_diffuses[i])
      fprintf (chaines_diff, "%s:%d:%s\n", ipOut[i], portOut[i], noms[i]);
  fclose (chaines_diff);

}
