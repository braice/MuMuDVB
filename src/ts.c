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

#define NO_START 0
#define START_TS 1
#define START_SECTION 2

void add_ts_packet_data(unsigned char *buf, mumudvb_ts_packet_t *pkt, int data_left, int start_flag, int pid, int cc);


/** @brief This function will join the 188 bytes packet until the PMT/PAT/SDT/EIT/... is full
 * Once it's full we check the CRC32 and say if it's ok or not
 * There is two important mpeg2-ts fields to do that
 *  * the continuity counter which is incremented for each packet
 *  * The payload_unit_start_indicator which says if it's the first packet
 *
 * When a packet is splitted in 188 bytes packets, there must be no other PID between two sub packets
 *
 * Return 1 when there is one packet full and OK
 *
 * @param buf : the received buffer from the card
 * @param ts_packet : the packet to be completed
 */
int get_ts_packet(unsigned char *buf, mumudvb_ts_packet_t *pkt)
{
	//see doc/diagrams/TS_packet_getting_all_cases.pdf for documentation
	pthread_mutex_lock(&pkt->packetmutex);
	//We check if there is already a full packet, in this case we remove one
	if(pkt->full_number > 0)
	{
		log_message( log_module,  MSG_FLOOD, "Full packet left: %d, we remove one\n",pkt->full_number);
		pkt->full_number--;
		//We update the size of the buffer
		pkt->full_buffer_len-=pkt->len_full;
		//if there is one packet left, we put it in the full data and remove one packet
		if(pkt->full_number > 0)
		{
			log_message( log_module,  MSG_FLOOD, "Remove one packet size %d, but another size: %d\n",pkt->len_full, pkt->full_lengths[1]);
			//We move the data inside the buffer full
			memmove(pkt->buffer_full,pkt->buffer_full+pkt->len_full,pkt->full_buffer_len);
			//we update the lengths of the full packets
			memmove(pkt->full_lengths,pkt->full_lengths+1,(MAX_FULL_PACKETS-1)*sizeof(int));
			//we update the length
			pkt->len_full= pkt->full_lengths[0];
			//we update the data
			memcpy(pkt->data_full,pkt->buffer_full,pkt->len_full);
		}
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
			pthread_mutex_unlock(&pkt->packetmutex);
			return (pkt->full_number > 0);
		}
	}
	else if (header->adaptation_field_control & 0x1)
	{
		if (buf[offset]==0x00 && buf[offset+1]==0x00 && buf[offset+2]==0x01)
		{
			// -- PES/PS
			//tspid->id   = buf[j+3];
			log_message( log_module,  MSG_FLOOD, "#PES/PS ----- We ignore \n");
			pthread_mutex_unlock(&pkt->packetmutex);
			return (pkt->full_number > 0);
		}
	}
	if (header->adaptation_field_control == 3)
	{
		log_message( log_module,  MSG_DEBUG, "adaptation_field_control 3\n");
		pthread_mutex_unlock(&pkt->packetmutex);
		return (pkt->full_number > 0);
	}


	//We are now at the beginning of the Transport stream packet, we check if there is a pointer field
	//the pointer fields tells if there is the end of the previous packet before the beginning of a new one
	//and how long is this data
	if(header->payload_unit_start_indicator) //There is AT LEAST one packet beginning here
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
				pkt->status_partial=EMPTY;
				pthread_mutex_unlock(&pkt->packetmutex);
				return (pkt->full_number > 0);
			}
			//We append the data of the ending packet
			add_ts_packet_data(buf+offset, pkt, pointer_field, NO_START, buf_pid, header->continuity_counter);
		}
		//we skip the pointer field_data
		offset+=pointer_field;
		//We add the data of the new packet
		add_ts_packet_data(buf+offset, pkt,TS_PACKET_SIZE-offset , START_TS, buf_pid, header->continuity_counter);
	}
	else
		//It's a continuing packet
	{
		//We append the data of the ending packet
		add_ts_packet_data(buf+offset, pkt,TS_PACKET_SIZE-offset , NO_START, buf_pid ,header->continuity_counter);
	}

	pthread_mutex_unlock(&pkt->packetmutex);
	return (pkt->full_number > 0);
}



/** @brief This function will add data to the current partial section
 * see doc/diagrams/TS_add_data_all_cases.pdf for documentation
 */
