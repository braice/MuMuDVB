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

#include <pthread.h>

#include "mumudvb.h"
#include "unicast_http.h"
#include "ts.h"
#include "tune.h"

/**No autoconfiguration, only send specified PIDs */
#define AUTOCONF_MODE_NONE 0
/**find everything */
#define AUTOCONF_MODE_FULL 2
/**parse the NIT*/
#define AUTOCONF_MODE_NIT 4

//timeout for autoconfiguration
#define AUTOCONFIGURE_TIME 10

/**@brief chained list of services for autoconfiguration
 *
 */
typedef struct mumudvb_service_t{
	/**The channel name*/
	char name[MAX_NAME_LEN];
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
typedef struct auto_p_t{
	/**Do we use autoconfiguration ?
Possible values for this variable
 0 : none
 AUTOCONF_MODE_FULL : we have only the tuning parameters, we search the channels and their pmt pids*/
	int autoconfiguration;
	/**do we autoconfigure the radios ?*/
	int autoconf_radios;
	/** The template of autoconfigured multicast addresses*/
	char autoconf_ip4[80];
	char autoconf_ip6[80];
	/**When did we started autoconfiguration ?*/
	long time_start_autoconfiguration;
	/**The transport stream id (used to read ATSC PSIP tables and for webservices)*/
	int transport_stream_id;
	/**The network id (used for webservices)*/
	int network_id;
	int original_network_id;
	/** Do we autoconfigure scrambled channels ? */
	int autoconf_scrambled;
	//Different packets used by autoconfiguration
	mumudvb_ts_packet_t *autoconf_temp_pat;
	int pat_need_update;
	int pat_version;
	int pat_sections_seen[256];
	int pat_all_sections_seen;
	mumudvb_ts_packet_t *autoconf_temp_sdt;
	int sdt_need_update;
	int sdt_version;
	int sdt_all_sections_seen;
	int sdt_sections_seen[256];
	mumudvb_ts_packet_t *autoconf_temp_nit;
	int nit_need_update;
	int nit_version;
	int nit_all_sections_seen;
	int nit_sections_seen[256];
	/**For ATSC Program and System Information Protocol*/
	mumudvb_ts_packet_t *autoconf_temp_psip;
	int psip_need_update;
	int psip_version;
	int psip_all_sections_seen;
	int psip_sections_seen[256];
	//Tell if we added or removed PIDs and we need to update some card filters or file descriptors
	//We also update the filters and the chan
	int need_filter_chan_update;

	/**the http unicast port (string with %card %number, * and + ) */
	char autoconf_unicast_port[256];
	/**the multicast port (string with %card %number, * and + ) */
	char autoconf_multicast_port[256];

	/**the list of SID for full autoconfiguration*/
	int service_id_list[MAX_CHANNELS];
	/**number of SID*/
	int num_service_id;

	/**the list of ignored SID for full autoconfiguration*/
	int service_id_list_ignore[MAX_CHANNELS];
	/**number of SID*/
	int num_service_id_ignore;

	/** the template for the channel name*/
	char name_template[MAX_NAME_LEN];

}auto_p_t;


void init_aconf_v(auto_p_t *aconf_p);
int autoconf_init(auto_p_t *auto_p);
void autoconf_freeing(auto_p_t *);
int read_autoconfiguration_configuration(auto_p_t *auto_p, char *substring);
int autoconf_new_packet(int pid, unsigned char *ts_packet, auto_p_t *auto_p, fds_t *fds, mumu_chan_p_t *chan_p, tune_p_t *tune_p, multi_p_t *multi_p,  unicast_parameters_t *unicast_vars, int server_id, void *scam_vars);
int autoconf_poll(long now, auto_p_t *auto_p, mumu_chan_p_t *chan_p, tune_p_t *tune_p, multi_p_t *multi_p, fds_t *fds, unicast_parameters_t *unicast_vars, int server_id, void *scam_vars);
void autoconf_pmt_follow( unsigned char *ts_packet, fds_t *fds, mumudvb_channel_t *actual_channel, char *card_base_path, int tuner, mumu_chan_p_t *chan_p );

#endif
