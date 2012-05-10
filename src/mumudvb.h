/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2010 Brice DUBOST
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
#include <pthread.h>
#include <net/if.h>

#ifdef ENABLE_TRANSCODING
#include "transcode_common.h"
#endif

#define IPV6_CHAR_LEN 64

/*Do we support ATSC ?*/
#undef ATSC
#if defined(DVB_API_VERSION_MINOR)
#if DVB_API_VERSION == 3 && DVB_API_VERSION_MINOR >= 1
#define ATSC 1
#endif
#endif
#if DVB_API_VERSION > 3
#define ATSC 1
#endif

/**the number of pids by channel*/
#define MAX_PIDS_PAR_CHAINE     18

/**the maximum channel number*/
#define MAX_CHANNELS		128

/**Size of an MPEG2-TS packet*/
#define TS_PACKET_SIZE 188

/**Default Maximum Number of TS packets in the TS buffer*/
#define DEFAULT_TS_BUFFER_SIZE 20

/**Default Maximum Number of TS packets in the thread buffer*/
#define DEFAULT_THREAD_BUFFER_SIZE 5000

#define ALARM_TIME_TIMEOUT 60
#define ALARM_TIME_TIMEOUT_NO_DIFF 600


/** MTU 
    1500 bytes - ip header (12bytes) - TCP header (biggest between TCP and udp) 24  : 7 mpeg2-ts packet per ethernet frame

We cannot discover easily the MTU with unconnected UDP
      http://linuxgazette.net/149/melinte.html

7*188 plus margin
*/
#define MAX_UDP_SIZE 1320

/**the max mandatory pid number*/
#define MAX_MANDATORY_PID_NUMBER   32
/**config line length*/
#define CONF_LINELEN 	        512
#define MAX_NAME_LEN		256
#define CONFIG_FILE_SEPARATOR   " ="

/**Maximum number of polling tries (excepted EINTR)*/
#define MAX_POLL_TRIES		5

#define DEFAULT_PATH_LEN 256
/**The path for the auto generated config file*/
#define GEN_CONF_PATH "/var/run/mumudvb/mumudvb_generated_conf_card%d_tuner%d"
/**The path for the list of streamed channels*/
#define STREAMED_LIST_PATH "/var/run/mumudvb/channels_streamed_adapter%d_tuner%d"
/**The path for the list of *not* streamed channels*/
#define NOT_STREAMED_LIST_PATH "/var/run/mumudvb/channels_unstreamed_adapter%d_tuner%d"
/**The path for the cam_info*/
#define CAM_INFO_LIST_PATH "/var/run/mumudvb/caminfo_adapter%d_tuner%d"
/** The path for the pid file */
#define PIDFILE_PATH "/var/run/mumudvb/mumudvb_adapter%card_tuner%tuner.pid"

/**RTP header length*/
#define RTP_HEADER_LEN 12

#define SAP_GROUP_LENGTH 20



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
    CAM_NEED_UPDATE,
    CAM_ASKED
  };

/** Enum to tell if the option is set*/
typedef enum option_status {
  OPTION_UNDEFINED,
  OPTION_OFF,
  OPTION_ON
} option_status_t;


/** The different PID types*/
enum
{
  PID_UNKNOW=0,
  PID_PMT,
  PID_PCR,
  PID_VIDEO_MPEG1,
  PID_VIDEO_MPEG2,
  PID_VIDEO_MPEG4_ASP,
  PID_VIDEO_MPEG4_AVC,
  PID_AUDIO_MPEG1,
  PID_AUDIO_MPEG2,
  PID_AUDIO_AAC_LATM,
  PID_AUDIO_AAC_ADTS,
  PID_AUDIO_ATSC,
  PID_AUDIO_AC3,
  PID_AUDIO_EAC3,
  PID_AUDIO_DTS,
  PID_AUDIO_AAC,
  PID_EXTRA_VBIDATA,
  PID_EXTRA_VBITELETEXT,
  PID_EXTRA_TELETEXT,
  PID_EXTRA_SUBTITLE
};

/**@brief file descriptors*/
typedef struct {
  /** the dvb dvr*/
  int fd_dvr;
  /** the dvb frontend*/
  int fd_frontend;
  /** demuxer file descriptors */
  int fd_demuxer[8193];
  /** poll file descriptors */
  struct pollfd *pfds;	//  DVR device + unicast http clients
  int pfdsnum;
}fds_t;
#ifdef ENABLE_SCAM_SUPPORT
/**@brief Structure containing ring buffer*/
typedef struct {
  unsigned char ** data;
  uint64_t * time;

  uint64_t * time_decsa;
  unsigned int read_t2_idx;
  unsigned int to_descramble;
  unsigned int write_idx, read_idx, read_idx2, write_t_idx, read_t_idx;
  unsigned int to_send;
  unsigned int num_packets;
}ring_buffer_t;  
  #endif

