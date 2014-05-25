/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for Autoconfiguration
 *
 * (C) 2008-2011 Brice DUBOST <mumudvb@braice.net>
 *
 * Parts of this code come from libdvb, modified for mumudvb
 * by Brice DUBOST 
 * Libdvb part : Copyright (C) 2000 Klaus Schmidinger
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
 *  @brief This file contain the code related to the NIT reading for autoconfiguration
 *
 */


#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "errors.h"
#include "mumudvb.h"
#include "autoconf.h"
#include "log.h"

static char *log_module="Autoconf: ";

void parse_nit_ts_descriptor(unsigned char *buf,int ts_descriptors_loop_len, mumudvb_channel_t *channels, int number_of_channels, int pat_tsid);
void parse_lcn_descriptor(unsigned char *buf, mumudvb_channel_t *channels, int number_of_channels);



void autoconf_nit_need_update(auto_p_t *auto_p, unsigned char *buf)
{
	nit_t       *nit=(nit_t*)(get_ts_begin(buf));
	if(nit) //It's the beginning of a new packet
		if(nit->version_number!=auto_p->nit_version && nit->table_id == 0x40 && !auto_p->nit_all_sections_seen)
		{
			/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
        sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
        and shall be the next table to become valid.*/
			if(nit->current_next_indicator == 0)
			{
				return;
			}
			log_message( log_module, MSG_DEBUG,"NIT Need update. stored version : %d, new: %d\n",auto_p->nit_version,nit->version_number);
			auto_p->nit_need_update=1;
		}
}




/** @brief Read the network information table (cf EN 300 468)
 *
 */
int autoconf_read_nit(auto_p_t *auto_p, mumu_chan_p_t *chan_p)
{
	mumudvb_ts_packet_t *nit_mumu;
	unsigned char *buf=NULL;
	log_message( log_module, MSG_FLOOD,"New NIT\n");
	//We get the packet
	nit_mumu=auto_p->autoconf_temp_nit;
	buf=nit_mumu->data_full;
	nit_t       *header=(nit_t*)(buf);

	//We look only for the following table Ox40 : network_information_section - actual_network
	if (header->table_id != 0x40)
	{
		log_message( log_module, MSG_FLOOD,"NIT :  Bad table %d\n", header->table_id);
		return 1;
	}

	if(header->version_number==auto_p->nit_version)
	{
		//check if we saw this section
		if(auto_p->nit_sections_seen[header->section_number])
		{
			log_message( log_module, MSG_FLOOD,"NIT section %d seen", header->section_number );
			return 0;
		}

	}
	else
	{
		//New version, no section seen
		for(int i=0;i<256;i++)
			auto_p->nit_sections_seen[i]=0;
		auto_p->nit_version=header->version_number;
		auto_p->nit_all_sections_seen=0;
		if(auto_p->nit_version!=-1)
			log_message( log_module, MSG_INFO,"The NIT version changed, channels number could have changed !");

	}
	//we store the section
	auto_p->nit_sections_seen[header->section_number]=1;




	/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
  sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
  and shall be the next table to become valid.*/
	if(header->current_next_indicator == 0)
	{
		log_message( log_module, MSG_FLOOD,"NIT not yet valid, we get a new one (current_next_indicator == 0)\n");
		return 1;
	}


	log_message( log_module, MSG_FLOOD, "-- NIT : Network Information Table --\n");

	log_message( log_module, MSG_FLOOD, "Network id 0x%02x see \"http://www.dvbservices.com/identifiers/export/network_id\"\n", HILO(header->network_id));
	//We store the network ID
	auto_p->network_id=HILO(header->network_id);
	int network_descriptors_length = HILO(header->network_descriptor_length);


	//Loop over different descriptors in the NIT
	buf+=NIT_LEN;

	//We read the descriptors
	ts_display_nit_network_descriptors(log_module, buf,network_descriptors_length);
	buf += network_descriptors_length;
	nit_mid_t *middle=(nit_mid_t *)buf;
	int ts_loop_length=HILO(middle->transport_stream_loop_length);
	buf +=SIZE_NIT_MID;
	parse_nit_ts_descriptor(buf,ts_loop_length, chan_p->channels, chan_p->number_of_channels, auto_p->transport_stream_id);


	int sections_missing=0;
	//We check if we saw all sections
	for(int i=0;i<=header->last_section_number;i++)
		if(auto_p->nit_sections_seen[i]==0)
			sections_missing++;
	if(sections_missing)
	{
		log_message( log_module, MSG_DETAIL,"NIT  %d sections on %d are missing",
				sections_missing,header->last_section_number);
		return 0;
	}
	else
	{
		auto_p->nit_all_sections_seen=1;
		auto_p->nit_need_update=0;
		log_message( log_module, MSG_DEBUG,"It seems that we have finished to get the logical channel number");
		return 1;
	}
	return 0;

}


