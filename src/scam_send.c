/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for software descrambling sending part
 * 
 * (C) 2004-2010 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.net
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

  int pid;			/** pid of the current mpeg2 packet */
  int ScramblingControl;
  int curr_pid = 0;
  int send_packet = 0;

  extern int dont_send_scrambled;

  scam_sendthread_p_t *params;
  params = ((scam_sendthread_p_t *) arg);
  mumudvb_channel_t *channel;
  channel = params->channel;
  unicast_parameters_t *unicast_vars;
  unicast_vars=params->unicast_vars;

  uint64_t res_time;
  struct timespec r_time;
  int first_run = 1;
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
      if (first_run) {
        first_run = 0;
        log_message( log_module, MSG_DEBUG, "first run waiting");
      } else
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

    pid = ((*(channel->ring_buf->data+TS_PACKET_SIZE*channel->ring_buf->read_send_idx+1) & 0x1f) << 8) | *(channel->ring_buf->data+TS_PACKET_SIZE*channel->ring_buf->read_send_idx+2);
    ScramblingControl = (*(channel->ring_buf->data+TS_PACKET_SIZE*channel->ring_buf->read_send_idx+3) & 0xc0) >> 6;

    pthread_mutex_lock(&channel->stats_lock);
    for (curr_pid = 0; (curr_pid < channel->pid_i.num_pids); curr_pid++)
      if ((channel->pid_i.pids[curr_pid] == pid) || (channel->pid_i.pids[curr_pid] == 8192)) //We can stream whole transponder using 8192
      {
        if ((ScramblingControl>0) && (pid != channel->pid_i.pmt_pid) )
           channel->num_scrambled_packets++;

         //check if the PID is scrambled for determining its state
         if (ScramblingControl>0) channel->pid_i.pids_num_scrambled_packets[curr_pid]++;

           //we don't count the PMT pid for up channels
         if (pid != channel->pid_i.pmt_pid)
             channel->num_packet++;
         break;
      }
    pthread_mutex_unlock(&channel->stats_lock);
    //avoid sending of scrambled channels if we asked to
    send_packet=1;
    if(dont_send_scrambled && (ScramblingControl>0)&& (channel->pid_i.pmt_pid) )
      send_packet=0;

    if (send_packet) {
      // we fill the channel buffer
      memcpy(channel->buf + channel->nb_bytes, channel->ring_buf->data+TS_PACKET_SIZE*channel->ring_buf->read_send_idx, TS_PACKET_SIZE);
      channel->nb_bytes += TS_PACKET_SIZE;
    }
    ++channel->ring_buf->read_send_idx;
    channel->ring_buf->read_send_idx&=(channel->ring_buffer_size -1);

    --channel->ring_buf->to_send;
    pthread_mutex_unlock(&channel->ring_buf->lock);

    //The buffer is full, we send it
    if ((!channel->rtp && ((channel->nb_bytes + TS_PACKET_SIZE) > MAX_UDP_SIZE))
      ||(channel->rtp && ((channel->nb_bytes + RTP_HEADER_LEN + TS_PACKET_SIZE) > MAX_UDP_SIZE)))
    {
      send_func(channel, send_time, unicast_vars);
    }
  }
  free(arg);
  return 0;
}





void scam_send_start(mumudvb_channel_t *channel, unicast_parameters_t *unicast_vars)
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
  scam_sendthread_p_t *thread_params = malloc(sizeof(scam_sendthread_p_t));
  thread_params->channel=channel;
  thread_params->unicast_vars=unicast_vars;
  pthread_create(&(channel->sendthread), &attr, sendthread_func, thread_params);
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
