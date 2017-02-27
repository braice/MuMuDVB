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

#define _GNU_SOURCE		//in order to use program_invocation_short_name and pthread_timedjoin_np


#include "config.h"

// Linux includes:
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <stdint.h>
#include <resolv.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#ifdef ANDROID
#include <limits.h>
#else
#include <values.h>
#endif
#include <string.h>
#include <syslog.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <linux/dvb/version.h>
#include <sys/mman.h>
#include <pthread.h>

#include "mumudvb.h"
#include "mumudvb_mon.h"
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

#if defined __UCLIBC__ || defined ANDROID
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
int *card_tuned;  	  			//Pointer to the card_tuned information
int received_signal = 0;

int timeout_no_diff = ALARM_TIME_TIMEOUT_NO_DIFF;

int  write_streamed_channels=1;

/** Do we send scrambled packets ? */
int dont_send_scrambled=0;





//logging
extern log_params_t log_params;

// prototypes
static void SignalHandler (int signum);//below
int read_multicast_configuration(multi_p_t *, mumudvb_channel_t *, char *); //in multicast.c
void init_multicast_v(multi_p_t *multi_p); //in multicast.c

void chan_new_pmt(unsigned char *ts_packet, mumu_chan_p_t *chan_p, int pid);

#ifndef __cplusplus
    typedef enum { false = 0, true = !false } bool;
#endif

bool t2mi_active=false;
bool t2mi_first=true;
int t2packetpos=0;

int t2_partial_size=0;
char t2packet[65536 + 10]; //Maximal T2 payload + header

