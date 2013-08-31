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
int read_scam_configuration(scam_parameters_t *scam_vars, mumudvb_channel_t *current_channel, int ip_ok, char *substring)
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
    log_message( log_module,  MSG_DEBUG, "Ring buffer default size set to %u\n",scam_vars->ring_buffer_default_size);

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
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "oscam : You have to start a channel first (using ip= or channel_next)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->scam_support = atoi (substring);
	if (current_channel->scam_support) {
		current_channel->need_scam_ask=CAM_NEED_ASK;
		current_channel->ring_buffer_size=scam_vars->ring_buffer_default_size;
		current_channel->decsa_delay=scam_vars->decsa_default_delay;
		current_channel->send_delay=scam_vars->send_default_delay;		
	}
  }
  else if (!strcmp (substring, "ring_buffer_size"))
  {
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "oscam : You have to start a channel first (using ip= or channel_next)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->ring_buffer_size = round_up(atoi (substring));
  }
  else if (!strcmp (substring, "decsa_delay"))
  {
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "oscam : You have to start a channel first (using ip= or channel_next)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->decsa_delay = atoi (substring);
	if (current_channel->decsa_delay > 10000000)
	  current_channel->decsa_delay = 10000000;
  }
  else if (!strcmp (substring, "send_delay"))
  {
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "oscam : You have to start a channel first (using ip= or channel_next)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->send_delay = atoi (substring);
  }
  else
    return 0; //Nothing concerning cam, we return 0 to explore the other possibilities

  return 1;//We found something for cam, we tell main to go for the next line

}






/** @brief initialize the pmt get for scam descrambled channels
 *
 */
int scam_init_no_autoconf(autoconf_parameters_t *autoconf_vars, scam_parameters_t *scam_vars, mumudvb_channel_t *channels,int number_of_channels)
{
  int curr_channel;


 if (scam_vars->scam_support){ 
  if (autoconf_vars->autoconfiguration==AUTOCONF_MODE_PIDS || autoconf_vars->autoconfiguration==AUTOCONF_MODE_NONE)
    for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
		if (channels[curr_channel].scam_support==1 && channels[curr_channel].num_pids>1)
		{
		  channels[curr_channel].pmt_pid=channels[curr_channel].pids[0];
	      channels[curr_channel].pids_type[0]=PID_PMT;
	      snprintf(channels[curr_channel].pids_language[0],4,"%s","---");
		  ++scam_vars->need_pmt_get;
		  channels[curr_channel].need_pmt_get=1;
		}
    }
 }

 return 0;

}

/** @brief pmt get for scam descrambled channels
 *
 */
int scam_new_packet(int pid, unsigned char *ts_packet, scam_parameters_t *scam_vars, mumudvb_channel_t *channels)
{
  int curr_channel;
  pmt_t *header;

  if(scam_vars->need_pmt_get) //We have the channels and their PMT, we search the other pids
  {
    for(curr_channel=0;curr_channel<MAX_CHANNELS;curr_channel++)
    {
      if((channels[curr_channel].pmt_pid==pid)&& pid && channels[curr_channel].scam_support && channels[curr_channel].need_pmt_get )
      {
		if(get_ts_packet(ts_packet,channels[curr_channel].pmt_packet))
		{
			header=(pmt_t *)channels[curr_channel].pmt_packet->data_full;
			if(channels[curr_channel].service_id == HILO(header->program_number)) {
			  --scam_vars->need_pmt_get;
			  channels[curr_channel].need_pmt_get=0;
			  log_message(log_module,MSG_DEBUG,"Got pmt for channel %s\n", channels[curr_channel].name);
			}
			else log_message(log_module,MSG_DEBUG,"pmt not for channel %s\n", channels[curr_channel].name);
		}
	  }
	}
 }
 return 0;

}

/** @brief create ring buffer and threads for sending and descrambling
 *
 */
int scam_channel_start(mumudvb_channel_t *channel)
{
	unsigned int i;
	
	channel->ring_buf=malloc(sizeof(ring_buffer_t));
  	if (channel->ring_buf == NULL) {
	  log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
	  return ERROR_MEMORY<<8;
 	} 
	memset (channel->ring_buf, 0, sizeof( ring_buffer_t));//we clear it
  
	channel->ring_buf->data=malloc(channel->ring_buffer_size*sizeof(char *));
  	if (channel->ring_buf->data == NULL) {
	  log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
	  return ERROR_MEMORY<<8;
 	} 	  
	for ( i = 0; i< channel->ring_buffer_size; i++)
	{
  		channel->ring_buf->data[i]=malloc(TS_PACKET_SIZE*sizeof(char));
	  	if (channel->ring_buf->data[i] == NULL) {
		  log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		  return ERROR_MEMORY<<8;
	 	} 
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
	scam_send_start(channel);
	scam_decsa_start(channel);
	return 0;
}

void scam_channel_stop(mumudvb_channel_t *channel)
{
	uint64_t i;
	scam_send_stop(channel);
	scam_decsa_stop(channel);
    for ( i = 0; i< channel->ring_buffer_size; i++)
    {
  		free(channel->ring_buf->data[i]);
	}
    free(channel->ring_buf->data);
    free(channel->ring_buf->time_send);
    free(channel->ring_buf->time_decsa);
					
	pthread_mutex_destroy(&channel->ring_buf->lock);
    free(channel->ring_buf);
}




