/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for Autoconfiguration
 * 
 * (C) 2008-2014 Brice DUBOST <mumudvb@braice.net>
 *
 * Parts of this code come from libdvb, modified for mumudvb
 * by Brice DUBOST 
 * Libdvb part : Copyright (C) 2000 Klaus Schmidinger
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
 */

/** @file
 *  @brief This file contain the code related to the ATSC tables reading for autoconfiguration
 *
 */

#include <errno.h>
#include <string.h>
#include <stddef.h>



#include "errors.h"
#include "mumudvb.h"
#include "autoconf.h"
#include "log.h"

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

//LIBUCSI for long channel names
#ifdef HAVE_LIBUCSI
#include <libucsi/atsc/types.h>
#endif

int autoconf_parse_vct_channel(unsigned char *buf, auto_p_t *auto_p, mumu_chan_p_t *chan_p);

static char *log_module="Autoconf: ";

/*******************************************************
  ATSC 
 ********************************************************/


void autoconf_psip_need_update(auto_p_t *auto_p, unsigned char *buf)
{
	psip_t       *psip=(psip_t*)(get_ts_begin(buf));
	if(psip) //It's the beginning of a new packet
		if(psip->version_number!=auto_p->psip_version && !auto_p->psip_all_sections_seen)
		{
			/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
        sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
        and shall be the next table to become valid.*/
			if(psip->current_next_indicator == 0)
			{
				return;
			}
			log_message( log_module, MSG_DEBUG,"PSIP Need update. stored version : %d, new: %d\n",
					auto_p->psip_version,psip->version_number);
			auto_p->psip_need_update=1;
		}
}


/** @brief Read a PSIP table to find channels names 
 *
 * We read the master PSIP pid, search for a (T/C)VCT table
 * If it find this table, searches for channels within the transport (check
 * the transport id found in the PAT) and for the extended channel name descriptor
 * For the moment if the name of the channel is compressed, it will use the short channel
 * 
 * @param auto_p : the structure containing autoconfiguration parameters
 */
int autoconf_read_psip(auto_p_t *auto_p, mumu_chan_p_t *chan_p)
{
	//DOC : www.atsc.org/cms/standards/a_65-2009.pdf‎

	mumudvb_ts_packet_t *psip_mumu;
	int number_of_channels_in_section=0;
	int delta=0;
	int i=0;
	unsigned char *buf=NULL;

	//We get the packet
	psip_mumu=auto_p->autoconf_temp_psip;
	buf=psip_mumu->data_full;
	psip_t       *psip=(psip_t*)(buf);

	//We look only for the following tables OxC8 : TVCT (Terrestrial Virtual Channel Table), 0XC9 : CVCT (Cable Virtual Channel Table)
	//We check the table id before the section number because the section numbers are for a given table_id
	//Since we don't have the two table_id in the same stream, we are safe
	if (psip->table_id != 0xc8 && psip->table_id != 0xc9)
		return 1;
	log_message( log_module, MSG_DEBUG,"---- ATSC : PSIP TVCT ot CVCT----\n");


	if(auto_p->transport_stream_id<0)
	{
		log_message( log_module, MSG_DEBUG,"We don't have a transport id from the PAT, we skip this PSIP\n");
		return 1;
	}

	log_message( log_module, MSG_DEBUG,"PSIP transport_stream_id : 0x%x PAT TSID 0x%x\n",
			HILO(psip->transport_stream_id),
			auto_p->transport_stream_id);

	if(HILO(psip->transport_stream_id)!=auto_p->transport_stream_id)
	{
		log_message( log_module, MSG_DEBUG,"This table belongs to another transponder, we skip\n");
		return 1;
	}


	if(psip->version_number==auto_p->psip_version)
	{
		//check if we saw this section
		if(auto_p->psip_sections_seen[psip->section_number])
			return 0;
	}
	else
	{
		//New version, no section seen
		for(i=0;i<256;i++)
			auto_p->psip_sections_seen[i]=0;
		auto_p->psip_version=psip->version_number;
		auto_p->psip_all_sections_seen=0;
		if(auto_p->psip_version!=-1)
			log_message( log_module, MSG_INFO,"The PSIP version changed, channels description have changed");

	}
	//we store the section
	auto_p->psip_sections_seen[psip->section_number]=1;




	number_of_channels_in_section=buf[PSIP_HEADER_LEN]; //This field is the byte just after the PSIP header
	delta=PSIP_HEADER_LEN+1;
	log_message( log_module, MSG_DEBUG,"VCT : number_of_channels_in_section %d\n",
			number_of_channels_in_section);

	//We parse the channels
	for(i=0;i<number_of_channels_in_section;i++)
		delta+=autoconf_parse_vct_channel(buf+delta,auto_p,chan_p);




	int sections_missing=0;
	//We check if we saw all sections
	for(i=0;i<=psip->last_section_number;i++)
		if(auto_p->psip_sections_seen[i]==0)
			sections_missing++;
	if(sections_missing)
	{
		log_message( log_module, MSG_DETAIL,"PSIP  %d sections on %d are missing",
				sections_missing,psip->last_section_number);
		return 0;
	}
	else
	{
		auto_p->psip_all_sections_seen=1;
		auto_p->psip_need_update=0;
		log_message( log_module, MSG_DEBUG,"It seems that we have finished to update get the channels basic info\n");
		auto_p->need_filter_chan_update=1; //We have updated lots of stuff we need to update the filters and channels
	}

	return 0;

}