/* rewritten by [anp/hsw], original code taken from https://github.com/newspaperman/t2-mi */
int processt2(unsigned char* input_buf, int input_buf_offset, unsigned char* output_buf, int output_buf_offset, uint8_t plpId) {

	unsigned int payload_start_offset=0;
	output_buf+=output_buf_offset;

	/* lookup for adaptation field control bits in TS input stream */
        switch(((input_buf[input_buf_offset + 3])&0x30)>>4) {
    	    case 0x03:	/* 11b = adaptation field followed by payload */
		/* number of bytes in AF, following this byte */
                payload_start_offset=(uint8_t)(input_buf[input_buf_offset + 4]) + 1;
                if(payload_start_offset > 183) {
            		log_message(log_module, MSG_DEBUG, "T2-MI: wrong AF len in input stream: %d\n", payload_start_offset);
                        return 0;
                }
            break;

            case 0x02:	/* 10b = adaptation field only, no payload */
                return 0;
            break;

            case 0x00:	/* 00b = reserved! */
            	log_message(log_module, MSG_DEBUG, "T2-MI: wrong AF (00) in input stream, accepting as ordinary packet\n");
            break;
	}

	/* source buffer pointer to beginning of payload in packet */
        unsigned char* buf = input_buf + input_buf_offset + 4 + payload_start_offset;
        unsigned int len = TS_PACKET_SIZE - 4 - payload_start_offset;

        int output_bytes=0;

	/* check for payload unit start indicator */
        if((((input_buf[input_buf_offset + 1])&0x40)>>4)==0x04) {
                unsigned int offset=1;
                offset+=(uint8_t)(buf[0]);
                if(t2mi_active) {
                        if( 1 < offset && offset < 184) {
                                memcpy(&t2packet[t2packetpos],&buf[1],offset-1);
                        } else if (offset >= 184) {
            			log_message(log_module, MSG_DEBUG, "T2-MI: invalid payload offset: %u\n", offset);
            			return 0;
                        }
                        
			/* select source PLP */
                        if(t2packet[7]==plpId) {
                    		/* extract TS packet from T2-MI payload */
				/* Sync distance (bits) in the BB header then points to the first CRC-8 present in the data field */
                                unsigned int syncd = ((uint8_t)(t2packet[16]) << 8) + (uint8_t)(t2packet[17]);
                                syncd >>= 3;

				/* user packet len (bits) = sync byte + payload, CRC-8 of payload replaces next sync byte */
                                unsigned int upl = ((uint8_t)(t2packet[13]) << 8) + (uint8_t)(t2packet[14]);
                                upl >>= 3;
                                upl+=19;

                                int dnp=0;

                                if(t2packet[9]&0x4) {
                        	    dnp=1; // Deleted Null Packet
                        	}
                                if(syncd==0x1FFF ) { /* maximal sync value (in bytes) */
            				log_message(log_module, MSG_DEBUG, "T2-MI: sync value 0x1FFF!\n");
                                        if(upl >19) {
                                                memcpy(output_buf + output_bytes, &t2packet[19], upl-19);
                                                output_bytes+=(upl-19);
                                        }

                                } else {
                                        if(!t2mi_first && syncd > 0) {
                                            if (syncd-dnp > (sizeof(t2packet)-19)) {
	                                	    log_message(log_module, MSG_DEBUG, "T2-MI: position (syncd) out of buffer bounds: %d\n", syncd-dnp);
	                                	    goto t2_copy_end;
                                            }
                                            memcpy(output_buf + output_bytes, &t2packet[19], syncd-dnp);
                                            output_bytes+=(syncd-dnp);
                                        }

					/* detect unaligned packet in buffer */
                                        unsigned int output_part = (output_buf_offset + output_bytes) % TS_PACKET_SIZE;
                                        
                                        if (output_part > 0) {
                                    	    log_message(log_module, MSG_DETAIL, "T2-MI: unaligned packet in buffer pos %d/%d\n", output_buf_offset, output_bytes);
                                    	    output_bytes -= output_part; /* drop packet; TODO: check if we can add padding instead of dropping */
                                        }

                                        t2mi_first=false;
                                        unsigned int t2_copy_pos=19+syncd;

                                        /* copy T2-MI packet payload to output, add sync bytes */
                                        for(; t2_copy_pos < upl - 187; t2_copy_pos+=(187+dnp)) {
                                    		/* fullsize TS frame */
                                                if (t2_copy_pos > ((sizeof(t2packet) - 187))) {
                                        	    log_message(log_module, MSG_DEBUG, "T2-MI: position (full TS) out of buffer bounds: %d\n", t2_copy_pos);
                                        	    goto t2_copy_end;
                                                }
                                                output_buf[output_bytes] = TS_SYNC_BYTE;
                                                output_bytes++;
                                                memcpy(output_buf + output_bytes, &t2packet[t2_copy_pos], 187);
                                                output_bytes+=187;
                                        }
                                        if(t2_copy_pos < upl )  {
                                    		/* partial TS frame, we will fill rest of frame at next call */
                                                if (t2_copy_pos > (sizeof(t2packet)-((upl-t2_copy_pos)+1))) {
                                        	    log_message(log_module, MSG_DEBUG, "T2-MI: position (part TS) out of buffer bounds: %d\n", t2_copy_pos);
                                        	    goto t2_copy_end;
                                                }
                                                output_buf[output_bytes] = TS_SYNC_BYTE;
                                                output_bytes++;
                                                memcpy(output_buf + output_bytes, &t2packet[t2_copy_pos], upl-t2_copy_pos);
                                                output_bytes+=(upl-t2_copy_pos);
                                        }
                                        t2_copy_end: ;
                                }
                        }
                        t2mi_active=false;
                        memset(&t2packet, 0, sizeof(t2packet)); // end of processing t2-mi packet, clear it
                }

                if((buf[offset])==0x0) { //Baseband Frame
			/*	TODO: padding
				pad (pad_len bits) shall be filled with between 0 and 7 bits of padding such that the T2-MI packet is always an integer
				number of bytes in length, i.e. payload_len+pad_len shall be a multiple of 8. Each padding bit shall have the value 0. 
			*/
                        if(len > offset) {
                                memcpy(t2packet,&buf[offset],len-offset);
                                t2packetpos=len-offset;
                                t2mi_active=true;
                        }
                }
        } else if(t2mi_active) {
                memcpy(t2packet+t2packetpos,buf,len);
                t2packetpos+=len;
        }
        return output_bytes;
}



