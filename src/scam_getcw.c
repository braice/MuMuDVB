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
  extern int Interrupted; 
  unsigned char buff[sizeof(int) + sizeof(ca_descr_t)];
  int cRead, *request;

  static char got_pid_t[2^13];
  char got_pid=0;
	
  //Loop
  while(!scam_params->getcwthread_shutdown) {
	if(scam_params->bint){
		cRead = read(scam_params->net_socket_fd, &buff, sizeof(buff));
		if (cRead <= 0)
		  break;
		request = (int *) &buff;
		if (*request == CA_SET_DESCR) {
			memcpy((&(scam_params->ca_descr)), &buff[sizeof(int)], sizeof(ca_descr_t));
			log_message( log_module,  MSG_DEBUG, "Got CA_SET_DESCR request index: %d, parity %d\n", scam_params->ca_descr.index, scam_params->ca_descr.parity);
			if(scam_params->ca_descr.index != (unsigned) -1) {

				if (chan_and_pids.started_pid_get[scam_params->ca_descr.index]) {
				  log_message( log_module,  MSG_DEBUG, "Got CA_SET_DESCR request for channel %s : index %d, parity %d, key %02x %02x %02x %02x  %02x %02x %02x %02x\n", chan_and_pids.scam_idx[scam_params->ca_descr.index]->name, scam_params->ca_descr.index, scam_params->ca_descr.parity, scam_params->ca_descr.cw[0], scam_params->ca_descr.cw[1], scam_params->ca_descr.cw[2], scam_params->ca_descr.cw[3], scam_params->ca_descr.cw[4], scam_params->ca_descr.cw[5], scam_params->ca_descr.cw[6], scam_params->ca_descr.cw[7]);
				  if (scam_params->ca_descr.parity) {
					memcpy(chan_and_pids.scam_idx[scam_params->ca_descr.index]->odd_cw,scam_params->ca_descr.cw,8);
					chan_and_pids.scam_idx[scam_params->ca_descr.index]->got_key_odd=1;					

				  }
				  else {
					memcpy(chan_and_pids.scam_idx[scam_params->ca_descr.index]->even_cw,scam_params->ca_descr.cw,8);		  
		      		chan_and_pids.scam_idx[scam_params->ca_descr.index]->got_key_even=1;	
		  		  }
			  	  chan_and_pids.scam_idx[scam_params->ca_descr.index]->got_cw_started=1;
				}
				else
					log_message( log_module,  MSG_DEBUG, "Got CA_SET_DESCR with index %d, but didn't get first pid", scam_params->ca_descr.index);					
			}
		    else {
			  log_message( log_module,  MSG_DEBUG, "Got CA_SET_DESCR removal request, ignoring");
		    }
		}
		if (*request == CA_SET_PID)
		{
			memcpy((&(scam_params->ca_pid)), &buff[sizeof(int)], sizeof(ca_pid_t));
			log_message( log_module,  MSG_DEBUG, "Got CA_SET_PID request index: %d pid: %d\n",scam_params->ca_pid.index, scam_params->ca_pid.pid);
			got_pid=0;
			if (!got_pid_t[scam_params->ca_pid.pid]) {
				got_pid_t[scam_params->ca_pid.pid]=1;
				if(scam_params->ca_pid.index != -1) {
					if (!(chan_and_pids.started_pid_get[scam_params->ca_pid.index])) {
					  for (curr_channel = 0; curr_channel < chan_and_pids.number_of_channels  && !got_pid; curr_channel++) {
						for (curr_pid = 1; curr_pid < chan_and_pids.channels[curr_channel].num_pids && !got_pid; curr_pid++) {
				  			if (scam_params->ca_pid.pid == (unsigned int)chan_and_pids.channels[curr_channel].pids[curr_pid]) {
				  				chan_and_pids.started_pid_get[scam_params->ca_pid.index] = 1;
				  				chan_and_pids.scam_idx[scam_params->ca_pid.index] = &chan_and_pids.channels[curr_channel];
								log_message( log_module,  MSG_DEBUG, "Got first CA_SET_PID request for channel: %s pid: %d\n",chan_and_pids.scam_idx[scam_params->ca_pid.index]->name, scam_params->ca_pid.pid);  
								Interrupted=scam_channel_start(&chan_and_pids.channels[curr_channel]);
								got_pid=1;
								break;
							}
						}
					  }

					}
					else
			    		log_message( log_module,  MSG_DEBUG, "Got CA_SET_PID request for channel: %s pid: %d\n",chan_and_pids.scam_idx[scam_params->ca_pid.index]->name, scam_params->ca_pid.pid);
				}
				else {
				  log_message( log_module,  MSG_DEBUG, "Got CA_SET_PID removal request, ignoring, pid: %d\n", scam_params->ca_pid.pid);
				}
		    }
			else {
			  log_message( log_module,  MSG_DEBUG, "Got CA_SET_PID with pid: %d that has been already seen\n", scam_params->ca_pid.pid);
			}
			  
		}
	}
  }
  return 0;
}