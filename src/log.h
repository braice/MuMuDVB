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



#define LOGGING_UNDEFINED       0
#define LOGGING_CONSOLE         1
#define LOGGING_SYSLOG          2
#define LOGGING_FILE            4

#define DEFAULT_LOG_HEADER "%priority:  %module "

typedef struct log_params_t{
  /** the verbosity level for log messages */
  int verbosity;
  /**say if we log to the console, syslog*/
  int log_type;
  /** Says if syslog is initialised */
  int syslog_initialised;
  /**Say if the logging file could be rotated*/
  int rotating_log_file;
  /** The logging file */
  FILE *log_file;
  /** The logging file path */
  char *log_file_path;
  /** The header with templates for the log messages*/
  char *log_header;
  /**  Flushing interval */
  float log_flush_interval;
}log_params_t;

typedef struct flag_descr_t
{
	int num;
	char descr[128];
}flag_descr_t;



void init_stats_v(stats_infos_t *stats_p);
void print_info ();
void usage (char *name);
void log_message( char* log_module, int , const char *, ... ) __attribute__ ((format (printf, 3, 4)));
void gen_file_streamed_channels (char *nom_fich_chaines_diff, char *nom_fich_chaines_non_diff, int nb_flux, mumudvb_channel_t *channels);
void log_streamed_channels(char *log_module,int number_of_channels, mumudvb_channel_t *channels, int multicast_ipv4, int multicast_ipv6, int unicast, int unicast_master_port, char *unicastipOut);
char *ca_sys_id_to_str(int id);
void display_service_type(int type, int loglevel,char *log_module);
char *pid_type_to_str(int type);
char *service_type_to_str(int type);
char *simple_service_type_to_str(int type);
void show_traffic(char *log_module, double now, int show_traffic_interval, mumu_chan_p_t *chan_p);
char *liben50221_error_to_str(int error);
char *liben50221_error_to_str_descr(int error);
void log_pids(char *log_module, mumudvb_channel_t *channel, int curr_channel);
int read_logging_configuration(stats_infos_t *stats_infos, char *substring);
void sync_logs();
char *running_status_to_str(int running_status);
int convert_en300468_string(char *string, int max_len);
void show_CA_identifier_descriptor(unsigned char *buf);
char *ready_f_to_str(chan_status_t flag);
#endif
