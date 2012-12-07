/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for software descrambling libdvbcsa part
 * 
 * (C) 2004-2010 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.braice.net
 *
 * Code inspired by TSDECRYPT
 * Copyright (C) 2011,2012 Georgi Chorbadzhiyski <georgi@unixsol.org>
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

#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>


#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>


#include "errors.h"
#include "ts.h"
#include "mumudvb.h"
#include "log.h"
#include "scam_common.h"

#include <dvbcsa/dvbcsa.h>

/**@file
 * @brief scam support
 * 
 * Code concerning software descrambling
 */

static void *decsathread_func(void* arg); //The polling thread
static char *log_module="SCAM_DECSA: ";



/** @brief Getting ts payload starting point
 *
 *
 */
unsigned char ts_packet_get_payload_offset(unsigned char *ts_packet) {
	if (ts_packet[0] != 0x47)
		return 0;

	unsigned char adapt_field   = (ts_packet[3] &~ 0xDF) >> 5; // 11x11111
	unsigned char payload_field = (ts_packet[3] &~ 0xEF) >> 4; // 111x1111

	if (!adapt_field && !payload_field) // Not allowed
		return 0;

	if (adapt_field) {
		unsigned char adapt_len = ts_packet[4];
		if (payload_field && adapt_len > 182) // Validity checks
			return 0;
		if (!payload_field && adapt_len > 183)
			return 0;
		if (adapt_len + 4 > 188) // adaptation field takes the whole packet
			return 0;
		return 4 + 1 + adapt_len; // ts header + adapt_field_len_byte + adapt_field_len
	} else {
		return 4; // No adaptation, data starts directly after TS header
	}
}


int scam_decsa_start(mumudvb_channel_t *channel)
{
  pthread_attr_t attr;
  struct sched_param param;
  size_t stacksize;
  unsigned int batch_size = dvbcsa_bs_batch_size();
  stacksize = sizeof(mumudvb_channel_t *)+3*sizeof(unsigned int)+2*batch_size*sizeof(struct dvbcsa_bs_batch_s)+4*sizeof(unsigned char)+2*sizeof(struct dvbcsa_bs_key_s *)+50000;
  
  pthread_attr_init(&attr);
  pthread_attr_setschedpolicy(&attr, SCHED_RR);
  param.sched_priority = sched_get_priority_max(SCHED_RR);
  pthread_attr_setschedparam(&attr, &param);
  pthread_attr_setstacksize (&attr, stacksize);
  pthread_create(&(channel->decsathread), &attr, decsathread_func, channel);	 

  log_message(log_module, MSG_DEBUG,"Decsa thread started, channel %s\n",channel->name);
  return 0;
  
}


void scam_decsa_stop(mumudvb_channel_t *channel)
{

    log_message(log_module,MSG_DEBUG,"Decsa Thread closing, channel %s\n", channel->name);
    channel->decsathread_shutdown=1;
    pthread_join(channel->decsathread,NULL); 
	log_message(log_module,MSG_DEBUG,"Decsa Thread closed, channel %s\n", channel->name);
}