void add_ts_packet_data(unsigned char *buf, mumudvb_ts_packet_t *pkt, int data_left, int start_flag, int pid, int cc)
{
	int copy_len;
	//We see if there is the start of a new section
	if(start_flag == START_TS || start_flag == START_SECTION)
	{
		//if the start was detected by the end of a section we check for stuffing bytes
		if(start_flag== START_SECTION)
		{
			/* Within a Transport Stream, packet stuffing bytes of value 0xFF may be found in the payload of Transport Stream
       packets carrying PSI and/or private_sections only after the last byte of a section. In this case all bytes until the end of
       the Transport Stream packet shall also be stuffing bytes of value 0xFF. These bytes may be discarded by a decoder. In
       such a case, the payload of the next Transport Stream packet with the same PID value shall begin with a pointer_field of
       value 0x00 indicating that the next section starts immediately thereafter.
			 */
			if(buf[0]==0xff)
			{
				log_message(log_module, MSG_FLOOD, "Stuffing bytes found data left %d\n",data_left);
				return;
			}
		}
		//We check if a packet has been started before, just for information
		if(pkt->status_partial!=EMPTY)
			log_message(log_module, MSG_FLOOD, "Unfinished packet and beginning of a new one, we drop the started one len: %d\n", pkt->len_partial);
		//We copy the data to the partial packet
		pkt->status_partial=STARTED;
		pkt->cc=cc;
		pkt->pid=pid;
		tbl_h_t *tbl_struct=(tbl_h_t *)buf;
		pkt->expected_len_partial=HILO(tbl_struct->section_length)+BYTES_BFR_SEC_LEN;
		//we copy the amount of data needed
		if(pkt->expected_len_partial<data_left)
			copy_len=pkt->expected_len_partial;
		else
			copy_len=data_left;
		pkt->len_partial=copy_len;
		//The real copy
		memcpy(pkt->data_partial,buf,pkt->len_partial);
		//we update the amount of data left
		data_left-=copy_len;
		//lot of debugging information
		log_message(log_module, MSG_FLOOD, "Starting a packet PID %d cc %d len %d expected len %d\n",
				pkt->pid,
				pkt->cc,
				pkt->len_partial,
				pkt->expected_len_partial);
		log_message(log_module, MSG_FLOOD, "First bytes\t 0x%02x 0x%02x 0x%02x 0x%02x  0x%02x 0x%02x 0x%02x 0x%02x\n",
				pkt->data_partial[0],
				pkt->data_partial[1],
				pkt->data_partial[2],
				pkt->data_partial[3],
				pkt->data_partial[4],
				pkt->data_partial[5],
				pkt->data_partial[6],
				pkt->data_partial[7]);
		log_message(log_module, MSG_FLOOD, "Struct data\t table_id 0x%02x section_syntax_indicator 0x%02x section_length 0x%02x transport_stream_id 0x%02x version_number 0x%02x current_next_indicator 0x%02x last_section_number 0x%02x\n",
				tbl_struct->table_id,
				tbl_struct->section_syntax_indicator,
				HILO(tbl_struct->section_length),
				HILO(tbl_struct->transport_stream_id),
				tbl_struct->version_number,
				tbl_struct->current_next_indicator,
				tbl_struct->last_section_number);
	}
	else
	{
		log_message(log_module, MSG_FLOOD, "Continuing packet, data left %d\n",data_left);
		if(pkt->status_partial!=STARTED)
		{
			log_message(log_module, MSG_FLOOD, "Continuing packet and saved packet not started or full, can be a continuity error\n");
			pkt->status_partial=EMPTY;
			return;
		}
		else if(pkt->cc==cc)
		{
			log_message(log_module, MSG_FLOOD, "Duplicate packet, continuity counter: %d\n", pkt->cc);
			return;
		}
		else if(((pkt->cc+1)%16)!=cc)
		{
			log_message(log_module, MSG_FLOOD, "The continuity counter is not valid saved packet cc %d actual cc %d\n", pkt->cc, cc);
			pkt->status_partial=EMPTY;
			return;
		}
		else if(pkt->pid!=pid)
		{
			log_message(log_module, MSG_FLOOD, "PID change. saved PID %d, actual pid %d\n", pkt->pid, pid);
			pkt->status_partial=EMPTY;
			return;
		}
		else
		{
			//packet started and continuing packet, we append the data
			//we copy the minimum amount of data
			if((pkt->len_partial+data_left)> pkt->expected_len_partial)
				copy_len=pkt->expected_len_partial - pkt->len_partial;
			else
				copy_len=data_left;
			//if too big we skip
			if(pkt->len_partial+copy_len > MAX_TS_SIZE)
			{
				log_message(log_module, MSG_FLOOD, "The packet seems too big pkt->len_partial %d copy_len %d pkt->len_partial+copy_len %d\n",
						pkt->len_partial,
						copy_len,
						pkt->len_partial+copy_len);
				copy_len=MAX_TS_SIZE-pkt->len_partial;
			}
			//We don't have any starting packet we make sure we don't believe there is
			data_left=0;

			memcpy(pkt->data_partial+pkt->len_partial,buf,copy_len);//we add the packet to the buffer
			pkt->len_partial+=copy_len;
			pkt->cc=cc; //update cc
			log_message(log_module, MSG_FLOOD, "Continuing a packet PID %d cc %d len %d expected %d\n",pkt->pid,pkt->cc,pkt->len_partial,pkt->expected_len_partial);
		}
	}

	//We check if the packet is full
	if(ts_partial_full(pkt))
	{
		//The partial packet is full, we check the CRC32
		if(ts_check_crc32(pkt))
			ts_move_part_to_full(pkt); //Everything is perfect, the packet full is ok
	}

	//If there is still data, a new section could begin, we call recursively
	if(data_left)
	{
		log_message(log_module, MSG_FLOOD, "Calling recursively, data left %d\n",data_left);
		add_ts_packet_data(buf+copy_len, pkt, data_left,START_SECTION, pid,cc);
	}
}