/**@brief Structure containing the card buffers*/
typedef struct card_buffer_t{
  /** The two buffers (we put from the card with one, we read from the other one)*/
  unsigned char *buffer1,*buffer2;
  int actual_read_buffer;
  /**The pointer to the reading buffer*/
  unsigned char *reading_buffer;
  /**The pointer to the writing buffer*/
  unsigned char *writing_buffer;
  /** The maximum number of packets in the buffer from DVR*/
  int dvr_buffer_size;
  /** The position in the DVR buffer */
  int read_buff_pos;
  /** number of bytes actually read */
  int bytes_read;
  /** Do the read is made using a thread */
  int threaded_read;
  /** The thread data buffer */
  int bytes_in_write_buffer;
  int write_buffer_size; /** @todo put a size for each buffer*/
  /** The number of partial packets received*/
  int partial_packet_number;
  /** The number of overflow errors*/
  int overflow_number;
  /**The maximum size of the thread buffer (in packets)*/
  int max_thread_buffer_size;
}card_buffer_t;




struct unicast_client_t;
/** @brief Structure for storing channels
 *
 */
typedef struct mumudvb_channel_t{
  /** The logical channel number*/
  int logical_channel_number;
  /**Tell the total packet number (without pmt) for the scrambling ratio and up/down detection*/
  int num_packet;
  /**Tell the scrambled packet number (without pmt) for the scrambling ratio*/
  int num_scrambled_packets;
  /**tell if this channel is actually streamed*/
  int streamed_channel;
  /**Ratio of scrambled packet versus all packets*/
  int ratio_scrambled;


  /**Tell if at least one of the PID related to the chanel is scrambled*/
  int scrambled_channel;
  /**the channel name*/
  char name[MAX_NAME_LEN];

  /**the channel pids*/
  int pids[MAX_PIDS_PAR_CHAINE];
  /**the channel pids type (PMT, audio, video etc)*/
  int pids_type[MAX_PIDS_PAR_CHAINE];
  /**the channel pids language (ISO639 - 3 characters)*/
  char pids_language[MAX_PIDS_PAR_CHAINE][4];
  /**count the number of scrambled packets for the PID*/
  int pids_num_scrambled_packets[MAX_PIDS_PAR_CHAINE];
  /**tell if the PID is scrambled (1) or not (0)*/
  char pids_scrambled[MAX_PIDS_PAR_CHAINE];
  /**number of channel pids*/
  int num_pids;

  /** Channel Type (Radio, TV, etc) / service type*/
  int channel_type;
  /**Transport stream ID*/
  int service_id;
  /**pmt pid number*/
  int pmt_pid;
  /**Say if we need to ask this channel to the cam*/
  int need_cam_ask;
  /** When did we asked the channel to the CAM */
  long cam_asking_time;
  /**The ca system ids*/
  int ca_sys_id[32];
  /** The version of the pmt */
  int pmt_version;
  /** Do the pmt needs to be updated ? */
  int pmt_needs_update;
  /**The PMT packet*/
  mumudvb_ts_packet_t *pmt_packet;
#ifdef ENABLE_CAM_SUPPORT
  /** The PMT packet for CAM purposes*/
  mumudvb_ts_packet_t *cam_pmt_packet;
#endif
#ifdef ENABLE_SCAM_SUPPORT
  /**Tell the total packet number (without pmt) for the scrambling ratio and up/down detection*/
  int num_packet_descrambled_sent;
  /** The camd socket for SCAM*/
  int camd_socket;
  /**Say if we need to ask this channel to the oscam*/
  int need_scam_ask;
  /**Say if this channel should be descrambled using scam*/
  int oscam_support;

//  unsigned char odd_cw_w_idx,odd_cw_r_idx;
//  unsigned char even_cw_w_idx,even_cw_r_idx;  
  unsigned char odd_cw[8];
  unsigned char even_cw[8];
  unsigned char got_key_even,got_key_odd;
  pthread_mutex_t decsa_key_odd_mutex;
  pthread_cond_t  decsa_key_odd_cond;
  pthread_mutex_t decsa_key_even_mutex;
  pthread_cond_t  decsa_key_even_cond;

  pthread_t decsathread;
  int decsathread_shutdown;
  pthread_mutex_t decsa_mutex;
  pthread_cond_t  decsa_cond;
  unsigned char started_cw_get;
  unsigned char scrambling_control;
  //unsigned int decsa_delay;
  
  /**ring buffer for sending and software descrambling*/
  ring_buffer_t* ring_buf;

  pthread_t sendthread;
  int sendthread_shutdown;
  unsigned char clock_rbuf_idx;
  unsigned char started_sending;
  uint64_t ring_buffer_size,decsa_delay,send_delay,decsa_wait;
#endif
  


  /**the RTP header (just before the buffer so it can be sended together)*/
  unsigned char buf_with_rtp_header[RTP_HEADER_LEN];
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
  char ip4Out[20];
  /**The multicast port*/
  int portOut;
  /**The multicast output socket*/
  struct sockaddr_in sOut4;
  /**The multicast output socket*/
  int socketOut4;
  /**The ipv6 multicast ip address*/
  char ip6Out[IPV6_CHAR_LEN];
  /**The multicast output socket*/
  struct sockaddr_in6 sOut6;
  /**The multicast output socket*/
  int socketOut6;


  /**Unicast clients*/
  struct unicast_client_t *clients;
  /**Unicast port (listening socket per channel) */
  int unicast_port;
  /**Unicast listening socket*/
  struct sockaddr_in sIn;
  /**Unicast listening socket*/
  int socketIn;

  /**The sap playlist group*/
  char sap_group[SAP_GROUP_LENGTH];

  /**The generated pat to be sent*/
  unsigned char generated_pat[TS_PACKET_SIZE]; /**@todo: allocate dynamically*/
  /** The version of the generated pat */
  int generated_pat_version;
  /**The generated sdt to be sent*/
  unsigned char generated_sdt[TS_PACKET_SIZE]; /**@todo: allocate dynamically*/
  /** The version of the generated sdt */
  int generated_sdt_version;
  /** If there is no channel found, we skip sdt rewrite */
  int sdt_rewrite_skip;

  /** The occupied traffic (in kB/s) */
  float traffic;

  /** Are we dropping the current EIT packet for this channel*/ 
  int eit_dropping;
  /**The continuity counter for the EIT*/
  int eit_continuity_counter;


#ifdef ENABLE_TRANSCODING
  void *transcode_handle;
  struct transcode_options_t transcode_options;
#endif

}mumudvb_channel_t;

