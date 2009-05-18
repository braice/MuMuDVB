/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) 2004-2009 Brice DUBOST <mumudvb@braice.net>
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

/**@file
 * @brief File for demuxing TS stream
 */

#include <string.h>

#include "ts.h"
#include "mumudvb.h"

#include <stdint.h>
extern uint32_t       crc32_table[256];

/**@brief This function will join the 188 bytes packet until the PMT/PAT/SDT is full
 * Once it's full we check the CRC32 and say if it's ok or not
 * 
 * There is two important mpeg2-ts fields to do that
 * 
 *  * the continuity counter wich is incremented for each packet
 * 
 *  * The payload_unit_start_indicator wich says if it's the first packet
 *
 * When a packet is cutted in 188 bytes packets, there should be no other pid between two sub packets
 *
 * Return 1 when the packet is full and OK
 *
 * @param buf : the received buffer from the card
 * @param ts_packet : the packet to be completed
 */
int get_ts_packet(unsigned char *buf, mumudvb_ts_packet_t *ts_packet)
{

  ts_header_t *header;
  int ok=0;
  int parsed=0;
  int delta,pid;

  //mapping of the buffer onto the TS header
  header=(ts_header_t *)buf;
  pid=HILO(header->pid);

  //delta used to remove TS HEADER
  delta = TS_HEADER_LEN-1; 

  //Sometimes there is some more data in the header, the adaptation field say it
  if (header->adaptation_field_control & 0x2)
    {
      log_message( MSG_DEBUG, "Read TS : Adaptation field \n");
      delta += buf[delta] ;        // add adapt.field.len
    }
  else if (header->adaptation_field_control & 0x1)
    {
      if (buf[delta]==0x00 && buf[delta+1]==0x00 && buf[delta+2]==0x01) 
	{
	  // -- PES/PS                                                                                                                               
	  //tspid->id   = buf[j+3];                                                                                                                  
	  log_message( MSG_DEBUG, " parse TS : #PES/PS ----- We ignore \n");
	  ok=0;
	}
      else
	  ok=1;
    }

  if (header->adaptation_field_control == 3)
    {
      log_message( MSG_DEBUG, " parse TS : adaptation_field_control 3\n");
      ok=0;
    }

  if(header->payload_unit_start_indicator) //It's the beginning of a new packet
    {
      if(ok)
	{
	  ts_packet->empty=0;
	  ts_packet->continuity_counter=header->continuity_counter;
	  ts_packet->pid=pid;
	  ts_packet->len=AddPacketStart(ts_packet->packet,buf+delta+1,188-delta-1); //we add the packet to the buffer
	}
    }
  else if(header->payload_unit_start_indicator==0) //Not the first, we check if che already registered packet corresponds
    {
      // -- pid change in stream? (without packet start). This is not supported
      if (ts_packet->pid != pid)
	{
	  log_message( MSG_DEBUG," TS parse. ERROR : PID change\n");
	  ts_packet->empty=1;
	}
      // -- discontinuity error in packet ?
      if  ((ts_packet->continuity_counter+1)%16 != header->continuity_counter) 
	{
	  log_message( MSG_DETAIL," TS parse : Continuity ERROR : ts_packet->continuity_counter %d header->continuity_counter %d\n",ts_packet->continuity_counter,header->continuity_counter);
	  ts_packet->empty=1;
	  return 0;
	}
      ts_packet->continuity_counter=header->continuity_counter;
      if(ts_packet->len+(188-delta)<4096)
	ts_packet->len=AddPacketContinue(ts_packet->packet,buf+delta,188-delta,ts_packet->len); //we add the packet to the buffer
      else
	{
	  log_message( MSG_INFO," TS parse ERROR : Packet to big\n");
	  ts_packet->empty=1;
	  return 0;
	}

    }      
  //We check if the TS is full
  if (ts_packet->len > ((HILO(((pmt_t *)ts_packet->packet)->section_length))+3)) //+3 is for the header
    {
      //Yes, it's full, I check the CRC32 to say it's valid
      parsed=ts_check_CRC(ts_packet); //TEST CRC32
    }

  return parsed;
}


/**@todo document*/
//Les fonctions qui permettent de coller les paquets les uns aux autres
// -- add TS data
// -- return: 0 = fail
int AddPacketStart (unsigned char *packet, unsigned char *buf, unsigned int len)
{
  memset(packet,0,4096);
  memcpy(packet,buf,len);
  return len;
}

int AddPacketContinue  (unsigned char *packet, unsigned char *buf, unsigned int len, unsigned int act_len)
{
  memcpy(packet+act_len,buf,len);
  return len+act_len;
}


/**@brief Checking of the CRC32
 * return 1 if crc32 is ok, 0 otherwise
 * @param pmt : the packet to be checked
 */
int ts_check_CRC( mumudvb_ts_packet_t *pmt)
{
  pmt_t *pmt_struct;
  uint32_t crc32;
  int i;

  pmt_struct=(pmt_t *)pmt->packet;

  //the real lenght
  pmt->len=HILO(pmt_struct->section_length)+3; //+3 for the first three bits

  //CRC32 calculation
  //Test of the crc32
  crc32=0xffffffff;
  //we compute the CRC32
  //we have two ways: either we compute untill the end and it should be 0
  //either we exclude the 4 last bits and in should be equal to the 4 last bits
  for(i = 0; i < pmt->len; i++) {
    crc32 = (crc32 << 8) ^ crc32_table[((crc32 >> 24) ^ pmt->packet[i])&0xff];
  }
  
  if(crc32!=0)
    {
      //Bad CRC32
      log_message( MSG_DETAIL,"\tBAD CRC32 PID : %d\n", pmt->pid);
      return 0; //We don't send this PMT
    }
  
  return 1;

}
