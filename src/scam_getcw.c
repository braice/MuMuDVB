/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for software descrambling connection with oscam
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

#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>


#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <sys/epoll.h>

#include "errors.h"
#include "ts.h"
#include "mumudvb.h"
#include "log.h"
#include "scam_common.h"

#include <dvbcsa/dvbcsa.h>


#define DVBAPI_OPCODE_LEN        5
#define DVBAPI_CA_SET_DESCR_LEN  21
#define DVBAPI_CA_SET_PID_LEN    13

/**@file
 * @brief scam support
 * 
 * Code for getting cw's from oscam
 */

static void *getcwthread_func(void* arg); //The polling thread
static char *log_module="SCAM_GETCW: ";

typedef struct getcw_params_t{
	scam_parameters_t *scam_params;
	mumu_chan_p_t *chan_p;
}getcw_params_t;

/** @brief start the thread for getting cw's from oscam
 * This function will create the communication layers*/
int scam_getcw_start(scam_parameters_t *scam_params, mumu_chan_p_t *chan_p)
{
  getcw_params_t *getcw_params=malloc(sizeof(getcw_params_t));
  getcw_params->scam_params=scam_params;
  getcw_params->chan_p=chan_p;
  pthread_create(&(scam_params->getcwthread), NULL, getcwthread_func, getcw_params);
  log_message(log_module, MSG_DEBUG,"Getcw thread started\n");
  return 0;
  
}

/** @brief stop the thread for getting cw's from oscam **/
void scam_getcw_stop(scam_parameters_t *scam_params)
{
  log_message( log_module,  MSG_DEBUG,  "Getcw thread stopping\n");

  // shutdown the getcw thread
  scam_params->getcwthread_shutdown = 1;
  pthread_cancel(scam_params->getcwthread);
  log_message( log_module,  MSG_DEBUG,  "Getcw thread stopped\n");
}

