/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Header file for software descrambling common functions
 * 
 * (C) 2004-2010 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.braice.net
 *
 * Code inspired by vdr plugin dvbapi
 * Copyright (C) 2011,2012 Mariusz Białończyk <manio@skyboo.net>
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



#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/dvb/ca.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>


#include "scam_common.h"
#include "errors.h"
#include "ts.h"
#include "mumudvb.h"
#include "log.h"
#include "unicast_http.h"
#include "rtp.h"
#include "scam_decsa.h"
#include "scam_send.h"


/**@file
 * @brief scam support
 * 
 * Code used by other software descrambling files
 */


static char *log_module="SCAM_COMMON: ";

/* See http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2. */
int round_up(int x)
{
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
}

/** @brief Read a line of the configuration file to check if there is a scam parameter
 *
 * @param scam_vars the scam parameters
 * @param substring The currrent line
 */
int read_scam_configuration(scam_parameters_t *scam_vars, mumudvb_channel_t *c_chan, char *substring)
{
  char delimiteurs[] = CONFIG_FILE_SEPARATOR;
  if (!strcmp (substring, "scam_support"))
  {
    substring = strtok (NULL, delimiteurs);
    scam_vars->scam_support = atoi (substring);
    if(scam_vars->scam_support)
    {
      log_message( log_module,  MSG_WARN,
                   "You have enabled the support for software descrambling (scrambled channels). Please report any bug/comment\n");
    }
  scam_vars->ring_buffer_default_size=RING_BUFFER_DEFAULT_SIZE;
  scam_vars->decsa_default_delay=DECSA_DEFAULT_DELAY;
  scam_vars->send_default_delay=SEND_DEFAULT_DELAY;
  }
  else if (!strcmp (substring, "ring_buffer_default_size"))
  {
    substring = strtok (NULL, delimiteurs);
    scam_vars->ring_buffer_default_size = round_up(atoi(substring));
    log_message( log_module,  MSG_DEBUG, "Ring buffer default size set to %llu",(long long unsigned int)scam_vars->ring_buffer_default_size);

  }
  else if (!strcmp (substring, "decsa_default_delay"))
  {
    substring = strtok (NULL, delimiteurs);
    scam_vars->decsa_default_delay = atoi (substring);
  if (scam_vars->decsa_default_delay > 10000000)
    scam_vars->decsa_default_delay = 10000000;
  }
  else if (!strcmp (substring, "send_default_delay"))
  {
    substring = strtok (NULL, delimiteurs);
    scam_vars->send_default_delay = atoi (substring);
  }

  else if (!strcmp (substring, "oscam"))
  {
    if ( c_chan == NULL )
    {
      log_message( log_module,  MSG_ERROR,
                   "oscam : You have to start a channel first (using new_channel)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    c_chan->scam_support = atoi (substring);
  if (c_chan->scam_support) {
    c_chan->need_scam_ask=CAM_NEED_ASK;
    c_chan->ring_buffer_size=scam_vars->ring_buffer_default_size;
    c_chan->decsa_delay=scam_vars->decsa_default_delay;
    c_chan->send_delay=scam_vars->send_default_delay;
    c_chan->scam_pmt_packet=malloc(sizeof(mumudvb_ts_packet_t));
	if(c_chan->scam_pmt_packet==NULL)
	{
		log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		set_interrupted(ERROR_MEMORY<<8);
		return -1;
	}
	memset (c_chan->scam_pmt_packet, 0, sizeof( mumudvb_ts_packet_t));//we clear it
	pthread_mutex_init(&c_chan->scam_pmt_packet->packetmutex, NULL);
  }
  }
  else if (!strcmp (substring, "ring_buffer_size"))
  {
    if ( c_chan == NULL)
    {
      log_message( log_module,  MSG_ERROR,
                   "ring_buffer_size : You have to start a channel first (using new_channel)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    c_chan->ring_buffer_size = round_up(atoi (substring));
  }
  else if (!strcmp (substring, "decsa_delay"))
  {
    if ( c_chan == NULL)
    {
      log_message( log_module,  MSG_ERROR,
                   "decsa_delay : You have to start a channel first (using new_channel)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    c_chan->decsa_delay = atoi (substring);
  if (c_chan->decsa_delay > 10000000)
    c_chan->decsa_delay = 10000000;
  }
  else if (!strcmp (substring, "send_delay"))
  {
    if ( c_chan == NULL)
    {
      log_message( log_module,  MSG_ERROR,
                   "send_delay : You have to start a channel first (using new_channel)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    c_chan->send_delay = atoi (substring);
  }
  else
    return 0; //Nothing concerning scam, we return 0 to explore the other possibilities

  return 1;//We found something for scam, we tell main to go for the next line

}






/** @brief initialize the pmt get for scam descrambled channels
 *
 */
int scam_init_no_autoconf(scam_parameters_t *scam_vars, mumudvb_channel_t *channels,int number_of_channels)
{
  int curr_channel;

  if (scam_vars->scam_support){
    for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      if (channels[curr_channel].scam_support==1 && channels[curr_channel].pid_i.num_pids>1) {
    	  if(!channels[curr_channel].pid_i.pmt_pid)
    	  {
    		  log_message( log_module,  MSG_WARN,
    		                     "channel %d with SCAM support and no PMT set I disable SCAM support for this channel",curr_channel);
    		  channels[curr_channel].scam_support=0;
    	  }
      }
    }
  }

 return 0;

}



/** @brief create ring buffer and threads for sending and descrambling
 *
 */
int scam_channel_start(mumudvb_channel_t *channel, unicast_parameters_t *unicast_vars)
{
  channel->ring_buf=malloc(sizeof(ring_buffer_t));
  if (channel->ring_buf == NULL) {
    log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return ERROR_MEMORY<<8;
  }
  memset (channel->ring_buf, 0, sizeof( ring_buffer_t));//we clear it
  
  channel->ring_buf->data=malloc(channel->ring_buffer_size*TS_PACKET_SIZE);
  if (channel->ring_buf->data == NULL) {
    log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return ERROR_MEMORY<<8;
  }
  channel->ring_buf->time_send=malloc(channel->ring_buffer_size * sizeof(uint64_t));
  if (channel->ring_buf->time_send == NULL) {
    log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return ERROR_MEMORY<<8;
  }
  channel->ring_buf->time_decsa=malloc(channel->ring_buffer_size * sizeof(uint64_t));
  if (channel->ring_buf->time_decsa == NULL) {
    log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return ERROR_MEMORY<<8;
  }
  memset (channel->ring_buf->time_send, 0, channel->ring_buffer_size * sizeof(uint64_t));//we clear it
  memset (channel->ring_buf->time_decsa, 0, channel->ring_buffer_size * sizeof(uint64_t));//we clear it

  pthread_mutex_init(&channel->ring_buf->lock, NULL);
  scam_send_start(channel, unicast_vars);
  scam_decsa_start(channel);
  return 0;
}

void scam_channel_stop(mumudvb_channel_t *channel)
{
  scam_send_stop(channel);
  scam_decsa_stop(channel);
  free(channel->ring_buf->data);
  free(channel->ring_buf->time_send);
  free(channel->ring_buf->time_decsa);

  pthread_mutex_destroy(&channel->ring_buf->lock);
  free(channel->ring_buf);
}

/** @brief This function is called when a new PMT packet is there */
void scam_new_packet(int pid, mumudvb_channel_t *actual_channel)
{
	if ((actual_channel->need_scam_ask==CAM_NEED_ASK) && (actual_channel->pid_i.pmt_pid == pid))
	{
		if(actual_channel->pmt_packet->len_full > 0)
		{
			//We check the transport stream id of the packet
			if(check_pmt_service_id(actual_channel->pmt_packet, actual_channel))
			{
				pthread_mutex_lock(&actual_channel->scam_pmt_packet->packetmutex);
				actual_channel->scam_pmt_packet->len_full = actual_channel->pmt_packet->len_full;
				memcpy(actual_channel->scam_pmt_packet->data_full, actual_channel->pmt_packet->data_full, actual_channel->pmt_packet->len_full);
				pthread_mutex_unlock(&actual_channel->scam_pmt_packet->packetmutex);
			}
		}
	}
}