/**The parameters concerning the multicast*/
typedef struct multicast_parameters_t{
  /** Do we activate multicast ? */
  int multicast;
  /** Do we activate multicast ? */
  int multicast_ipv4;
  /** Do we activate multicast ? */
  int multicast_ipv6;
  /** Time to live of sent packets */
  int ttl;
  /** the default port*/
  int common_port;
  /** Does MuMuDVB have to join the created multicast groups ?*/
  int auto_join;
  /**Do we send the rtp header ? */
  int rtp_header;
  /** The interface for IPv4 */
  char iface4[IF_NAMESIZE+1];
  /** The interface for IPv6 */
  char iface6[IF_NAMESIZE+1];
  /** num mpeg packets in one sent packet */
  unsigned char num_pack;
}multicast_parameters_t;

/** No PSI tables filtering */
#define PSI_TABLES_FILTERING_NONE 0
/** Keep only PAT and CAT */
#define PSI_TABLES_FILTERING_PAT_CAT_ONLY 1
/** Keep only PAT */
#define PSI_TABLES_FILTERING_PAT_ONLY 2

/** structure containing the channels and the asked pids information*/
typedef struct mumudvb_chan_and_pids_t{
  /** The number of channels ... */
  int number_of_channels;
  /** Do we send scrambled packets ? */
  int dont_send_scrambled;
  /** Do we send packets with error bit set by decoder ? */
  int filter_transport_error;
  /** Do we do filtering to keep only PSI tables (without DVB tables) ? **/
  int psi_tables_filtering;
  /** The channels array */
  mumudvb_channel_t channels[MAX_CHANNELS];  /**@todo use realloc*/
//Asked pids //used for filtering
  /** this array contains the pids we want to filter,*/
  uint8_t asked_pid[8193];
  /** the number of channels who want this pid (used by autoconfiguration update)*/
  uint8_t number_chan_asked_pid[8193];
  /** The number of TS discontinuities per PID **/
  int16_t continuity_counter_pid[8193]; //on 16 bits for storing the initial -1
  uint8_t check_cc;
#ifdef ENABLE_SCAM_SUPPORT
  mumudvb_channel_t* send_capmt_idx[MAX_CHANNELS]; 
  mumudvb_channel_t* scam_idx[MAX_CHANNELS]; 
  uint8_t started_pid_get[MAX_CHANNELS]; 
#endif
}mumudvb_chan_and_pids_t;


typedef struct monitor_parameters_t{
  int threadshutdown;
  int wait_time;
  struct autoconf_parameters_t *autoconf_vars;
  struct sap_parameters_t *sap_vars;
  mumudvb_chan_and_pids_t *chan_and_pids;
  multicast_parameters_t *multicast_vars;
  struct unicast_parameters_t *unicast_vars;
  struct tuning_parameters_t *tuneparams;
  struct stats_infos_t *stats_infos;
  int server_id;
  char *filename_channels_not_streamed;
  char *filename_channels_streamed;
}monitor_parameters_t;




/** struct containing a string */
typedef struct mumu_string_t{
  char *string;
  int length; //string length (not including \0)
}mumu_string_t;

#define EMPTY_STRING {NULL,0}

int mumu_string_append(mumu_string_t *string, const char *psz_format, ...);
void mumu_free_string(mumu_string_t *string);


int mumudvb_poll(fds_t *fds);
char *mumu_string_replace(char *source, int *length, int can_realloc, char *toreplace, char *replacement);
int string_comput(char *string);
uint64_t get_time(void);


long int mumu_timing();

#endif





