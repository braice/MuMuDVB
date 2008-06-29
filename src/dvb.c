/* 
 * dvb.h dvb part (except tune) of mumudvb
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) Brice DUBOST
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

  char *frontend_name;
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
set_ts_filt (int fd, uint16_t pid, dmx_pes_type_t pestype)
{
  struct dmx_pes_filter_params pesFilterParams;

  log_message( MSG_DETAIL, "Setting filter for PID %d\n", pid);
  pesFilterParams.pid = pid;
  pesFilterParams.input = DMX_IN_FRONTEND;
  pesFilterParams.output = DMX_OUT_TS_TAP;
  pesFilterParams.pes_type = pestype;
  pesFilterParams.flags = DMX_IMMEDIATE_START;

  if (ioctl (fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0)
    {
      log_message( MSG_ERROR, "FILTER %i: ", pid);
      perror ("DMX SET PES FILTER");
    }
}


void
affiche_puissance (fds_t fds)
{
  int strength, ber, snr;
  strength = ber = snr = 0;
  if (ioctl (fds.fd_frontend, FE_READ_BER, &ber) >= 0)
    if (ioctl (fds.fd_frontend, FE_READ_SIGNAL_STRENGTH, &strength) >= 0)
      if (ioctl (fds.fd_frontend, FE_READ_SNR, &snr) >= 0)
	log_message( MSG_INFO, "Bit error rate: %10d Signal strength: %10d SNR: %10d\n", ber,strength,snr);
}

int
create_card_fd(int card, int nb_flux, mumudvb_channel_t *channels, int *mandatory_pid, fds_t *fds)
{

  int i=0;
  int j=0;
  int curr_pid_mandatory = 0;
  char *demuxdev_name;
  char *dvrdev_name;
  asprintf(&demuxdev_name,DEMUX_DEV_PATH,card);

  for(curr_pid_mandatory=0;curr_pid_mandatory<MAX_MANDATORY;curr_pid_mandatory++)
    //file descriptors for the mandatory pids
    //we check if we need to open the file descriptor (some cards are limited)
    if ((mandatory_pid[curr_pid_mandatory] != 0)&& ((fds->fd_mandatory[curr_pid_mandatory] = open (demuxdev_name, O_RDWR)) < 0) )	
      {
	log_message( MSG_ERROR, "FD Mandatory %i: ", curr_pid_mandatory);
	perror ("DEMUX DEVICE: ");
	free(demuxdev_name);
	return -1;
      }

  for (i = 0; i < nb_flux; i++)
    {
      for(j=0;j<channels[i].num_pids;j++)
	{
	  if ((fds->fd[i][j] = open (demuxdev_name, O_RDWR)) < 0)
	    {
	      log_message( MSG_ERROR, "FD %i: ", i);
	      perror ("DEMUX DEVICE: ");
	      free(demuxdev_name);
	      return -1;
	    }
	}
    }
  asprintf(&dvrdev_name,DVR_DEV_PATH,card);
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


int complete_card_fds(int card, int nb_flux, mumudvb_channel_t *channels, fds_t *fds, int autoconf)
{
  //this function open the descriptors for the new pids found by autoconfiguration
  int i=0;
  int j=0;
  int start=0;

  if(autoconf==1)
    start=1;
  if(autoconf==2) //full autoconf we didn't opened the PMT
    start=0;

  char *demuxdev_name;
  asprintf(&demuxdev_name,DEMUX_DEV_PATH,card);

  for (i = 0; i < nb_flux; i++)
    {
      for(j=start;j<channels[i].num_pids;j++) //the 1 is important, because when we call this function, it's after autoconf and we'va already opened a file descriptor for the first (pmt) pid
	{
	  if ((fds->fd[i][j] = open (demuxdev_name, O_RDWR)) < 0)
	    {
	      log_message( MSG_ERROR, "FD %i: ", i);
	      perror ("DEMUX DEVICE: ");
	      free(demuxdev_name);
	      return -1;
	    }
	}
    }
  free(demuxdev_name);
  return 0;

}

void
close_card_fd(int card, int nb_flux, mumudvb_channel_t *channels, int *mandatory_pid, fds_t fds)
{
  int i=0;
  int j=0;
  int curr_pid_mandatory = 0;

  for(curr_pid_mandatory=0;curr_pid_mandatory<MAX_MANDATORY;curr_pid_mandatory++)
    {
      //      if(mandatory_pid[curr_pid_mandatory] != 0)
	close(fds.fd_mandatory[curr_pid_mandatory]);
    }

  for (i = 0; i < nb_flux; i++)
    {
      for(j=0;j<channels[i].num_pids;j++)
	close (fds.fd[i][j]);
    }
  close (fds.fd_dvr);
  close (fds.fd_frontend);

}
