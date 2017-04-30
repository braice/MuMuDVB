/*
 * MuMuDVB - Stream a DVB transport stream.
 *
 * (C) 2004-2013 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.net
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
#include "rewrite.h"
#include "log.h"
#include <stdint.h>

extern uint32_t       crc32_table[256];
static char *log_module="PAT Rewrite: ";

/** @brief, tell if the pat have a newer version than the one recorded actually
 * In the PAT pid there is a field to say if the PAT was updated
 * This function check if it has changed (in order to rewrite the pat only once)
 * General Note : in case it change during streaming it can be a problem ane we would have to deal with re-autoconfiguration
 * Note this function can give flase positive since it doesn't check the CRC32
 *
 *@param rewrite_vars the parameters for pat rewriting 
 *@param buf : the received buffer
 */
int pat_need_update(rewrite_parameters_t *rewrite_vars, unsigned char *buf)
{
	pat_t       *pat=(pat_t*)(get_ts_begin(buf));


	if(pat) //It's the beginning of a new packet
	{
		/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
    sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
    and shall be the next table to become valid.*/
		if(pat->current_next_indicator == 0)
		{
			return 0;
		}
		if(pat->version_number!=rewrite_vars->pat_version)
		{
			log_message( log_module, MSG_DEBUG,"Need update. stored version : %d, new: %d\n",rewrite_vars->pat_version,pat->version_number);
			return 1;
		}
	}
	return 0;

}

/** @brief update the version using the dowloaded pat*/
void update_pat_version(rewrite_parameters_t *rewrite_vars)
{
	pat_t       *pat=(pat_t*)(rewrite_vars->full_pat->data_full);
	if(rewrite_vars->pat_version!=pat->version_number)
	{
		log_message( log_module, MSG_DEBUG,"New pat version. Old : %d, new: %d\n",rewrite_vars->pat_version,pat->version_number);
		if(rewrite_vars->pat_version!=-1)
			log_message( log_module, MSG_WARN,"The PAT version changed, so the channels changed probably. If you are using autoconfiguration it's safer to relaunch MuMuDVB or if the PIDs are set manually, check the PMTs. It can also happend when services are added/removed\n");
	}

	rewrite_vars->pat_version=pat->version_number;
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
int pat_channel_rewrite(rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channel,  unsigned char *buf, int curr_channel)
{
	ts_header_t *ts_header=(ts_header_t *)buf;
	pat_t       *pat=(pat_t*)(rewrite_vars->full_pat->data_full);
	pat_prog_t  *prog;
	unsigned long crc32;

	//destination buffer
	unsigned char buf_dest[TS_PACKET_SIZE];
	int buf_dest_pos=0;
	int delta=PAT_LEN;

	int section_length=0;
	int i;
	int new_section_length;

	if(ts_header->payload_unit_start_indicator)
	{
		log_message( log_module, MSG_DEBUG,"PAT rewrite : pointer field 0x%x \n", buf[TS_HEADER_LEN-1]);
	}
	section_length=HILO(pat->section_length);

	//lets start the copy
	//we copy the ts header and adapt it a bit
	//the continuity counter is updated elswhere
	ts_header->payload_unit_start_indicator=1;
	buf[TS_HEADER_LEN-1]=0;//we erase the pointer field
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
		prog=(pat_prog_t*)((char*)rewrite_vars->full_pat->data_full+delta);
		if(HILO(prog->program_number)!=0)
		{
			/*We check the transport stream id if present and the size of the packet*/
			/* + 4 for the CRC32*/
			if((buf_dest_pos+PAT_PROG_LEN+4<TS_PACKET_SIZE) &&
					(!channel->service_id || (channel->service_id == HILO(prog->program_number)) ))
			{
				for(i=0;i<channel->pid_i.num_pids;i++)
					if(channel->pid_i.pids[i]==HILO(prog->network_pid))
					{
						if(buf_dest_pos+PAT_PROG_LEN+4+1>TS_PACKET_SIZE) //The +4 is for CRC32 +1 is because indexing starts at 0
						{
							log_message( log_module, MSG_WARN,"The generated PAT is too big for channel %d : \"%s\", we skip the other pids/programs\n", curr_channel, channel->name);
							i=channel->pid_i.num_pids;
						}
						else
						{
							log_message( log_module, MSG_DETAIL,"NEW program for channel %d : \"%s\". PMT pid : %d\n", curr_channel, channel->name,channel->pid_i.pids[i]);
							/*we found a announce for a PMT PID in our stream, we keep it*/
							memcpy(buf_dest+buf_dest_pos,rewrite_vars->full_pat->data_full+delta,PAT_PROG_LEN);
							buf_dest_pos+=PAT_PROG_LEN;
						}
					}
			}
			else
				log_message( log_module, MSG_DEBUG,"Program dropped because of service_id. channel %d :\"%s\". service_id chan : %d service_id prog %d\n",
						curr_channel,
						channel->name,
						channel->service_id,
						HILO(prog->program_number));
		}
		delta+=PAT_PROG_LEN;
	}

	//we compute the new section length
	//section length is the size of the section after section_length (crc32 included : 4 bytes)
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
	memcpy(channel->generated_pat,buf_dest,TS_PACKET_SIZE);

	//Everything is Ok ....
	return 1;
}

