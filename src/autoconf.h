/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) 2008-2009 Brice DUBOST
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
 */


#ifndef _AUTOCONF_H
#define _AUTOCONF_H

#include "mumudvb.h"
#include "ts.h"

//find the audio and video pids from the PMT
#define AUTOCONF_MODE_PIDS 1
//find the pmt pids and the channels from the pat, and go to AUTOCONF_MODE_PIDS
#define AUTOCONF_MODE_FULL 2

//chained list of services
//for autoconfiguration
typedef struct mumudvb_service_t{
  char name[MAX_NAME_LEN];  //the channel name

  int running_status;
  int type;
  int pmt_pid;
  int id;
  int free_ca_mode;
  struct mumudvb_service_t *next;
}mumudvb_service_t;

//The different parameters used for autoconfiguration
typedef struct autoconf_parameters_t{
//Do we use autoconfiguration ?
//Possible values for this variable
// 0 : none (or autoconf finished)
// 1 : we have the PMT pids and the channels, we search the audio and video
// 2 : we have only the tuning parameters, we search the channels and their pmt pids
  int autoconfiguration;
  char autoconf_ip_header[10];
  long time_start_autoconfiguration; //When did we started autoconfiguration ?

  //Different packets used by autoconfiguration
  mumudvb_ts_packet_t *autoconf_temp_pmt;
  mumudvb_ts_packet_t *autoconf_temp_pat;
  mumudvb_ts_packet_t *autoconf_temp_sdt;
  mumudvb_service_t   *services;

}autoconf_parameters_t;


int autoconf_read_pmt(mumudvb_ts_packet_t *pmt, mumudvb_channel_t *channel);
int autoconf_read_sdt(unsigned char *buf, int len, mumudvb_service_t *services);
int autoconf_read_pat(mumudvb_ts_packet_t *pat, mumudvb_service_t *services);
int services_to_channels(mumudvb_service_t *services, mumudvb_channel_t *channels, int cam_support, int port, int card);
void autoconf_end(int card, int number_of_channels, mumudvb_channel_t *channels, uint8_t *asked_pid, fds_t *fds);
void autoconf_free_services(mumudvb_service_t *services);

#endif
