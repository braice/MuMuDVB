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


#include "errors.h"
#include "ts.h"
#include "mumudvb.h"
#include "log.h"
#include "scam_common.h"

#include <dvbcsa/dvbcsa.h>

/**@file
 * @brief scam support
 * 
 * Code for getting cw's from oscam
 */

static void *getcwthread_func(void* arg); //The polling thread
static char *log_module="SCAM_GETCW: ";

/** @brief start the thread for getting cw's from oscam
 * This function will create the communication layers*/
int scam_getcw_start(scam_parameters_t *scam_params, int adapter_id)
{
  struct sockaddr_in socketAddr;
  memset(&socketAddr, 0, sizeof(struct sockaddr_in));
  const struct hostent *const hostaddr = gethostbyname("127.0.0.1");
  if (hostaddr)
  {
    unsigned int port;
    port = 9000 + adapter_id;
    socketAddr.sin_family = AF_INET;
    socketAddr.sin_port = htons(port);
    socketAddr.sin_addr.s_addr = ((struct in_addr *) hostaddr->h_addr)->s_addr;
    const struct protoent *const ptrp = getprotobyname("udp");
    if (ptrp)
    {
      scam_params->net_socket_fd = socket(PF_INET, SOCK_DGRAM, ptrp->p_proto);
      if (scam_params->net_socket_fd > 0)
      {
        scam_params->bint = (bind(scam_params->net_socket_fd, (struct sockaddr *) &socketAddr, sizeof(socketAddr)) >= 0);
        if (scam_params->bint >= 0)
          log_message( log_module,  MSG_DEBUG, "network socket bint\n");
        else {
          log_message( log_module,  MSG_ERROR, "cannot bint network socket\n");
          return 1;
        }
      }
    }
  }

  pthread_create(&(scam_params->getcwthread), NULL, getcwthread_func, scam_params);
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
  scam_parameters_t *scam_params;
  scam_params= (scam_parameters_t *) arg;
  extern mumudvb_chan_and_pids_t chan_and_pids; /** @todo ugly way to access channel data */
  int curr_channel = 0;
  int curr_pid = 0;
  unsigned char buff[sizeof(int) + sizeof(ca_descr_t)];
  int cRead, *request;

  //Loop
  while(!scam_params->getcwthread_shutdown) {
    if(scam_params->bint){
      cRead = recv(scam_params->net_socket_fd, &buff, sizeof(buff), 0);
      if (cRead <= 0)
        break;
      request = (int *) &buff;
      if (*request == CA_SET_DESCR) {
        memcpy((&(scam_params->ca_descr)), &buff[sizeof(int)], sizeof(ca_descr_t));
        log_message( log_module,  MSG_DEBUG, "Got CA_SET_DESCR request index: %d, parity %d, key %02x %02x %02x %02x %02x %02x %02x %02x\n", scam_params->ca_descr.index, scam_params->ca_descr.parity, scam_params->ca_descr.cw[0], scam_params->ca_descr.cw[1], scam_params->ca_descr.cw[2], scam_params->ca_descr.cw[3], scam_params->ca_descr.cw[4], scam_params->ca_descr.cw[5], scam_params->ca_descr.cw[6], scam_params->ca_descr.cw[7]);
        if(scam_params->ca_descr.index != (unsigned) -1) {
          pthread_mutex_lock(&chan_and_pids.lock);
          for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++) {
            mumudvb_channel_t *channel = &chan_and_pids.channels[curr_channel];
            pthread_mutex_lock(&channel->cw_lock);
            if (channel->ca_idx == scam_params->ca_descr.index + 1) {
              if (scam_params->ca_descr.parity) {
                memcpy(channel->odd_cw,scam_params->ca_descr.cw,8);
                channel->got_key_odd=1;
              }
              else {
                memcpy(channel->even_cw,scam_params->ca_descr.cw,8);
                channel->got_key_even=1;
              }
            }
            pthread_mutex_unlock(&channel->cw_lock);
          }
          pthread_mutex_unlock(&chan_and_pids.lock);
        } else {
          log_message( log_module,  MSG_DEBUG, "Got CA_SET_DESCR removal request, ignoring");
        }
      }
      if (*request == CA_SET_PID)
      {
        memcpy((&(scam_params->ca_pid)), &buff[sizeof(int)], sizeof(ca_pid_t));
        log_message( log_module,  MSG_DEBUG, "Got CA_SET_PID request index: %d pid: %d\n",scam_params->ca_pid.index, scam_params->ca_pid.pid);
        if(scam_params->ca_pid.index == -1) {
          log_message( log_module,  MSG_DEBUG, "Got CA_SET_PID removal request, removing pid: %d\n", scam_params->ca_pid.pid);
          pthread_mutex_lock(&chan_and_pids.lock);
          for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++) {
            mumudvb_channel_t *channel = &chan_and_pids.channels[curr_channel];
            for (curr_pid = 1; curr_pid < channel->num_pids; curr_pid++) {
              if ((channel->pids_type[curr_pid] != PID_EXTRA_VBIDATA) && (channel->pids_type[curr_pid] != PID_EXTRA_VBITELETEXT) && (channel->pids_type[curr_pid] != PID_EXTRA_TELETEXT) && (channel->pids_type[curr_pid] != PID_EXTRA_SUBTITLE) && (channel->pids[curr_pid] == (int) scam_params->ca_pid.pid)) {
                pthread_mutex_lock(&channel->cw_lock);
                --channel->ca_idx_refcnt;
                if (!channel->ca_idx_refcnt) {
                    channel->ca_idx = 0;
                    log_message( log_module,  MSG_DEBUG, "Got CA_SET_PID removal request: %d setting channel %s with ca_idx to 0 %d\n", scam_params->ca_pid.pid, channel->name, scam_params->ca_pid.index+1);
                }
                pthread_mutex_unlock(&channel->cw_lock);
                break;
              }
            }
          }
          pthread_mutex_unlock(&chan_and_pids.lock);
        } else {
          pthread_mutex_lock(&chan_and_pids.lock);
          for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++) {
            mumudvb_channel_t *channel = &chan_and_pids.channels[curr_channel];
            for (curr_pid = 1; curr_pid < channel->num_pids; curr_pid++) {
              if ((channel->pids_type[curr_pid] != PID_EXTRA_VBIDATA) && (channel->pids_type[curr_pid] != PID_EXTRA_VBITELETEXT) && (channel->pids_type[curr_pid] != PID_EXTRA_TELETEXT) && (channel->pids_type[curr_pid] != PID_EXTRA_SUBTITLE) && (channel->pids[curr_pid] == (int) scam_params->ca_pid.pid)) {
                pthread_mutex_lock(&channel->cw_lock);
                if(!channel->ca_idx_refcnt) {
                  channel->ca_idx = scam_params->ca_pid.index+1;
                  log_message( log_module,  MSG_DEBUG, "Got CA_SET_PID with pid: %d setting channel %s ca_idx %d\n", scam_params->ca_pid.pid, channel->name, scam_params->ca_pid.index+1);
                }
                ++channel->ca_idx_refcnt;
                pthread_mutex_unlock(&channel->cw_lock);
                break;
              }
            }
          }
          pthread_mutex_unlock(&chan_and_pids.lock);
        }
      }
    }
  }
  return 0;
}