void parse_nit_ts_descriptor(unsigned char* buf, int ts_descriptors_loop_len, mumudvb_channel_t* channels, int number_of_channels, int pat_tsid)
{
	int descriptors_loop_len;
	nit_ts_t *descr_header;
	int ts_id,orig_network_id;
	while (ts_descriptors_loop_len > 0)
	{
		descr_header=(nit_ts_t *)(buf);
		descriptors_loop_len=HILO(descr_header->transport_descriptors_length);
		log_message( log_module, MSG_FLOOD, " --- NIT ts_descriptors_loop_len %d descriptors_loop_len %d\n", ts_descriptors_loop_len, descriptors_loop_len);
		orig_network_id=HILO(descr_header->original_network_id);
		log_message( log_module, MSG_FLOOD, " --- NIT descriptor concerning the network id 0x%04x\n", orig_network_id);
		ts_id=HILO(descr_header->transport_stream_id);
		log_message( log_module, MSG_FLOOD, " --- NIT descriptor concerning the multiplex 0x%04x our multiplex 0x%04x\n", ts_id, pat_tsid);
		if(ts_id != pat_tsid)
			log_message( log_module, MSG_FLOOD, " ---   Other multiplex, we skip");
		buf +=NIT_TS_LEN;
		ts_descriptors_loop_len -= (descriptors_loop_len+NIT_TS_LEN);
		while (descriptors_loop_len > 0)
		{
			unsigned char descriptor_tag = buf[0];
			unsigned char descriptor_len = buf[1] + 2;

			if (!descriptor_len)
			{
				log_message( log_module, MSG_FLOOD, " --- NIT descriptor --- descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
				break;
			}
			//We parse only descriptors with the right transport_stream_id
			if(pat_tsid==ts_id)
			{
				if(descriptor_tag==0x83)
				{
					ts_display_lcn_descriptor(log_module, buf);
					parse_lcn_descriptor(buf, channels, number_of_channels);
				}
				else if(descriptor_tag==0x41)
					ts_display_service_list_descriptor(log_module, buf);
				else if(descriptor_tag==0x43)
					ts_display_satellite_delivery_system_descriptor(log_module, buf);
				else if(descriptor_tag==0x5A)
					ts_display_terrestrial_delivery_system_descriptor(log_module, buf);
				else if(descriptor_tag==0x62)
					ts_display_frequency_list_descriptor(log_module, buf);
				else
					log_message( log_module, MSG_FLOOD, " --- NIT TS descriptor --- descriptor_tag == 0x%02x len %d descriptors_loop_len %d ------------\n", descriptor_tag, descriptor_len, descriptors_loop_len);
			}
			buf += descriptor_len;
			descriptors_loop_len -= descriptor_len;
		}

	}
}


/** @brief Parse the lcn descriptor
 * It's used to get the logical channel number
 * @param buf the buffer containing the descriptor
 */
void parse_lcn_descriptor(unsigned char* buf, mumudvb_channel_t* channels, int number_of_channels)
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
	int service_id, i_lcn, curr_channel;

	while (descriptor_len > 0)
	{
		lcn=(nit_lcn_t *)buf;
		buf+=NIT_LCN_LEN;
		service_id= HILO(lcn->service_id);
		i_lcn=HILO(lcn->logical_channel_number);
		for(curr_channel=0;curr_channel<number_of_channels;curr_channel++)
		{
			if(channels[curr_channel].service_id==service_id)
			{
				log_message( log_module, MSG_DETAIL, "NIT LCN channel FOUND id %d, LCN %d name \"%s\"\n",service_id,i_lcn, channels[curr_channel].name);
				channels[curr_channel].logical_channel_number=i_lcn;
			}
		}
		descriptor_len -= NIT_LCN_LEN;
	}
}


