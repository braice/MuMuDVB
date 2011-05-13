/* 
 * MuMuDVB - Stream a DVB transport stream.
 * 
 * (C) 2004-2011 Brice DUBOST <mumudvb@braice.net>
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


//Helper functions for get_ts_packet
void ts_move_part_to_full(mumudvb_ts_packet_t *ts_packet);
int  ts_check_crc32(mumudvb_ts_packet_t *ts_packet);
int  ts_partial_full(mumudvb_ts_packet_t *ts_packet);



/** @brief This function will join the 188 bytes packet until the PMT/PAT/SDT is full
 * Once it's full we check the CRC32 and say if it's ok or not
 * 
 * There is two important mpeg2-ts fields to do that
 * 
 *  * the continuity counter wich is incremented for each packet
 * 
 *  * The payload_unit_start_indicator wich says if it's the first packet
 *
 * When a packet is cutted in 188 bytes packets, there must be no other pid between two sub packets
 *
 * This function deals partially with the case of very dense transponders (ie using pointer field)
 *
 * Return 1 when the packet is full and OK
 *
 * @param buf : the received buffer from the card
 * @param ts_packet : the packet to be completed
 */
int get_ts_packet(unsigned char *buf, mumudvb_ts_packet_t *ts_packet)
{
  pthread_mutex_lock(&ts_packet->packetmutex);
  if(ts_packet->status_full==VALID)
  {
    log_message( log_module,  MSG_FLOOD, "Full packet left, we mark it empty\n");
    ts_packet->status_full=EMPTY;
  }
  //If on a previous call we left the partial packet with all the data
  //This can append if after a finishing packet (with pointer field) there was a very small one
  //   OR if all the packets are small
  //we check the CRC32
  //if ok tranfer this packet to the full
  if(ts_partial_full( ts_packet))
  {
    log_message( log_module,  MSG_FLOOD, "Full partial_packet left we check the CRC32\n");
    //The partial packet is full, we check the CRC32
    if(ts_check_crc32(ts_packet))
      ts_move_part_to_full(ts_packet); //Everything is perfect, the packet full is ok
  }

  ts_header_t *header;
  //mapping of the buffer onto the TS header
  header=(ts_header_t *)buf;
  int buf_pid;
  buf_pid=HILO(header->pid);

  //the current packet position
  int offset;
  //delta used to remove TS HEADER
  offset = TS_HEADER_LEN-1;

  //Is the buffer ok for parsing/adding
  //int buf_ok=1;

  log_message(log_module, MSG_FLOOD, "General information PID %d adaptation_field_control %d payload_unit_start_indicator %d continuity_counter %d\n",
              buf_pid,
              header->adaptation_field_control,
              header->payload_unit_start_indicator,
              header->continuity_counter);


  //we skip the adaptation field
  //Sometimes there is some more data in the header, the adaptation field say it
  if (header->adaptation_field_control & 0x2)
  {
    log_message( log_module,  MSG_DEBUG, "Read TS : Adaptation field \n");
    offset += buf[offset] ;        // add adapt.field.len
    //we check if the adapt.field.len is valid
    if(offset>=TS_PACKET_SIZE)
    {
      log_message( log_module,  MSG_DEBUG, "Invalid adapt.field.len \n");
      goto final_check;
    }
  }
  else if (header->adaptation_field_control & 0x1)
  {
    if (buf[offset]==0x00 && buf[offset+1]==0x00 && buf[offset+2]==0x01)
    {
      // -- PES/PS
      //tspid->id   = buf[j+3];
      log_message( log_module,  MSG_FLOOD, "#PES/PS ----- We ignore \n");
      goto final_check;
    }
  }
  if (header->adaptation_field_control == 3)
  {
    log_message( log_module,  MSG_DEBUG, "adaptation_field_control 3\n");
    goto final_check;
  }

  //We are now at the beginning of the Transport stream packet, we check if there is a pointer field
  //the pointer fields tells if there is the end of the previous packet before the beginning of a new one
  //and how long is this data

  if(header->payload_unit_start_indicator) //It's the beginning of a new packet
  {
    //Pointer field
    //This is an 8-bit field whose value shall be the number of bytes, immediately following the pointer_field
    //until the first byte of the first section that is present in the payload of the Transport Stream packet (so a value of 0x00 in
    //the pointer_field indicates that the section starts immediately after the pointer_field). When at least one section begins in
    //a given Transport Stream packet, then the payload_unit_start_indicator (refer to 2.4.3.2) shall be set to 1 and the first
    //byte of the payload of that Transport Stream packet shall contain the pointer. When no section begins in a given
    //Transport Stream packet, then the payload_unit_start_indicator shall be set to 0 and no pointer shall be sent in the
    //payload of that packet.
    int pointer_field=*(buf+offset);
    offset++; //we've read the pointer field
    if(pointer_field!=0)
    {
      log_message(log_module, MSG_FLOOD, "Pointer field 0x%02x %02d \n",pointer_field,pointer_field);
      if((TS_PACKET_SIZE-offset-pointer_field)<0)
      {
        log_message(log_module, MSG_DETAIL, "Pointer field too big 0x%02x, packet dropped\n",pointer_field);
        ts_packet->status_partial=EMPTY;
        goto final_check;
      }

      //We have a pointer field, we should happend this data to the actual partial packet, we check if it's started
      if(ts_packet->status_partial!=STARTED)
      {
        log_message(log_module, MSG_FLOOD, "Pointer field and packet not started or full, we mark empty\n");
        ts_packet->status_partial=EMPTY;
      }
      else if(ts_packet->cc==header->continuity_counter)
      {
        log_message(log_module, MSG_FLOOD, "Duplicate packet, continuity counter: %d\n", ts_packet->cc);
      }
      else if(((ts_packet->cc+1)%16)!=header->continuity_counter)
      {
        log_message(log_module, MSG_FLOOD, "The continuity counter is not valid saved packet cc %d actual cc %d\n", ts_packet->cc, header->continuity_counter);
        ts_packet->status_partial=EMPTY;
      }
      else if(ts_packet->pid!=buf_pid)
      {
        log_message(log_module, MSG_FLOOD, "PID change. saved PID %d, actual pid %d\n", ts_packet->pid, buf_pid);
        ts_packet->status_partial=EMPTY;
      }
      else
      {
        //packet started and pointer field, we append the data
        tbl_h_t *tbl_struct=(tbl_h_t *)ts_packet->data_partial;
        log_message(log_module, MSG_FLOOD, "Pointer field and packet started, act packet len: %d, new len %d, expected len %d\n",
                    ts_packet->len_partial,
                    ts_packet->len_partial+pointer_field,
                    HILO(tbl_struct->section_length)+BYTES_BFR_SEC_LEN);
        //we copy the data
        memcpy(ts_packet->data_partial+ts_packet->len_partial,buf+offset,pointer_field);
        //update the len
        ts_packet->len_partial+=pointer_field;
        //check if the packet is now full and valid
        if(ts_partial_full(ts_packet))
        {
          //The partial packet is full, we check the CRC32
          if(ts_check_crc32(ts_packet))
            ts_move_part_to_full(ts_packet); //Everything is perfect, the packet full is ok
        }
        else
        {
          log_message(log_module, MSG_FLOOD, "The data from the pointer field didn't finished the packet len: %d\n", ts_packet->len_partial);
          ts_packet->len_partial=0;
          ts_packet->status_partial=EMPTY;
        }
      }
    }
    //We don't have a pointer field or we have finished dealing with the pointer field
    //We check if a packet has been started before, just for information
    if(ts_packet->status_partial!=EMPTY)
    {
      log_message(log_module, MSG_FLOOD, "Unfinished packet and beginning of a new one, we drop the started one len: %d\n", ts_packet->len_partial);
      ts_packet->status_partial=EMPTY;
      ts_packet->len_partial=0;
    }
    //We've read the pointer field data, we skip it
    offset+=pointer_field;
    //We copy the data to the partial packet
    ts_packet->status_partial=STARTED;
    ts_packet->cc=header->continuity_counter;
    ts_packet->pid=buf_pid;
    ts_packet->len_partial=TS_PACKET_SIZE-offset;
    memcpy(ts_packet->data_partial,buf+offset,ts_packet->len_partial);
    tbl_h_t *tbl_struct=(tbl_h_t *)ts_packet->data_partial;
    ts_packet->expected_len_partial=HILO(tbl_struct->section_length)+BYTES_BFR_SEC_LEN;
    log_message(log_module, MSG_FLOOD, "Starting a packet PID %d cc %d len %d expected len %d\n",
                ts_packet->pid,
                ts_packet->cc,
                ts_packet->len_partial,
                ts_packet->expected_len_partial);
    log_message(log_module, MSG_FLOOD, "First bytes\t 0x%02x 0x%02x 0x%02x 0x%02x  0x%02x 0x%02x 0x%02x 0x%02x\n",
                ts_packet->data_partial[0],
                ts_packet->data_partial[1],
                ts_packet->data_partial[2],
                ts_packet->data_partial[3],
                ts_packet->data_partial[4],
                ts_packet->data_partial[5],
                ts_packet->data_partial[6],
                ts_packet->data_partial[7]);
       log_message(log_module, MSG_FLOOD, "Struct data\t table_id 0x%02x section_syntax_indicator 0x%02x section_length_hi 0x%02x section_length_lo 0x%02x transport_stream_id_hi 0x%02x transport_stream_id_lo 0x%02x version_number 0x%02x current_next_indicator 0x%02x last_section_number 0x%02x\n",
                tbl_struct->table_id,
                tbl_struct->section_syntax_indicator,
                tbl_struct->section_length_hi,
                tbl_struct->section_length_lo,
                tbl_struct->transport_stream_id_hi,
                tbl_struct->transport_stream_id_lo,
                tbl_struct->version_number,
                tbl_struct->current_next_indicator,
                tbl_struct->last_section_number);
    if(pointer_field==0)
    {
      //No pointer field_ we can check if the packet is full
      if(ts_partial_full(ts_packet))
      {
        //The partial packet is full, we check the CRC32
        if(ts_check_crc32(ts_packet))
          ts_move_part_to_full(ts_packet); //Everything is perfect, the packet full is ok
      }
    }
    else //pointer field, read the comment below
    {
      //we don't check if the packet is full here, this to avoid erasing the full if it has just been
      //filled with the pointer_field
      //The packet will be checked in the next call of this function, it basically add one packet delay
      //check for infomation and debugging
      if(ts_partial_full(ts_packet))
      {
        log_message(log_module, MSG_FLOOD, "Small full packet after a pointer field, this packet will be given at the next call. Len %d\n",ts_packet->len_partial);
        //In fact they can hide another small packet, ie a big ends, a small is full and another one starts
        if(offset+ts_packet->len_partial+7<TS_PACKET_SIZE)
        {
          //Note this part have to be rewritten
          log_message(log_module, MSG_INFO, "!!! Please contact me, you are on a strange transponder and I would need a dump to improve MuMuDVB !!!\n");
          log_message(log_module, MSG_FLOOD, "!!! There is another one !!! First bytes of the other one\t 0x%02x 0x%02x 0x%02x 0x%02x  0x%02x 0x%02x 0x%02x 0x%02x\n",
                buf[offset+ts_packet->len_partial+0],
                buf[offset+ts_packet->len_partial+1],
                buf[offset+ts_packet->len_partial+2],
                buf[offset+ts_packet->len_partial+3],
                buf[offset+ts_packet->len_partial+4],
                buf[offset+ts_packet->len_partial+5],
                buf[offset+ts_packet->len_partial+6],
                buf[offset+ts_packet->len_partial+7]);
          tbl_h_t *tbl_struct=(tbl_h_t *)(buf+offset+ts_packet->len_partial);
          log_message(log_module, MSG_FLOOD, "Struct data\t table_id 0x%02x section_syntax_indicator 0x%02x section_length_hi 0x%02x section_length_lo 0x%02x transport_stream_id_hi 0x%02x transport_stream_id_lo 0x%02x version_number 0x%02x current_next_indicator 0x%02x last_section_number 0x%02x\n",
                tbl_struct->table_id,
                tbl_struct->section_syntax_indicator,
                tbl_struct->section_length_hi,
                tbl_struct->section_length_lo,
                tbl_struct->transport_stream_id_hi,
                tbl_struct->transport_stream_id_lo,
                tbl_struct->version_number,
                tbl_struct->current_next_indicator,
                tbl_struct->last_section_number);
        }
      }
    }
  }
  else //It's a continuing packet
  {
    if(ts_packet->status_partial!=STARTED)
    {
      log_message(log_module, MSG_FLOOD, "Continuing packet and saved packet not started or full, can be a continuity error\n");
      ts_packet->status_partial=EMPTY;
    }
    else if(ts_packet->cc==header->continuity_counter)
    {
      log_message(log_module, MSG_FLOOD, "Duplicate packet, continuity counter: %d\n", ts_packet->cc);
    }
    else if(((ts_packet->cc+1)%16)!=header->continuity_counter)
    {
      log_message(log_module, MSG_FLOOD, "The continuity counter is not valid saved packet cc %d actual cc %d\n", ts_packet->cc, header->continuity_counter);
      ts_packet->status_partial=EMPTY;
    }
    else if(ts_packet->pid!=buf_pid)
    {
      log_message(log_module, MSG_FLOOD, "PID change. saved PID %d, actual pid %d\n", ts_packet->pid, buf_pid);
      ts_packet->status_partial=EMPTY;
    }
    else
    {
      //packet started and continuing packet, we append the data
      int copy_len;
      if(ts_packet->len_partial+(TS_PACKET_SIZE-offset)<MAX_TS_SIZE)
        copy_len=TS_PACKET_SIZE-offset;
      else
      {
        copy_len=MAX_TS_SIZE-ts_packet->len_partial;
        log_message( log_module, MSG_DETAIL,"The packet seems too big, we copy only some bits : %d instead of %d\n",copy_len,TS_PACKET_SIZE-offset);
      }
      memcpy(ts_packet->data_partial+ts_packet->len_partial,buf+offset,copy_len);//we add the packet to the buffer
      ts_packet->len_partial+=copy_len;
      ts_packet->cc=header->continuity_counter; //update cc
      log_message(log_module, MSG_FLOOD, "Continuing a packet PID %d cc %d len %d\n",ts_packet->pid,ts_packet->cc,ts_packet->len_partial);
      //we check if the packet is full
      if(ts_partial_full(ts_packet))
      {
        //The partial packet is full, we check the CRC32
        if(ts_check_crc32(ts_packet))
          ts_move_part_to_full(ts_packet); //Everything is perfect, the packet full is ok
      }
    }
  }

final_check:
  pthread_mutex_unlock(&ts_packet->packetmutex);
  //If the packet is full we return 1
  if(ts_packet->status_full==VALID)
  {
    log_message(log_module, MSG_DETAIL, "Packet full and valid PID %d length %d\n",
                ts_packet->pid,
                ts_packet->len_full);
    return 1;
  }
  return 0;
}


