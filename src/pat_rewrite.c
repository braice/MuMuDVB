/* 
 * mumudvb - UDP-ize a DVB transport stream.
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


#include <stdlib.h>
#include <string.h>

#include "mumudvb.h"
#include "ts.h"

extern unsigned long       crc32_table[256];


int
pat_rewrite(unsigned char *buf,int num_pids, int *pids)
{
  int i,pos_buf,buf_pos;
  

  //destination buffer
  unsigned char buf_dest[188];
  int buf_dest_pos=0;

  pat_t       *pat=(pat_t*)(buf+TS_HEADER_LEN);
  pat_prog_t  *prog;
  int delta=PAT_LEN+TS_HEADER_LEN;
  int section_length=0;
  int new_section_length;
  unsigned long crc32;
  unsigned long calc_crc32;


  //PAT reading
  section_length=HILO(pat->section_length);
  if((section_length>(TS_PACKET_SIZE-TS_HEADER_LEN)) && section_length)
    {
      if (section_length)
	{
	  log_message( MSG_INFO,"PAT too big : %d, don't know how rewrite, sent as is\n", section_length);
	}
      else //empty PAT
	{
	  return 1;
	}
      return 0; //we sent as is
    }
  //CRC32
  //CRC32 calculation taken from the xine project
  //Test of the crc32
  calc_crc32=0xffffffff;
  //we compute the CRC32
  for(i = 0; i < section_length-1; i++) {
    calc_crc32 = (calc_crc32 << 8) ^ crc32_table[(calc_crc32 >> 24) ^ buf[i+TS_HEADER_LEN]];
  }
 
  crc32=0x00000000;

  crc32|=buf[TS_HEADER_LEN+section_length+3-4]<<24;
  crc32|=buf[TS_HEADER_LEN+section_length+3-4+1]<<16;
  crc32|=buf[TS_HEADER_LEN+section_length+3-4+2]<<8;
  crc32|=buf[TS_HEADER_LEN+section_length+3-4+3];
  
  if((calc_crc32-crc32)!=0)
    {
      //Bad CRC32
      return 1; //We don't send this PAT
    }

/*   fprintf (stderr, "table_id %x ",pat->table_id); */
/*   fprintf (stderr, "dummy %x ",pat->dummy); */
/*   fprintf (stderr, "ts_id 0x%04x ",HILO(pat->transport_stream_id)); */
/*   fprintf (stderr, "section_length %d ",HILO(pat->section_length)); */
/*   fprintf (stderr, "version %i ",pat->version_number); */
/*   fprintf (stderr, "last_section_number %x ",pat->last_section_number); */
/*   fprintf (stderr, "\n"); */


  //sounds good, lets start the copy
  //we copy the ts header
  for(i=0;i<TS_HEADER_LEN;i++)
    buf_dest[i]=buf[i];
  //we copy the PAT header
  for(i=TS_HEADER_LEN;i<TS_HEADER_LEN+PAT_LEN;i++)
    buf_dest[i]=buf[i];

  buf_dest_pos=TS_HEADER_LEN+PAT_LEN;

  //We copy what we need : EIT announce and present PMT announce
  //strict comparaison due to the calc of section len cf down
  while((delta+PAT_PROG_LEN)<(section_length+TS_HEADER_LEN))
    {
      prog=(pat_prog_t*)((char*)buf+delta);
      if(HILO(prog->program_number)==0)
	{
	  //we found the announce for the EIT pid
	  for(pos_buf=0;pos_buf<PAT_PROG_LEN;pos_buf++)
	    buf_dest[buf_dest_pos+pos_buf]=buf[pos_buf+delta];
	  buf_dest_pos+=PAT_PROG_LEN;
	}
      else
	{
	  for(i=0;i<num_pids;i++)
	    if(pids[i]==HILO(prog->network_pid))
	      {
		//we found a announce for a PMT pid in our stream, we keep it
		for(pos_buf=0;pos_buf<PAT_PROG_LEN;pos_buf++)
		  buf_dest[buf_dest_pos+pos_buf]=buf[pos_buf+delta];
		buf_dest_pos+=PAT_PROG_LEN;
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


  //CRC32 calculation taken from the xine project
  //Now we must adjust the CRC32
  //we compute the CRC32
  crc32=0xffffffff;
  for(i = 0; i < new_section_length-1; i++) {
    crc32 = (crc32 << 8) ^ crc32_table[(crc32 >> 24) ^ buf_dest[i+TS_HEADER_LEN]];
  }


  //We write the CRC32 to the buffer
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


  //We copy the result to the original buffer
  for(buf_pos=0;buf_pos<TS_PACKET_SIZE;buf_pos++)
    buf[buf_pos]=buf_dest[buf_pos];

  //Everything is Ok ....
  return 0;

}

