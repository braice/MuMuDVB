#ifndef _MUMUDVB_H
#define _MUMUDVB_H

#include "ts.h"
#include "udp.h"

//the number of pids by channel
#define MAX_PIDS_PAR_CHAINE     18

//the maximum channel number
#define MAX_CHAINES		128

//Size of an MPEG2-TS packet
#define TS_PACKET_SIZE 188

// How often (in seconds) to update the "now" variable
#define ALARM_TIME 2 //Temporary change for CAM support, value before : 5
#define ALARM_TIME_TIMEOUT 60
#define ALARM_TIME_TIMEOUT_NO_DIFF 600

#define AUTOCONFIGURE_TIME 10

// seven dvb paquets in one UDP
#define MAX_UDP_SIZE (TS_PACKET_SIZE*7)

//the max mandatory pid number
#define MAX_MANDATORY           32
//config line length
#define CONF_LINELEN 	        512
#define ALARM_COUNT_LIMIT	1024
#define MAX_LEN_NOM		256

//errors
enum
  {
    MSG_ERROR=-2,
    MSG_WARN,
    MSG_INFO,
    MSG_DETAIL,
    MSG_DEBUG
  };

//Channels
typedef struct{
  int streamed_channel;    //tell if this channel is actually streamed
  int streamed_channel_old;//tell if this channel is actually streamed (precedent test, to see if it's changed)

  char name[MAX_LEN_NOM];  //the channel name

  int pids[MAX_PIDS_PAR_CHAINE];   //the channel pids
  int num_pids;                    //number of channel pids
  int cam_pmt_pid;                 //pmt pid number for cam support

  unsigned char buf[MAX_UDP_SIZE]; //the buffer wich will be sent once it's full
  int nb_bytes;                    //number of bytes actually in the buffer

  int autoconfigurated;            //is the channel autoconfigurated ?

  char ipOut[20];
  int portOut;
  struct sockaddr_in sOut;
  int socketOut;

}mumudvb_channel_t;


//logging
void log_message( int , const char *, ... );

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
int services_to_channels(mumudvb_service_t *services, mumudvb_channel_t *channels, int cam_support, int common_port); 


#endif
