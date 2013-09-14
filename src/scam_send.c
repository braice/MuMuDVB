/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for software descrambling sending part
 * 
 * (C) 2004-2010 Brice DUBOST
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
 */

#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>


#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>


#include "errors.h"
#include "ts.h"
#include "mumudvb.h"
#include "log.h"
#include "scam_common.h"


/**@file
 * @brief scam support
 * 
 * Code for sending thread
 */

static void *sendthread_func(void* arg); //The polling thread
static char *log_module="SCAM_SEND: ";



/** @brief Sending available data with given delay
 *
 *
 */

void *sendthread_func(void* arg)
{
  extern unicast_parameters_t unicast_vars;
  extern multicast_parameters_t multicast_vars;
  extern mumudvb_chan_and_pids_t chan_and_pids;
  extern fds_t fds;
  mumudvb_channel_t *channel;
  channel = ((mumudvb_channel_t *) arg);
  uint64_t res_time;
  struct timespec r_time;
  channel->num_packet_descrambled_sent=0;
  while(!channel->sendthread_shutdown) {
	int to_send;
	pthread_mutex_lock(&channel->ring_buf->lock);
	to_send = channel->ring_buf->to_send;
	pthread_mutex_unlock(&channel->ring_buf->lock);
	if (to_send)
		break;
	else
		usleep(50000);
  }
	
  while(!channel->sendthread_shutdown) {
	  pthread_mutex_lock(&channel->ring_buf->lock);
	  uint64_t now_time=get_time();
	  uint64_t send_time = channel->ring_buf->time_send[channel->ring_buf->read_send_idx];
	  int to_descramble = channel->ring_buf->to_descramble;
	  int to_send = channel->ring_buf->to_send;
	  pthread_mutex_unlock(&channel->ring_buf->lock);

	  if (to_send == 0) {
	    log_message( log_module, MSG_ERROR, "thread starved, channel %s %u %u\n",channel->name,to_descramble,to_send); 
	    usleep(50000);
	    continue;
	  }

	  if (now_time < send_time) {
		  res_time=send_time - now_time;
		  r_time.tv_sec=res_time/(1000000ull);
		  r_time.tv_nsec=1000*(res_time%(1000000ull));
		  while(nanosleep(&r_time, &r_time));
	  }

	  pthread_mutex_lock(&channel->ring_buf->lock);
	  memcpy(channel->buf + channel->nb_bytes, channel->ring_buf->data[channel->ring_buf->read_send_idx], TS_PACKET_SIZE);

	  ++channel->ring_buf->read_send_idx;
	  channel->ring_buf->read_send_idx&=(channel->ring_buffer_size -1);

	  channel->nb_bytes += TS_PACKET_SIZE;

	  --channel->ring_buf->to_send;
	  ++channel->num_packet_descrambled_sent;
	  pthread_mutex_unlock(&channel->ring_buf->lock);

          //The buffer is full, we send it
	  if ((!multicast_vars.rtp_header && ((channel->nb_bytes + TS_PACKET_SIZE) > MAX_UDP_SIZE))
	    ||(multicast_vars.rtp_header && ((channel->nb_bytes + RTP_HEADER_LEN + TS_PACKET_SIZE) > MAX_UDP_SIZE)))
          {
			  send_func(channel, send_time, &unicast_vars, &multicast_vars, &chan_and_pids, &fds);
          }
  }
  return 0;
}





void scam_send_start(mumudvb_channel_t *channel)
{
  pthread_attr_t attr;
  struct sched_param param;
  size_t stacksize;
  stacksize = sizeof(mumudvb_channel_t)+sizeof(uint64_t)+sizeof(struct timespec)+50000;
  
  pthread_attr_init(&attr);
  pthread_attr_setschedpolicy(&attr, SCHED_RR);
  param.sched_priority = sched_get_priority_max(SCHED_RR);
  pthread_attr_setschedparam(&attr, &param);
  pthread_attr_setstacksize (&attr, stacksize);

  pthread_create(&(channel->sendthread), &attr, sendthread_func, channel);
  log_message(log_module, MSG_DEBUG,"Send thread started, channel %s\n",channel->name);
  pthread_attr_destroy(&attr);
}

void scam_send_stop(mumudvb_channel_t *channel)
{
    log_message(log_module,MSG_DEBUG,"Send Thread closing, channel %s\n", channel->name);
	channel->sendthread_shutdown=1;
	pthread_join(channel->sendthread,NULL);
	log_message(log_module,MSG_DEBUG,"Send Thread closed, channel %s\n", channel->name);
}