/** @brief The thread function for getting cw's from oscam */
static void *getcwthread_func(void* arg)
{
  struct getcw_params_t *getcw_params;
  getcw_params= (struct getcw_params_t *) arg;
  scam_parameters_t *scam_params;
  mumu_chan_p_t *chan_p;
  scam_params=getcw_params->scam_params;
  chan_p=getcw_params->chan_p;
  int curr_channel = 0;
  unsigned char buff[1 + sizeof(int) + sizeof(ca_descr_t)];
  int cRead, *request;
  struct epoll_event events[MAX_CHANNELS];
  int num_of_events;
  int i;

  //Loop
  while(!scam_params->getcwthread_shutdown) {
    num_of_events = epoll_wait (scam_params->epfd, events, MAX_CHANNELS, -1);
    if (num_of_events < 0) {
      set_interrupted(ERROR_NETWORK<<8);
      break;
    }
    pthread_mutex_lock(&chan_p->lock);
    for (i = 0; i < num_of_events; i++) {
      for (curr_channel = 0; curr_channel < chan_p->number_of_channels; curr_channel++) {
        mumudvb_channel_t *channel = &chan_p->channels[curr_channel];
        if (events[i].data.fd == channel->camd_socket) {
          if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP) {
            log_message(log_module, MSG_INFO,"channel %s socket not alive, will try to reconnect\n", channel->name);
            int s = epoll_ctl(scam_params->epfd, EPOLL_CTL_DEL, channel->camd_socket, &events[i]);
            if (s == -1)
            {
              log_message(log_module, MSG_ERROR,"channel %s: unsuccessful epoll_ctl EPOLL_CTL_DEL", channel->name);
              set_interrupted(ERROR_NETWORK<<8);
              free(getcw_params);
              pthread_mutex_unlock(&chan_p->lock);
              return 0;
            }
            close(channel->camd_socket);
            channel->camd_socket=-1;
            channel->need_scam_ask=CAM_NEED_ASK;
            pthread_mutex_lock(&channel->cw_lock);
            channel->ca_idx_refcnt = 0;
            channel->ca_idx = 0;
            pthread_mutex_unlock(&channel->cw_lock);
          } else {
            cRead = recv(channel->camd_socket, &buff, DVBAPI_OPCODE_LEN, 0);
            if (cRead <= 0) {
              log_message(log_module, MSG_ERROR,"channel: %s recv", channel->name);
              set_interrupted(ERROR_NETWORK<<8);
              free(getcw_params);
              pthread_mutex_unlock(&chan_p->lock);
              return 0;
            }
            request = (int *) (buff + 1);
            if (*request == CA_SET_DESCR)
            {
                //read upt to DVBAPI_CA_SET_DESCR_LEN
                cRead = recv(channel->camd_socket, (buff + DVBAPI_OPCODE_LEN), (DVBAPI_CA_SET_DESCR_LEN - DVBAPI_OPCODE_LEN), 0);
                if (cRead != (DVBAPI_CA_SET_DESCR_LEN - DVBAPI_OPCODE_LEN))
                    *request = 0;
            }
            else if (*request == CA_SET_PID)
            {
                //read upt to DVBAPI_CA_SET_PID_LEN
                cRead = recv(channel->camd_socket, (buff + DVBAPI_OPCODE_LEN), (DVBAPI_CA_SET_PID_LEN - DVBAPI_OPCODE_LEN), 0);
                if (cRead != (DVBAPI_CA_SET_PID_LEN - DVBAPI_OPCODE_LEN))
                    *request = 0;
            }
            if (*request == CA_SET_DESCR) {
              memcpy((&(scam_params->ca_descr)), buff + 1 + sizeof(int), sizeof(ca_descr_t));
              log_message( log_module,  MSG_DEBUG, "Got CA_SET_DESCR request for channel: %s, index: %d, parity %d, key %02x %02x %02x %02x %02x %02x %02x %02x\n", channel->name, scam_params->ca_descr.index, scam_params->ca_descr.parity, scam_params->ca_descr.cw[0], scam_params->ca_descr.cw[1], scam_params->ca_descr.cw[2], scam_params->ca_descr.cw[3], scam_params->ca_descr.cw[4], scam_params->ca_descr.cw[5], scam_params->ca_descr.cw[6], scam_params->ca_descr.cw[7]);
              if(scam_params->ca_descr.index != (unsigned) -1) {
                pthread_mutex_lock(&channel->cw_lock);
                if (scam_params->ca_descr.parity) {
                  memcpy(channel->odd_cw,scam_params->ca_descr.cw,8);
                  channel->got_key_odd=1;
                }
                else {
                  memcpy(channel->even_cw,scam_params->ca_descr.cw,8);
                  channel->got_key_even=1;
                }
                pthread_mutex_unlock(&channel->cw_lock);
              } else {
                log_message( log_module,  MSG_DEBUG, "Got CA_SET_DESCR removal request, ignoring");
              }
            }
            if (*request == CA_SET_PID)
            {
              memcpy((&(scam_params->ca_pid)), buff + 1 + sizeof(int), sizeof(ca_pid_t));
              log_message( log_module,  MSG_DEBUG, "Got CA_SET_PID request channel: %s, index: %d pid: %d\n", channel->name, scam_params->ca_pid.index, scam_params->ca_pid.pid);
              if(scam_params->ca_pid.index == -1) {
                pthread_mutex_lock(&channel->cw_lock);
                if (channel->ca_idx_refcnt) --channel->ca_idx_refcnt;
                if (!channel->ca_idx_refcnt) {
                  channel->ca_idx = 0;
                  log_message( log_module,  MSG_INFO, "Got CA_SET_PID removal request: %d setting channel %s with ca_idx %d to 0\n", scam_params->ca_pid.pid, channel->name, scam_params->ca_pid.index+1);
                }
                pthread_mutex_unlock(&channel->cw_lock);
              } else {
                pthread_mutex_lock(&channel->cw_lock);
                if(scam_params->ca_pid.index != -1 && !channel->ca_idx_refcnt) {
                  channel->ca_idx = scam_params->ca_pid.index+1;
                  log_message( log_module,  MSG_INFO, "Got CA_SET_PID with pid: %d setting channel %s ca_idx %d\n", scam_params->ca_pid.pid, channel->name, scam_params->ca_pid.index+1);
                }
                ++channel->ca_idx_refcnt;
                pthread_mutex_unlock(&channel->cw_lock);
              }
            }
          }
          break;
        }
      }
    }
    pthread_mutex_unlock(&chan_p->lock);
  }
  free(getcw_params);
  return 0;
}
