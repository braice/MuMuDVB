/*
 * MuMuDVB - Stream a DVB transport stream.
 *
 * (C) 2004-2013 Brice DUBOST
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
 * @brief This file contains the function for rewriting the EIT pid
 *
 * The EIT rewrite is made to announce only the video stream associated with the channel in the EIT pid
 * It avoids to have ghost channels which can disturb the clients
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mumudvb.h"
#include "ts.h"
#include "rewrite.h"
#include "log.h"
#include <stdint.h>



static char *log_module="EIT rewrite: ";


/** @brief Display the contents of the EIT table
 *
 */
void eit_display_header(eit_t *eit)
{

        log_message( log_module, MSG_DEBUG,"--- EIT TABLE ---");

        log_message( log_module, MSG_DEBUG,"table_id\t\t\t 0x%x",eit->table_id);
        log_message( log_module, MSG_DEBUG,"section_length\t\t %d",HILO(eit->section_length));
        log_message( log_module, MSG_DEBUG,"section_syntax_indicator\t 0x%x",eit->section_syntax_indicator);
        log_message( log_module, MSG_DEBUG,"service_id\t\t\t 0x%x",HILO(eit->service_id));
        log_message( log_module, MSG_DEBUG,"current_next_indicator\t 0x%x",eit->current_next_indicator);
        log_message( log_module, MSG_DEBUG,"version_number\t\t 0x%x",eit->version_number);
        log_message( log_module, MSG_DEBUG,"section_number\t\t 0x%x",eit->section_number);
        log_message( log_module, MSG_DEBUG,"last_section_number\t 0x%x",eit->last_section_number);
        log_message( log_module, MSG_DEBUG,"transport_stream_id\t 0x%x",HILO(eit->transport_stream_id));
        log_message( log_module, MSG_DEBUG,"original_network_id\t 0x%x",HILO(eit->original_network_id));
        log_message( log_module, MSG_DEBUG,"segment_last_section_number\t 0x%x",eit->segment_last_section_number);
        log_message( log_module, MSG_DEBUG,"segment_last_table_id\t\t 0x%x",eit->segment_last_table_id);

}

void eit_show_stored(rewrite_parameters_t *rewrite_vars)
{
	eit_packet_t *actual_eit=rewrite_vars->eit_packets;
	int i;
	while(actual_eit!=NULL)
	{
		log_message( log_module, MSG_FLOOD,"stored EIT SID %d table_id 0X%02x version %d last section number %d",
				actual_eit->service_id,
				actual_eit->table_id,
				actual_eit->version,
				actual_eit->last_section_number);
		for(i=0;i<=actual_eit->last_section_number;i++)
			if(actual_eit->sections_stored[i])
				log_message( log_module, MSG_FLOOD,"\t stored section %d",i);
		actual_eit=actual_eit->next;
	}


}

/**@brief increment the table_id
 *
 */
uint8_t eit_next_table_id(uint8_t table_id)
{
	if(table_id==0)
		return 0x4E;
	if(table_id==0x4E)
		return 0x50;
	if(table_id==0x5F)
		return 0x4E;
	return table_id+1;
}

void eit_free_packet_contents(eit_packet_t *eit_packet)
{
	//free the different packets
	for(int i=0;i<256;i++)
		if(eit_packet->full_eit_sections[i]!=NULL)
			free(eit_packet->full_eit_sections[i]);

	//we don't break the chained list
	eit_packet_t *next;
	next=eit_packet->next;
	memset (eit_packet, 0, sizeof( eit_packet_t));//we clear it
	eit_packet->next=next;
}


/** @brief Try to find the eit specified by id, if not found create a new one.
 * if the service is not found, it returns a pointer to the new service, and NULL if
 * the service is found or run out of memory.
 *
 */
