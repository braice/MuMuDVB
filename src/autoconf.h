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

/**@file
 * @brief Autoconfiguration structures
 */

#ifndef _AUTOCONF_H
#define _AUTOCONF_H

#include "mumudvb.h"
#include "ts.h"

/**find the audio and video pids from the PMT*/
#define AUTOCONF_MODE_PIDS 1
/**find the pmt pids and the channels from the pat, and go to AUTOCONF_MODE_PIDS*/
#define AUTOCONF_MODE_FULL 2

//timeout for autoconfiguration
#define AUTOCONFIGURE_TIME 10

/**Flag for memory freeing*/
#define DONT_FREE_PMT 0x01

/**@brief chained list of services for autoconfiguration
 *
 * @name - The channel name
 * @running_status - Is the channel running ? Not used for the moment
 * @type - The service type : television, radio, data, ...
 * @pmt_pit - The PMT pid of the service
 * @id - The program ID, also called program number in the PAT or in ATSC
 * @free_ca_mode - Tell if this service is scrambled
 * @next - The next service in the chained list
*/
typedef struct mumudvb_service_t{
  char name[MAX_NAME_LEN];  //the channel name
  int running_status;
  int type;
  int pmt_pid;
  int id;
  int free_ca_mode;
  struct mumudvb_service_t *next;
}mumudvb_service_t;

/**@brief The different parameters used for autoconfiguration*/
typedef struct autoconf_parameters_t{
  /**Do we use autoconfiguration ?

Possible values for this variable

 0 : none (or autoconf finished)

 1 or AUTOCONF_MODE_PIDS : we have the PMT pids and the channels, we search the audio and video

 2 or AUTOCONF_MODE_FULL : we have only the tuning parameters, we search the channels and their pmt pids*/
  int autoconfiguration;
  /**do we autoconfigure the radios ?*/
  int autoconf_radios;
  /** The beginning of autoconfigured multicast adresses*/
  char autoconf_ip_header[10];
  /**When did we started autoconfiguration ?*/
  long time_start_autoconfiguration; 
  /**The transport stream id (used to read ATSC PSIP tables)*/
  int transport_stream_id;
  /** Do we autoconfigure scrambled channels ? */
  int autoconf_scrambled;
  
  //Different packets used by autoconfiguration
  mumudvb_ts_packet_t *autoconf_temp_pmt;
  mumudvb_ts_packet_t *autoconf_temp_pat;
  mumudvb_ts_packet_t *autoconf_temp_sdt;
  /**For ATSC Program and System Information Protocol*/
  mumudvb_ts_packet_t *autoconf_temp_psip; /**@todo : see if it's really necesarry to split it from the sdt*/
  mumudvb_service_t   *services;

}autoconf_parameters_t;



int autoconf_read_pmt(mumudvb_ts_packet_t *pmt, mumudvb_channel_t *channel);
int autoconf_read_sdt(unsigned char *buf, int len, mumudvb_service_t *services);
int autoconf_read_psip(autoconf_parameters_t *);
void autoconf_freeing(autoconf_parameters_t *, int);
int autoconf_read_pat(autoconf_parameters_t *);
int services_to_channels(autoconf_parameters_t parameters, mumudvb_channel_t *channels, int port, int card);
int autoconf_finish_full(int *number_of_channels, mumudvb_channel_t *channels, autoconf_parameters_t *autoconf_vars, int common_port, int card, fds_t *fds,uint8_t *asked_pid, int multicast_ttl);
void autoconf_end(int card, int number_of_channels, mumudvb_channel_t *channels, uint8_t *asked_pid, fds_t *fds);
void autoconf_free_services(mumudvb_service_t *services);

#endif
