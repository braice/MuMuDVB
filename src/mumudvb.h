/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2010 Brice DUBOST
 * 
 * The latest version can be found at http://mumudvb.net/
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

#define MAX_FILENAME_LEN 256

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

/*Do we support DVBT2 ?*/
#undef DVBT2
#if defined(DVB_API_VERSION_MINOR)
#if DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 3
#define DVBT2 1
#endif
#endif
#if DVB_API_VERSION >= 6
#define DVBT2 1
#endif


/**the number of pids by channel*/
#define MAX_PIDS     128

/**the maximum channel number*/
#define MAX_CHANNELS		128

/**the maximum number of CA systems*/
#define MAX_CA_SYSTEMS		32

/**Size of an MPEG2-TS packet*/
#define TS_PACKET_SIZE 188
/**Null PID used for bandwith padding**/
#define TS_PADDING_PID 8191

#define TS_SYNC_BYTE 0x47

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
#define MAX_NAME_LEN			512
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

/** define bool for plain C code **/
#ifndef __cplusplus
    typedef enum { false = 0, true = !false } bool;
#endif

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

/** Enum to tell if a channel parameter is user set or autodetected, to avoid erasing of user set params*/
typedef enum mumu_f {
	F_UNDEF,
	F_USER,
	F_DETECTED
} mumu_f_t;


//Macro helpers to specify if a channel variable is UNDEF:USER_DEFINED:AUTODETECTED
#define MU_F_V(type, x) type x; mumu_f_t x##_f;
#define MU_F_T(x) mumu_f_t x##_f;
#define MU_F(x) x##_f




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
	PID_VIDEO_MPEG4_HEVC,
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
	PID_EXTRA_SUBTITLE,
	PID_EXTRA_APPLICATION_SIGNALLING,
	PID_ECM,
	PID_EMM
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
	struct pollfd *pfds;	//  DVR device
	int pfdsnum;
}fds_t;

#ifdef ENABLE_SCAM_SUPPORT
/**@brief Structure containing ring buffer*/
typedef struct {
	/** A mutex protecting all the other members. */
	pthread_mutex_t lock;
	/** Buffer with dvb packets*/
	unsigned char * data;
	/** Write index of buffer */
	unsigned int write_idx;
	/** Buffer with descrambling timestamps*/
	uint64_t * time_decsa;
	/** Number of packets left to descramble*/
	unsigned int to_descramble;
	/** Read index of buffer for descrambling thread */
	unsigned int read_decsa_idx;
	/** Buffer with sending timestamps*/
	uint64_t * time_send;
	/** Number of packets left to send*/
	unsigned int to_send;
	/** Read index of buffer for sending thread */
	unsigned int read_send_idx;
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
	/* t2-mi demux buffer */
	unsigned char *t2mi_buffer;
}card_buffer_t;


typedef struct pid_i_t{
	/* The flag for the PIDs*/
	mumu_f_t pid_f;
	/**the channel pids*/
	int pids[MAX_PIDS];
	/**the channel pids type (PMT, audio, video etc)*/
	int pids_type[MAX_PIDS];
	/**the channel pids language (ISO639 - 3 characters)*/
	char pids_language[MAX_PIDS][4];
	/**count the number of scrambled packets for the PID*/
	int pids_num_scrambled_packets[MAX_PIDS];
	/**tell if the PID is scrambled (1) or not (0)*/
	char pids_scrambled[MAX_PIDS];
	/**number of channel pids*/
	int num_pids;
	/**PMT PID number*/
	MU_F_V(int,pmt_pid)
	/**PCR PID number*/
	int pcr_pid;
}pid_i_t;

struct unicast_client_t;


//Channel status, for autoconfiguration
typedef enum chan_status {
	REMOVED=-3,			//Service removed from the PAT (we keep them in case they become up again)
	NO_STREAMING,		//Service not streaming, (eg bad service ID)
	NOT_READY,			//Service not ready, we just autodetected it
	ALMOST_READY,		//Service OK but network is down for example
	READY,				//Service OK and streaming (first flag >0)
	READY_EXISTING,		//Service OK, flag for detecting removed services
} chan_status_t;

/** @brief Structure for storing channels
 *
 * All members are protected by the global lock in chan_p, with the
 * following exceptions:
 *
 *  - The EIT variables, since they are only ever accessed from the main thread. 
 *  - buf/nb_bytes, since they are only ever accessed from one thread: SCAM_SEND
 *    if we are using scam, or the main thread otherwise.
 *  - the odd/even keys, since they have their own locking.
 */