/** @brief Parse the contents of a (CT)VCT channel descriptor
 *
 * This function parse the channel and add it to the services list
 * 
 * @param buf - The channel descriptor
 * @param auto_p - The structure containing autoconfiguration parameters
 */

int autoconf_parse_vct_channel(unsigned char *buf, auto_p_t *auto_p, mumu_chan_p_t *chan_p)
{
	psip_vct_channel_t *vct_channel;
	char unconverted_short_name[15];//2*7 + 1 (for '\0')
#ifdef HAVE_ICONV
	char *inbuf, *dest; //Pointers for iconv conversion
#endif
	char utf8_short_name[15];
	char *channel_name=NULL;
	char long_name[MAX_NAME_LEN];

	int mpeg2_service_type=0;
	vct_channel=(psip_vct_channel_t *)buf;
	long_name[0]='\0';

	// *********** We get the channel short name *******************
	memcpy (unconverted_short_name, vct_channel->short_name, 14*sizeof(uint8_t));
	unconverted_short_name[14] = '\0';

#ifdef HAVE_ICONV
	//Conversion to utf8 of the short name
	iconv_t cd;
	//we open the conversion table
	cd = iconv_open( "UTF8", "UTF-16BE" );
	log_message( log_module, MSG_DEBUG,"We use big Endian UTF16 as source for channel name, if it give weird characters please contact\n");
	size_t inSize, outSize=14;
	inSize=14;
	//pointers initialization
	dest=utf8_short_name;
	inbuf=unconverted_short_name;
	//conversion
	iconv(cd, &inbuf, &inSize, &dest, &outSize );
	*dest = '\0';
	iconv_close( cd );
#else
	log_message( log_module, MSG_DETAIL, "Iconv not present, no name encoding conversion the result will be probably bad\n");
	memcpy (utf8_short_name, vct_channel->short_name, 14*sizeof(uint8_t));
	utf8_short_name[14] = '\0';
#endif
	log_message( log_module, MSG_DEBUG, "\tchannel short_name : \"%s\"\n", utf8_short_name);


	//************ We skip "uninteresting" channels  ****************
	if(vct_channel->modulation_mode==0x01)
	{
		log_message( log_module, MSG_DEBUG, "\tAnalog channel, we skip\n");
		return PSIP_VCT_LEN + HILO(vct_channel->descriptor_length); //We return the length
	}
	if(HILO(vct_channel->channel_tsid)!=auto_p->transport_stream_id)
	{
		log_message( log_module, MSG_DEBUG,"Channel for another transponder, we skip :  Channel TSID 0x%x , PAT TSID 0x%x\n",
				HILO(vct_channel->channel_tsid),
				auto_p->transport_stream_id);
		return PSIP_VCT_LEN + HILO(vct_channel->descriptor_length); //We return the length
	}
	if(vct_channel->hidden)
	{
		log_message( log_module, MSG_DEBUG,"This channel is supposed to be hidden, we skip. Please contact if you want to bypass\n");
		return PSIP_VCT_LEN + HILO(vct_channel->descriptor_length); //We return the length
	}


	//We "convert" ATSC service type to the "equivalent" MPEG2 service type
	switch(vct_channel->service_type)
	{
	case 0x02://ATSC_digital_television — The virtual channel carries television programming (audio, video and
		//optional associated data) conforming to ATSC standards
		mpeg2_service_type=0x01; //service_type "digital television service" (0x01)
		log_message( log_module, MSG_DEBUG,"vct_channel->service_type ATSC_digital_television\n");
		break;
	case 0x03://ATSC_audio — The virtual channel carries audio programming (audio service and optional
		//associated data) conforming to ATSC standards.
		mpeg2_service_type=0x02;//service_type digital radio sound service  (0x02)
		log_message( log_module, MSG_DEBUG,"vct_channel->service_type ATSC_audio\n");
		break;
	case 0x04://ATSC_data_only_service — The virtual channel carries a data service conforming to ATSC
		//standards, but no video of stream_type 0x02 or audio of stream_type 0x81.
		mpeg2_service_type=0x0c;//service_type data broadcast service
		log_message( log_module, MSG_DEBUG,"vct_channel->service_type ATSC_data_only_service\n");
		break;
	default:
		log_message( log_module, MSG_DEBUG,"Unknown vct_channel->service_type 0x%02x\n",vct_channel->service_type);
		break;
	}

#ifdef HAVE_LIBUCSI //used to decompress the atsc_text_descriptor
	int descriptor_delta;
	uint8_t *dest8=NULL; //Pointer for libusci conversion
	size_t destbufsize=MAX_NAME_LEN;
	size_t destbufpos=0;
	int descriptor_len;
	int atsc_decode_out;
	int delta_multiple_string_structure; //the beginning of tmultiple string structure
	//We loop on the different descriptors to find the long channel name
	for(descriptor_delta=0;descriptor_delta<HILO(vct_channel->descriptor_length);)
	{
		descriptor_len=buf[PSIP_VCT_LEN+descriptor_delta+1];
		if(buf[PSIP_VCT_LEN+descriptor_delta]==0xA0) //Extended channel name descriptor
		{
			log_message( log_module, MSG_DEBUG, "Extended channel name descriptor, we try to decode long channel name\n");
			dest8=(uint8_t *)long_name; //same type size, just the sign change but we don't care
			//check
			delta_multiple_string_structure=PSIP_VCT_LEN+descriptor_delta+2;//+2 to skip descriptor tag and descriptor len
			if (atsc_text_validate(((uint8_t*)(buf + delta_multiple_string_structure) ),
					buf[PSIP_VCT_LEN+descriptor_delta+1]))
			{
				log_message( log_module, MSG_DEBUG, "Error when VALIDATING long channel name, we take the short one\n");
			}
			else
			{
				//If we have multiple strings for the channel name we ask people to report
				if(buf[delta_multiple_string_structure]!=1 || buf[delta_multiple_string_structure+1+3] !=1)
				{
					log_message( log_module, MSG_WARN, "!!!!! Please report : parsing of long name :  number strings : %d number segments : %d\n",
							buf[delta_multiple_string_structure],
							buf[delta_multiple_string_structure+1+3]);
				}

				//Since it's only the channel name, we don't loop over strings and segments
				//We decode the text using LIBUCSI
				atsc_decode_out=atsc_text_segment_decode((struct atsc_text_string_segment *) (buf +delta_multiple_string_structure + MULTIPLE_STRING_STRUCTURE_HEADER),
						&dest8,
						&destbufsize,
						&destbufpos);
				if(atsc_decode_out!=-1) //No errors
				{
					dest8[atsc_decode_out]='\0';
					//We take the long one
					log_message( log_module, MSG_DEBUG, "Decoded long channel name : \"%s\"\n",dest8);
					channel_name=long_name;
				}
				else
					log_message( log_module, MSG_DEBUG, "Error when decoding long channel name, we take the short one\n");
			}
		}
		//Take the short name if error
		if(!channel_name)
			channel_name=utf8_short_name;

		//Next descriptor
		descriptor_delta+=descriptor_len+2;//We add the descriptor_len +2 for descriptor tag and descriptor len
	}
#else
	//We don't use libusci, we don't try to get long channel name
	channel_name=utf8_short_name;
#endif

	//************** We update a channel if stored channel  *****************
	//We base the detection of the services on the PAT, the PSIP gives extra information

	int chan=-1;
	for(int i=0;i<chan_p->number_of_channels && i< MAX_CHANNELS;i++)
	{
		if(chan_p->channels[i].service_id==HILO(vct_channel->program_number))
			chan=i;
	}
	if(chan!=-1)
	{
		log_message( log_module, MSG_DETAIL, "Updating channel %s, program number : 0x%x \n",
				channel_name,
				HILO(vct_channel->program_number));
		//we store the data
		chan_p->channels[chan].service_type=mpeg2_service_type;
		chan_p->channels[chan].free_ca_mode=vct_channel->access_controlled;
		log_message( log_module, MSG_DEBUG, "access_controlled : 0x%x\n", chan_p->channels[chan].free_ca_mode);
		memcpy (chan_p->channels[chan].service_name, channel_name, strlen(channel_name));
		chan_p->channels[chan].service_name[strlen(channel_name)] = '\0';

	}

	//**************** Work done for this channel -- goodbye *********************
	return PSIP_VCT_LEN + HILO(vct_channel->descriptor_length); //We return the length

}
