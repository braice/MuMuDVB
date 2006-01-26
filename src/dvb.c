/* 
dvb.h dvb part (except tune) of mumudvb
mumudvb - UDP-ize a DVB transport stream.
(C) Dave Chapman <dave@dchapman.com> 2001, 2002.
Modified By Brice DUBOST
 * 
The latest version can be found at http://mumudvb.braice.net

Copyright notice:

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
    
*/

#include "dvb.h"


char *frontenddev[6] =
  { "/dev/dvb/adapter0/frontend0", "/dev/dvb/adapter1/frontend0",
  "/dev/dvb/adapter2/frontend0", "/dev/dvb/adapter3/frontend0",
  "/dev/dvb/adapter4/frontend0", "/dev/dvb/adapter5/frontend0"
};

char *demuxdev[6] = { "/dev/dvb/adapter0/demux0", "/dev/dvb/adapter1/demux0",
  "/dev/dvb/adapter2/demux0", "/dev/dvb/adapter3/demux0",
  "/dev/dvb/adapter4/demux0", "/dev/dvb/adapter5/demux0"
};

char *dvrdev[6] = { "/dev/dvb/adapter0/dvr0", "/dev/dvb/adapter1/dvr0",
  "/dev/dvb/adapter2/dvr0", "/dev/dvb/adapter3/dvr0",
  "/dev/dvb/adapter4/dvr0", "/dev/dvb/adapter5/dvr0"
};



int
open_fe (int *fd_frontend, int card)
{
  if ((*fd_frontend = open (frontenddev[card], O_RDWR | O_NONBLOCK)) < 0)
    {
      perror ("FRONTEND DEVICE: ");
      return -1;
    }
  return 1;
}



void
set_ts_filt (int fd, uint16_t pid, dmx_pes_type_t pestype)
{
  struct dmx_pes_filter_params pesFilterParams;

  fprintf (stderr, "Setting filter for PID %d\n", pid);
  pesFilterParams.pid = pid;
  pesFilterParams.input = DMX_IN_FRONTEND;
  pesFilterParams.output = DMX_OUT_TS_TAP;
  pesFilterParams.pes_type = pestype;
  pesFilterParams.flags = DMX_IMMEDIATE_START;

  if (ioctl (fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0)
    {
      fprintf (stderr, "FILTER %i: ", pid);
      perror ("DMX SET PES FILTER");
    }
}


void
affiche_puissance (fds_t fds, int no_daemon)
{
  int strength, ber, snr;
  strength = ber = snr = 0;
  if (ioctl (fds.fd_frontend, FE_READ_BER, &ber) >= 0)
    if (ioctl (fds.fd_frontend, FE_READ_SIGNAL_STRENGTH, &strength) >= 0)
      if (ioctl (fds.fd_frontend, FE_READ_SNR, &snr) >= 0)
	{
	  if(!no_daemon)
	    syslog(LOG_USER, "Bit error rate: %10d Signal strength: %10d SNR: %10d\n", ber,strength,snr);
	  else
	    fprintf (stderr, "Bit error rate: %10d Signal strength: %10d SNR: %10d\n", ber,strength,snr);
	}
}

int
create_card_fd(int card, int nb_flux, int *num_pids, fds_t *fds)
{

  int i=0;
  int j=0;
  int curr_pid_mandatory = 0;

  for(curr_pid_mandatory=0;curr_pid_mandatory<MAX_MANDATORY;curr_pid_mandatory++)
    if ((fds->fd_mandatory[curr_pid_mandatory] = open (demuxdev[card], O_RDWR)) < 0)	//file descriptors for the mandatory pids
    {
      fprintf (stderr, "FD Mandatory %i: ", curr_pid_mandatory);
      perror ("DEMUX DEVICE: ");
      return -1;
    }

  for (i = 0; i < nb_flux; i++)
    {
      for(j=0;j<num_pids[i];j++)
	{
	  if ((fds->fd[i][j] = open (demuxdev[card], O_RDWR)) < 0)
	    {
	      fprintf (stderr, "FD %i: ", i);
	      perror ("DEMUX DEVICE: ");
	    return -1;
	    }
	}
    }
  if ((fds->fd_dvr = open (dvrdev[card], O_RDONLY | O_NONBLOCK)) < 0)
    {
      perror ("DVR DEVICE: ");
      return -1;
    }


  return 0;

}


void
close_card_fd(int card, int nb_flux, int *num_pids, fds_t fds)
{
  int i=0;
  int j=0;
  int curr_pid_mandatory = 0;

  for(curr_pid_mandatory=0;curr_pid_mandatory<MAX_MANDATORY;curr_pid_mandatory++)
    close(fds.fd_mandatory[curr_pid_mandatory]);

  for (i = 0; i < nb_flux; i++)
    {
      for(j=0;j<num_pids[i];j++)
	close (fds.fd[i][j]);
    }
  close (fds.fd_dvr);

}
