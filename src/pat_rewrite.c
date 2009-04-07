/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) 2004-2009 Brice DUBOST
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

/**@file
 * @brief This file contains the function for rewriting the pat pid
 *
 * The pat rewrite is made to announce only the video stream associated with the channel in the PAT pid
 * Some set top boxes need it.
 */

#include <stdlib.h>
#include <string.h>

#include "mumudvb.h"
#include "ts.h"
#include "pat_rewrite.h"
#include <stdint.h>

extern uint32_t       crc32_table[256];


/** @Brief, tell if the pat have a newer version than the one recorded actually
 * In the PAT pid there is a field to say if the PAT was updated
 * This function check if it has changed (in order to rewrite the pat only once)
 * General Note : in case it change during streaming it can be a problem ane we would have to deal with re-autoconfiguration
 *
 *@param rewrite_vars the parameters for pat rewriting 
 *@param buf : the received buffer
 */
int pat_need_update(pat_rewrite_parameters_t *rewrite_vars, unsigned char *buf)
{
  pat_t       *pat=(pat_t*)(buf+TS_HEADER_LEN);
  if(pat->version_number!=rewrite_vars->pat_version)
    {
      log_message(MSG_DEBUG,"Pat rewrite : New pat version. Old : %d, new: %d\n",rewrite_vars->pat_version,pat->version_number);
      return 1;
    }
  return 0;

}

/** @brief update the version using the dowloaded pat*/
void update_version(pat_rewrite_parameters_t *rewrite_vars)
{
  pat_t       *pat=(pat_t*)(rewrite_vars->full_pat->packet);
  
  rewrite_vars->pat_version=pat->version_number;
}

/** @brief Just a small function to change the continuity counter of a packet
 * This function will overwrite the continuity counter of the packet with the one given in argument
 *
 */
void pat_rewrite_set_continuity_counter(unsigned char *buf,int continuity_counter)
{
  ts_header_t *ts_header=(ts_header_t *)buf;
  ts_header->continuity_counter=continuity_counter;
}

/** @brief Main function for pat rewriting 
 * The goal of this function is to make a new pat with only the announement for the streamed channel
 * by default it contains all the channels of the transponder. For each channel descriptor this function will search
 * the pmt pid of the channel in the given pid list. if found it keeps it otherwise it drops.
 * At the end, a new CRC32 is computed. The buffer is overwritten, so the caller have to save it before.
 *
 * @param rewrite_vars the parameters for pat rewriting
 * @param channels The array of channels
 * @param curr_channel the channel for wich we want to generate a PAT
 * @param buf : the received buffer, to get the TS header
 */
int pat_channel_rewrite(pat_rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channels, int curr_channel, unsigned char *buf)
{
  ts_header_t *ts_header=(ts_header_t *)buf;
  pat_t       *pat=(pat_t*)(rewrite_vars->full_pat->packet);
  pat_prog_t  *prog;
  unsigned long crc32;

  //destination buffer
  unsigned char buf_dest[188];
  int buf_dest_pos=0;
  int delta=PAT_LEN;

  int section_length=0;
  int i;
  int new_section_length;


  section_length=HILO(pat->section_length);

  //lets start the copy
  //we copy the ts header and adapt it a bit
  //the continuity counter is updated elswhere
  ts_header->payload_unit_start_indicator=1;
  memcpy(buf_dest,ts_header,TS_HEADER_LEN);
  //we copy the modified PAT header
  pat->current_next_indicator=1; //applicable immediately
  pat->section_number=0;         //only one pat
  pat->last_section_number=0;
  memcpy(buf_dest+TS_HEADER_LEN,pat,PAT_LEN);
	   
  buf_dest_pos=TS_HEADER_LEN+PAT_LEN;


  //We copy what we need : EIT announce and present PMT announce
  //strict comparaison due to the calc of section len cf down
  while((delta+PAT_PROG_LEN)<(section_length))
    {
      prog=(pat_prog_t*)((char*)rewrite_vars->full_pat->packet+delta);
      if(HILO(prog->program_number)==0)
	{
	  //we found the announce for the EIT pid
	  memcpy(buf_dest+buf_dest_pos,rewrite_vars->full_pat->packet+delta,PAT_PROG_LEN);
	  buf_dest_pos+=PAT_PROG_LEN;
	}
      else
	{
	  for(i=0;i<channels[curr_channel].num_pids;i++)
	    if(channels[curr_channel].pids[i]==HILO(prog->network_pid))
	      {
		log_message(MSG_DEBUG,"Pat rewrite : NEW program for channel %d : \"%s\". PTM pid : %d\n", curr_channel, channels[curr_channel].name,channels[curr_channel].pids[i]);
		//we found a announce for a PMT pid in our stream, we keep it
		memcpy(buf_dest+buf_dest_pos,rewrite_vars->full_pat->packet+delta,PAT_PROG_LEN);
		buf_dest_pos+=PAT_PROG_LEN;
		if(buf_dest_pos+4>TS_PACKET_SIZE) //The +4 is for CRC32
		  {
		    log_message(MSG_WARN,"Pat rewrite : The generated PAT is too big for channel %d : \"%s\", we skip the other pids\n", curr_channel, channels[curr_channel].name);
		    i=channels[curr_channel].num_pids;
		  }
	      }
	}
      delta+=PAT_PROG_LEN;
    }
 
  //we compute the new section length
  //section lenght is the size of the section after section_length (crc32 included : 4 bytes)
  //so it's size of the crc32 + size of the pat prog + size of the pat header - 3 first bytes (the pat header until section length included)
  //Finally it's total_pat_data_size + 1
  new_section_length=buf_dest_pos-TS_HEADER_LEN + 1;

  //We write the new section length
  buf_dest[1+TS_HEADER_LEN]=(((new_section_length)&0x0f00)>>8)  | (0xf0 & buf_dest[1+TS_HEADER_LEN]);
  buf_dest[2+TS_HEADER_LEN]=new_section_length & 0xff;


  //CRC32 calculation inspired by the xine project
  //Now we must adjust the CRC32
  //we compute the CRC32
  crc32=0xffffffff;
  for(i = 0; i < new_section_length-1; i++) {
    crc32 = (crc32 << 8) ^ crc32_table[((crc32 >> 24) ^ buf_dest[i+TS_HEADER_LEN])&0xff];
  }


  //We write the CRC32 to the buffer
  /** @todo check if Is this one safe with little/big endian ?*/
  buf_dest[buf_dest_pos]=(crc32>>24) & 0xff;
  buf_dest_pos+=1;
  buf_dest[buf_dest_pos]=(crc32>>16) & 0xff;
  buf_dest_pos+=1;
  buf_dest[buf_dest_pos]=(crc32>>8) & 0xff;
  buf_dest_pos+=1;
  buf_dest[buf_dest_pos]=crc32 & 0xff;
  buf_dest_pos+=1;
  

  //Padding with 0xFF 
  memset(buf_dest+buf_dest_pos,0xFF,TS_PACKET_SIZE-buf_dest_pos);

  //We copy the result to the intended buffer
  memcpy(rewrite_vars->generated_pats[curr_channel],buf_dest,TS_PACKET_SIZE);

  //Everything is Ok ....
  return 1;


}



