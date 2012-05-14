/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Header file for software descrambling common functions
 * 
 * (C) 2004-2010 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.braice.net
 *
 * Code inspired by vdr plugin dvbapi
 * Copyright (C) 2011,2012 Mariusz Białończyk <manio@skyboo.net>
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
 */


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/dvb/ca.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>


#include "scam_common.h"
#include "errors.h"
#include "ts.h"
#include "mumudvb.h"
#include "log.h"
#include "unicast_http.h"
#include "rtp.h"


static char *log_module="SCAM_COMMON: ";

/** @brief Read a line of the configuration file to check if there is a cam parameter
 *
 * @param cam_vars the cam parameters
 * @param substring The currrent line
 */
int read_scam_configuration(scam_parameters_t *scam_vars, mumudvb_channel_t *current_channel, int ip_ok, char *substring)
{
  char delimiteurs[] = CONFIG_FILE_SEPARATOR;
  if (!strcmp (substring, "scam_support"))
  {
    substring = strtok (NULL, delimiteurs);
    scam_vars->scam_support = atoi (substring);
    if(scam_vars->scam_support)
    {
      log_message( log_module,  MSG_WARN,
                   "You have enabled the support for software descrambling (scrambled channels). Please report any bug/comment\n");
    }
	scam_vars->ring_buffer_default_size=RING_BUFFER_DEFAULT_SIZE;
	scam_vars->decsa_default_delay=DECSA_DEFAULT_DELAY;
	scam_vars->send_default_delay=SEND_DEFAULT_DELAY;
	scam_vars->decsa_default_wait=DECSA_DEFAULT_WAIT;
  }
  else if (!strcmp (substring, "ring_buffer_default_size"))
  {
    substring = strtok (NULL, delimiteurs);
    scam_vars->ring_buffer_default_size = 1<<((uint64_t)ceil(log2((long double)atoi (substring))));
    log_message( log_module,  MSG_DEBUG, "Ring buffer default size set to %u\n",scam_vars->ring_buffer_default_size);

  }
  else if (!strcmp (substring, "decsa_default_delay"))
  {
    substring = strtok (NULL, delimiteurs);
    scam_vars->decsa_default_delay = atoi (substring);
	if (scam_vars->decsa_default_delay > 10000000)
	  scam_vars->decsa_default_delay = 10000000;
  }
  else if (!strcmp (substring, "send_default_delay"))
  {
    substring = strtok (NULL, delimiteurs);
    scam_vars->send_default_delay = atoi (substring);
  }
  else if (!strcmp (substring, "decsa_default_wait"))
  {
    substring = strtok (NULL, delimiteurs);
    scam_vars->decsa_default_wait = atoi (substring);
  }
  else if (!strcmp (substring, "oscam"))
  {
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "oscam : You have to start a channel first (using ip= or channel_next)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->oscam_support = atoi (substring);
	if (current_channel->oscam_support) {
		current_channel->need_scam_ask=CAM_NEED_ASK;
		current_channel->ring_buffer_size=scam_vars->ring_buffer_default_size;
		current_channel->decsa_delay=scam_vars->decsa_default_delay;
		current_channel->send_delay=scam_vars->send_default_delay;
		current_channel->decsa_wait=scam_vars->decsa_default_wait;
	}
  }
  else if (!strcmp (substring, "ring_buffer_size"))
  {
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "oscam : You have to start a channel first (using ip= or channel_next)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->ring_buffer_size = 1<<((uint64_t)ceil(log2((long double)atoi (substring))));
  }
  else if (!strcmp (substring, "decsa_delay"))
  {
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "oscam : You have to start a channel first (using ip= or channel_next)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->decsa_delay = atoi (substring);
	if (current_channel->decsa_delay > 10000000)
	  current_channel->decsa_delay = 10000000;
  }
  else if (!strcmp (substring, "send_delay"))
  {
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "oscam : You have to start a channel first (using ip= or channel_next)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->send_delay = atoi (substring);
  }
  else if (!strcmp (substring, "decsa_wait"))
  {
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "oscam : You have to start a channel first (using ip= or channel_next)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->decsa_wait = atoi (substring);
  }
  else
    return 0; //Nothing concerning cam, we return 0 to explore the other possibilities

  return 1;//We found something for cam, we tell main to go for the next line

}

unsigned char ts_packet_get_payload_offset(unsigned char *ts_packet) {
	if (ts_packet[0] != 0x47)
		return 0;

	unsigned char adapt_field   = (ts_packet[3] &~ 0xDF) >> 5; // 11x11111
	unsigned char payload_field = (ts_packet[3] &~ 0xEF) >> 4; // 111x1111

	if (!adapt_field && !payload_field) // Not allowed
		return 0;

	if (adapt_field) {
		unsigned char adapt_len = ts_packet[4];
		if (payload_field && adapt_len > 182) // Validity checks
			return 0;
		if (!payload_field && adapt_len > 183)
			return 0;
		if (adapt_len + 4 > 188) // adaptation field takes the whole packet
			return 0;
		return 4 + 1 + adapt_len; // ts header + adapt_field_len_byte + adapt_field_len
	} else {
		return 4; // No adaptation, data starts directly after TS header
	}
}

