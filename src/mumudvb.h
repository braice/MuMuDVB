/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2009 Brice DUBOST
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
 *     
 */

/**@file
 * @brief Global parameters and structures
 */


#ifndef _MUMUDVB_H
#define _MUMUDVB_H

#include "network.h"  //for the sockaddr
#include "ts.h"
#include "config.h"

/**the number of pids by channel*/
#define MAX_PIDS_PAR_CHAINE     18

/**the maximum channel number*/
#define MAX_CHANNELS		128

/**Size of an MPEG2-TS packet*/
#define TS_PACKET_SIZE 188

/**Default Maximum Number of TS packets in the TS buffer*/
#define DEFAULT_TS_BUFFER_SIZE 1

/** How often (in seconds) to update the "now" variable*/
#define ALARM_TIME 2
#define ALARM_TIME_TIMEOUT 60
#define ALARM_TIME_TIMEOUT_NO_DIFF 600


/** MTU 
    1500 bytes - ip header (12bytes) - TCP header (biggest between TCP and udp) 24  : 7 mpeg2-ts packet per ethernet frame

We cannot discover easily the MTU with unconnected UDP
      http://linuxgazette.net/149/melinte.html
*/
#define MAX_UDP_SIZE 1464

/**the max mandatory pid number*/
#define MAX_MANDATORY_PID_NUMBER   32
/**config line length*/
#define CONF_LINELEN 	        512
#define MAX_NAME_LEN		256
#define CONFIG_FILE_SEPARATOR   " ="

/**Maximum number of polling tries (excepted EINTR)*/
#define MAX_POLL_TRIES		5

/**The path for the auto generated config file*/
#define GEN_CONF_PATH "/var/run/mumudvb/mumudvb_generated_conf_card%d"
/**The path for the list of streamed channels*/
#define STREAMED_LIST_PATH "/var/run/mumudvb/chaines_diffusees_carte%d"
/**The path for the list of *not* streamed channels*/
#define NOT_STREAMED_LIST_PATH "/var/run/mumudvb/chaines_non_diffusees_carte%d"
/**The path for the cam_info*/
#define CAM_INFO_LIST_PATH "/var/run/mumudvb/caminfo_carte%d"



enum
  {
    MSG_ERROR=-2,
    MSG_WARN,
    MSG_INFO,
    MSG_DETAIL,
    MSG_DEBUG
  };

enum
  {
    FULLY_UNSCRAMBLED=0,
    PARTIALLY_UNSCRAMBLED,
    HIGHLY_SCRAMBLED
  };

//for need_cam_ask
enum
  {
    CAM_NO_ASK=0,
    CAM_NEED_ASK,
    CAM_ASKED
  };

  /** The different PID types*/
enum
{
  PID_UNKNOW=0,
  PID_PMT,
  PID_PCR,
  PID_VIDEO,
  PID_VIDEO_MPEG4,
  PID_AUDIO,
  PID_AUDIO_AAC,
  PID_AUDIO_AC3,
  PID_AUDIO_EAC3,
  PID_AUDIO_DTS,
  PID_SUBTITLE,
  PID_TELETEXT,
};

/**@brief file descriptors*/
typedef struct {
  /** the dvb dvr*/
  int fd_dvr;
  /** the dvb frontend*/
  int fd_frontend;
  /** demuxer file descriptors */
  int fd_demuxer[8192];
  /** poll file descriptors */
  struct pollfd *pfds;	//  DVR device + unicast http clients
  int pfdsnum;
}fds_t;

struct unicast_client_t;

/** @brief Structure for storing channels
 *
 */
typedef struct mumudvb_channel_t{
  /**tell if this channel is actually streamed*/
  int streamed_channel;
  /**Tell the total packet number (without pmt) for the scrambling ratio*/
  int num_packet;
  /**tell if this channel is actually streamed (precedent test, to see if it's changed)*/
  int streamed_channel_old;
  /**Ratio of scrambled packet versus all packets*/
  int ratio_scrambled;

  /**Tell if at least one of the PID related to the chanel is scrambled*/
  int scrambled_channel;
  /** Old state to manage state change display*/
  int scrambled_channel_old;

  /**the channel name*/
  char name[MAX_NAME_LEN];

  /**the channel pids*/
  int pids[MAX_PIDS_PAR_CHAINE];
  /**the channel pids type (PMT, audio, video etc)*/
  int pids_type[MAX_PIDS_PAR_CHAINE];
  /**number of channel pids*/
  int num_pids;

  /** Channel Type (Radio, TV, etc) / service type*/
  int channel_type;
  /**Transport stream ID*/
  int ts_id;
  /**pmt pid number*/
  int pmt_pid;
  /**Say if we need to ask this channel to the cam*/
  int need_cam_ask;
  /**The ca system ids*/
  int ca_sys_id[32];
  /** The version of the pmt */
  int pmt_version;
  /** Do the pmt needs to be updated ? */
  int pmt_needs_update;
  /**The PMT packet*/
  mumudvb_ts_packet_t *pmt_packet;

  /**the buffer wich will be sent once it's full*/
  unsigned char buf[MAX_UDP_SIZE];
  /**number of bytes actually in the buffer*/
  int nb_bytes;
  /**The data sent to this channel*/
  long sent_data;

  /** The packet number for rtp*/
  int rtp_packet_num;

  /**is the channel autoconfigurated ?*/
  int autoconfigurated;

  /**The multicast ip address*/
  char ipOut[20];
  /**The multicast port*/
  int portOut;
  /**The multicast output socket*/
  struct sockaddr_in sOut;
  /**The multicast output socket*/
  int socketOut;

  /**Unicast clients*/
  struct unicast_client_t *clients;
  /**Unicast port (listening socket per channel) */
  int unicast_port;
  /**Unicast listening socket*/
  struct sockaddr_in sIn;
  /**Unicast listening socket*/
  int socketIn;

  /**The sap playlist group*/
  char sap_group[20];

  /**The generated pat to be sent*/
  unsigned char generated_pat[TS_PACKET_SIZE]; /**@todo: allocate dynamically*/
  /** The version of the generated pat */
  int generated_pat_version;

  /** The occupied traffic (in kB/s) */
  float traffic;

}mumudvb_channel_t;


//logging
void log_message( int , const char *, ... );
void gen_file_streamed_channels (char *nom_fich_chaines_diff, char *nom_fich_chaines_non_diff, int nb_flux, mumudvb_channel_t *channels);
void log_streamed_channels(int number_of_channels, mumudvb_channel_t *channels);

void gen_config_file_header(char *orig_conf_filename, char *saving_filename);
void gen_config_file(int number_of_channels, mumudvb_channel_t *channels, char *saving_filename);
void display_ca_sys_id(int id);
void display_service_type(int type, int loglevel);
int pid_type_to_str(char *dest,int type);
int service_type_to_str(char *dest, int type);

int mumudvb_close(int Interrupted);

#endif
