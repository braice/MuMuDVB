/*
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 *
 * (C) 2004-2010 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.net/
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

#include <stdio.h>
#ifndef _WIN32
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <resolv.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "config.h"

#ifndef DISABLE_DVB_API
 // DVB includes:
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#endif

#include "mumudvb.h"
#include "tune.h"

#define DVB_DEV_PATH "/dev/dvb/adapter%card"
#define FRONTEND_DEV_NAME "frontend"
#define DEMUX_DEV_NAME    "demux"
#define DVR_DEV_NAME      "dvr"

//The timeout for DVB polling, must exist otherwise the program would block without data on the card
#define DVB_POLL_TIMEOUT 100

enum
{
	PID_NOT_ASKED=0,
	PID_ASKED,
	PID_FILTERED,
};


/** The parameters for the thread for showing the strength */
typedef struct strength_parameters_t{
	tune_p_t *tune_p;
	fds_t *fds;
	fe_status_t festatus;
	int strength, ber, snr, ub;
	int ts_discontinuities;
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
	volatile int threadshutdown;
	//The buffer for the card
	card_buffer_t *card_buffer;
	//
	int thread_running;
	/** Is main waiting ?*/
	int main_waiting;
}card_thread_parameters_t;

void *read_card_thread_func(void* arg);



int open_fe (int *fd_frontend, char *base_path, int tuner, int rw, int full_path);
void set_ts_filt (int fd,uint16_t pid);
int create_card_fd(char *base_path, int tuner, uint8_t *asked_pid, fds_t *fds);
void set_filters(uint8_t *asked_pid, fds_t *fds);
void close_card_fd(fds_t *fds);

void *show_power_func(void* arg);
int card_read(int fd_dvr, unsigned char *dest_buffer, card_buffer_t *card_buffer);

void list_dvb_cards(void);
#endif