eit_packet_t *eit_new_packet(rewrite_parameters_t *rewrite_vars, int sid, uint8_t table_id)
{
	eit_packet_t *actual_eit=rewrite_vars->eit_packets;

	//go to the last one or return the already found
	while(actual_eit && actual_eit->next!=NULL)
	{
		if((actual_eit->service_id==sid)&&(actual_eit->table_id==table_id))
			return actual_eit;
		actual_eit=actual_eit->next;
	}

	log_message( log_module, MSG_FLOOD,"EIT Stored before allocation sid %d table id 0x%02x",sid,table_id);
	eit_show_stored(rewrite_vars);

	if(actual_eit==NULL)
	{
		rewrite_vars->eit_packets=calloc(1,sizeof(eit_packet_t));
		actual_eit=rewrite_vars->eit_packets;
	}
	else
	{
		actual_eit->next=calloc(1,sizeof(eit_packet_t));
		actual_eit=actual_eit->next;
	}

	if(actual_eit==NULL)
	{
		log_message( log_module, MSG_ERROR,"Problem with calloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		return NULL;
	}

	return actual_eit;

}

/** @brief, Search an EIT in the stored list
 *
 */
eit_packet_t *eit_find_by_tsid(rewrite_parameters_t *rewrite_vars,int service_id, uint8_t table_id) //and table id
{
	eit_packet_t *found=NULL;
	eit_packet_t *actual_eit=rewrite_vars->eit_packets;

	while(found==NULL && actual_eit!=NULL)
	{
		if((actual_eit->service_id==service_id)&&(actual_eit->table_id==table_id))
			found=actual_eit;
		else
			actual_eit=actual_eit->next;
	}

	return found;
}

/** @brief, tell if the eit have a newer version than the one recorded actually
 * In the EIT pid there is a field to say if the EIT was updated
 * This function check if it has changed (in order to rewrite the eit only once)
 * Note this function can give false positive since it doesn't check the CRC32
 *
 *@param rewrite_vars the parameters for eit rewriting
 *@param buf : the received buffer
 */
int eit_need_update(rewrite_parameters_t *rewrite_vars, unsigned char *buf, int raw)
{

	//Get the SID of the new EIT
	//loop over the stored EIT
	//if found and version > : need update
	//if not found need update


	eit_t       *eit;
	//if it's a raw TS packet we search for the beginning
	if(raw)
		eit=(eit_t*)(get_ts_begin(buf));
	else
		eit=(eit_t*)(buf);

	eit_packet_t *eit_packet;
	if(eit) //It's the beginning of a new packet
	{
		// 0x4E event_information_section - actual_transport_stream, present/following
		// 0x50 to 0x5F event_information_section - actual_transport_stream, schedule
		//all these table id_ which could have different version number for the same service
		if((eit->table_id!=0x4E)&&((eit->table_id&0xF0)!=0x50))
			return 0;
		/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the table
    	sent is currently applicable.*/
		if(eit->current_next_indicator == 0)
			return 0;

		eit_packet=eit_find_by_tsid(rewrite_vars,HILO(eit->service_id),eit->table_id);
		if(eit_packet==NULL)
		{
			log_message( log_module, MSG_DETAIL,"EIT sid %d table id 0x%02x not stored, need update.",
					HILO(eit->service_id),eit->table_id);
			return 1;
		}

		if(eit->version_number!=eit_packet->version)
		{
			log_message( log_module, MSG_DETAIL,"EIT sid %d need update. stored version : %d, new: %d",
					HILO(eit->service_id),
					eit_packet->version,
					eit->version_number);
			return 1;
		}
		if(!eit_packet->sections_stored[eit->section_number] )
		{
			log_message( log_module, MSG_DETAIL,"EIT sid %d new section %d version : %d",
					HILO(eit->service_id),
					eit->section_number,
					eit->version_number);
			return 1;
		}
	}
	return 0;
}

/** @brief This function is called when a new EIT packet for all channels is there and we asked for rewrite
 * this function save the full EIT for each service.
 * @return return 1 when the packet is updated
 */
int eit_rewrite_new_global_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars)
{
	eit_t       *eit=NULL;
	/*Check the version before getting the full packet*/
	if(!rewrite_vars->eit_needs_update)
		rewrite_vars->eit_needs_update=eit_need_update(rewrite_vars,ts_packet,1);

	/*We need to update the full packet, we download it*/
	if(rewrite_vars->eit_needs_update )
	{
		if(get_ts_packet(ts_packet,rewrite_vars->full_eit))
		{

			//We check if we have to store this new EIT packet (CRC32 can make false alarms)
			if(!eit_need_update(rewrite_vars,rewrite_vars->full_eit->data_full,0))
			{
				return 0;
			}

			log_message( log_module, MSG_DETAIL,"New full EIT for update");
			//For debugging purposes
			eit=(eit_t*)(rewrite_vars->full_eit->data_full);
			eit_display_header(eit);

			eit_packet_t *eit_packet;
			eit_packet=eit_new_packet(rewrite_vars,HILO(eit->service_id), eit->table_id);
			if(NULL==eit_packet)
				return 0;

			if(eit->version_number!=eit_packet->version)
			{
				log_message( log_module, MSG_DETAIL,"New version for EIT sid %d need update. stored version : %d, new: %d",
						HILO(eit->service_id),
						eit_packet->version,
						eit->version_number);
				//New version so we clear all contents
				eit_free_packet_contents(eit_packet);
			}

			eit_packet->last_section_number = eit->last_section_number;
			eit_packet->version=eit->version_number;
			eit_packet->service_id = HILO(eit->service_id);
			eit_packet->table_id = eit->table_id;
			eit_packet->full_eit_ok=1;
			/*We've got the FULL EIT packet*/
			//we copy the data to the right section
			if(eit_packet->full_eit_sections[eit->section_number]==NULL)
				eit_packet->full_eit_sections[eit->section_number]=calloc(1,sizeof(mumudvb_ts_packet_t));
			if(eit_packet->full_eit_sections[eit->section_number]==NULL)
			{
				log_message( log_module, MSG_ERROR,"Problem with calloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
				return 0;
			}
			memcpy(eit_packet->full_eit_sections[eit->section_number], rewrite_vars->full_eit,sizeof( mumudvb_ts_packet_t));
			//We store that we saw this section number
			eit_packet->sections_stored[eit->section_number]=1;
			log_message( log_module, MSG_DETAIL,"Full EIT updated. sid %d section number %d, last_section_number %d\n",
					eit_packet->service_id,
					eit->section_number,
					eit_packet->last_section_number);


			rewrite_vars->eit_needs_update = 0;

			log_message( log_module, MSG_WARN,"!!Stored after update");
			eit_show_stored(rewrite_vars);

		}
	}
  return 0;
}






/** @brief This function is called when a new EIT packet for a channel is there and we asked for rewrite
 * This function copy the rewritten EIT to the buffer. And checks if the EIT was changed so the rewritten version have to be updated
 */
void eit_rewrite_new_channel_packet(unsigned char *ts_packet, rewrite_parameters_t *rewrite_vars, mumudvb_channel_t *channel,
		multicast_parameters_t *multicast_vars, unicast_parameters_t *unicast_vars, mumudvb_chan_and_pids_t *chan_and_pids,fds_t *fds)
{
	int i=0;
	//If payload unit start indicator , we will send all the present EIT for this service, otherwise nothing
	//We generate the TS packets on by one, and for each one, we check if we have to send
	//Otherwise we skip the packet


	//If payload unit start indicator, we will send all the present EIT for this service, otherwise nothing
	//just a matter to send an EIT per service only if an EIT is starting in the stream,
	//the better way would be an EIT starting and corresponding to this SID but, it's more difficult to get this information
	ts_header_t *ts_header=(ts_header_t *)ts_packet;
	if(!(ts_header->payload_unit_start_indicator))
		return;

	//If there is an EIT PID sorted for this channel
	eit_packet_t *eit_pkt;
	uint8_t section_start;
	//we check we start with a valid section
	if((channel->eit_table_id_to_send!=0x4E)&&((channel->eit_table_id_to_send&0xF0)!=0x50))
		channel->eit_table_id_to_send=0x4E;

	//We search for the EIT packet to send, if not found we loop on the sections
	section_start=channel->eit_table_id_to_send;
	do
	{
		eit_pkt=eit_find_by_tsid(rewrite_vars,channel->service_id,channel->eit_table_id_to_send);
		//loop over the table id
		if((eit_pkt==NULL)||(!eit_pkt->full_eit_ok))
		{
			channel->eit_table_id_to_send=eit_next_table_id(channel->eit_table_id_to_send);
			eit_pkt=NULL;
		}
	}
	while((eit_pkt==NULL)&&(channel->eit_table_id_to_send!=section_start));

	//we go away if there is no EIT packet to send
	if(eit_pkt==NULL)
		return;
	//search for the next section we can send
	//just in case we have a new version with less sections
	channel->eit_section_to_send=channel->eit_section_to_send % (eit_pkt->last_section_number+1);
	//the real search
	while((i<=eit_pkt->last_section_number)&&(!eit_pkt->sections_stored[channel->eit_section_to_send]))
	{
		channel->eit_section_to_send++;
		channel->eit_section_to_send=channel->eit_section_to_send % (eit_pkt->last_section_number+1);
		i++;
	}
	//if nothing found
	if(!eit_pkt->sections_stored[channel->eit_section_to_send])
	{
		//bye (we should be here BTW) but we avoid to stay on invalid packet by going to next section
		channel->eit_table_id_to_send=eit_next_table_id(channel->eit_table_id_to_send);
		return;
	}

	//ok we send this!
	mumudvb_ts_packet_t *pkt_to_send;
	int data_left_to_send,sent;
	unsigned char send_buf[TS_PACKET_SIZE];
	ts_header=(ts_header_t *)send_buf;
	pkt_to_send=eit_pkt->full_eit_sections[channel->eit_section_to_send];
	data_left_to_send=pkt_to_send->full_buffer_len;
	sent=0;
	//log_message(log_module,MSG_FLOOD,"Sending EIT to channel %s (sid %d) section %d table_id 0x%02x data_len %d",
	//		channel->name,
	//		channel->service_id,
	//		channel->eit_section_to_send,
	//		channel->eit_table_id_to_send,
	//		data_left_to_send);
	while(data_left_to_send>0)
	{
		int header_len;
		memset(send_buf,0,TS_PACKET_SIZE*sizeof(unsigned char));
		//we fill the TS header
		ts_header->sync_byte=0x47;
		if(sent==0)
		{
			ts_header->payload_unit_start_indicator=1;
			header_len=TS_HEADER_LEN; //includes the pointer field
		}
		else
			header_len=TS_HEADER_LEN-1; //the packet has started, we don't count the pointer field
		ts_header->pid_lo=18;							//specify the PID
		ts_header->adaptation_field_control=1;			//always one
		ts_header->continuity_counter=channel->eit_cc;	//continuity counter
		channel->eit_cc++;
		channel->eit_cc= channel->eit_cc % 16;
		//We send the data
		//plus one because of pointer field
		if(data_left_to_send>(TS_PACKET_SIZE-header_len))
		{
			memcpy(send_buf+header_len,pkt_to_send->data_full+sent,(TS_PACKET_SIZE-header_len)*sizeof(unsigned char));
			sent+=(TS_PACKET_SIZE-header_len);
			data_left_to_send-=(TS_PACKET_SIZE-header_len);
		}
		else
		{
			memcpy(send_buf+header_len,pkt_to_send->data_full+sent,data_left_to_send*sizeof(unsigned char));
			sent+=data_left_to_send;
			//Padding with OxFF
			memset(send_buf+header_len+data_left_to_send,
						0xFF,
						(TS_PACKET_SIZE-(header_len+data_left_to_send))*sizeof(unsigned char));
			data_left_to_send=0;
		}
		//NOW we really send the data over the network
		// we fill the channel buffer
		memcpy(channel->buf + channel->nb_bytes, send_buf, TS_PACKET_SIZE*sizeof(unsigned char));
		channel->nb_bytes += TS_PACKET_SIZE;
		//The buffer is full, we send it
		if ((!multicast_vars->rtp_header && ((channel->nb_bytes + TS_PACKET_SIZE) > MAX_UDP_SIZE))
				||(multicast_vars->rtp_header && ((channel->nb_bytes + RTP_HEADER_LEN + TS_PACKET_SIZE) > MAX_UDP_SIZE)))
		{
			uint64_t now_time;
			now_time=get_time();
			send_func(channel, now_time, unicast_vars, multicast_vars, chan_and_pids, fds);
		}

	}

	//We update which section we want to send
	channel->eit_section_to_send++;
	channel->eit_section_to_send=channel->eit_section_to_send % (eit_pkt->last_section_number+1);
	//if we reached the end, we go to the next table_id
	if(channel->eit_section_to_send==0)
		channel->eit_table_id_to_send=eit_next_table_id(channel->eit_table_id_to_send);


}