/** @brief move the partial packet to the full packet */
void ts_move_part_to_full(mumudvb_ts_packet_t *ts_packet)
{
  //copy the data
  memcpy(ts_packet->data_full,ts_packet->data_partial,MAX_TS_SIZE);
  ts_packet->len_full=ts_packet->len_partial;
  ts_packet->status_full=ts_packet->status_partial;
  ts_packet->len_partial=0;
  ts_packet->status_partial=EMPTY;
  log_message(log_module, MSG_FLOOD, "Packet full updated, len %d\n",ts_packet->len_full);
}

/**@brief Checking of the CRC32 of a raw buffer
 * return 1 if crc32 is ok, 0 otherwise
 * @param packet : the packet to be checked
 */
int ts_check_raw_crc32(unsigned char *data)
{
  int i,len;
  uint32_t crc32;
  tbl_h_t *tbl_struct;
  tbl_struct=(tbl_h_t *)data;

  //the real length (it cannot overflow due to the way tbl_h_t is made)
  len=HILO(tbl_struct->section_length)+BYTES_BFR_SEC_LEN;

  //CRC32 calculation
  //Test of the crc32
  crc32=0xffffffff;
  //we compute the CRC32
  //we have two ways: either we compute untill the end and it should be 0
  //either we exclude the 4 last bits and in should be equal to the 4 last bits
  for(i = 0; i < len; i++) {
    crc32 = (crc32 << 8) ^ crc32_table[((crc32 >> 24) ^ data[i])&0xff];
  }
  return (crc32 == 0);
}