/** @brief move the partial packet to the full packet */
void ts_move_part_to_full(mumudvb_ts_packet_t *pkt)
{
	//append the data
	memcpy(pkt->buffer_full+pkt->full_buffer_len,pkt->data_partial,pkt->len_partial);
	pkt->full_buffer_len+=pkt->len_partial;
	pkt->full_lengths[pkt->full_number]=pkt->len_partial;
	pkt->full_number++;
	log_message(log_module, MSG_FLOOD, "New full packet len %d. There's now %d full packet%c\n",pkt->len_partial,pkt->full_number,pkt->full_number>1?'s':' ');
	//if it's the first we copy it to the full
	if(pkt->full_number==1)
	{
		pkt->len_full=pkt->full_lengths[0];
		log_message(log_module, MSG_FLOOD, "First full packet. len %d\n",pkt->len_full);
		memcpy(pkt->data_full,pkt->buffer_full,pkt->len_full);
	}
	pkt->len_partial=0;
	pkt->status_partial=EMPTY;
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
	//we have two ways: either we compute until the end and it should be 0
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
		//we set the good length
		packet->len_partial=packet->expected_len_partial;
		return 1;
	}
	return 0;
}



/** @brief This function will return a pointer to the beginning of the first payload of a TS packet and NULL if no payload or error
 * It returns NULL in case of error
 *
 * @param buf : the received buffer from the card
 */
