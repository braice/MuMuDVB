/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Header file for software descrambling common functions
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

#include "errors.h"
#include "ts.h"
#include "mumudvb.h"
#include "log.h"
#include "unicast_http.h"
#include "rtp.h"
#include "autoconf.h"

#define RING_BUFFER_DEFAULT_SIZE   65536

#define DECSA_DEFAULT_DELAY 4500000
#define SEND_DEFAULT_DELAY 5500000
#define DECSA_DEFAULT_WAIT 500000

typedef struct scam_parameters_t{
  int scam_support;
  int need_pmt_get;
  pthread_t scamthread;
  int scamthread_shutdown;
  int net_socket_fd;
  int bint;
  ca_descr_t ca_descr;
  ca_pid_t ca_pid;
  uint64_t ring_buffer_default_size,decsa_default_delay,send_default_delay,decsa_default_wait;
}scam_parameters_t;  

int scam_init(autoconf_parameters_t *autoconf_vars, scam_parameters_t *scam_vars, mumudvb_channel_t *channels, int number_of_channels);
int scam_new_packet(int pid, unsigned char *ts_packet, scam_parameters_t *scam_vars, mumudvb_channel_t *channels);
int read_scam_configuration(scam_parameters_t *scam_vars, mumudvb_channel_t *current_channel, int ip_ok, char *substring);
unsigned char ts_packet_get_payload_offset(unsigned char *);
int start_thread_with_priority(pthread_t* thread, void *(*start_routine)(void*), void* arg);
void *sendthread_func(void* arg); 



#endif