/**@brief Checking of the CRC32
 * return 1 if crc32 is ok, 0 otherwise
 * @param packet : the packet to be checked
 */
int ts_check_crc32( mumudvb_ts_packet_t *packet)
{

  if(ts_check_raw_crc32(packet->data_partial)==0)
  {
    log_message( log_module,  MSG_DETAIL,"\tpacket BAD CRC32 PID : %d\n", packet->pid);
    //Bad CRC32
    packet->status_partial=EMPTY;
    packet->len_partial=0;
    return 0;
  }
  packet->status_partial=VALID;
  return 1;
}



/**@brief Tell if the partial packet is full
 * return 1 if full, 0 otherwise
 * @param packet : the packet to be checked
 */
int ts_partial_full( mumudvb_ts_packet_t *packet)
{
  //the real length
  if(packet->len_partial>=packet->expected_len_partial)
  {
    packet->status_partial=FULL;
    //we set the good length
    packet->len_partial=packet->expected_len_partial;
    return 1;
  }
  return 0;
}






/** @brief This function will return a pointer to the beginning of the payload for a ts_packet with payload_unit_start_indicator set to 1
 * It returns NULL in case of error
 *
 * @param buf : the received buffer from the card
 */
unsigned char *get_ts_begin(unsigned char *buf)
{

  ts_header_t *header;
  int ok=0;
  int delta;

  //mapping of the buffer onto the TS header
  header=(ts_header_t *)buf;

  //delta used to remove TS HEADER
  delta = TS_HEADER_LEN-1;

  //Sometimes there is some more data in the header, the adaptation field say it
  if (header->adaptation_field_control & 0x2)
    {
      log_message( log_module,  MSG_DEBUG, "Read TS : Adaptation field, len %d \n",buf[delta]);
      if((TS_PACKET_SIZE-delta-buf[delta])<0)
      {
        log_message(log_module, MSG_DETAIL, "Adaptation field too big 0x%02x, packet dropped\n",buf[delta]);
        return NULL;
      }
      delta += buf[delta] ;        // add adapt.field.len
    }
  if (header->adaptation_field_control & 0x1) //There is a payload
    {
      if (buf[delta]==0x00 && buf[delta+1]==0x00 && buf[delta+2]==0x01)
        {
          // -- PES/PS
          //tspid->id   = buf[j+3];
          log_message( log_module,  MSG_FLOOD, "#PES/PS ----- We ignore \n");
          ok=0;
        }
      else
          ok=1;
    }

  if (header->adaptation_field_control == 3)
    {
      log_message( log_module,  MSG_DEBUG, "adaptation_field_control 3\n");
    }

  if ((header->adaptation_field_control == 2)||(header->adaptation_field_control == 0))
    {
      log_message( log_module,  MSG_DEBUG, "adaptation_field_control %d ie no payload\n", header->adaptation_field_control);
      ok=0;
    }

  if(header->payload_unit_start_indicator) //It's the beginning of a new packet
  {
    if(ok)
    {
      int pointer_field=*(buf+delta);
      delta++;
      if(pointer_field!=0)
      {
        log_message(log_module, MSG_FLOOD, "Pointer field 0x%02x\n",pointer_field);
      }
      if((TS_PACKET_SIZE-delta-pointer_field)<0)
      {
        log_message(log_module, MSG_DETAIL, "Pointer field too big 0x%02x, packet dropped\n",pointer_field);
        return NULL;
      }
      return buf+delta+pointer_field; //we give the address of the beginning of the payload
      /*buf+delta+*1+pointer_field* because of pointer_field
      This is an 8-bit field whose value shall be the number of bytes, immediately following the pointer_field
      until the first byte of the first section that is present in the payload of the Transport Stream packet (so a value of 0x00 in
      the pointer_field indicates that the section starts immediately after the pointer_field). When at least one section begins in
      a given Transport Stream packet, then the payload_unit_start_indicator (refer to 2.4.3.2) shall be set to 1 and the first
      byte of the payload of that Transport Stream packet shall contain the pointer. When no section begins in a given
      Transport Stream packet, then the payload_unit_start_indicator shall be set to 0 and no pointer shall be sent in the
      payload of that packet.
      */
    }
  }
  return NULL;
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


  header=(pmt_t *)pmt->data_full;

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