int start_thread_with_priority(pthread_t* thread, void *(*start_routine)(void*), void* arg)
{
  pthread_attr_t attr;
  struct sched_param param;
  
  pthread_attr_init(&attr);
  pthread_attr_setschedpolicy(&attr, SCHED_RR);
  param.sched_priority = sched_get_priority_max(SCHED_RR);
  pthread_attr_setschedparam(&attr, &param);

  return pthread_create(thread, &attr, start_routine, arg);
}

/** @brief Sending available data with given delay
 *
 *
 */

void *sendthread_func(void* arg)
{
  extern uint64_t now_time;
  extern unicast_parameters_t unicast_vars;
  extern multicast_parameters_t multicast_vars;
  extern mumudvb_chan_and_pids_t chan_and_pids;
  extern fds_t fds;
  mumudvb_channel_t *channel;
  channel = ((mumudvb_channel_t *) arg);
  uint64_t res_time;
  struct timespec r_time;
  log_message( "SEND: ", MSG_DEBUG, "thread started, channel %s\n",channel->name); 
  channel->num_packet_descrambled_sent=0;
  while(!channel->ring_buf->to_send && !channel->sendthread_shutdown)
	usleep(50000);
	
  while(!channel->sendthread_shutdown) {
	  if (now_time<channel->ring_buf->time_send[(channel->ring_buf->read_send_idx>>2)]) {
	  	now_time=get_time();
		if (now_time<channel->ring_buf->time_send[(channel->ring_buf->read_send_idx>>2)]) {
			res_time=channel->ring_buf->time_send[(channel->ring_buf->read_send_idx>>2)] - now_time;
			r_time.tv_sec=res_time/(1000000ull);
			r_time.tv_nsec=1000*(res_time%(1000000ull));
			while(nanosleep(&r_time, &r_time));
			now_time=get_time();
		}
	  }
	  else if (channel->ring_buf->to_send) {
		  memcpy(channel->buf + channel->nb_bytes, channel->ring_buf->data[channel->ring_buf->read_send_idx], TS_PACKET_SIZE);

		  ++channel->ring_buf->read_send_idx;
		
		  channel->ring_buf->read_send_idx&=(channel->ring_buffer_size -1);

          channel->nb_bytes += TS_PACKET_SIZE;
		  --channel->ring_buf->to_send;
		  --channel->num_packets;
		  ++channel->num_packet_descrambled_sent;
		
          //The buffer is full, we send it
          if ((!multicast_vars.rtp_header && ((channel->nb_bytes + TS_PACKET_SIZE) > MAX_UDP_SIZE))
	    ||(multicast_vars.rtp_header && ((channel->nb_bytes + RTP_HEADER_LEN + TS_PACKET_SIZE) > MAX_UDP_SIZE)))
          {
			  send_func(channel, &channel->ring_buf->time_send[(channel->ring_buf->read_send_idx>>2)], &unicast_vars, &multicast_vars, &chan_and_pids, &fds);
          }
  		}
	  else {
	    log_message( "SEND: ", MSG_DEBUG, "buffer starved, channel %s %u %u\n",channel->name,channel->ring_buf->to_descramble,channel->ring_buf->to_send); 
	    usleep(1000);
	  }

  }
  return 0;
}

/** @brief initialize the autoconfiguration : alloc the memory etc...
 *
 */
int scam_init(autoconf_parameters_t *autoconf_vars, scam_parameters_t *scam_vars, mumudvb_channel_t *channels,int number_of_channels)
{
  int curr_channel;


 if (scam_vars->scam_support){ 
  if (autoconf_vars->autoconfiguration==AUTOCONF_MODE_PIDS || autoconf_vars->autoconfiguration==AUTOCONF_MODE_NONE)
    for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
		if (channels[curr_channel].oscam_support==1 && channels[curr_channel].num_pids>1)
		{
		  //Only one pid with autoconfiguration=partial, it's the PMT pid
		  channels[curr_channel].pmt_pid=channels[curr_channel].pids[0];
		      channels[curr_channel].pids_type[0]=PID_PMT;
		      snprintf(channels[curr_channel].pids_language[0],4,"%s","---");
		  ++scam_vars->need_pmt_get;
		}
    }
 }

 return 0;

}


int scam_new_packet(int pid, unsigned char *ts_packet, scam_parameters_t *scam_vars, mumudvb_channel_t *channels)
{
  int curr_channel;

  if(scam_vars->need_pmt_get) //We have the channels and their PMT, we search the other pids
  {
    for(curr_channel=0;curr_channel<MAX_CHANNELS;curr_channel++)
    {
      if((channels[curr_channel].pmt_pid==pid)&& pid && channels[curr_channel].oscam_support)
      {
		if(get_ts_packet(ts_packet,channels[curr_channel].pmt_packet))
		{
		  --scam_vars->need_pmt_get;
		}
	  }
	}
 }
 return 0;

}

