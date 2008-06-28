/* 
 * mumudvb - UDP-ize a DVB transport stream.
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
 */


#ifndef _AUTOCONF_H
#define _AUTOCONF_H

#include "mumudvb.h"

//autoconfiguration
//chained list of services
//for autoconfiguration
typedef struct mumudvb_service_t{
  char name[MAX_LEN_NOM];  //the channel name

  int running_status;
  int type;
  int pmt_pid;
  int id;
  int free_ca_mode;
  struct mumudvb_service_t *next;
}mumudvb_service_t;

int autoconf_read_pmt(mumudvb_ts_packet_t *pmt, mumudvb_channel_t *channel);
int autoconf_read_sdt(unsigned char *buf, int len, mumudvb_service_t *services);
int autoconf_read_pat(mumudvb_ts_packet_t *pat, mumudvb_service_t *services);
int services_to_channels(mumudvb_service_t *services, mumudvb_channel_t *channels, int cam_support, int port, int card);


#endif