typedef struct mumu_chan_t{

	/** Flag to say the channel is ready for streaming */
	chan_status_t channel_ready;


	/** Mutex for statistics counters. */
	pthread_mutex_t stats_lock;

	//TODO : structure stats
	/** The logical channel number*/
	int logical_channel_number;
	/**Tell the total packet number (without pmt) for the scrambling ratio and up/down detection*/
	int num_packet;
	/**Tell the scrambled packet number (without pmt) for the scrambling ratio*/
	int num_scrambled_packets;
	/**tell if this channel is actually streamed ie packets are going out*/
	int has_traffic;
	/**Ratio of scrambled packet versus all packets*/
	int ratio_scrambled;


	/**Tell if at least one of the PID related to the channel is scrambled*/
	int scrambled_channel;
	/**the channel name*/
	char user_name[MAX_NAME_LEN];
	char name[MAX_NAME_LEN];
	MU_F_T(name);
	char service_name[MAX_NAME_LEN];

	/* The PID information for this channel*/
	pid_i_t pid_i;

	/** The service Type from the SDT */
	int service_type;
	/**Transport stream ID*/
	MU_F_V(int,service_id);

	/**Say if we need to ask this channel to the cam*/
	MU_F_V(int,need_cam_ask);
	/** When did we asked the channel to the CAM */
	long cam_asking_time;
	/**The ca system ids*/
	int ca_sys_id[32];
	//CAM and softcam
	int free_ca_mode;

	/**The PMT packet*/
	mumudvb_ts_packet_t *pmt_packet;
	/** The version of the pmt */
	int pmt_version;
	/** Do the pmt needs to be updated ? */
	int pmt_need_update;
	/** Tells if the PMT was updated and autoconf nneds to read it */
	int autoconf_pmt_need_update;


	/** Say if we need to ask this channel to the oscam*/
	int need_scam_ask;
#ifdef ENABLE_SCAM_SUPPORT
	/**The PMT packet copy for scam purposes*/
	mumudvb_ts_packet_t *scam_pmt_packet;
	/** The camd socket for SCAM*/
	int camd_socket;
	/** Say if this channel should be descrambled using scam*/
	int scam_support;
	/** Say if we started the threads for oscam */
	int scam_support_started;
	/** Mutex for odd_cw and even_cw. */
	pthread_mutex_t cw_lock;
	/** Odd control word for descrambling */
	unsigned char odd_cw[8];
	/** Even control word for descrambling */
	unsigned char even_cw[8];

	/** Indicating if we have another odd cw for descrambling */
	unsigned char got_key_odd;
	/** Indicating if we have another even cw for descrambling */
	unsigned char got_key_even;

	unsigned int ca_idx;
	unsigned int ca_idx_refcnt;



	/** Thread for software descrambling */
	pthread_t decsathread;
	/** Descrambling thread shutdown control */
	int decsathread_shutdown;


	/**ring buffer for sending and software descrambling*/
	ring_buffer_t* ring_buf;
	/** Thread for sending packets software descrambled */
	pthread_t sendthread;
	/** Sending thread shutdown control */
	int sendthread_shutdown;
	/** Size of ring buffer */
	uint64_t ring_buffer_size;
	/** Delay of descrambling in us*/
	uint64_t decsa_delay;
	/** Delay of sending in us*/
	uint64_t send_delay;
	/** Says if we've got first cw for channel.
	 * NOTE: This is _not_ under cw_lock, but under the regular chan_p lock. */
	int got_cw_started;
#endif


	//Do we send with RTP
	int rtp;
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



	/**The multicast ip address*/
	char ip4Out[20];
	MU_F_T(ip4Out)
	/**The multicast port*/
	MU_F_V(int,portOut)
	/**The multicast output socket*/
	struct sockaddr_in sOut4;
	/**The multicast output socket*/
	int socketOut4;
	/**The ipv6 multicast ip address*/
	char ip6Out[IPV6_CHAR_LEN];
	MU_F_T(ip6Out)
	/**The multicast output socket*/
	struct sockaddr_in6 sOut6;
	/**The multicast output socket*/
	int socketOut6;


	/**Unicast clients*/
	struct unicast_client_t *clients;
	/**Count of unicast clients*/
	int num_clients;
	/**Unicast port (listening socket per channel) */
	MU_F_V(int,unicast_port)
	/**Unicast listening socket*/
	struct sockaddr_in sIn;
	/**Unicast listening socket*/
	int socketIn;

	/**The sap playlist group*/
	char sap_group[SAP_GROUP_LENGTH];
	MU_F_T(sap_group);
	//do we need to update the SAP announce (typically a name change)
	int sap_need_update;

	/**The generated PMT to be sent*/
	unsigned char generated_pmt[TS_PACKET_SIZE];
	/** Do we rewrite PMT for this channel? */
	int pmt_rewrite;
	/** PMT can span over multiple TS packets */
	int pmt_part_num;
	int pmt_part_count;
	unsigned char original_pmt[TS_PACKET_SIZE*10];
	int original_pmt_ready;
	/** The version of the generated pmt */
	int generated_pmt_version;
	/** The continuity counter for pmt packets */
	int pmt_continuity_counter;
	/**The generated pat to be sent*/
	unsigned char generated_pat[TS_PACKET_SIZE];
	/** The version of the generated pat */
	int generated_pat_version;
	/**The generated sdt to be sent*/
	unsigned char generated_sdt[TS_PACKET_SIZE];
	/** The version of the generated sdt */
	int generated_sdt_version;
	/** If there is no service id for the channel found, we skip sdt rewrite */
	int sdt_rewrite_skip;
	/** The version of the generated EIT */
	int eit_section_to_send;
	/** The table we are currently sending */
	uint8_t eit_table_id_to_send;
	/** the continuity counter for the EIT */
	int eit_cc;


	/** The occupied traffic (in kB/s) */
	float traffic;


}mumudvb_channel_t;

