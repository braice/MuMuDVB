/* 
 * MuMuDVB -Stream a DVB transport stream.
 * 
 * (C) 2008-2010 Brice DUBOST
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
#include "unicast_http.h"
#include "ts.h"
#include "tune.h"

/**find the audio and video pids from the PMT*/
#define AUTOCONF_MODE_PIDS 1
/**find the pmt pids and the channels from the pat, and go to AUTOCONF_MODE_PIDS*/
#define AUTOCONF_MODE_FULL 2
/**parse the NIT*/
#define AUTOCONF_MODE_NIT 3

//timeout for autoconfiguration
#define AUTOCONFIGURE_TIME 10

/**@brief chained list of services for autoconfiguration
 *
*/
typedef struct mumudvb_service_t{
  /**The channel name*/
  char name[MAX_NAME_LEN];
  /**Is the channel running ? Not used for the moment*/
  int running_status; 
  /**The service type : television, radio, data, ...*/
  int type; 
  /**The PMT pid of the service*/
  int pmt_pid; 
  /**The program ID, also called program number in the PAT or in ATSC*/
  int id;
  /**Tell if this service is scrambled*/
  int free_ca_mode;
  /**The next service in the chained list*/
  struct mumudvb_service_t *next;
}mumudvb_service_t;

/**@brief The different parameters used for autoconfiguration*/
typedef struct autoconf_parameters_t{
  /**Do we use autoconfiguration ?

Possible values for this variable

 0 : none (or autoconf finished)

 AUTOCONF_MODE_PIDS : we have the PMT pids and the channels, we search the audio and video

 AUTOCONF_MODE_FULL : we have only the tuning parameters, we search the channels and their pmt pids*/
  int autoconfiguration;
  /**do we autoconfigure the radios ?*/
  int autoconf_radios;
  /** The beginning of autoconfigured multicast addresses*/
  char autoconf_ip_header[10];
  /**When did we started autoconfiguration ?*/
  long time_start_autoconfiguration; 
  /**The transport stream id (used to read ATSC PSIP tables)*/
  int transport_stream_id;
  /** Do we autoconfigure scrambled channels ? */
  int autoconf_scrambled;
  /** Do we follow pmt changes*/
  int autoconf_pid_update;
  /**Do we search the logical channel number */
  int autoconf_lcn;
  //Different packets used by autoconfiguration
  mumudvb_ts_packet_t *autoconf_temp_pat;
  mumudvb_ts_packet_t *autoconf_temp_sdt;
  mumudvb_ts_packet_t *autoconf_temp_nit;
  /**For ATSC Program and System Information Protocol*/
  mumudvb_ts_packet_t *autoconf_temp_psip;
  mumudvb_service_t   *services;

  /**the starting http unicast port */
  int autoconf_unicast_start_port;

  /**the list of TS ID for full autoconfiguration*/
  int ts_id_list[MAX_CHANNELS];
  /**number of TS ID*/
  int num_ts_id;
  /** the template for the channel name*/
  char name_template[MAX_NAME_LEN];

}autoconf_parameters_t;



int autoconf_init(autoconf_parameters_t *autoconf_vars, mumudvb_channel_t *channels,int number_of_channels);
void autoconf_freeing(autoconf_parameters_t *);
int read_autoconfiguration_configuration(autoconf_parameters_t *autoconf_vars, char *substring);
int autoconf_new_packet(int pid, unsigned char *ts_packet, autoconf_parameters_t *autoconf_vars, fds_t *fds, mumudvb_chan_and_pids_t *chan_and_pids, tuning_parameters_t *tuneparams, multicast_parameters_t *multicast_vars,  unicast_parameters_t *unicast_vars);
int autoconf_poll(long now, autoconf_parameters_t *autoconf_vars, mumudvb_chan_and_pids_t *chan_and_pids, tuning_parameters_t *tuneparams, multicast_parameters_t *multicast_vars, fds_t *fds, unicast_parameters_t *unicast_vars);
void autoconf_pmt_follow( unsigned char *ts_packet, fds_t *fds, mumudvb_channel_t *actual_channel, char *card_base_path,mumudvb_chan_and_pids_t *chan_and_pids );

#endif
