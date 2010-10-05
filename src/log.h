/* 
 * MuMuDVB - UDP-ize a DVB transport stream.
 * 
 * (C) 2009 Brice DUBOST
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
 
#ifndef _LOG_H
#define _LOG_H

#include "mumudvb.h"

enum
  {
    MSG_ERROR=-2,
    MSG_WARN,
    MSG_INFO,
    MSG_DETAIL,
    MSG_DEBUG,
    MSG_FLOOD
  };

typedef struct stats_infos_t{
  //statistics for the big buffer
  /** */
  int stats_num_packets_received;
  /** */
  int stats_num_reads;
  /** */
  int show_buffer_stats;
  /** */
  double show_buffer_stats_time;
  /** How often we how the statistics about the DVR buffer*/
  int show_buffer_stats_interval;
  //statistics for the traffic
  /** do we periodically show the traffic ?*/
  int show_traffic;
  /** last time we show the traffic */
  long show_traffic_time;
  /** last time we computed the traffic */
  double compute_traffic_time;
  /** The interval for the traffic display */
  int show_traffic_interval;
  /** The interval for the traffic calculation */
  int compute_traffic_interval;
  /** The number of packets per second (PMT excluded) for going to the UP state */
  int up_threshold;
  /** The number of packets per second (PMT excluded) for going to the DOWN state */
  int down_threshold;
  /** Do we display the number of packets per second to debug up/down detection ? */
  int debug_updown;
}stats_infos_t;


typedef enum
  {
    LOGGING_UNDEFINED,
    LOGGING_CONSOLE,
    LOGGING_SYSLOG
  }log_type_t;

typedef struct log_params_t{
  /** the verbosity level for log messages */
  int verbosity;
  /**say if we log to the console, syslog*/
  log_type_t log_type;
}log_params_t;



void print_info ();
void usage (char *name);
void log_message( char* log_module, int , const char *, ... );
void gen_file_streamed_channels (char *nom_fich_chaines_diff, char *nom_fich_chaines_non_diff, int nb_flux, mumudvb_channel_t *channels);
void log_streamed_channels(char *log_module,int number_of_channels, mumudvb_channel_t *channels, int multicast, int unicast, int unicast_master_port, char *unicastipOut);
void gen_config_file_header(char *orig_conf_filename, char *saving_filename);
void gen_config_file(int number_of_channels, mumudvb_channel_t *channels, char *saving_filename);
char *ca_sys_id_to_str(int id);
void display_service_type(int type, int loglevel,char *log_module);
char *pid_type_to_str(int type);
char *service_type_to_str(int type);
char *simple_service_type_to_str(int type);
void show_traffic(char *log_module, double now, int show_traffic_interval, mumudvb_chan_and_pids_t *chan_and_pids);
char *liben50221_error_to_str(int error);
char *liben50221_error_to_str_descr(int error);
void log_pids(char *log_module, mumudvb_channel_t *channel, int curr_channel);
int read_logging_configuration(stats_infos_t *stats_infos, char *substring);

#endif
