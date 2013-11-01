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
  unsigned char scrambling_control_packet=0;
  unsigned int ca_idx = 0;
  mumudvb_channel_t *channel;
  channel = ((mumudvb_channel_t *) arg);
  unsigned int batch_size = dvbcsa_bs_batch_size();
  struct dvbcsa_bs_batch_s odd_batch[batch_size+1];
  struct dvbcsa_bs_batch_s even_batch[batch_size+1];
  unsigned char *odd_scnt_field[batch_size+1];
  unsigned char *even_scnt_field[batch_size+1];
  unsigned char odd_batch_idx=0;
  unsigned char even_batch_idx=0;
  unsigned char offset=0,len=0;
  unsigned int nscrambled=0, scrambled=0;
  unsigned int i;
  struct dvbcsa_bs_key_s *odd_key;
  struct dvbcsa_bs_key_s *even_key;
  odd_key=dvbcsa_bs_key_alloc();
  even_key=dvbcsa_bs_key_alloc();
  int first_run = 1;
  int got_first_even_key = 0, got_first_odd_key = 0;


  /* For simplicity, and to avoid taking the lock anew for every packet,
   * we only release the lock when sleeping or doing CPU-intensive work. */
  pthread_mutex_lock(&channel->ring_buf->lock);
  while(!channel->decsathread_shutdown) {
    uint64_t now_time=get_time();
    uint64_t decsa_time = channel->ring_buf->time_decsa[channel->ring_buf->read_decsa_idx];

    if (channel->ring_buf->to_descramble == 0) {
      if (first_run) {
        first_run = 0;
        log_message( log_module, MSG_DEBUG, "first run waiting");
      } else
        log_message( log_module, MSG_ERROR, "thread starved, channel %s %u %u\n",channel->name,channel->ring_buf->to_descramble,channel->ring_buf->to_send);
      pthread_mutex_unlock(&channel->ring_buf->lock);
      usleep(50000);
      pthread_mutex_lock(&channel->ring_buf->lock);
      continue;
    }

    if (now_time < decsa_time) {
      pthread_mutex_unlock(&channel->ring_buf->lock);
      usleep(decsa_time - now_time);
      pthread_mutex_lock(&channel->ring_buf->lock);
    }

    scrambling_control_packet = ((channel->ring_buf->data[channel->ring_buf->read_decsa_idx][3] & 0xc0) >> 6);

    offset = ts_packet_get_payload_offset(channel->ring_buf->data[channel->ring_buf->read_decsa_idx]);
    len=188-offset;

    switch (scrambling_control_packet) {
          case 2:
            ++scrambled;
            if (ca_idx) {
              even_batch[even_batch_idx].data = channel->ring_buf->data[channel->ring_buf->read_decsa_idx] + offset;
              even_batch[even_batch_idx].len = len;
              even_scnt_field[even_batch_idx] = &channel->ring_buf->data[channel->ring_buf->read_decsa_idx][3];
              ++even_batch_idx;
            }
            break;
          case 3:
            ++scrambled;
            if (ca_idx) {
              odd_batch[odd_batch_idx].data = channel->ring_buf->data[channel->ring_buf->read_decsa_idx] + offset;
              odd_batch[odd_batch_idx].len = len;
              odd_scnt_field[odd_batch_idx] = &channel->ring_buf->data[channel->ring_buf->read_decsa_idx][3];
              ++odd_batch_idx;
            }
            break;
          default :
            ++nscrambled;
            break;
    }
    ++channel->ring_buf->read_decsa_idx;
    channel->ring_buf->read_decsa_idx&=(channel->ring_buffer_size -1);

    --channel->ring_buf->to_descramble;

    if ((scrambled==batch_size) || (nscrambled==batch_size)) {
      even_batch[even_batch_idx].data=0;
      odd_batch[odd_batch_idx].data=0;

      /* Load new keys if they are ready and we no longer use the old one. */
      if ((odd_batch_idx != 0 && even_batch_idx == 0) || !got_first_even_key) {
        pthread_mutex_lock(&channel->cw_lock);
        if (channel->got_key_even) {
          dvbcsa_bs_key_set(channel->even_cw, even_key);
          log_message( log_module, MSG_DEBUG, "%016llx even key %02x %02x %02x %02x %02x %02x %02x %02x, channel %s\n", now_time, channel->even_cw[0], channel->even_cw[1], channel->even_cw[2], channel->even_cw[3], channel->even_cw[4], channel->even_cw[5], channel->even_cw[6], channel->even_cw[7],channel->name);
          channel->got_key_even = 0;
          got_first_even_key = 1;
        }
        pthread_mutex_unlock(&channel->cw_lock);
      }
      if ((even_batch_idx != 0 && odd_batch_idx == 0) || !got_first_odd_key) {
        pthread_mutex_lock(&channel->cw_lock);
        if (channel->got_key_odd) {
          dvbcsa_bs_key_set(channel->odd_cw, odd_key);
          log_message( log_module, MSG_DEBUG, " %016llx odd key %02x %02x %02x %02x %02x %02x %02x %02x, channel %s\n",now_time, channel->odd_cw[0], channel->odd_cw[1], channel->odd_cw[2], channel->odd_cw[3], channel->odd_cw[4], channel->odd_cw[5], channel->odd_cw[6], channel->odd_cw[7], channel->name);
          channel->got_key_odd = 0;
          got_first_odd_key = 1;
        }
        pthread_mutex_unlock(&channel->cw_lock);
      }
      pthread_mutex_unlock(&channel->ring_buf->lock);
      if (even_batch_idx) {
        dvbcsa_bs_decrypt(even_key, even_batch, 184);

        // We zero the scrambling control field to mark stream as unscrambled.
        for (i = 0; i < even_batch_idx; ++i) {
          *even_scnt_field[i] &= 0x3f;
        }
      }
      if (odd_batch_idx) {
        dvbcsa_bs_decrypt(odd_key, odd_batch, 184);

        // We zero the scrambling control field to mark stream as unscrambled.
        for (i = 0; i < odd_batch_idx; ++i) {
          *odd_scnt_field[i] &= 0x3f;
        }
      }
      even_batch_idx = 0;
      odd_batch_idx = 0;
      pthread_mutex_lock(&channel->ring_buf->lock);

      channel->ring_buf->to_send+= scrambled  + nscrambled;
      nscrambled=0;
      scrambled=0;

      pthread_mutex_lock(&channel->cw_lock);
      ca_idx = channel->ca_idx;
      pthread_mutex_unlock(&channel->cw_lock);
    }
  }
  pthread_mutex_unlock(&channel->ring_buf->lock);
  if(odd_key)
    dvbcsa_bs_key_free(odd_key);
  if(even_key)
    dvbcsa_bs_key_free(even_key);


  return 0;
}
