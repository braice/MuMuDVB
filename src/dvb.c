/* 
 * dvb.h dvb part (except tune) of mumudvb
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) 2004-2008 Brice DUBOST
 * (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
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

#define _GNU_SOURCE
#include "dvb.h"
#include <stdio.h>
#include <stdlib.h>

#define FRONTEND_DEV_PATH "/dev/dvb/adapter%d/frontend0"
#define DEMUX_DEV_PATH    "/dev/dvb/adapter%d/demux0"
#define DVR_DEV_PATH      "/dev/dvb/adapter%d/dvr0"

int
open_fe (int *fd_frontend, int card)
{

  char *frontend_name=NULL;
  asprintf(&frontend_name,FRONTEND_DEV_PATH,card);
  if ((*fd_frontend = open (frontend_name, O_RDWR | O_NONBLOCK)) < 0)
    {
      perror ("FRONTEND DEVICE: ");
      free(frontend_name);
      return -1;
    }
  free(frontend_name);
  return 1;
}



void
set_ts_filt (int fd, uint16_t pid)
{
  struct dmx_pes_filter_params pesFilterParams;

  log_message( MSG_DEBUG, "Setting filter for PID %d\n", pid);
  pesFilterParams.pid = pid;
  pesFilterParams.input = DMX_IN_FRONTEND;
  pesFilterParams.output = DMX_OUT_TS_TAP;
  pesFilterParams.pes_type = DMX_PES_OTHER;
  pesFilterParams.flags = DMX_IMMEDIATE_START;

  if (ioctl (fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0)
    {
      log_message( MSG_ERROR, "FILTER %i: ", pid);
      perror ("DMX SET PES FILTER");
    }
}


void
show_power (fds_t fds)
{
  int strength, ber, snr;
  strength = ber = snr = 0;
  if (ioctl (fds.fd_frontend, FE_READ_BER, &ber) >= 0)
    if (ioctl (fds.fd_frontend, FE_READ_SIGNAL_STRENGTH, &strength) >= 0)
      if (ioctl (fds.fd_frontend, FE_READ_SNR, &snr) >= 0)
	log_message( MSG_INFO, "Bit error rate: %10d Signal strength: %10d SNR: %10d\n", ber,strength,snr);
}

/**
 * Open file descriptors for the card. open dvr and one demuxer fd per asked pid. This function can be called 
 * more than one time if new pids are added (typical case autoconf)
 * @param card the card number
 * @param asked_pid the array of asked pids
 * @param fds the structure with the file descriptors
 */
int
create_card_fd(int card, uint8_t *asked_pid, fds_t *fds)
{

  int curr_pid = 0;
  char *demuxdev_name=NULL;
  char *dvrdev_name=NULL;
  asprintf(&demuxdev_name,DEMUX_DEV_PATH,card);

  for(curr_pid=0;curr_pid<8192;curr_pid++)
    //file descriptors for the demuxer (used to set the filters)
    //we check if we need to open the file descriptor (some cards are limited)
    if ((asked_pid[curr_pid] != 0)&& (fds->fd_demuxer[curr_pid]==0) )
      if((fds->fd_demuxer[curr_pid] = open (demuxdev_name, O_RDWR)) < 0)
	{
	  log_message( MSG_ERROR, "FD PID %i: ", curr_pid);
	  perror ("DEMUX DEVICE: ");
	  free(demuxdev_name);
	  return -1;
	}


  asprintf(&dvrdev_name,DVR_DEV_PATH,card);
  if (fds->fd_dvr==0)  //this function can be called more than one time, we check if we opened it before
    if ((fds->fd_dvr = open (dvrdev_name, O_RDONLY | O_NONBLOCK)) < 0)
      {
	perror ("DVR DEVICE: ");
	free(dvrdev_name);
	return -1;
      }


  free(dvrdev_name);
  free(demuxdev_name);
  return 0;

}


/**
 * Open filters for the pids in asked_pid. This function update the asked_pid array and 
 * can be called more than one time if new pids are added (typical case autoconf)
 * @param asked_pid the array of asked pids
 * @param fds the structure with the file descriptors
 */
void set_filters(uint8_t *asked_pid, fds_t *fds)
{

  int curr_pid = 0;

  for(curr_pid=0;curr_pid<8192;curr_pid++)
    if ((asked_pid[curr_pid] == PID_ASKED) )
      {
	set_ts_filt (fds->fd_demuxer[curr_pid], curr_pid);
	asked_pid[curr_pid] = PID_FILTERED;
      }

}


/**
 * Close the file descriptors associated with the card
 * @param fds the structure with the file descriptors
 */
void
close_card_fd(fds_t fds)
{
  int curr_pid = 0;

  for(curr_pid=0;curr_pid<8192;curr_pid++)
    {
	close(fds.fd_demuxer[curr_pid]);
    }

  close (fds.fd_dvr);
  close (fds.fd_frontend);

}
