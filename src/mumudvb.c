/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2013 Brice DUBOST
 * 
 * Code for dealing with libdvben50221 inspired from zap_ca
 * Copyright (C) 2004, 2005 Manu Abraham <abraham.manu@gmail.com>
 * Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)
 * 
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

#define _GNU_SOURCE		//in order to use program_invocation_short_name (GNU extension)


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
#include <sys/epoll.h>
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

int  write_streamed_channels=1;
pthread_t signalpowerthread;
pthread_t cardthread;
pthread_t monitorthread;
card_thread_parameters_t cardthreadparams;

/** Do we send scrambled packets ? */
int dont_send_scrambled=0;

//multicast parameters
multi_p_t multi_p={
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



//logging
extern log_params_t log_params;

// prototypes
static void SignalHandler (int signum);//below
int read_multicast_configuration(multi_p_t *, mumudvb_channel_t *, char *); //in multicast.c
void *monitor_func(void* arg);
int mumudvb_close(int no_daemon,
		monitor_parameters_t *monitor_thread_params,
		rewrite_parameters_t *rewrite_vars,
		auto_p_t *auto_p,
		unicast_parameters_t *unicast_vars,
		volatile int *strengththreadshutdown,
		void *cam_p_v,
		void *scam_vars_v,
		char *filename_channels_not_streamed,
		char *filename_channels_streamed,
		char *filename_pid,
		int Interrupted,
		mumu_chan_p_t *chan_p);

void chan_new_pmt(unsigned char *ts_packet, mumu_chan_p_t *chan_p, int pid);

int
main (int argc, char **argv)
{

	//Channel information
	mumu_chan_p_t chan_p;
	memset(&chan_p,0,sizeof(chan_p));

	pthread_mutex_init(&chan_p.lock,NULL);
	chan_p.psi_tables_filtering=PSI_TABLES_FILTERING_NONE;

	//sap announces variables
	sap_p_t sap_p;
	init_sap_v(&sap_p);

	//Statistics
	stats_infos_t stats_infos;
	init_stats_v(&stats_infos);

	//tuning parameters
	tune_p_t tune_p;
	init_tune_v(&tune_p);
	card_tuned=&tune_p.card_tuned;

#ifdef ENABLE_CAM_SUPPORT
	//CAM (Conditionnal Access Modules : for scrambled channels)
	cam_p_t cam_p;
	init_cam_v(&cam_p);
	cam_p_t *cam_p_ptr=&cam_p;
#else
	void *cam_p_ptr=NULL;
#endif

#ifdef ENABLE_SCAM_SUPPORT
	//SCAM (software conditionnal Access Modules : for scrambled channels)
	scam_parameters_t scam_vars={
			.scam_support = 0,
			.getcwthread_shutdown=0,
	};
	scam_vars.epfd = epoll_create(MAX_CHANNELS);
	scam_parameters_t *scam_vars_ptr=&scam_vars;
	int scam_threads_started=0;
#else
	void *scam_vars_ptr=NULL;
#endif

	//autoconfiguration
	auto_p_t auto_p;
	init_aconf_v(&auto_p);

	//Parameters for rewriting
	rewrite_parameters_t rewrite_vars;
	init_rewr_v(&rewrite_vars);

	int no_daemon = 0;

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
	int ichan = 0;
	int ipid = 0;
	int send_packet=0;
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
			tune_p.display_strenght = 1;
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
	memset (&chan_p.channels, 0, sizeof (mumudvb_channel_t)*MAX_CHANNELS);
#ifdef ENABLE_SCAM_SUPPORT
	for (int i = 0; i < MAX_CHANNELS; ++i) {
          pthread_mutex_init(&chan_p.channels[i].stats_lock, NULL);
          pthread_mutex_init(&chan_p.channels[i].cw_lock, NULL);
          chan_p.channels[i].camd_socket = -1;
	}
#endif


	/******************************************************/
	// config file displaying
	/******************************************************/
	if (conf_filename == NULL)
	{
		log_message( log_module,  MSG_ERROR, "No configuration file specified");
		exit(ERROR_CONF_FILE);
	}
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

	ichan=-1;
	mumudvb_channel_t *c_chan;
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
			//We check if it's not a new_channel line
			substring = strtok (current_line, delimiteurs);
			//If nothing in the substring we avoid the segfault in the next line
			if(substring == NULL)
				continue;
			if(strcmp (substring, "new_channel") )
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

		if(ichan<0)
			c_chan=NULL;
		else
			c_chan=&chan_p.channels[ichan];

		if((iRet=read_tuning_configuration(&tune_p, substring))) //Read the line concerning the tuning parameters
		{
			if(iRet==-1)
				exit(ERROR_CONF);
		}
		else if((iRet=read_autoconfiguration_configuration(&auto_p, substring))) //Read the line concerning the autoconfiguration parameters
		{
			if(iRet==-1)
				exit(ERROR_CONF);
		}
		else if((iRet=read_sap_configuration(&sap_p, c_chan, substring))) //Read the line concerning the sap parameters
		{
			if(iRet==-1)
				exit(ERROR_CONF);
		}
#ifdef ENABLE_CAM_SUPPORT
		else if((iRet=read_cam_configuration(&cam_p, c_chan, substring))) //Read the line concerning the cam parameters
		{
			if(iRet==-1)
				exit(ERROR_CONF);
		}
#endif
#ifdef ENABLE_SCAM_SUPPORT
		else if((iRet=read_scam_configuration(scam_vars_ptr, c_chan, substring))) //Read the line concerning the cam parameters
		{
			if(iRet==-1)
				exit(ERROR_CONF);
		}
#endif
		else if((iRet=read_unicast_configuration(&unicast_vars, c_chan, substring))) //Read the line concerning the unicast parameters
		{
			if(iRet==-1)
				exit(ERROR_CONF);
		}
		else if((iRet=read_multicast_configuration(&multi_p, c_chan, substring))) //Read the line concerning the multicast parameters
		{
			if(iRet==-1)
				exit(ERROR_CONF);
		}
		else if((iRet=read_rewrite_configuration(&rewrite_vars, substring))) //Read the line concerning the rewrite parameters
		{
			if(iRet==-1)
				exit(ERROR_CONF);
		}
		else if((iRet=read_logging_configuration(&stats_infos, substring))) //Read the line concerning the logging parameters
		{
			if(iRet==-1)
				exit(ERROR_CONF);
		}
		else if (!strcmp (substring, "new_channel"))
		{
			ichan++;
			chan_p.channels[ichan].channel_ready=ALMOST_READY;
			log_message( log_module, MSG_INFO,"New channel, current number %d", ichan);
		}
		else if (!strcmp (substring, "timeout_no_diff"))
		{
			substring = strtok (NULL, delimiteurs);
			timeout_no_diff= atoi (substring);
		}
		else if (!strcmp (substring, "dont_send_scrambled"))
		{
			substring = strtok (NULL, delimiteurs);
			dont_send_scrambled = atoi (substring);
		}
		else if (!strcmp (substring, "filter_transport_error"))
		{
			substring = strtok (NULL, delimiteurs);
			chan_p.filter_transport_error = atoi (substring);
		}
		else if (!strcmp (substring, "psi_tables_filtering"))
		{
			substring = strtok (NULL, delimiteurs);
			if (!strcmp (substring, "pat"))
				chan_p.psi_tables_filtering = PSI_TABLES_FILTERING_PAT_ONLY;
			else if (!strcmp (substring, "pat_cat"))
				chan_p.psi_tables_filtering = PSI_TABLES_FILTERING_PAT_CAT_ONLY;
			else if (!strcmp (substring, "none"))
				chan_p.psi_tables_filtering = PSI_TABLES_FILTERING_NONE;
			if (chan_p.psi_tables_filtering == PSI_TABLES_FILTERING_PAT_ONLY)
				log_message( log_module,  MSG_INFO, "You have enabled PSI tables filtering, only PAT will be send\n");
			if (chan_p.psi_tables_filtering == PSI_TABLES_FILTERING_PAT_CAT_ONLY)
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
			if ( c_chan == NULL)
			{
				log_message( log_module,  MSG_ERROR,
						"service_id : You have to start a channel first (using new_channel)\n");
				exit(ERROR_CONF);
			}
			substring = strtok (NULL, delimiteurs);
			c_chan->service_id = atoi (substring);
		}
		else if (!strcmp (substring, "pids"))
		{
			ipid = 0;
			if ( c_chan == NULL)
			{
				log_message( log_module,  MSG_ERROR,
						"pids : You have to start a channel first (using new_channel)\n");
				exit(ERROR_CONF);
			}
			//Pids are now user set, they won't be overwritten by autoconfiguration
			c_chan->pid_i.pid_f=F_USER;
			while ((substring = strtok (NULL, delimiteurs)) != NULL)
			{
				c_chan->pid_i.pids[ipid] = atoi (substring);
				// we see if the given pid is good
				if (c_chan->pid_i.pids[ipid] < 10 || c_chan->pid_i.pids[ipid] >= 8193)
				{
					log_message( log_module,  MSG_ERROR,
							"Config issue : %s in pids, given pid : %d\n",
							conf_filename, c_chan->pid_i.pids[ipid]);
					exit(ERROR_CONF);
				}
				ipid++;
				if (ipid >= MAX_PIDS)
				{
					log_message( log_module,  MSG_ERROR,
							"Too many pids : %d channel : %d\n",
							ipid, ichan);
					exit(ERROR_CONF);
				}
			}
			c_chan->pid_i.num_pids = ipid;
		}
		else if (!strcmp (substring, "pmt_pid"))
		{
			if ( c_chan == NULL)
			{
				log_message( log_module,  MSG_ERROR,
						"pmt_pid : You have to start a channel first (using new_channel)\n");
				return -1;
			}
			substring = strtok (NULL, delimiteurs);
			c_chan->pid_i.pmt_pid = atoi (substring);
			if (c_chan->pid_i.pmt_pid < 10 || c_chan->pid_i.pmt_pid > 8191){
				log_message( log_module,  MSG_ERROR,
						"Configuration issue in pmt_pid, given PID : %d\n",
						c_chan->pid_i.pmt_pid);
				return -1;
			}
			MU_F(c_chan->pid_i.pmt_pid)=F_USER;
		}
		else if (!strcmp (substring, "name"))
		{
			if ( c_chan == NULL)
			{
				log_message( log_module,  MSG_ERROR,
						"name : You have to start a channel first (using new_channel)\n");
				exit(ERROR_CONF);
			}
			//name is now user set
			MU_F(c_chan->name)=F_USER;
			// other substring extraction method in order to keep spaces
			substring = strtok (NULL, "=");
			strncpy(c_chan->name,strtok(substring,"\n"),MAX_NAME_LEN-1);
			c_chan->name[MAX_NAME_LEN-1]='\0';
			//We store the user name for being able to use templates
			strncpy(c_chan->user_name,strtok(substring,"\n"),MAX_NAME_LEN-1);
			c_chan->user_name[MAX_NAME_LEN-1]='\0';
			if (strlen (substring) >= MAX_NAME_LEN - 1)
				log_message( log_module,  MSG_WARN,"Channel name too long\n");
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
			chan_p.check_cc = atoi (substring);
		}
		else
		{
			if(strlen (current_line) > 1)
				log_message( log_module,  MSG_WARN,
						"Config issue : unknow symbol : %s\n\n", substring);
			continue;
		}

		if (ichan > MAX_CHANNELS)
		{
			log_message( log_module,  MSG_ERROR, "Too many channels : %d limit : %d\n",
					ichan, MAX_CHANNELS);
			exit(ERROR_TOO_CHANNELS);
		}

		//A new channel have been defined
		if(curr_channel_old != ichan)
		{
			curr_channel_old = ichan;
		}
	}
	fclose (conf_file);


	//if Autoconfiguration, we set other option default
	if(auto_p.autoconfiguration!=AUTOCONF_MODE_NONE)
	{
		if((sap_p.sap == OPTION_UNDEFINED) && (multi_p.multicast))
		{
			log_message( log_module,  MSG_INFO,
					"Autoconfiguration, we activate SAP announces. if you want to disable them see the README.\n");
			sap_p.sap=OPTION_ON;
		}
		if(rewrite_vars.rewrite_pat == OPTION_UNDEFINED)
		{
			rewrite_vars.rewrite_pat=OPTION_ON;
			log_message( log_module,  MSG_INFO,
					"Autoconfiguration, we activate PAT rewriting. if you want to disable it see the README.\n");
		}
		if(rewrite_vars.rewrite_sdt == OPTION_UNDEFINED)
		{
			rewrite_vars.rewrite_sdt=OPTION_ON;
			log_message( log_module,  MSG_INFO,
					"Autoconfiguration, we activate SDT rewriting. if you want to disable it see the README.\n");
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
		tune_p.card=cmdlinecard;

	//Template for the card dev path
	char number[10];
	sprintf(number,"%d",tune_p.card);
	int l=sizeof(tune_p.card_dev_path);
	mumu_string_replace(tune_p.card_dev_path,&l,0,"%card",number);

	//If we specified a string for the unicast port out, we parse it
	if(unicast_vars.portOut_str!=NULL)
	{
		int len;
		len=strlen(unicast_vars.portOut_str)+1;
		sprintf(number,"%d",tune_p.card);
		unicast_vars.portOut_str=mumu_string_replace(unicast_vars.portOut_str,&len,1,"%card",number);
		sprintf(number,"%d",tune_p.tuner);
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
		sprintf(number,"%d",tune_p.card);
		log_params.log_file_path=mumu_string_replace(log_params.log_file_path,&len,1,"%card",number);
		sprintf(number,"%d",tune_p.tuner);
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
	pthread_mutex_lock(&chan_p.lock);
	chan_p.number_of_channels = ichan+1;
	pthread_mutex_unlock(&chan_p.lock);
	/*****************************************************/
	//Autoconfiguration init
	/*****************************************************/

	if(auto_p.autoconfiguration)
	{
		if(!auto_p.autoconf_pid_update)
		{
			log_message( "Autoconf: ", MSG_INFO,
					"The autoconfiguration auto update is NOT enabled. Please report why you need it");
		}
	}
	else
		auto_p.autoconf_pid_update=0;
	/*****************************************************/
	//End of Autoconfiguration init
	/*****************************************************/

	//We disable things depending on multicast if multicast is suppressed
	if(!multi_p.ttl)
	{
		log_message( log_module,  MSG_INFO, "The multicast TTL is set to 0, multicast will be disabled.\n");
		multi_p.multicast=0;
	}
	if(!multi_p.multicast)
	{
		if(multi_p.rtp_header)
		{
			multi_p.rtp_header=0;
			log_message( log_module,  MSG_INFO, "NO Multicast, RTP Header is disabled.\n");
		}
		if(sap_p.sap==OPTION_ON)
		{
			log_message( log_module,  MSG_INFO, "NO Multicast, SAP announces are disabled.\n");
			sap_p.sap=OPTION_OFF;
		}
	}
	free(conf_filename);
	if(!multi_p.multicast && !unicast_vars.unicast)
	{
		log_message( log_module,  MSG_ERROR, "NO Multicast AND NO unicast. No data can be send :(, Exciting ....\n");
		set_interrupted(ERROR_CONF<<8);
		goto mumudvb_close_goto;
	}



	// we clear them by paranoia
	sprintf (filename_channels_streamed, STREAMED_LIST_PATH,
			tune_p.card, tune_p.tuner);
	sprintf (filename_channels_not_streamed, NOT_STREAMED_LIST_PATH,
			tune_p.card, tune_p.tuner);
#ifdef ENABLE_CAM_SUPPORT
	sprintf (cam_p.filename_cam_info, CAM_INFO_LIST_PATH,
			tune_p.card, tune_p.tuner);
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
	if (channels_not_streamed == NULL)
	{
		write_streamed_channels=0;
		log_message( log_module,  MSG_WARN,
				"Can't create %s: %s\n",
				filename_channels_not_streamed, strerror (errno));
	}
	else
		fclose (channels_not_streamed);


#ifdef ENABLE_CAM_SUPPORT
	if(cam_p.cam_support)
	{
		cam_info = fopen (cam_p.filename_cam_info, "w");
		if (cam_info == NULL)
		{
			log_message( log_module,  MSG_WARN,
					"Can't create %s: %s\n",
					cam_p.filename_cam_info, strerror (errno));
		}
		else
			fclose (cam_info);
	}
#endif


	log_message( log_module,  MSG_INFO, "Streaming. Freq %d\n",
			tune_p.freq);


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
	if(tune_p.tuning_timeout)
	{
		alarm (tune_p.tuning_timeout);
	}


	// We tune the card
	iRet =-1;

	if (open_fe (&fds.fd_frontend, tune_p.card_dev_path, tune_p.tuner,1))
	{

		/*****************************************************/
		//daemon part two, we write our PID as we are tuned
		/*****************************************************/

		// We write our pid in a file if we deamonize
		if (!no_daemon)
		{
			int len;
			len=DEFAULT_PATH_LEN;
			sprintf(number,"%d",tune_p.card);
			mumu_string_replace(filename_pid,&len,0,"%card",number);
			sprintf(number,"%d",tune_p.tuner);
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
				tune_it (fds.fd_frontend, &tune_p);
	}

	if (iRet < 0)
	{
		log_message( log_module,  MSG_INFO, "Tuning issue, card %d\n", tune_p.card);
		// we close the file descriptors
		close_card_fd(&fds);
		set_interrupted(ERROR_TUNE<<8);
		goto mumudvb_close_goto;
	}
	log_message( log_module,  MSG_INFO, "Card %d, tuner %d tuned\n", tune_p.card, tune_p.tuner);
	tune_p.card_tuned = 1;

	//Thread for showing the strength
	strength_parameters_t strengthparams;
	strengthparams.fds = &fds;
	strengthparams.tune_p = &tune_p;
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
			.auto_p=&auto_p,
			.sap_p=&sap_p,
			.chan_p=&chan_p,
			.multi_p=&multi_p,
			.unicast_vars=&unicast_vars,
			.tune_p=&tune_p,
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
		if(scam_getcw_start(scam_vars_ptr,&chan_p))
		{
			log_message("SCAM_GETCW: ", MSG_ERROR,"Cannot initalise scam\n");
			scam_vars.scam_support=0;
		}
		else
		{
			//If the scam is properly initialized, we autoconfigure scrambled channels
			auto_p.autoconf_scrambled=1;
		}
	}
#endif

	/*****************************************************/
	//cam_support
	/*****************************************************/

#ifdef ENABLE_CAM_SUPPORT
	if(cam_p.cam_support){
		//We initialize the cam. If fail, we remove cam support
		if(cam_start(&cam_p,tune_p.card,&chan_p))
		{
			log_message("CAM: ", MSG_ERROR,"Cannot initalise cam\n");
			cam_p.cam_support=0;
		}
		else
		{
			//If the cam is properly initialized, we autoconfigure scrambled channels
			auto_p.autoconf_scrambled=1;
		}
	}
#endif

	/*****************************************************/
	//autoconfiguration
	//memory allocation for MPEG2-TS
	//packet structures
	/*****************************************************/
	iRet=autoconf_init(&auto_p);
	if(iRet)
	{
		set_interrupted(ERROR_GENERIC<<8);
		goto mumudvb_close_goto;
	}

#ifdef ENABLE_SCAM_SUPPORT
	/*****************************************************/
	//scam
	/*****************************************************/
	if (auto_p.autoconfiguration==AUTOCONF_MODE_NONE)
	{
		iRet=scam_init_no_autoconf(scam_vars_ptr, chan_p.channels,chan_p.number_of_channels);
		if(iRet)
		{
			set_interrupted(ERROR_GENERIC<<8);
			goto mumudvb_close_goto;
		}
	}

#endif
	/*****************************************************/
	//Rewriting initialization and allocation
	/*****************************************************/
	if(rewrite_init(&rewrite_vars))
		goto mumudvb_close_goto;
	/*****************************************************/
	//Some initializations
	/*****************************************************/
	if(multi_p.rtp_header)
		multi_p.num_pack=(MAX_UDP_SIZE-TS_PACKET_SIZE)/TS_PACKET_SIZE;
	else
		multi_p.num_pack=(MAX_UDP_SIZE)/TS_PACKET_SIZE;


	// initialization of active channels list
	pthread_mutex_lock(&chan_p.lock);
	for (ichan = 0; ichan < chan_p.number_of_channels; ichan++)
	{
		if(mumu_init_chan(&chan_p.channels[ichan])<0)
			goto mumudvb_close_goto;
	}
	pthread_mutex_unlock(&chan_p.lock);
	//We initialize asked PID table
	memset (chan_p.asked_pid, 0, sizeof( uint8_t)*8193);//we clear it

	// We initialize the table for checking the TS discontinuities
	for (ipid = 0; ipid < 8193; ipid++)
		chan_p.continuity_counter_pid[ipid]=-1;

	//We initialize mandatory PID table
	memset (mandatory_pid, 0, sizeof( uint8_t)*MAX_MANDATORY_PID_NUMBER);//we clear it

	//mandatory PIDs (always sent with all channels)
	//PAT : Program Association Table
	mandatory_pid[0]=1;
	//CAT : Conditional Access Table
	mandatory_pid[1]=1;
	//NIT : Network Information Table
	//It is intended to provide information about the physical network.
	mandatory_pid[16]=1;
	//SDT : Service Description Table
	//the SDT contains data describing the services in the system e.g. names of services, the service provider, etc.
	mandatory_pid[17]=1;
	//EIT : Event Information Table
	//the EIT contains data concerning events or programs such as event name, start time, duration, etc.
	mandatory_pid[18]=1;
	//TDT : Time and Date Table
	//the TDT gives information relating to the present time and date.
	//This information is given in a separate table due to the frequent updating of this information.
	mandatory_pid[20]=1;
	for (ipid = 0; ipid < 21; ipid++)
		if(mandatory_pid[ipid])
			chan_p.asked_pid[ipid]=PID_ASKED;

	//PSIP : Program and System Information Protocol
	//Specific to ATSC, this is more or less the equivalent of sdt plus other stuff
	if(tune_p.fe_type==FE_ATSC)
		chan_p.asked_pid[PSIP_PID]=PID_ASKED;

	/*****************************************************/
	//Set the filters
	/*****************************************************/
	update_chan_filters(&chan_p, tune_p.card_dev_path, tune_p.tuner, &fds);

	//We take care of the poll descriptors
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
	//We open the socket for the http unicast if needed and we update the poll structure
	if(unicast_vars.unicast)
	{
		log_message("Unicast: ", MSG_INFO,"We open the Master http socket for address %s:%d\n",unicast_vars.ipOut, unicast_vars.portOut);
		unicast_create_listening_socket(UNICAST_MASTER, -1, unicast_vars.ipOut, unicast_vars.portOut, &unicast_vars.sIn, &unicast_vars.socketIn, &fds, &unicast_vars);
	}
	chan_update_net(&chan_p, &auto_p, &multi_p, &unicast_vars, server_id, tune_p.card, tune_p.tuner,&fds);





	/*****************************************************/
	// init sap
	/*****************************************************/

	iRet=init_sap(&sap_p, multi_p);
	if(iRet)
	{
		set_interrupted(ERROR_GENERIC<<8);
		goto mumudvb_close_goto;
	}

	/*****************************************************/
	// Information about streamed channels
	/*****************************************************/

	if(auto_p.autoconfiguration==AUTOCONF_MODE_NONE)
		log_streamed_channels(log_module,
				chan_p.number_of_channels,
				chan_p.channels,
				multi_p.multicast_ipv4,
				multi_p.multicast_ipv6,
				unicast_vars.unicast,
				unicast_vars.portOut,
				unicast_vars.ipOut);

	if(auto_p.autoconfiguration)
		log_message("Autoconf: ",MSG_INFO,"Autoconfiguration is now ready to work for you !");


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
				iRet=unicast_handle_fd_event(&unicast_vars, &fds, chan_p.channels, chan_p.number_of_channels, &strengthparams, &auto_p, cam_p_ptr, scam_vars_ptr);
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
				iRet=unicast_handle_fd_event(&unicast_vars, &fds, chan_p.channels, chan_p.number_of_channels, &strengthparams, &auto_p, cam_p_ptr, scam_vars_ptr);
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

			//If the user asked to dump the streams it's here that it should be done
			if(dump_file)
				if(fwrite(actual_ts_packet,sizeof(unsigned char),TS_PACKET_SIZE,dump_file)<TS_PACKET_SIZE)
					log_message( log_module,MSG_WARN,"Error while writing the dump : %s", strerror(errno));

			// Test if the error bit is set in the TS packet received
			if ((actual_ts_packet[1] & 0x80) == 0x80)
			{
				log_message( log_module, MSG_FLOOD,"Error bit set in TS packet!\n");
				// Test if we discard the packets with error bit set
				if (chan_p.filter_transport_error>0) continue;
			}

			// Get the PID of the received TS packet
			pid = ((actual_ts_packet[1] & 0x1f) << 8) | (actual_ts_packet[2]);

			// Check the continuity
			if(chan_p.check_cc)
			{
				continuity_counter=actual_ts_packet[3] & 0x0f;
				if (chan_p.continuity_counter_pid[pid]!=-1 && chan_p.continuity_counter_pid[pid]!=continuity_counter && ((chan_p.continuity_counter_pid[pid]+1) & 0x0f)!=continuity_counter)
					strengthparams.ts_discontinuities++;
				chan_p.continuity_counter_pid[pid]=continuity_counter;
			}

			//Software filtering in case the card doesn't have hardware filtering
			if(chan_p.asked_pid[8192]==PID_NOT_ASKED && chan_p.asked_pid[pid]==PID_NOT_ASKED)
				continue;

			ScramblingControl = (actual_ts_packet[3] & 0xc0) >> 6;
			/* 0 = Not scrambled
         1 = Reserved for future use
         2 = Scrambled with even key
         3 = Scrambled with odd key*/
			/******************************************************/
			//   PMT UPDATE PART: Autoconf, CAM, SCAM
			/******************************************************/
			chan_new_pmt(actual_ts_packet, &chan_p, pid);
			/******************************************************/
			//   PMT UPDATE PART FINISHED
			/******************************************************/

			/******************************************************/
			//   AUTOCONFIGURATION PART
			/******************************************************/
			if(!ScramblingControl &&  auto_p.autoconfiguration)
			{
				iRet = autoconf_new_packet(pid, actual_ts_packet, &auto_p,  &fds, &chan_p, &tune_p, &multi_p, &unicast_vars, server_id, scam_vars_ptr);
				if(iRet)
					set_interrupted(iRet);
			}

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
					for (ichan = 0; ichan < chan_p.number_of_channels; ichan++)
						chan_p.channels[ichan].sdt_rewrite_skip=0; //no lock needed, accessed only by main thread
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
			pthread_mutex_lock(&chan_p.lock);
			for (ichan = 0; ichan < chan_p.number_of_channels; ichan++)
			{
				//We test if the channel is ready (manually configured or autoconf)
				if(!(chan_p.channels[ichan].channel_ready>0))
					continue;

				//we'll see if we must send this PID for this channel
				send_packet=0;

				//If it's a mandatory PID we send it
				if((pid<MAX_MANDATORY_PID_NUMBER) && (mandatory_pid[pid]==1))
					send_packet=1;
				if ((pid == PSIP_PID) && (tune_p.fe_type==FE_ATSC))
					send_packet=1;


				//if it isn't mandatory wee see if it is in the channel list
				if(!send_packet)
					for (ipid = 0; (ipid < chan_p.channels[ichan].pid_i.num_pids)&& !send_packet; ipid++)
						if ((chan_p.channels[ichan].pid_i.pids[ipid] == pid) || (chan_p.channels[ichan].pid_i.pids[ipid] == 8192)) //We can stream whole transponder using 8192
						{
							send_packet=1;

						}

				/******************************************************/
				//cam support
				// If we send the packet, we look if it's a cam pmt pid
				/******************************************************/
#ifdef ENABLE_CAM_SUPPORT
				if((cam_p.cam_support && send_packet==1) &&  //no need to check packets we don't send
						cam_p.ca_resource_connected &&
						((now-cam_p.cam_pmt_send_time)>=cam_p.cam_interval_pmt_send ))
				{
					if(cam_new_packet(pid, ichan, &cam_p, &chan_p.channels[ichan]))
						cam_p.cam_pmt_send_time=now; //A packet was sent to the CAM
				}
#endif

				/******************************************************/
				//Rewrite PAT
				/******************************************************/
				if((send_packet==1) && //no need to check packets we don't send
						(pid == 0) && //This is a PAT PID
						rewrite_vars.rewrite_pat == OPTION_ON )  //AND we asked for rewrite
					send_packet=pat_rewrite_new_channel_packet(actual_ts_packet, &rewrite_vars, &chan_p.channels[ichan], ichan);

				/******************************************************/
				//Rewrite SDT
				/******************************************************/
				if((send_packet==1) && //no need to check packets we don't send
						(pid == 17) && //This is a SDT PID
						rewrite_vars.rewrite_sdt == OPTION_ON &&  //AND we asked for rewrite
						!chan_p.channels[ichan].sdt_rewrite_skip ) //AND the generation was successful
					send_packet=sdt_rewrite_new_channel_packet(actual_ts_packet, &rewrite_vars, &chan_p.channels[ichan], ichan);

				/******************************************************/
				//Rewrite EIT
				/******************************************************/
				if((send_packet==1) &&//no need to check packets we don't send
						(pid == 18) && //This is a EIT PID
						(chan_p.channels[ichan].service_id) && //we have the service_id
						rewrite_vars.rewrite_eit == OPTION_ON) //AND we asked for EIT sorting
				{
					eit_rewrite_new_channel_packet(actual_ts_packet, &rewrite_vars, &chan_p.channels[ichan],
							&multi_p, &unicast_vars, scam_vars_ptr, &fds);
					send_packet=0; //for EIT it is sent by the rewrite function itself
				}

				/******************************************************/
				// Test if PSI tables filtering is activated
				/******************************************************/
				if (send_packet==1 && chan_p.psi_tables_filtering>0 && pid<32)
				{
					// Keep only PAT and CAT
					if (chan_p.psi_tables_filtering==1 && pid>1) send_packet=0;
					// Keep only PAT
					if (chan_p.psi_tables_filtering==2 && pid>0) send_packet=0;
				}
				mumudvb_channel_t *channel = &chan_p.channels[ichan];
				/******************************************************/
				//Ok we must send this packet,
				// we add it to the channel buffer
				/******************************************************/
				if(send_packet==1)
				{
					buffer_func(channel, actual_ts_packet, &unicast_vars, &multi_p, scam_vars_ptr, &fds);
				}

			}
			pthread_mutex_unlock(&chan_p.lock);
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
	if(card_buffer.overflow_number)
		log_message( log_module,  MSG_INFO,
				"We have got %d overflow errors\n",card_buffer.overflow_number );
	mumudvb_close_goto:
	//If the thread is not started, we don't send the nonexistent address of monitor_thread_params
	return mumudvb_close(no_daemon,
			monitorthread == 0 ? NULL:&monitor_thread_params,
					&rewrite_vars,
					&auto_p,
					&unicast_vars,
					&tune_p.strengththreadshutdown,
					cam_p_ptr,
					scam_vars_ptr,
					filename_channels_not_streamed,
					filename_channels_streamed,
					filename_pid,
					get_interrupted(),
					&chan_p);

}

/** @brief Clean closing and freeing
 *
 *
 */
int mumudvb_close(int no_daemon,
		monitor_parameters_t *monitor_thread_params,
		rewrite_parameters_t *rewrite_vars,
		auto_p_t *auto_p,
		unicast_parameters_t *unicast_vars,
		volatile int *strengththreadshutdown,
		void *cam_p_v,
		void *scam_vars_v,
		char *filename_channels_not_streamed,
		char *filename_channels_streamed,
		char *filename_pid,
		int Interrupted,
		mumu_chan_p_t *chan_p)
{

	int curr_channel;
	int iRet;

#ifndef ENABLE_CAM_SUPPORT
	(void) cam_p_v; //to make compiler happy
#else
	cam_p_t *cam_p=(cam_p_t *)cam_p_v;
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

	for (curr_channel = 0; curr_channel < chan_p->number_of_channels; curr_channel++)
	{
		if(chan_p->channels[curr_channel].socketOut4>0)
			close (chan_p->channels[curr_channel].socketOut4);
		if(chan_p->channels[curr_channel].socketOut6>0)
			close (chan_p->channels[curr_channel].socketOut6);
		if(chan_p->channels[curr_channel].socketIn>0)
			close (chan_p->channels[curr_channel].socketIn);
		//Free the channel structures
		if(chan_p->channels[curr_channel].pmt_packet)
			free(chan_p->channels[curr_channel].pmt_packet);
		chan_p->channels[curr_channel].pmt_packet=NULL;


#ifdef ENABLE_SCAM_SUPPORT
		if(chan_p->channels[curr_channel].scam_pmt_packet)
			free(chan_p->channels[curr_channel].scam_pmt_packet);
		chan_p->channels[curr_channel].scam_pmt_packet=NULL;

		if (chan_p->channels[curr_channel].scam_support && scam_vars->scam_support) {
			scam_channel_stop(&chan_p->channels[curr_channel]);
		}
#endif



	}

	// we close the file descriptors
	close_card_fd(&fds);

	//We close the unicast connections and free the clients
	unicast_freeing(unicast_vars);

#ifdef ENABLE_CAM_SUPPORT
	if(cam_p->cam_support)
	{
		// stop CAM operation
		cam_stop(cam_p);
		// delete cam_info file
		if (remove (cam_p->filename_cam_info))
		{
			log_message( log_module,  MSG_WARN,
					"%s: %s\n",
					cam_p->filename_cam_info, strerror (errno));
		}
		mumu_free_string(&cam_p->cam_menulist_str);
		mumu_free_string(&cam_p->cam_menu_string);
	}
#endif
#ifdef ENABLE_SCAM_SUPPORT
	if(scam_vars->scam_support)
	{
		scam_getcw_stop(scam_vars);
	}
#endif

	//autoconf variables freeing
	autoconf_freeing(auto_p);

	//sap variables freeing
	if(monitor_thread_params && monitor_thread_params->sap_p->sap_messages4)
		free(monitor_thread_params->sap_p->sap_messages4);
	if(monitor_thread_params && monitor_thread_params->sap_p->sap_messages6)
		free(monitor_thread_params->sap_p->sap_messages6);

	//Pat rewrite freeing
	if(rewrite_vars->full_pat)
		free(rewrite_vars->full_pat);

	//SDT rewrite freeing
	if(rewrite_vars->full_sdt)
		free(rewrite_vars->full_sdt);

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
			params->tune_p->display_strenght = params->tune_p->display_strenght ? 0 : 1;
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
			log_message( log_module, MSG_DEBUG,"Sync logs\n");
			sync_logs();
			received_signal = 0;
		}

		pthread_mutex_lock(&params->chan_p->lock);

		/*we are not doing autoconfiguration we can do something else*/
		/*sap announces*/
		sap_poll(params->sap_p,params->chan_p->number_of_channels,params->chan_p->channels,*params->multi_p, (long)monitor_now);



		/*******************************************/
		/* compute the bandwidth occupied by        */
		/* each channel                            */
		/*******************************************/
		float time_interval;
		if(!params->stats_infos->compute_traffic_time)
			params->stats_infos->compute_traffic_time=monitor_now;
		if((monitor_now-params->stats_infos->compute_traffic_time)>=params->stats_infos->compute_traffic_interval)
		{
			time_interval=monitor_now-params->stats_infos->compute_traffic_time;
			params->stats_infos->compute_traffic_time=monitor_now;
			for (curr_channel = 0; curr_channel < params->chan_p->number_of_channels; curr_channel++)
			{
				mumudvb_channel_t *current;
				current=&params->chan_p->channels[curr_channel];
				pthread_mutex_lock(&current->stats_lock);
				if (time_interval!=0)
					params->chan_p->channels[curr_channel].traffic=((float)params->chan_p->channels[curr_channel].sent_data)/time_interval*1/1000;
				else
					params->chan_p->channels[curr_channel].traffic=0;
				params->chan_p->channels[curr_channel].sent_data=0;
				pthread_mutex_unlock(&current->stats_lock);
			}
		}

		/*******************************************/
		/*show the bandwidth measurement            */
		/*******************************************/
		if(params->stats_infos->show_traffic)
		{
			show_traffic(log_module,monitor_now, params->stats_infos->show_traffic_interval, params->chan_p);
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
		/* Check if the channel scrambling state    */
		/* has changed                             */
		/*******************************************/
		// Current thresholds for calculation
		// (<2%) FULLY_UNSCRAMBLED
		// (5%<=ratio<=75%) PARTIALLY_UNSCRAMBLED
		// (>80%) HIGHLY_SCRAMBLED
		// The gap is an hysteresis to avoid excessive jumping between states
		for (curr_channel = 0; curr_channel < params->chan_p->number_of_channels; curr_channel++)
		{
			mumudvb_channel_t *current;
			current=&params->chan_p->channels[curr_channel];
			if(current->channel_ready<READY)
				continue;
			pthread_mutex_lock(&current->stats_lock);
			/* Calculation of the ratio (percentage) of scrambled packets received*/
			if (current->num_packet >0 && current->num_scrambled_packets>10)
				current->ratio_scrambled = (int)(current->num_scrambled_packets*100/(current->num_packet));
			else
				current->ratio_scrambled = 0;

			/* Test if we have only unscrambled packets (<2%) - scrambled_channel=FULLY_UNSCRAMBLED : fully unscrambled*/
			if ((current->ratio_scrambled < 2) && (current->scrambled_channel != FULLY_UNSCRAMBLED))
			{
				log_message( log_module,  MSG_INFO,
						"Channel \"%s\" is now fully unscrambled (%d%% of scrambled packets). Card %d\n",
						current->name, current->ratio_scrambled, params->tune_p->card);
				current->scrambled_channel = FULLY_UNSCRAMBLED;// update
			}
			/* Test if we have partially unscrambled packets (5%<=ratio<=75%) - scrambled_channel=PARTIALLY_UNSCRAMBLED : partially unscrambled*/
			if ((current->ratio_scrambled >= 5) && (current->ratio_scrambled <= 75) && (current->scrambled_channel != PARTIALLY_UNSCRAMBLED))
			{
				log_message( log_module,  MSG_INFO,
						"Channel \"%s\" is now partially unscrambled (%d%% of scrambled packets). Card %d\n",
						current->name, current->ratio_scrambled, params->tune_p->card);
				current->scrambled_channel = PARTIALLY_UNSCRAMBLED;// update
			}
			/* Test if we have nearly only scrambled packets (>80%) - scrambled_channel=HIGHLY_SCRAMBLED : highly scrambled*/
			if ((current->ratio_scrambled > 80) && current->scrambled_channel != HIGHLY_SCRAMBLED)
			{
				log_message( log_module,  MSG_INFO,
						"Channel \"%s\" is now highly scrambled (%d%% of scrambled packets). Card %d\n",
						current->name, current->ratio_scrambled, params->tune_p->card);
				current->scrambled_channel = HIGHLY_SCRAMBLED;// update
			}
			/* Check the PID scrambling state */
			int curr_pid;
			for (curr_pid = 0; curr_pid < current->pid_i.num_pids; curr_pid++)
			{
				if (current->pid_i.pids_num_scrambled_packets[curr_pid]>0)
					current->pid_i.pids_scrambled[curr_pid]=1;
				else
					current->pid_i.pids_scrambled[curr_pid]=0;
				current->pid_i.pids_num_scrambled_packets[curr_pid]=0;
			}
			pthread_mutex_unlock(&current->stats_lock);
		}







		/*******************************************/
		/* Check if the channel stream state       */
		/* has changed                             */
		/*******************************************/
		if(last_updown_check)
		{
			/* Check if the channel stream state has changed*/
			for (curr_channel = 0; curr_channel < params->chan_p->number_of_channels; curr_channel++)
			{
				mumudvb_channel_t *current;
				current=&params->chan_p->channels[curr_channel];
				if(current->channel_ready<READY)
					continue;
				double packets_per_sec;
				int num_scrambled;
				pthread_mutex_lock(&current->stats_lock);
				if(dont_send_scrambled) {
					num_scrambled=current->num_scrambled_packets;
				}
				else
					num_scrambled=0;
				if (monitor_now>last_updown_check)
					packets_per_sec=((double)current->num_packet-num_scrambled)/(monitor_now-last_updown_check);
				else
					packets_per_sec=0;
				pthread_mutex_unlock(&current->stats_lock);
				if( params->stats_infos->debug_updown)
				{
					log_message( log_module,  MSG_FLOOD,
							"Channel \"%s\" streamed_channel %f packets/s\n",
							current->name,packets_per_sec);
				}
				if ((packets_per_sec >= params->stats_infos->up_threshold) && (!current->has_traffic))
				{
					log_message( log_module,  MSG_INFO,
							"Channel \"%s\" back.Card %d\n",
							current->name, params->tune_p->card);
					current->has_traffic = 1;  // update
				}
				else if ((current->has_traffic) && (packets_per_sec < params->stats_infos->down_threshold))
				{
					log_message( log_module,  MSG_INFO,
							"Channel \"%s\" down.Card %d\n",
							current->name, params->tune_p->card);
					current->has_traffic = 0;  // update
				}
			}
		}
		/* reinit */
		for (curr_channel = 0; curr_channel < params->chan_p->number_of_channels; curr_channel++)
		{
			mumudvb_channel_t *current;
			current=&params->chan_p->channels[curr_channel];
			if(current->channel_ready<READY)
				continue;
			pthread_mutex_lock(&current->stats_lock);
			params->chan_p->channels[curr_channel].num_packet = 0;
			params->chan_p->channels[curr_channel].num_scrambled_packets = 0;
			pthread_mutex_unlock(&current->stats_lock);
		}
		last_updown_check=monitor_now;





		/*******************************************/
		/* we count active channels                */
		/*******************************************/
		int count_of_active_channels=0;
		for (curr_channel = 0; curr_channel < params->chan_p->number_of_channels; curr_channel++)
			if (params->chan_p->channels[curr_channel].has_traffic && params->chan_p->channels[curr_channel].channel_ready>=READY )
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
					params->tune_p->card, timeout_no_diff);
			set_interrupted(ERROR_NO_DIFF<<8); //the <<8 is to make difference between signals and errors
		}


#ifdef ENABLE_SCAM_SUPPORT
		if (scam_vars->scam_support) {
			/*******************************************/
			/* we check num of packets in ring buffer                */
			/*******************************************/
			for (curr_channel = 0; curr_channel < params->chan_p->number_of_channels; curr_channel++) {
				mumudvb_channel_t *channel = &params->chan_p->channels[curr_channel];
				if (channel->scam_support && channel->channel_ready>=READY) {
					//send capmt if needed
					if (channel->need_scam_ask==CAM_NEED_ASK) {
						if (channel->scam_support) {
							pthread_mutex_lock(&channel->scam_pmt_packet->packetmutex);
							if (channel->scam_pmt_packet->len_full != 0 ) {
								if (!scam_send_capmt(channel, scam_vars,params->tune_p->card))
								{
										channel->need_scam_ask=CAM_ASKED;
								}
							}
							pthread_mutex_unlock(&channel->scam_pmt_packet->packetmutex);
						}
					}
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
						log_message( log_module,  MSG_ERROR, "%s: ring buffer overflow, packets in ring buffer %u, ring buffer size %llu\n",channel->name, ring_buffer_num_packets, (long long unsigned int)channel->ring_buffer_size);
					else
						log_message( log_module,  MSG_DEBUG, "%s: packets in ring buffer %u, ring buffer size %llu, to descramble %u, to send %u\n",channel->name, ring_buffer_num_packets, (long long unsigned int)channel->ring_buffer_size, to_descramble, to_send);
				}
			}
		}

#endif



		/*******************************************/
		/* generation of the file which says       */
		/* the streamed channels                   */
		/*******************************************/
		if (write_streamed_channels)
			gen_file_streamed_channels(params->filename_channels_streamed, params->filename_channels_not_streamed, params->chan_p->number_of_channels, params->chan_p->channels);



		pthread_mutex_unlock(&params->chan_p->lock);

		for(i=0;i<params->wait_time && !params->threadshutdown;i++)
			usleep(100000);
	}

	log_message(log_module,MSG_DEBUG, "Monitor thread stopping, it lasted %f seconds\n", monitor_now);
	return 0;

}