unsigned char *get_ts_begin(unsigned char *buf)
{
	ts_header_t *header;
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
		delta += buf[delta];        // add adapt.field.len
	}
	if (header->adaptation_field_control & 0x1) //There is a payload
	{
		if (buf[delta]==0x00 && buf[delta+1]==0x00 && buf[delta+2]==0x01)
		{
			// -- PES/PS
			//tspid->id   = buf[j+3];
			log_message( log_module,  MSG_FLOOD, "#PES/PS ----- We ignore \n");
			return NULL;
		}
	}

	if (header->adaptation_field_control == 3)
		log_message( log_module,  MSG_DEBUG, "adaptation_field_control 3\n");

	if ((header->adaptation_field_control == 2)||(header->adaptation_field_control == 0))
	{
		log_message( log_module,  MSG_DEBUG, "adaptation_field_control %d ie no payload\n", header->adaptation_field_control);
		return NULL;
	}

	if(header->payload_unit_start_indicator) //It's the beginning of a new packet
	{
		int pointer_field=*(buf+delta);
		delta++;
		if(pointer_field!=0)
			log_message(log_module, MSG_FLOOD, "Pointer field 0x%02x\n",pointer_field);
		if((TS_PACKET_SIZE-delta-pointer_field)<0)
		{
			log_message(log_module, MSG_DETAIL, "Pointer field too big 0x%02x, packet dropped\n",pointer_field);
			return NULL;
		}
		return buf+delta+pointer_field; //we give the address of the beginning of the payload
		/*
		 *     This is an 8-bit field whose value shall be the number of bytes, immediately following the pointer_field
		 *     until the first byte of the first section that is present in the payload of the Transport Stream packet (so a value of 0x00 in
		 *     the pointer_field indicates that the section starts immediately after the pointer_field). When at least one section begins in
		 *     a given Transport Stream packet, then the payload_unit_start_indicator (refer to 2.4.3.2) shall be set to 1 and the first
		 *     byte of the payload of that Transport Stream packet shall contain the pointer. When no section begins in a given
		 *     Transport Stream packet, then the payload_unit_start_indicator shall be set to 0 and no pointer shall be sent in the
		 *     payload of that packet.
		 */
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





/** @brief Display the PAT contents
 *
 * @param buf The buffer containing the PAT
 */
void ts_display_pat(char* log_module,unsigned char *buf)
{
	pat_t       *pat=(pat_t*)(buf);
	pat_prog_t  *prog;
	int delta=PAT_LEN;
	int section_length=0;
	int number_of_services=0;
	log_message( log_module, MSG_FLOOD,"-------------- Display PAT ----------------\n");
	section_length=HILO(pat->section_length);
	log_message( log_module, MSG_FLOOD,  "transport stream id 0x%04x section_length %d version %i section_number %d last_section_number %d current_next_indicator %d\n",
			HILO(pat->transport_stream_id),
			HILO(pat->section_length),
			pat->version_number,
			pat->section_number,
			pat->last_section_number,
			pat->current_next_indicator);

	/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
  sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
  and shall be the next table to become valid.*/
	if(pat->current_next_indicator == 0)
		log_message( log_module, MSG_FLOOD,"The current_next_indicator is set to 0, this PAT is not valid for the current stream\n");

	//We loop over the different programs included in the pat
	while((delta+PAT_PROG_LEN)<(section_length))
	{
		prog=(pat_prog_t*)((char*)buf+delta);
		if(HILO(prog->program_number)==0)
		{
			log_message( log_module, MSG_DEBUG,"Network PID %d (PID of the NIT)\n", HILO(prog->network_pid));
		}
		else
		{
			number_of_services++;
			log_message( log_module, MSG_DEBUG,"service %d id 0x%04x %d\t PMT PID : %d",
					number_of_services,
					HILO(prog->program_number),
					HILO(prog->program_number),
					HILO(prog->network_pid));
		}
		delta+=PAT_PROG_LEN;
	}
	log_message( log_module, MSG_DEBUG,"This PAT contains %d services\n",number_of_services);
	log_message( log_module, MSG_FLOOD,"-------------- PAT Displayed ----------------\n");


}


typedef struct {
	u_char descriptor_tag                         :8;
	u_char descriptor_length                      :8;
#if BYTE_ORDER == BIG_ENDIAN
	u_char country_availability_flag              :1;
	u_char                                        :7;
#else
	u_char                                        :7;
	u_char country_availability_flag              :1;
#endif
} country_avaibility_descr_t;

/** @brief : show the contents of the country avaibility descriptor
 *
 * @param buf : the buffer containing the descriptor
 */
void ts_display_country_avaibility_descriptor(char* log_module,unsigned char *buf)
{
	int length,i;
	country_avaibility_descr_t *descr;
	log_message( log_module, MSG_DETAIL, "--- descriptor --- country avaibility descriptor\n");

	descr=(country_avaibility_descr_t *)buf;
	length=descr->descriptor_length-1;
	if(descr->country_availability_flag)
		log_message( log_module, MSG_DETAIL, "The reception is intended for the following countries : \n");
	else
		log_message( log_module, MSG_DETAIL, "The reception is NOT intended for the following countries : \n");
	for(i=0;i<length;i+=3)
	{
		log_message( log_module,  MSG_DETAIL,"Country : %c%c%c\n",buf[i+3], buf[i+3+1],buf[i+3+2]);
	}
}


/** @brief show the NIT Network descriptors
 * Loop over the NIT descriptors and call other parsing functions if necessary
 * @param buf the buffer containing the descriptors
 * @param descriptors_loop_len the len of buffer containing the descriptors
 * @param service the associated service
 */
void ts_display_nit_network_descriptors(char* log_module,unsigned char *buf,int descriptors_loop_len)
{

	while (descriptors_loop_len > 0)
	{
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (!descriptor_len)
		{
			log_message( log_module, MSG_DEBUG, " --- NIT descriptor --- descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
			break;
		}

		//The service descriptor provides the names of the service provider and the service in text form together with the service_type.
		if(descriptor_tag==0x40)
			ts_display_network_name_descriptor(log_module,buf);
		else if(descriptor_tag==0x5B)
			ts_display_multilingual_network_name_descriptor(log_module,buf);
		else
			log_message( log_module, MSG_FLOOD, "NIT network descriptor_tag : 0x%2x\n", descriptor_tag);

		buf += descriptor_len;
		descriptors_loop_len -= descriptor_len;
	}
}



/** @brief Parse the network name descriptor
 * It's used to get the network name
 * @param buf the buffer containing the descriptor
 */
void ts_display_network_name_descriptor(char* log_module,unsigned char *buf)
{
	/* Service descriptor :
     descriptor_tag			8
     descriptor_length			8
     for (i=0;i<N;I++){
       char				8
     }
	 */
	char *dest;
	unsigned char descriptor_len = buf[1];
	buf += 2;

	log_message( log_module, MSG_DEBUG, "NIT network name descriptor \n");
	log_message( log_module, MSG_FLOOD, "NIT network descriptor_len %d\n",descriptor_len);
	dest=malloc(sizeof(char)*(descriptor_len+1));
	memcpy (dest, buf, descriptor_len);
	dest[descriptor_len] = '\0';
	convert_en300468_string(dest,descriptor_len);
	log_message( log_module, MSG_DEBUG, "network name : \"%s\"\n", dest);
	free(dest);

}


/** @brief Parse the network name descriptor
 * It's used to get the network name
 * @param buf the buffer containing the descriptor
 */
void ts_display_multilingual_network_name_descriptor(char *log_module, unsigned char *buf)
{
	/* Service descriptor :
     descriptor_tag			8
     descriptor_length			8
     for (i=0;i<N;I++){
       ISO_639_language_code		24
       network_name_length		8
       for (i=0;i<N;I++){
         char				8
       }
     }
	 */
	char *dest;
	unsigned char descriptor_tag = buf[0];
	unsigned char descriptor_len = buf[1];
	int name_len;
	char language_code[4];
	buf += 2;

	log_message( log_module, MSG_FLOOD, "NIT Multilingual network name descriptor  0x%02x len %d\n",descriptor_tag,descriptor_len);

	while (descriptor_len > 0)
	{
		language_code[0]=*buf;buf++;
		language_code[1]=*buf;buf++;
		language_code[2]=*buf;buf++;
		language_code[3]='\0';
		name_len=*buf;buf++;
		log_message( log_module, MSG_FLOOD, "NIT network descriptor_len %d, name_len %d\n",descriptor_len , name_len);
		dest=malloc(sizeof(char)*(name_len+1));
		memcpy (dest, buf, name_len);
		dest[name_len] = '\0';
		buf += name_len;
		convert_en300468_string(dest,name_len);
		log_message( log_module, MSG_DEBUG, "lang code %s network name : \"%s\"\n",language_code, dest);
		descriptor_len -= (name_len+4);
		free(dest);
	}

}



/**
 */
void ts_display_service_list_descriptor(char* log_module,unsigned char *buf)
{
	/* Service list descriptor :
     descriptor_tag                     8
     descriptor_length                  8
     for (i=0;i<N;I++){
       service_id                       8
       service_type                     16
     }
	 */

	int i,service_id,service_type;
	unsigned char descriptor_len = buf[1];
	buf += 2;
	log_message( log_module, MSG_DETAIL, "--- NIT descriptor --- Service list descriptor\n");
	for(i=0;i<descriptor_len;i+=3)
	{
		service_id=(buf[i]<<8)+buf[i+1];;
		service_type=buf[i+2];
		log_message( log_module, MSG_DETAIL, "Service ID : 0x%02x service type: 0x%02x : %s \n",service_id, service_type, service_type_to_str(service_type));
	}
	log_message( log_module, MSG_DETAIL, "--- descriptor done ---\n");
}


/** @brief display the contents of satellite_delivery_system_descriptor
 * EN 300 468 V1.10.1   6.2.13.2 Satellite delivery system descriptor
 */
void ts_display_satellite_delivery_system_descriptor(char* log_module, unsigned char *buf)
{
	descr_sat_delivery_t *descr;
	descr=(descr_sat_delivery_t *)buf;

	log_message( log_module, MSG_DETAIL, "--- NIT descriptor --- satellite delivery system descriptor\n");

	// The frequency is a 32-bit field giving the 4-bit BCD values specifying 8 characters of the frequency value.
	log_message( log_module, MSG_DETAIL, "Frequency: %x%02x%02x.%02x MHz", descr->frequency_4, descr->frequency_3, descr->frequency_2, descr->frequency_1);
	log_message( log_module, MSG_DETAIL, "Orbital position: %d%01d,%01d°", descr->orbital_position_hi,(descr->orbital_position_lo>>4)&0x0f, descr->orbital_position_lo&0x0f);
	if(descr->west_east_flag)
		log_message( log_module, MSG_DETAIL, "Estern position");
	else
		log_message( log_module, MSG_DETAIL, "Western position");
	switch(descr->polarization)
	{
	log_message( log_module, MSG_DETAIL, "Polarization: (0x%02x)", descr->polarization);
	case 0:
		log_message( log_module, MSG_DETAIL, "Polarization: linear - horizontal");
		break;
	case 1:
		log_message( log_module, MSG_DETAIL, "Polarization: linear - vertical");
		break;
	case 2:
		log_message( log_module, MSG_DETAIL, "Polarization: circular - left");
		break;
	case 3:
		log_message( log_module, MSG_DETAIL, "Polarization: circular - right");
		break;
	default:
		log_message( log_module, MSG_DETAIL, "Polarization: BUG");
		break;
	}
	if(descr->modulation_system)
		log_message( log_module, MSG_DETAIL, "Modulation system: DVB-S2");
	else
		log_message( log_module, MSG_DETAIL, "Modulation system: DVB-S");
	if(descr->modulation_system) {
		switch(descr->roll_off) {
		case 0:
			log_message( log_module, MSG_DETAIL, "Roll-off factor: α = 0,35");
			break;
		case 1:
			log_message( log_module, MSG_DETAIL, "Roll-off factor: α = 0,25");
			break;
		case 2:
			log_message( log_module, MSG_DETAIL, "Roll-off factor: α = 0,20");
			break;
		case 3:
			log_message( log_module, MSG_DETAIL, "Roll-off factor: reserved");
			break;
		default:
			log_message( log_module, MSG_DETAIL, "Roll-off factor: BUG");
			break;
		}
	}
	switch(descr->modulation_type)
	{
	case 0:
		log_message( log_module, MSG_DETAIL, "Constellation: Auto");
		break;
	case 1:
		log_message( log_module, MSG_DETAIL, "Constellation: QPSK");
		break;
	case 2:
		log_message( log_module, MSG_DETAIL, "Constellation: 8PSK");
		break;
	case 3:
		log_message( log_module, MSG_DETAIL, "Constellation: 16-QAM");
		break;
	default:
		log_message( log_module, MSG_DETAIL, "Constellation: BUG");
		break;
	}

	log_message( log_module, MSG_DETAIL, "Symbol rate: %d%d%d,%d%d%d%d Msymbol/s", BCDHI(descr->symbol_rate_12), BCDLO(descr->symbol_rate_12), BCDHI(descr->symbol_rate_34), BCDLO(descr->symbol_rate_34), BCDHI(descr->symbol_rate_56), BCDLO(descr->symbol_rate_56),  BCDLO(descr->symbol_rate_7) );

	switch(descr->FEC_inner)
	{
	case 0:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: not defined");
		break;
	case 1:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 1/2");
		break;
	case 2:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 2/3");
		break;
	case 3:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 3/4");
		break;
	case 4:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 5/6");
		break;
	case 5:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 7/8");
		break;
	case 6:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 8/9");
		break;
	case 7:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 3/5");
		break;
	case 8:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 4/5");
		break;
	case 9:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 9/10");
		break;
	case 10:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: Reserved for future use");
		break;
	case 11:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: Reserved for future use");
		break;
	case 12:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: no convolutional coding");
		break;
	default:
		log_message( log_module, MSG_DETAIL, "Inner FEC scheme: BUG please contact");
		break;
	}
	log_message( log_module, MSG_DETAIL, "--- descriptor done ---\n");
}


/** @brief display the contents of terrestrial_delivery_system_descriptor
 * EN 300 468 V1.10.1   6.2.13.4 Terrestrial delivery system descriptor
 */
void ts_display_terrestrial_delivery_system_descriptor(char *log_module, unsigned char *buf)
{
	descr_terr_delivery_t *descr;
	descr=(descr_terr_delivery_t *)buf;

	log_message( log_module, MSG_DETAIL, "--- NIT descriptor --- terrestrial delivery system descriptor\n");

	log_message( log_module, MSG_DETAIL, "Frequency: %d Hz", ((descr->frequency_4<<24)+(descr->frequency_3<<16)+(descr->frequency_2<<8)+descr->frequency_1) *10 );
	if(descr->bandwidth<=3)
		log_message( log_module, MSG_DETAIL, "Bandwidth: %d MHz",8-descr->bandwidth);
	else
		log_message( log_module, MSG_DETAIL, "Bandwidth: Reserved for future use");
	if(descr->priority)
		log_message( log_module, MSG_DETAIL, "Priority: HP (high priority)");
	else
		log_message( log_module, MSG_DETAIL, "Priority: LP (low priority)");
	log_message( log_module, MSG_DETAIL, "Time_Slicing_indicator: %d",descr->Time_Slicing_indicator);
	log_message( log_module, MSG_DETAIL, "MPE_FEC_indicator: %d",descr->MPE_FEC_indicator );
	switch(descr->constellation)
	{
	case 0:
		log_message( log_module, MSG_DETAIL, "Constellation: QPSK");
		break;
	case 1:
		log_message( log_module, MSG_DETAIL, "Constellation: 16-QAM");
		break;
	case 2:
		log_message( log_module, MSG_DETAIL, "Constellation: 64-QAM");
		break;
	case 3:
		log_message( log_module, MSG_DETAIL, "Constellation: RFU");
		break;
	default:
		log_message( log_module, MSG_DETAIL, "Constellation: BUG");
		break;
	}
	switch(descr->hierarchy_information)
	{
	case 0:
		log_message( log_module, MSG_DETAIL, "hierarchy_information: non-hierarchical, native interleaver");
		break;
	case 1:
		log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 1, native interleaver");
		break;
	case 2:
		log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 2, native interleaver");
		break;
	case 3:
		log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 4, native interleaver");
		break;
	case 4:
		log_message( log_module, MSG_DETAIL, "hierarchy_information: non-hierarchical, in-depth interleaver");
		break;
	case 5:
		log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 1, in-depth interleaver");
		break;
	case 6:
		log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 2, in-depth interleaver");
		break;
	case 7:
		log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 4, in-depth interleaver");
		break;
	default:
		log_message( log_module, MSG_DETAIL, "hierarchy_information: BUG please contact");
		break;
	}

	switch(descr->code_rate_HP_stream)
	{
	case 0:
		log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: 1/2");
		break;
	case 1:
		log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: 2/3");
		break;
	case 2:
		log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: 3/4");
		break;
	case 3:
		log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: 5/6");
		break;
	case 4:
		log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: 7/8");
		break;
	case 5:
	case 6:
	case 7:
	default:
		log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: RFU");
		break;
	}
	switch(descr->code_rate_LP_stream)
	{
	case 0:
		log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: 1/2");
		break;
	case 1:
		log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: 2/3");
		break;
	case 2:
		log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: 3/4");
		break;
	case 3:
		log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: 5/6");
		break;
	case 4:
		log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: 7/8");
		break;
	case 5:
	case 6:
	case 7:
	default:
		log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: RFU");
		break;
	}


	switch(descr->guard_interval)
	{
	case 0:
		log_message( log_module, MSG_DETAIL, "guard_interval: 1/32");
		break;
	case 1:
		log_message( log_module, MSG_DETAIL, "guard_interval: 1/16");
		break;
	case 2:
		log_message( log_module, MSG_DETAIL, "guard_interval: 1/8");
		break;
	case 3:
		log_message( log_module, MSG_DETAIL, "guard_interval: 1/4");
		break;
	default:
		log_message( log_module, MSG_DETAIL, "guard_interval: BUG");
		break;
	}

	switch(descr->transmission_mode)
	{
	case 0:
		log_message( log_module, MSG_DETAIL, "transmission_mode: 2k");
		break;
	case 1:
		log_message( log_module, MSG_DETAIL, "transmission_mode: 8k");
		break;
	case 2:
		log_message( log_module, MSG_DETAIL, "transmission_mode: 4k");
		break;
	case 3:
		log_message( log_module, MSG_DETAIL, "transmission_mode: RFU");
		break;
	default:
		log_message( log_module, MSG_DETAIL, "transmission_mode: BUG");
		break;
	}

	if(descr->other_frequency_flag)
		log_message( log_module, MSG_DETAIL, "other_frequency_flag: one or more other frequencies are in use");
	else
		log_message( log_module, MSG_DETAIL, "other_frequency_flag: no other frequency is in use");

	log_message( log_module, MSG_DETAIL, "--- descriptor done ---\n");
}



/** @brief Parse the lcn descriptor
 * It's used to get the logical channel number
 * @param buf the buffer containing the descriptor
 */
void ts_display_lcn_descriptor(char *log_module, unsigned char* buf)
{
	/* Service descriptor :
     descriptor_tag			8
     descriptor_length			8
     for (i=0;i<N;I++){
       service_id			16
       visible_service_flag		1
       reserved				5
       logical_channel_number		10
     }
	 */

	nit_lcn_t *lcn;
	int descriptor_len = buf[1];
	buf += 2;
	int service_id, i_lcn;
	log_message( log_module, MSG_DEBUG, "NIT  0x83 descriptor (probably LCN) \n");
	log_message( log_module, MSG_FLOOD, "NIT  0x83 descriptor (probably LCN) descriptor_len %d\n",descriptor_len);

	while (descriptor_len > 0)
	{
		lcn=(nit_lcn_t *)buf;
		buf+=NIT_LCN_LEN;
		service_id= HILO(lcn->service_id);
		i_lcn=HILO(lcn->logical_channel_number);
		log_message( log_module, MSG_DEBUG, "NIT LCN channel number %d, service id %d visible %d\n",i_lcn ,service_id, lcn->visible_service_flag);
		descriptor_len -= NIT_LCN_LEN;
	}
}


/**
 */
void ts_display_frequency_list_descriptor(char* log_module,unsigned char *buf)
{
	/* Service list descriptor :
     descriptor_tag                     8
     descriptor_length                  8
     RFU								6
     coding type						2
     for (i=0;i<N;I++){
       center_frequency                 32
     }
	 */


	int i;
	unsigned char descriptor_len = buf[1];
	buf += 2;
	log_message( log_module, MSG_DETAIL, "--- NIT descriptor --- frequency list descriptor\n");
	switch((*buf)&0x3)
	{
	case 1:
		log_message( log_module, MSG_DETAIL, "\t coding type : satellite");
		break;
	case 2:
		log_message( log_module, MSG_DETAIL, "\t coding type : cable");
		break;
	case 3:
		log_message( log_module, MSG_DETAIL, "\t coding type : terrestrial");
		break;
	case 0:
	default:
		log_message( log_module, MSG_DETAIL, "\t coding type : not defined");
		break;
	}
	buf++;

	for(i=0;i<(descriptor_len-1);i+=4)
	{
		uint32_t frequency;
		frequency=((buf[0]<<24)+(buf[1]<<16)+(buf[2]<<8)+buf[3]);
		log_message( log_module, MSG_DETAIL, "Frequency: %u (Hz or *10Hz)", frequency );
		buf+=4;
	}
	log_message( log_module, MSG_DETAIL, "--- descriptor done ---\n");
}


