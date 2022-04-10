/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Header file for software descrambling common functions
 * 
 * (C) 2004-2010 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.net/
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


#ifndef _SCAM_COMMON_H
#define _SCAM_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/dvb/ca.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/time.h>


#include "mumudvb.h"
#include "log.h"
#include "unicast_http.h"
#include "rtp.h"
#include "autoconf.h"

/**@file
 * @brief scam support
 * 
 * Header file for code used by other software descrambling files
 */

#define RING_BUFFER_DEFAULT_SIZE   32768

#define DECSA_DEFAULT_DELAY 500000
#define SEND_DEFAULT_DELAY 1500000

#define MAX_STATIC_KEYS 24

//Quick hack around the removal of ca_pid_t and CA_GET_PID in recent kernels
//https://github.com/torvalds/linux/commit/833ff5e7feda1a042b83e82208cef3d212ca0ef1
#ifndef CA_SET_PID
typedef struct ca_pid {
	unsigned int pid;
	int index;      /* -1 == disable*/
	} ca_pid_t;
// In older kernels < 4.14, CA_SET_PID was _IOW('o', 135, ca_pid_t) really.
// Oscam defines it as a fixed integer value in module-dvbapi.h file
// under DVBAPI_CA_SET_PID macro.
#define CA_SET_PID 0x40086F87
#endif


/** @brief the parameters for the scam
 * This structure contain the parameters needed for the SCAM
 */
typedef struct scam_parameters_t{
  int scam_support;
  pthread_t getcwthread;
  int getcwthread_shutdown;
  ca_descr_t ca_descr;
  ca_pid_t ca_pid;
  uint64_t ring_buffer_default_size,decsa_default_delay,send_default_delay;
  int epfd;
  /* biss key */
  unsigned char const_key_odd[MAX_STATIC_KEYS][8];
  unsigned char const_key_even[MAX_STATIC_KEYS][8];
  int const_sid[MAX_STATIC_KEYS];
  int const_key_count; 
}scam_parameters_t;  

//The structure for argument passing to sendthread_func
typedef struct scam_sendthread_p_t{
	mumudvb_channel_t *channel;
	unicast_parameters_t *unicast_vars;
}scam_sendthread_p_t;



int scam_init_no_autoconf(scam_parameters_t *scam_vars, mumudvb_channel_t *channels, int number_of_channels);
void scam_new_packet(int pid, mumudvb_channel_t *channels);
int read_scam_configuration(scam_parameters_t *scam_vars, mumudvb_channel_t *c_chan, char *substring);
int scam_channel_start(mumudvb_channel_t *channel, unicast_parameters_t *unicast_vars);
void scam_channel_stop(mumudvb_channel_t *channel);




#endif