static void *decsathread_func(void* arg)
{
  unsigned char scrambling_control_packet=0, scrambling_control=0;
  
  mumudvb_channel_t *channel;
  channel = ((mumudvb_channel_t *) arg);
  unsigned int batch_size = dvbcsa_bs_batch_size();
  struct dvbcsa_bs_batch_s odd_batch[batch_size+1];
  struct dvbcsa_bs_batch_s even_batch[batch_size+1];
  unsigned char odd_batch_idx=0;
  unsigned char even_batch_idx=0;
  unsigned char offset=0,len=0;
  unsigned int nscrambled=0,scrambled=0;
  struct dvbcsa_bs_key_s *odd_key;
  struct dvbcsa_bs_key_s *even_key;
  odd_key=dvbcsa_bs_key_alloc();
  even_key=dvbcsa_bs_key_alloc();

  extern uint64_t now_time;
  
  while (!channel->decsathread_shutdown) {
	if ((now_time >=channel->ring_buf->time_decsa[(channel->ring_buf->read_decsa_idx>>2)] )&& channel->got_cw_started && channel->ring_buf->to_descramble) {
	  
		scrambling_control=((channel->ring_buf->data[channel->ring_buf->read_decsa_idx][3] & 0xc0) >> 6);
		if (channel->got_key_even) {	  
			dvbcsa_bs_key_set(channel->even_cw, even_key);
			--channel->got_key_even;
			log_message( log_module, MSG_DEBUG, "set first even key, channel %s, got %d, scr_cont %d\n",channel->name,channel->got_key_even, scrambling_control);
		}
	    if (channel->got_key_odd) {
			dvbcsa_bs_key_set(channel->odd_cw, odd_key);
			--channel->got_key_odd;
			log_message( log_module, MSG_DEBUG, "set first odd key, channel %s, got %d, scr_cont %d\n",channel->name,channel->got_key_even, scrambling_control);
		}
    	break;
	}
   else
	  usleep(50000);

}
	
  while(!channel->decsathread_shutdown) {
	now_time=get_time();
	  if ((now_time >=channel->ring_buf->time_decsa[(channel->ring_buf->read_decsa_idx>>2)] )) {
		if (channel->ring_buf->to_descramble!=0) {		 
		  scrambling_control_packet = ((channel->ring_buf->data[channel->ring_buf->read_decsa_idx][3] & 0xc0) >> 6);
	      offset = ts_packet_get_payload_offset(channel->ring_buf->data[channel->ring_buf->read_decsa_idx]);
		  len=188-offset;
			
		  //We NULL the scrambling control field to mark stream as unscrambled 
		  channel->ring_buf->data[channel->ring_buf->read_decsa_idx][3] &= 0x3f;
			
		  switch (scrambling_control_packet) {
			case 0:
			  ++nscrambled;
			  break;
			case 2:
			  ++scrambled;
			  even_batch[even_batch_idx].data = channel->ring_buf->data[channel->ring_buf->read_decsa_idx] + offset;
			  even_batch[even_batch_idx].len = len;
			  ++even_batch_idx;
			  break;
			case 3:
			  ++scrambled;
			  odd_batch[odd_batch_idx].data = channel->ring_buf->data[channel->ring_buf->read_decsa_idx] + offset;
			  odd_batch[odd_batch_idx].len = len;
			  ++odd_batch_idx;
			  break;
			default :
			  ++nscrambled;
			  break;
		  }
		  ++channel->ring_buf->read_decsa_idx;
		  channel->ring_buf->read_decsa_idx&=(channel->ring_buffer_size -1);	

		  pthread_mutex_lock(&channel->ring_buf->to_descramble_mutex);	
		  --channel->ring_buf->to_descramble;
		  pthread_mutex_unlock(&channel->ring_buf->to_descramble_mutex);
			
		  if ((scrambled==batch_size) || (nscrambled==batch_size)) {
			even_batch[even_batch_idx].data=0;
			odd_batch[odd_batch_idx].data=0;
			if (scrambling_control==3) {
				 if (even_batch_idx) {
						scrambling_control=2;
						if (channel->got_key_even) {
						  dvbcsa_bs_key_set(channel->even_cw, even_key);
						  log_message( log_module, MSG_DEBUG, "even key %016llx, channel %s, got %d, scr_cont %d\n",now_time,channel->name,channel->got_key_even, scrambling_control);
						  --channel->got_key_even;
						}
				 }
			}

			else {
				if (odd_batch_idx) {		  
						scrambling_control=3;
						if (channel->got_key_odd) {
						  dvbcsa_bs_key_set(channel->odd_cw, odd_key);
						  log_message( log_module, MSG_DEBUG, "odd key %016llx, channel %s, got %d, scr_cont %d\n",now_time,channel->name,channel->got_key_odd, scrambling_control);
						  --channel->got_key_odd;
						}
				}

			}
			if (even_batch_idx) {

			  dvbcsa_bs_decrypt(even_key, even_batch, 184);
			}
			if (odd_batch_idx) {

			  dvbcsa_bs_decrypt(odd_key, odd_batch, 184);
			}
			even_batch_idx = 0;
			odd_batch_idx = 0;

			pthread_mutex_lock(&channel->ring_buf->to_send_mutex);
			channel->ring_buf->to_send+= (scrambled  + nscrambled);
			pthread_mutex_unlock(&channel->ring_buf->to_send_mutex);  
			  
			nscrambled=0;
			scrambled=0;
		 }	
	  }
	  else {
	    log_message( log_module, MSG_ERROR, "thread starved, channel %s %u %u\n",channel->name,channel->ring_buf->to_descramble,channel->ring_buf->to_send); 
	    usleep(50000);
	  }

	}
	else {
	 usleep(((channel->ring_buf->time_decsa[(channel->ring_buf->read_decsa_idx>>2)])-now_time));
	}
		

  }
	if(odd_key)
      dvbcsa_bs_key_free(odd_key);
	if(even_key)
      dvbcsa_bs_key_free(even_key);
  return 0;
}