/**The parameters concerning the multicast*/
typedef struct multi_p_t{
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
}multi_p_t;

/** No PSI tables filtering */
#define PSI_TABLES_FILTERING_NONE 0
/** Keep only PAT and CAT */
#define PSI_TABLES_FILTERING_PAT_CAT_ONLY 1
/** Keep only PAT */
#define PSI_TABLES_FILTERING_PAT_ONLY 2

/** structure containing the channels and the asked pids information*/
typedef struct mumu_chan_p_t{
	/** Protects all the members, including most of the channels (see the documentation
	 * for mumudvb_channel_t for details).
	 */
	pthread_mutex_t lock;
	/** The number of channels ... */
	int number_of_channels;
	/** Do we send packets with error bit set by decoder ? */
	int filter_transport_error;
	/** Do we do filtering to keep only PSI tables (without DVB tables) ? **/
	int psi_tables_filtering;
	/** The channels array */
	mumudvb_channel_t channels[MAX_CHANNELS];  /**@todo use realloc*/
	//Asked pids //used for filtering
	/** this array contains the pids we want to filter,*/
	uint8_t asked_pid[8193];
	/** The number of TS discontinuities per PID **/
	int16_t continuity_counter_pid[8193]; //on 16 bits for storing the initial -1
	uint8_t check_cc;
	/** t2mi demux parameters **/
	int t2mi_pid;
	uint8_t t2mi_plp;
}mumu_chan_p_t;


typedef struct monitor_parameters_t{
	volatile int threadshutdown;
	int wait_time;
	struct auto_p_t *auto_p;
	struct sap_p_t *sap_p;
	mumu_chan_p_t *chan_p;
	multi_p_t *multi_p;
	struct unicast_parameters_t *unicast_vars;
	struct tune_p_t *tune_p;
	fds_t *fds;
	struct stats_infos_t *stats_infos;
	void *scam_vars_v;
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

/** global scope variables for T2-MI processiong **/
extern bool t2mi_active;
extern bool t2mi_first;
extern int t2_partial_size;

int mumu_string_append(mumu_string_t *string, const char *psz_format, ...);
void mumu_free_string(mumu_string_t *string);


int mumudvb_poll(struct pollfd *, int , int );
char *mumu_string_replace(char *source, int *length, int can_realloc, char *toreplace, char *replacement);
int string_comput(char *string);
uint64_t get_time(void);
void buffer_func (mumudvb_channel_t *channel, unsigned char *ts_packet, struct unicast_parameters_t *unicast_vars, void *scam_vars_v);
void send_func(mumudvb_channel_t *channel, uint64_t now_time, struct unicast_parameters_t *unicast_vars);

int mumu_init_chan(mumudvb_channel_t *chan);
void chan_update_CAM(mumu_chan_p_t *chan_p, struct auto_p_t *auto_p,  void *scam_vars_v);
void update_chan_net(mumu_chan_p_t *chan_p, struct auto_p_t *auto_p, multi_p_t *multi_p, struct unicast_parameters_t *unicast_vars, int server_id, int card, int tuner);
void update_chan_filters(mumu_chan_p_t *chan_p, char *card_base_path, int tuner, fds_t *fds);
long int mumu_timing();

/** Sets the interrupted flag if value != 0 and it is not already set.
 * In any case, returns the given value back. Thread- and signal-safe. */
int set_interrupted(int value);

/** Gets the interrupted flag; 0 if we have not been interrupted. */
int get_interrupted();

#endif
