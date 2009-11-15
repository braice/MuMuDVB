/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2009 Brice DUBOST
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


#ifndef _DVB_H
#define _DVB_H

#include <syslog.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <resolv.h>
#include <sys/poll.h>

// DVB includes:
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include "mumudvb.h"
#include "tune.h"

#define FRONTEND_DEV_PATH "/dev/dvb/adapter%d/frontend0"
#define DEMUX_DEV_PATH    "/dev/dvb/adapter%d/demux0"
#define DVR_DEV_PATH      "/dev/dvb/adapter%d/dvr0"

enum
{
  PID_NOT_ASKED=0,
  PID_ASKED,
  PID_FILTERED,
};

#ifdef HAVE_LIBPTHREAD

/** The parameters for the thread for showing the strength */
typedef struct strength_parameters_t{
  tuning_parameters_t *tuneparams;
  fds_t *fds;
}strength_parameters_t;

/** The parameters for the thread for reading the data from the card */
typedef struct card_thread_parameters_t{
  //mutex for the data buffer
  pthread_mutex_t carddatamutex;
  //Condition variable for locking the main program in order to wait for new data
  pthread_cond_t threadcond;
  //file descriptors
  fds_t *fds;
  //The shutdown for the thread
  int threadshutdown;
  //The buffer for the card
  card_buffer_t *card_buffer;
  //
  int thread_running;
}card_thread_parameters_t;

void *read_card_thread_func(void* arg);


#endif


int open_fe (int *fd_frontend, int card);
void set_ts_filt (int fd,uint16_t pid);
int create_card_fd(int card, uint8_t *asked_pid, fds_t *fds);
void set_filters(uint8_t *asked_pid, fds_t *fds);
void close_card_fd(fds_t fds);

void *show_power_func(void* arg);
#endif
