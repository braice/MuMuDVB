/* 
 * MuMuDVB - Stream a DVB transport stream.
 * 
 * (C) 2004-2010 Brice DUBOST <mumudvb@braice.net>
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
#include "log.h"

#include <stdint.h>
extern uint32_t       crc32_table[256];
static char *log_module="TS: ";

/** @brief This function will join the 188 bytes packet until the PMT/PAT/SDT is full
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
  pthread_mutex_lock(&ts_packet->packetmutex);
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
      log_message( log_module,  MSG_DEBUG, "Read TS : Adaptation field \n");
      delta += buf[delta] ;        // add adapt.field.len
    }
  else if (header->adaptation_field_control & 0x1)
    {
      if (buf[delta]==0x00 && buf[delta+1]==0x00 && buf[delta+2]==0x01) 
	{
	  // -- PES/PS
	  //tspid->id   = buf[j+3];
	  log_message( log_module,  MSG_DEBUG, "#PES/PS ----- We ignore \n");
	  ok=0;
	}
      else
	  ok=1;
    }

  if (header->adaptation_field_control == 3)
    {
      log_message( log_module,  MSG_DEBUG, "adaptation_field_control 3\n");
      ok=0;
    }

  if(header->payload_unit_start_indicator) //It's the beginning of a new packet
    {
      if(ok)
	{
	  ts_packet->empty=0;
	  ts_packet->continuity_counter=header->continuity_counter;
	  ts_packet->pid=pid;
          int pointer_field=*(buf+delta);
          if(pointer_field!=0)
          {
            log_message(log_module, MSG_FLOOD, "Pointer field 0x%02x\n",pointer_field);
          }
          if((188-delta-1-pointer_field)<0)
          {
            log_message(log_module, MSG_DETAIL, "Pointer field too big 0x%02x, packet dropped\n",pointer_field);
            ts_packet->empty=1;
            pthread_mutex_unlock(&ts_packet->packetmutex);
            return 0;
          }
          ts_packet->len=AddPacketStart(ts_packet->packet,buf+delta+1+pointer_field,188-delta-1-pointer_field); //we add the packet to the buffer
          /*buf+delta+*1+pointer_field* because of pointer_field
          This is an 8-bit field whose value shall be the number of bytes, immediately following the pointer_field
          until the first byte of the first section that is present in the payload of the Transport Stream packet (so a value of 0x00 in
          the pointer_field indicates that the section starts immediately after the pointer_field). When at least one section begins in
          a given Transport Stream packet, then the payload_unit_start_indicator (refer to 2.4.3.2) shall be set to 1 and the first
          byte of the payload of that Transport Stream packet shall contain the pointer. When no section begins in a given
          Transport Stream packet, then the payload_unit_start_indicator shall be set to 0 and no pointer shall be sent in the
          payload of that packet.
          */
	  ts_packet->packet_ok=0;
	}
    }
  else if(header->payload_unit_start_indicator==0) //Not the first, we check if the already registered packet corresponds
    {
      if(ts_packet->empty)
	{
	  //log_message( log_module,  MSG_DEBUG," TS parse : Kind of Continuity ERROR packet empty and payload start\n");
          pthread_mutex_unlock(&ts_packet->packetmutex);
          return 0;
	}
      // -- pid change in stream? (without packet start). This is not supported
      if (ts_packet->pid != pid)
	{
	  log_message( log_module,  MSG_DEBUG,"error : PID change\n");
	  ts_packet->empty=1;
	}
      // -- discontinuity error in packet ?
      if  (ts_packet->continuity_counter == header->continuity_counter) 
	{
	  log_message( log_module,  MSG_DETAIL," Duplicate packet : ts_packet->continuity_counter %d\n");
          pthread_mutex_unlock(&ts_packet->packetmutex);
	  return 0;
	}
      if  ((ts_packet->continuity_counter+1)%16 != header->continuity_counter) 
	{
	  log_message( log_module,  MSG_DETAIL,"Continuity ERROR : ts_packet->continuity_counter %d header->continuity_counter %d\n",ts_packet->continuity_counter,header->continuity_counter);
	  ts_packet->empty=1;
          pthread_mutex_unlock(&ts_packet->packetmutex);
	  return 0;
	}
      ts_packet->packet_ok=0;
      ts_packet->continuity_counter=header->continuity_counter;
      if(ts_packet->len+(188-delta)<4096)
	ts_packet->len=AddPacketContinue(ts_packet->packet,buf+delta,188-delta,ts_packet->len); //we add the packet to the buffer
      else
	{
	  log_message( log_module,  MSG_INFO,"Packet to big\n");
	  ts_packet->empty=1;
          pthread_mutex_unlock(&ts_packet->packetmutex);
	  return 0;
	}

    }
  //We check if the TS is full
  pmt_t *pmt;
  pmt=((pmt_t *)ts_packet->packet);
  if (ts_packet->len > ((HILO(pmt->section_length))+3)) //+3 is for the header
  {
    //Yes, it's full, I check the CRC32 to say it's valid
    parsed=ts_check_CRC(ts_packet); //TEST CRC32
  }

  if(parsed)
    ts_packet->packet_ok=1;
  pthread_mutex_unlock(&ts_packet->packetmutex);
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
      log_message( log_module,  MSG_DETAIL,"\tBAD CRC32 PID : %d\n", pmt->pid);
      return 0; //We don't send this PMT
    }

  return 1;

}



/** @brief compare the SERVICE_ID contained in the channel and in the PMT
 *
 * Return 1 if match or no service_id info, 0 otherwise
 * 
 * @param pmt the pmt packet
 * @param channel the channel to be checked
 */
int check_pmt_service_id(mumudvb_ts_packet_t *pmt, mumudvb_channel_t *channel)
{

  pmt_t *header;


  header=(pmt_t *)pmt->packet;

  if(header->table_id!=0x02)
  {
    log_message( log_module,  MSG_INFO,"Packet PID %d for channel \"%s\" is not a PMT PID.\n", pmt->pid, channel->name);
    return 0;
  }


  /*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
  sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
  and shall be the next table to become valid.*/
  if(header->current_next_indicator == 0)
  {
    log_message( log_module, MSG_DEBUG,"The current_next_indicator is set to 0, this PMT is not valid for the current stream\n");
    return 0;
  }


  //We check if this PMT belongs to the current channel. (Only works with autoconfiguration full for the moment because it stores the service_id)
  if(channel->service_id && (channel->service_id != HILO(header->program_number)) )
  {
    log_message( log_module,  MSG_DETAIL,"The PMT %d not belongs to channel \"%s\"\n", pmt->pid, channel->name);
    log_message( log_module,  MSG_DETAIL,"Debug channel->service_id %d pmt service_id %d\n", channel->service_id, HILO(header->program_number));
    return 0;
  }
  else if(channel->service_id)
    log_message( log_module,  MSG_DETAIL,"GOOD service_id for PMT %d and channel \"%s\"\n", pmt->pid, channel->name);

  if(!channel->service_id)
    log_message( log_module,  MSG_DEBUG,"no service_id information for channel \"%s\"\n", channel->name);

  return 1;


}


