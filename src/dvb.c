/* dvb.c
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) 2004-2009 Brice DUBOST
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

/** @file
 * @brief dvb part (except tune) of mumudvb
 * Ie : setting the filters, openning the file descriptors etc...
 */

#define _GNU_SOURCE
#include "dvb.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h> 
#include <string.h> 

/**
 * @brief Open the frontend associated with card
 * Return 1 in case of succes, -1 otherwise
 * 
 * @param fd_frontend the file descriptor for the frontend 
 * @param card the card number 
*/
int
open_fe (int *fd_frontend, int card)
{

  char *frontend_name=NULL;
  int asprintf_ret;
  asprintf_ret=asprintf(&frontend_name,FRONTEND_DEV_PATH,card);
  if(asprintf_ret==-1)
    return -1;
  if ((*fd_frontend = open (frontend_name, O_RDWR | O_NONBLOCK)) < 0)
    {
      log_message( MSG_ERROR, "FRONTEND DEVICE: %s : %s\n", frontend_name, strerror(errno));
      free(frontend_name);
      return -1;
    }
  free(frontend_name);
  return 1;
}


/**
 * @brief Set a filter of the pid asked. The file descriptor has to be
 * opened before. Ie it will ask the card for this PID.
 * @param fd the file descriptor
 * @param pid the pid for the filter
 */
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
      log_message( MSG_ERROR, "DMX SET PES FILTER : %s\n", strerror(errno));
    }
}


/**
 * @brief Show the reception power.
 * This information is not alway reliable
 *
 * @param fds the file descriptors of the card
 */
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
 * @brief Open file descriptors for the card. open dvr and one demuxer fd per asked pid. This function can be called 
 * more than one time if new pids are added (typical case autoconf)
 * return -1 in case of error
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
  int asprintf_ret;

  asprintf_ret=asprintf(&demuxdev_name,DEMUX_DEV_PATH,card);
  if(asprintf_ret==-1)
    return -1;

  for(curr_pid=0;curr_pid<8192;curr_pid++)
    //file descriptors for the demuxer (used to set the filters)
    //we check if we need to open the file descriptor (some cards are limited)
    if ((asked_pid[curr_pid] != 0)&& (fds->fd_demuxer[curr_pid]==0) )
      if((fds->fd_demuxer[curr_pid] = open (demuxdev_name, O_RDWR)) < 0)
	{
	  log_message( MSG_ERROR, "FD PID %i: ", curr_pid);
	  log_message( MSG_ERROR, "DEMUX DEVICE: %s : %s\n", demuxdev_name, strerror(errno));
	  free(demuxdev_name);
	  return -1;
	}


  asprintf_ret=asprintf(&dvrdev_name,DVR_DEV_PATH,card);
  if(asprintf_ret==-1)
    return -1;
  if (fds->fd_dvr==0)  //this function can be called more than one time, we check if we opened it before
    if ((fds->fd_dvr = open (dvrdev_name, O_RDONLY | O_NONBLOCK)) < 0)
      {
	log_message( MSG_ERROR, "DVR DEVICE: %s : %s\n", dvrdev_name, strerror(errno));
	free(dvrdev_name);
	return -1;
      }


  free(dvrdev_name);
  free(demuxdev_name);
  return 0;

}


/**
 * @brief Open filters for the pids in asked_pid. This function update the asked_pid array and 
 * can be called more than one time if new pids are added (typical case autoconf)
 * Ie it asks the card for the pid list by calling set_ts_filt
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
 * @brief Close the file descriptors associated with the card
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
