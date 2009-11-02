/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) 2009 Brice DUBOST <mumudvb@braice.net>
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


/** @file
 * This file contains the functions concerning the RTP header
 */

#include "mumudvb.h"


/**@brief : This function add the RTP header to a channel
 *
 * Note : it reset the buffer of the channel
 *
 * @param channel the channel to be initialised
 */
void init_rtp_header(mumudvb_channel_t *channel)
{
  // See RFC 1889
  channel->buf_with_rtp_header[0]=128; //version=2 padding=0 extension=0 CSRC=0
  channel->buf_with_rtp_header[1]=33;  // marker=0 payload type=33 (MP2T)
  channel->buf_with_rtp_header[2]=0;   // sequence number
  channel->buf_with_rtp_header[3]=0;   // sequence number
  channel->buf_with_rtp_header[4]=0;   // timestamp
  channel->buf_with_rtp_header[5]=0;   // timestamp
  channel->buf_with_rtp_header[6]=0;   // timestamp
  channel->buf_with_rtp_header[7]=0;   // timestamp
  channel->buf_with_rtp_header[8]= (char)(rand() % 256); // synchronization source
  channel->buf_with_rtp_header[9]= (char)(rand() % 256); // synchronization source
  channel->buf_with_rtp_header[10]=(char)(rand() % 256); // synchronization source
  channel->buf_with_rtp_header[11]=(char)(rand() % 256); // synchronization source

}

void rtp_update_sequence_number(mumudvb_channel_t *channel)
{
  // Change the header (sequence number)
  channel->buf_with_rtp_header[2]=(char)((channel->rtp_packet_num >> 8) & 0xff); // sequence number (high)
  channel->buf_with_rtp_header[3]=(char)(channel->rtp_packet_num & 0xff);        // sequence number (low)
  channel->rtp_packet_num++;
  channel->rtp_packet_num &= 0xffff;
}