/** @brief This function is called when a new PAT packet for all channels is there and we asked for rewrite
 * this function save the full PAT which will be the source PAT for all the channels
 */
void pat_rewrite_new_global_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars)
{
	/*Check the version before getting the full packet*/
	if(!rewrite_vars->pat_needs_update)
	{
		rewrite_vars->pat_needs_update=pat_need_update(rewrite_vars,ts_packet);
	}
	/*We need to update the full packet, we download it*/
	if(rewrite_vars->pat_needs_update)
	{
		while(get_ts_packet(ts_packet,rewrite_vars->full_pat))
		{
			ts_packet=NULL; // next call we only POP packets from the stack
			pat_t       *pat=(pat_t*)(rewrite_vars->full_pat->data_full);
			/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
      	  	  sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
      	  	  and shall be the next table to become valid.*/
			if(pat->current_next_indicator == 0)
			{
				log_message( log_module, MSG_FLOOD,"PAT not yet valid, we get a new one (current_next_indicator == 0)\n");
			}
			else
			{
				log_message( log_module, MSG_DEBUG,"Full PAT updated\n");
				/*We've got the FULL PAT packet*/
				update_pat_version(rewrite_vars);
				rewrite_vars->pat_needs_update=0;
				rewrite_vars->full_pat_ok=1;
				ts_display_pat(log_module,rewrite_vars->full_pat->data_full);
			}
		}
	}
	//To avoid the duplicates, we have to update the continuity counter
	rewrite_vars->pat_continuity_counter++;
	rewrite_vars->pat_continuity_counter= rewrite_vars->pat_continuity_counter % 32;
}


/** @brief This function is called when a new PAT packet for a channel is there and we asked for rewrite
 * This function copy the rewritten PAT to the buffer. And checks if the PAT was changed so the rewritten version have to be updated
 */
int pat_rewrite_new_channel_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channel, int curr_channel)
{
	if(rewrite_vars->full_pat_ok ) //the global full pat is ok
	{
		/*We check if it's the first pat packet ? or we send it each time ?*/
		/*We check if the versions corresponds*/
		if(!rewrite_vars->pat_needs_update
				&& channel->channel_ready>=READY
				&& channel->generated_pat_version!=rewrite_vars->pat_version)//We check the version only if the PAT is not currently updated
		{
			log_message( log_module, MSG_DEBUG,"We need to rewrite the PAT for the channel %d : \"%s\"\n", curr_channel, channel->name);
			/*They mismatch*/
			/*We generate the rewritten packet*/
			if(pat_channel_rewrite(rewrite_vars, channel, ts_packet, curr_channel))
			{
				/*We update the version*/
				channel->generated_pat_version=rewrite_vars->pat_version;
			}
			else
			{
				log_message( log_module, MSG_DEBUG,"ERROR with the PAT for the channel %d : \"%s\"\n", curr_channel, channel->name);
				return 0;
			}
		}
		if(channel->generated_pat_version==rewrite_vars->pat_version)
		{
			/*We send the new PAT from channel->generated_pat*/
			memcpy(ts_packet,channel->generated_pat,TS_PACKET_SIZE);
			//To avoid the duplicates, we have to update the continuity counter
			set_continuity_counter(ts_packet,rewrite_vars->pat_continuity_counter);
		}
		else
		{
			log_message( log_module, MSG_DEBUG,"Bad pat channel version, we don't send the pat for the channel %d : \"%s\"\n", curr_channel, channel->name);
			return 0;
		}
	}
	else
	{
		log_message( log_module, MSG_DEBUG,"We need a global pat update, we don't send the pat for the channel %d : \"%s\"\n", curr_channel, channel->name);
		return 0;
	}
	return 1;

}



