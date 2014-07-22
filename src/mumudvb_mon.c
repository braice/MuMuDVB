/*
 * mumudvb_mon.c
 *  Monitor and init functions for MuMuDVB
 *  Created on: Mar 29, 2014
 *      Author: braice
 */

#define _GNU_SOURCE		//in order to use program_invocation_short_name and pthread_timedjoin_np

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

extern long now;
extern long real_start_time;
extern int received_signal;
//logging
extern log_params_t log_params;
extern int dont_send_scrambled;
extern int write_streamed_channels;
extern int timeout_no_diff;




void parse_cmd_line(int argc, char **argv,char *(*conf_filename),tune_p_t *tune_p,stats_infos_t *stats_infos,int *server_id, int *no_daemon,char **dump_filename, int *listingcards)
{
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
		*conf_filename = (char *) malloc (strlen (optarg) + 1);
		if (!*conf_filename)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			exit(ERROR_MEMORY);
		}
		strncpy (*conf_filename, optarg, strlen (optarg) + 1);
		break;
		case 'a':
			tune_p->card=atoi(optarg);
			break;
		case 's':
			tune_p->display_strenght = 1;
			break;
		case 'i':
			*server_id = atoi(optarg);
			break;
		case 't':
			stats_infos->show_traffic = 1;
			break;
		case 'd':
			*no_daemon = 1;
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
			*listingcards=1;
			break;
		case 'z':
			*dump_filename = (char *) malloc (strlen (optarg) + 1);
			if (!*dump_filename)
			{
				log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
				exit(ERROR_MEMORY);
			}
			strncpy (*dump_filename, optarg, strlen (optarg) + 1);
			log_message( log_module, MSG_WARN,"You've decided to dump the received stream into %s. Be warned, it can grow quite fast", *dump_filename);
			break;
		}
	}
	if (optind < argc)
	{
		usage (program_invocation_short_name);
		exit(ERROR_ARGS);
	}

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
		mumu_chan_p_t *chan_p,
		pthread_t *signalpowerthread,
		pthread_t *monitorthread,
		card_thread_parameters_t *cardthreadparams,
		fds_t *fds)
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

	if(*signalpowerthread)
	{
		log_message(log_module,MSG_DEBUG,"Signal/power Thread closing\n");
		*strengththreadshutdown=1;
#if !defined __UCLIBC__ && !defined ANDROID
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 5;
		iRet=pthread_timedjoin_np(*signalpowerthread, NULL, &ts);
#else
		iRet=pthread_join(*signalpowerthread, NULL);
#endif
		if(iRet)
			log_message(log_module,MSG_WARN,"Signal/power Thread badly closed: %s\n", strerror(iRet));

	}
	if(cardthreadparams->thread_running)
	{
		log_message(log_module,MSG_DEBUG,"Card reading Thread closing\n");
		cardthreadparams->threadshutdown=1;
		pthread_mutex_destroy(&cardthreadparams->carddatamutex);
		pthread_cond_destroy(&cardthreadparams->threadcond);
	}
	//We shutdown the monitoring thread
	if(*monitorthread)
	{
		log_message(log_module,MSG_DEBUG,"Monitor Thread closing\n");
		monitor_thread_params->threadshutdown=1;
#if !defined __UCLIBC__ && !defined ANDROID
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 5;
		iRet=pthread_timedjoin_np(*monitorthread, NULL, &ts);
#else
		iRet=pthread_join(*monitorthread, NULL);
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
		//Free the channel structures
		if(chan_p->channels[curr_channel].scam_pmt_packet)
			free(chan_p->channels[curr_channel].scam_pmt_packet);
		chan_p->channels[curr_channel].scam_pmt_packet=NULL;

		if (chan_p->channels[curr_channel].scam_support && scam_vars->scam_support) {
			scam_channel_stop(&chan_p->channels[curr_channel]);
		}
#endif



	}

	// we close the file descriptors
	close_card_fd(fds);

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
	if(fds->pfds)
		free(fds->pfds);
	fds->pfds=NULL;
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
#ifndef ANDROID
	munlockall();
#endif
	// End
	return(ExitCode);

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



