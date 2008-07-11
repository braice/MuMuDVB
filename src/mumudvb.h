/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
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
 *     
 */


#ifndef _MUMUDVB_H
#define _MUMUDVB_H

//#include "ts.h"
#include "udp.h"  //for the sockaddr

//the number of pids by channel
#define MAX_PIDS_PAR_CHAINE     18

//the maximum channel number
#define MAX_CHANNELS		128

//Size of an MPEG2-TS packet
#define TS_PACKET_SIZE 188

// How often (in seconds) to update the "now" variable
#define ALARM_TIME 2 //Temporary change for CAM support, value before : 5
#define ALARM_TIME_TIMEOUT 60
#define ALARM_TIME_TIMEOUT_NO_DIFF 600

//timeout for autoconfiguration
#define AUTOCONFIGURE_TIME 10

// seven dvb paquets in one UDP
#define MAX_UDP_SIZE (TS_PACKET_SIZE*7)

//the max mandatory pid number
#define MAX_MANDATORY_PID_NUMBER   32
//config line length
#define CONF_LINELEN 	        512
#define ALARM_COUNT_LIMIT	1024
#define MAX_NAME_LEN		256

//Maximum number of polling tries
#define MAX_POLL_TRIES		5

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

  char name[MAX_NAME_LEN];  //the channel name

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
void gen_chaines_diff (char *nom_fich_chaines_diff, char *nom_fich_chaines_non_diff, int nb_flux, mumudvb_channel_t *channels);

//pat_rewrite
int pat_rewrite(unsigned char *buf,int num_pids, int *pids);

#endif
