/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) Brice DUBOST
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

#ifndef _DVB_H
#define _DVB_H

#include <syslog.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <resolv.h>


// DVB includes:
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include "mumudvb.h"

//file descriptors
//TODO : define here poll file descriptors
typedef struct {
  int fd_dvr;
  int fd_frontend;
  int fd[MAX_CHANNELS][MAX_PIDS_PAR_CHAINE];
  int fd_mandatory[MAX_MANDATORY_PID_NUMBER];
}fds_t;


int open_fe (int *fd_frontend, int card);
void set_ts_filt (int fd,uint16_t pid, dmx_pes_type_t pestype);
void affiche_puissance (fds_t fds);
int create_card_fd(int card, int nb_flux, mumudvb_channel_t *channels, int *mandatory_pid, fds_t *fds);
int complete_card_fds(int card, int nb_flux, mumudvb_channel_t *channels, fds_t *fds, int pmt_fd_opened);
void close_card_fd(int nb_flux, mumudvb_channel_t *channels, fds_t fds);
#endif