int
main (int argc, char **argv)
{
	// file descriptors
	fds_t fds; /** File descriptors associated with the card */
	memset(&fds,0,sizeof(fds_t));

	//Thread information
	pthread_t signalpowerthread=0;
	pthread_t cardthread;
	pthread_t monitorthread=0;
	card_thread_parameters_t cardthreadparams;
	memset(&cardthreadparams,0,sizeof(card_thread_parameters_t));

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

	//unicast
	//Parameters for HTTP unicast
	unicast_parameters_t unic_p;
	init_unicast_v(&unic_p);

	//multicast
	//multicast parameters
	multi_p_t multi_p;
	init_multicast_v(&multi_p);

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

	int iRet;


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


	int listingcards=0;
	//Getopt
	parse_cmd_line(argc,argv,
			&conf_filename,
			&tune_p,
			&stats_infos,
			&server_id,
			&no_daemon,
			&dump_filename,
			&listingcards);


	//List the detected cards
	if(listingcards)
	{
		print_info ();
		list_dvb_cards ();
		exit(0);
	}

	// DO NOT REMOVE (make MuMuDVB a deamon)
	if(!no_daemon)
		if(daemon(42,0))
		{
			log_message( log_module,  MSG_WARN, "Cannot daemonize: %s\n",
					strerror (errno));
			exit(666); //Right code for a bad daemon no ?
		}

	//If the user didn't defined a preferred logging way, and we daemonize, we set to syslog
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


	// configuration file parsing
	int ichan = 0;
	int ipid = 0;
	int send_packet=0;
	char current_line[CONF_LINELEN];
	char *substring=NULL;
	char delimiteurs[] = CONFIG_FILE_SEPARATOR;
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
		else if((iRet=read_scam_configuration(scam_vars_ptr, c_chan, substring))) //Read the line concerning the software cam parameters
		{
			if(iRet==-1)
				exit(ERROR_CONF);
		}
#endif
		else if((iRet=read_unicast_configuration(&unic_p, c_chan, substring))) //Read the line concerning the unicast parameters
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
		else if (!strcmp (substring, "t2mi_pid"))
		{
			substring = strtok (NULL, delimiteurs);
			chan_p.t2mi_pid = atoi (substring);
			log_message(log_module,MSG_INFO,"Demuxing T2-MI stream on pid %d as input\n", chan_p.t2mi_pid);
			if(chan_p.t2mi_pid < 1 || chan_p.t2mi_pid > 8192)
			{
				log_message(log_module,MSG_WARN,"wrong t2mi pid, forced to 4096\n");
				chan_p.t2mi_pid=4096;
			}
		}
		else if (!strcmp (substring, "t2mi_plp"))
		{
			substring = strtok (NULL, delimiteurs);
			chan_p.t2mi_plp = atoi (substring);
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
	free(conf_filename);


	//Set default card if not specified
	if(tune_p.card==-1)
		tune_p.card=0;


	/*************************************/
	//End of configuration file reading
	/*************************************/




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

	if(chan_p.t2mi_pid > 0 && card_buffer.dvr_buffer_size < 20)
	{
		log_message( log_module,  MSG_WARN,
				"Warning : You set a DVR buffer size too low to accept T2-MI frames, I increase your dvr_buffer_size to 20 ...\n");
		card_buffer.dvr_buffer_size=20;
	}

	if(card_buffer.max_thread_buffer_size<card_buffer.dvr_buffer_size)
	{
		log_message( log_module,  MSG_WARN,
				"Warning : You set a thread buffer size lower than your DVR buffer size, it's not possible to use such values. I increase your dvr_thread_buffer_size ...\n");
		card_buffer.max_thread_buffer_size=card_buffer.dvr_buffer_size;
	}



	//Template for the card dev path
	char number[10];
	sprintf(number,"%d",tune_p.card);
	int l=sizeof(tune_p.card_dev_path);
	mumu_string_replace(tune_p.card_dev_path,&l,0,"%card",number);

	//If we specified a string for the unicast port out, we parse it
	if(unic_p.portOut_str!=NULL)
	{
		int len;
		len=strlen(unic_p.portOut_str)+1;
		sprintf(number,"%d",tune_p.card);
		unic_p.portOut_str=mumu_string_replace(unic_p.portOut_str,&len,1,"%card",number);
		sprintf(number,"%d",tune_p.tuner);
		unic_p.portOut_str=mumu_string_replace(unic_p.portOut_str,&len,1,"%tuner",number);
		sprintf(number,"%d",server_id);
		unic_p.portOut_str=mumu_string_replace(unic_p.portOut_str,&len,1,"%server",number);
		unic_p.portOut=string_comput(unic_p.portOut_str);
		log_message( "Unicast: ", MSG_DEBUG, "computed unicast master port : %d\n",unic_p.portOut);
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



	if(!multi_p.multicast && !unic_p.unicast)
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



	printf("%s %p\n", __func__,  &chan_p);
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
			.unicast_vars=&unic_p,
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
		if(scam_getcw_start(scam_vars_ptr, &chan_p))
		{
			log_message("SCAM_GETCW: ", MSG_ERROR,"Cannot initialize scam");
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
		scam_init_no_autoconf(scam_vars_ptr, chan_p.channels,chan_p.number_of_channels);

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
	unic_p.fd_info=NULL;
	unic_p.fd_info=realloc(unic_p.fd_info,(fds.pfdsnum)*sizeof(unicast_fd_info_t));
	if (unic_p.fd_info==NULL)
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
	if(unic_p.unicast)
	{
		log_message("Unicast: ", MSG_INFO,"We open the Master http socket for address %s:%d\n",unic_p.ipOut, unic_p.portOut);
		unicast_create_listening_socket(UNICAST_MASTER, -1, unic_p.ipOut, unic_p.portOut, &unic_p.sIn, &unic_p.socketIn, &unic_p);
	}
	update_chan_net(&chan_p, &auto_p, &multi_p, &unic_p, server_id, tune_p.card, tune_p.tuner);





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
				unic_p.unicast,
				unic_p.portOut,
				unic_p.ipOut);

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
		if (chan_p.t2mi_pid > 0) {
		    card_buffer.t2mi_buffer=malloc(sizeof(unsigned char)*card_buffer.write_buffer_size*2);
		}
		
	}else
	{
		//We alloc the buffer
		card_buffer.reading_buffer=malloc(sizeof(unsigned char)*TS_PACKET_SIZE*card_buffer.dvr_buffer_size);
		if (chan_p.t2mi_pid > 0) {
		    card_buffer.t2mi_buffer=malloc(sizeof(unsigned char)*TS_PACKET_SIZE*card_buffer.dvr_buffer_size*2);
		}
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
#ifndef ANDROID
	mlockall(MCL_CURRENT | MCL_FUTURE);
#endif
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
			if(!card_buffer.bytes_in_write_buffer)
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
			/**************************************************************/
			/* UNICAST HTTP                                               */
			/**************************************************************/
			if(unic_p.pfdsnum)
			{
				if(mumudvb_poll(unic_p.pfds,unic_p.pfdsnum,0)>0)
				{
					iRet=unicast_handle_fd_event(&unic_p, chan_p.channels, chan_p.number_of_channels, &strengthparams, &auto_p, cam_p_ptr, scam_vars_ptr, rewrite_vars.eit_packets);
					if(iRet)
					{
						log_message( log_module,  MSG_ERROR, "unicast fd error %d", iRet);
						set_interrupted(iRet);
					}
				}
			}
			/**************************************************************/
			/* END OF UNICAST HTTP                                        */
			/**************************************************************/
		}
		else
		{
			/* Poll the open file descriptors : we wait for data*/
			poll_ret=mumudvb_poll(fds.pfds,fds.pfdsnum,DVB_POLL_TIMEOUT);
			if(poll_ret<0)
			{
				log_message( log_module,  MSG_ERROR, "Poll error %d", poll_ret);
				set_interrupted(poll_ret);
				continue;
			}
			/**************************************************************/
			/* UNICAST HTTP                                               */
			/**************************************************************/
			if(unic_p.pfdsnum)
			{
				poll_ret=mumudvb_poll(unic_p.pfds,unic_p.pfdsnum,0);
				if(poll_ret>0)
				{
					iRet=unicast_handle_fd_event(&unic_p, chan_p.channels, chan_p.number_of_channels, &strengthparams, &auto_p, cam_p_ptr, scam_vars_ptr,rewrite_vars.eit_packets);
					if(iRet)
					{
						log_message( log_module,  MSG_ERROR, "unicast fd error %d", iRet);
						set_interrupted(iRet);
					}
				}
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

		if (chan_p.t2mi_pid > 0 && card_buffer.bytes_read > 0) {
			int processed = 0;
			int errorcounter = 0;

			for(card_buffer.read_buff_pos=0;
			    (card_buffer.read_buff_pos+TS_PACKET_SIZE)<=card_buffer.bytes_read;
			    card_buffer.read_buff_pos+=TS_PACKET_SIZE)//we loop on the subpackets
			{
			    /* check for sync byte and transport error bit if requested */
			    if((card_buffer.reading_buffer[card_buffer.read_buff_pos] != TS_SYNC_BYTE ||
				(card_buffer.reading_buffer[card_buffer.read_buff_pos+1] & 0x80) == 0x80) &&
				chan_p.filter_transport_error > 0)
			    {
        			log_message(log_module, MSG_FLOOD, "T2-MI: input TS packet damaged, buf offset %d\n", card_buffer.read_buff_pos);
        			errorcounter++;
				continue;
			    }
			    /* target t2mi stream pid */
                    	    if(chan_p.t2mi_pid != (((card_buffer.reading_buffer[card_buffer.read_buff_pos+1] & 0x1f) << 8) | (card_buffer.reading_buffer[card_buffer.read_buff_pos+2]))) {
                                continue;
                    	    }
                    	    processed += processt2(card_buffer.reading_buffer, card_buffer.read_buff_pos, card_buffer.t2mi_buffer, t2_partial_size + processed, chan_p.t2mi_plp);
    			}

			/* in case we got too much errors */
			if (errorcounter * 100 > card_buffer.dvr_buffer_size * 50) {
		    	    log_message(log_module, MSG_DEBUG,"T2-MI: too many errors in input buffer (%d/%d)\n", errorcounter, card_buffer.dvr_buffer_size);

		    	    t2_partial_size=0;
			    card_buffer.bytes_read=0;

		    	    t2mi_active=false;
			    t2mi_first=true;
		    	    continue;
			}

			/* we got no data from demux */
			if (processed + t2_partial_size == 0) {
			    card_buffer.bytes_read = 0;
			    continue;
			}

			card_buffer.bytes_read = processed + t2_partial_size;
		    	t2_partial_size=0;
		    
			/* if buffer is damaged, reset demux */
			if (!((card_buffer.t2mi_buffer[TS_PACKET_SIZE * 0] == TS_SYNC_BYTE) &&
			      (card_buffer.t2mi_buffer[TS_PACKET_SIZE * 1] == TS_SYNC_BYTE) &&
			      (card_buffer.t2mi_buffer[TS_PACKET_SIZE * 2] == TS_SYNC_BYTE) &&
			      (card_buffer.t2mi_buffer[TS_PACKET_SIZE * 3] == TS_SYNC_BYTE)) ) {

		    	    log_message( log_module, MSG_INFO,"T2-MI: buffer out of sync: %02x, %02x, %02x, %02x\n",
		    		card_buffer.t2mi_buffer[TS_PACKET_SIZE * 0],
		    		card_buffer.t2mi_buffer[TS_PACKET_SIZE * 1],
		    		card_buffer.t2mi_buffer[TS_PACKET_SIZE * 2],
		    		card_buffer.t2mi_buffer[TS_PACKET_SIZE * 3]
		    	    );

		    	    t2mi_active=false;
			    t2mi_first=true;
		    	    continue;
			}
		}

		for(card_buffer.read_buff_pos=0;
				(card_buffer.read_buff_pos+TS_PACKET_SIZE)<=card_buffer.bytes_read;
				card_buffer.read_buff_pos+=TS_PACKET_SIZE)//we loop on the subpackets
		{
			if (chan_p.t2mi_pid > 0) {
			    actual_ts_packet=card_buffer.t2mi_buffer+card_buffer.read_buff_pos;
			} else {
			    actual_ts_packet=card_buffer.reading_buffer+card_buffer.read_buff_pos;
			}

			//If the user asked to dump the streams it's here that it should be done
			if(dump_file)
				if(fwrite(actual_ts_packet,sizeof(unsigned char),TS_PACKET_SIZE,dump_file)<TS_PACKET_SIZE)
					log_message( log_module,MSG_WARN,"Error while writing the dump : %s", strerror(errno));

			/* check for sync byte and transport error bit if requested */
			if ((actual_ts_packet[0] != TS_SYNC_BYTE || actual_ts_packet[1] & 0x80) == 0x80)
			{
				log_message( log_module, MSG_FLOOD,"Error bit set or no sync in TS packet!\n");
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
			if(chan_p.asked_pid[8192]==PID_NOT_ASKED && chan_p.asked_pid[pid]==PID_NOT_ASKED && chan_p.t2mi_pid == 0)
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
				iRet = autoconf_new_packet(pid, actual_ts_packet, &auto_p,  &fds, &chan_p, &tune_p, &multi_p, &unic_p, server_id, scam_vars_ptr);
				if(iRet)
				{
					log_message( log_module,  MSG_ERROR, "Autoconf error %d", iRet);
					set_interrupted(iRet);
				}
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
					(rewrite_vars.rewrite_eit == OPTION_ON || //AND we asked for rewrite
							rewrite_vars.store_eit == OPTION_ON )) //OR to store it
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
				//Scam support
				// copy proper pmt to scam_pmt_packet
				/******************************************************/
#ifdef ENABLE_SCAM_SUPPORT
				if (scam_vars.scam_support && send_packet==1)  //no need to check packets we don't send
				{
					scam_new_packet(pid, &chan_p.channels[ichan]);
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
							&unic_p, scam_vars_ptr);
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
					buffer_func(channel, actual_ts_packet, &unic_p, scam_vars_ptr);
				}

			}
			pthread_mutex_unlock(&chan_p.lock);
		}

		/* in case we got partial packet from t2mi demux, save it */
		if (chan_p.t2mi_pid > 0 && card_buffer.bytes_read > card_buffer.read_buff_pos) {
		    t2_partial_size = card_buffer.bytes_read - card_buffer.read_buff_pos;
		    /* we will overlap if buffer is 1 packet in size! */
		    memcpy(card_buffer.t2mi_buffer, card_buffer.t2mi_buffer + card_buffer.read_buff_pos, t2_partial_size);
		} else {
		    t2_partial_size = 0;
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
					&unic_p,
					&tune_p.strengththreadshutdown,
					cam_p_ptr,
					scam_vars_ptr,
					filename_channels_not_streamed,
					filename_channels_streamed,
					filename_pid,
					get_interrupted(),
					&chan_p,
					&signalpowerthread,
					&monitorthread,
					&cardthreadparams,
					&fds);

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
		log_message( log_module,  MSG_ERROR, "Caught signal %d", signum);
		set_interrupted(signum);
	}
	signal (signum, SignalHandler);
}




