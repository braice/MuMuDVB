/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for Autoconfiguration
 * 
 * (C) 2008-2013 Brice DUBOST <mumudvb@braice.net>
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
 *  @brief This file contain the code related to the PMT reading for autoconfiguration
 *
 */


static char *log_module="Autoconf: ";

#include <errno.h>
#include <string.h>

#include "errors.h"
#include "mumudvb.h"
#include "autoconf.h"
#include "log.h"
#include "dvb.h"

int pmt_find_descriptor(uint8_t tag, unsigned char *buf, int descriptors_loop_len, int *pos);
void pmt_print_descriptor_tags(unsigned char *buf, int descriptors_loop_len);
mumudvb_ca_system_t* autoconf_get_ca_system(auto_p_t *auto_p, int ca_system_id);






/****************************************************************************/
//Parts of this code (read of the pmt and read of the pat)
// from libdvb, strongly modified, with commentaries added
/****************************************************************************/
void autoconf_get_pmt_pids(auto_p_t *auto_p, mumudvb_ts_packet_t *pmt, int *pids, int *num_pids, int *pids_type, char (*pids_language)[4], int *ca_sys_id)
{
	*num_pids=0;

	char temp_pids_language[MAX_PIDS][4];
	memset(temp_pids_language,0,MAX_PIDS*4*sizeof(char));
	pmt_t *header;
	pmt_info_t *descr_header;
	int program_info_length;
	int section_len,descr_section_len;
	int pid;
	int pid_type=0;

	section_len=pmt->len_full;
	header=(pmt_t *)pmt->data_full;

	program_info_length=HILO(header->program_info_length); //program_info_length
	char language[4]="";
	int pos=0;

	//For CAM debugging purposes, we look if we can find a CA descriptor to display CA system IDs
	//Also find ECM pid...
	int pos_ca_descr[255];
	int n_ca_descr;
	int len;
	n_ca_descr = 0;
	//search in the main loop
	if(program_info_length > 0)
	{
		while(len = pmt_find_descriptor(0x09,pmt->data_full+PMT_LEN,PMT_LEN+program_info_length,&pos),len)
		{
			log_message( log_module,  MSG_FLOOD,"  Found a CA descr in main loop at pos %d", pos);

			pos_ca_descr[n_ca_descr] = pos+PMT_LEN;
			pos+=len;
			n_ca_descr ++;

		}
	}
	//also search in the program loops
	for (int i=program_info_length+PMT_LEN; i<=section_len-(PMT_INFO_LEN+4); i+=descr_section_len+PMT_INFO_LEN)
	{
		pos=0;
		descr_header=(pmt_info_t *)(pmt->data_full+i);
		//We get the length of the descriptor
		descr_section_len=HILO(descr_header->ES_info_length);        //ES_info_length
		while(len = pmt_find_descriptor(0x09,pmt->data_full+i+PMT_INFO_LEN,descr_section_len, &pos),len)
		{
			log_message( log_module,  MSG_FLOOD,"  Found a CA descr in program loop (%d) at relative pos %d", i, pos);
			pos_ca_descr[n_ca_descr] = pos+i+PMT_INFO_LEN;
			//we need to skip this descriptor
			pos+=len;
			n_ca_descr ++;
		}
	}
	//now we can loop on the ca descriptors
	for (int i=0; i<n_ca_descr; i++)
	{
		descr_ca_t *ca_descriptor;
		ca_descriptor=(descr_ca_t *)(pmt->data_full+pos_ca_descr[i]);
		int ca_type = HILO(ca_descriptor->CA_type);
		pid=HILO(ca_descriptor->CA_PID);
		pid_type=PID_ECM;
		log_message( log_module,  MSG_DEBUG,"  ECM \tPID %d\n",pid);
		pids[*num_pids]=pid;
		pids_type[*num_pids]=pid_type;
		snprintf(temp_pids_language[*num_pids],4,"%s",language);
		(*num_pids)++;

		// get the EMM PID
		mumudvb_ca_system_t *ca_system;
		ca_system = autoconf_get_ca_system(auto_p, ca_type);
		if(ca_system != NULL)
		{
			log_message( log_module,  MSG_DEBUG,"  EMM \tPID %d\n", ca_system->emm_pid);
			// The pid might be already added (the EMM pid might be used by different CA systems)
			// Check if it was already added
			int pid_already_added = 0;
			for(int i = 0; i < *num_pids; i++)
			{
				if(pids[i] == ca_system->emm_pid)
				{
					pid_already_added = 1;
					break;
				}
			}
			if (pid_already_added == 0)
			{
				pids[*num_pids]=ca_system->emm_pid;
				pids_type[*num_pids]=PID_EMM;
				(*num_pids)++;
			}
		}
		else if(auto_p->cat_version != -1)
		{
			log_message( log_module,  MSG_ERROR, "Couldn't find CAT CA system for id 0x%04x\n", ca_type);
		}
	
		int casysid = 0;	
		while(casysid<32 && ca_sys_id[casysid] && ca_sys_id[casysid]!=ca_type)
			casysid++;
		if(casysid<32 && !ca_sys_id[casysid])
		{
			ca_sys_id[casysid]=ca_type;
			log_message( log_module,  MSG_DETAIL,"CA system id 0x%04x : %s\n", ca_type, ca_sys_id_to_str(ca_type));//we display it with the description
		}
		if(casysid==32)
			log_message( log_module,  MSG_WARN,"Too much CA system id line %d file %s\n", __LINE__,__FILE__);
	}

	pos=0;
	//we read the different descriptors included in the pmt
	//for more information see ITU-T Rec. H.222.0 | ISO/IEC 13818 table 2-34
	for (int i=program_info_length+PMT_LEN; i<=section_len-(PMT_INFO_LEN+4); i+=descr_section_len+PMT_INFO_LEN)
	{
		//we parse the part after the descriptors
		//we map the descriptor header
		descr_header=(pmt_info_t *)(pmt->data_full+i);
		//We get the length of the descriptor
		descr_section_len=HILO(descr_header->ES_info_length);        //ES_info_length

		// Default language value if not found
		snprintf(language,4,"%s","---");
		// Default, no position found
		pos=0;

		pid=HILO(descr_header->elementary_PID);
		//Depending of the stream type we'll take or not this pid
		switch(descr_header->stream_type)
		{
		case 0x01:
			pid_type=PID_VIDEO_MPEG1;
			log_message( log_module,  MSG_DEBUG,"  Video MPEG1 \tPID %d\n",pid);
			break;
		case 0x02:
			pid_type=PID_VIDEO_MPEG2;
			log_message( log_module,  MSG_DEBUG,"  Video MPEG2 \tPID %d\n",pid);
			break;
		case 0x10: /* ISO/IEC 14496-2 Visual - MPEG4 video */
			pid_type=PID_VIDEO_MPEG4_ASP;
			log_message( log_module,  MSG_DEBUG,"  Video MPEG4-ASP \tPID %d\n",pid);
			break;
		case 0x1b: /* AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC 14496-10 Video */
			pid_type=PID_VIDEO_MPEG4_AVC;
			log_message( log_module,  MSG_DEBUG,"  Video MPEG4-AVC \tPID %d\n",pid);
			break;
		case 0x24: /*HEVC video stream TODO: enter quote*/
			pid_type=PID_VIDEO_MPEG4_HEVC;
			log_message( log_module,  MSG_DEBUG,"  Video MPEG4-HVC \tPID %d\n",pid);
			break;
		case 0x03:
			pid_type=PID_AUDIO_MPEG1;
			log_message( log_module,  MSG_DEBUG,"  Audio MPEG1 \tPID %d\n",pid);
			break;
		case 0x04:
			pid_type=PID_AUDIO_MPEG2;
			log_message( log_module,  MSG_DEBUG,"  Audio MPEG2 \tPID %d\n",pid);
			break;
		case 0x11: /* ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3 */
			pid_type=PID_AUDIO_AAC_LATM;
			log_message( log_module,  MSG_DEBUG,"  Audio AAC-LATM \tPID %d\n",pid);
			break;
		case 0x0f: /* ISO/IEC 13818-7 Audio with ADTS transport syntax - usually AAC */
			pid_type=PID_AUDIO_AAC_ADTS;
			log_message( log_module,  MSG_DEBUG,"  Audio AAC-ADTS \tPID %d\n",pid);
			break;
		case 0x81: /* Audio per ATSC A/53B [2] Annex B */
			pid_type=PID_AUDIO_ATSC;
			log_message( log_module,  MSG_DEBUG,"  Audio ATSC A/53B \tPID %d\n",pid);
			break;



		case 0x06: /* Descriptor defined in EN 300 468 */
			if(descr_section_len) //If we have an associated descriptor, we'll search information in it
			{
				if(pmt_find_descriptor(0x45,pmt->data_full+i+PMT_INFO_LEN,descr_section_len, NULL)){
					log_message( log_module,  MSG_DEBUG,"  VBI Data \tPID %d\n",pid);
					pid_type=PID_EXTRA_VBIDATA;
				}else if(pmt_find_descriptor(0x46,pmt->data_full+i+PMT_INFO_LEN,descr_section_len, NULL)){
					log_message( log_module,  MSG_DEBUG,"  VBI Teletext \tPID %d\n",pid);
					pid_type=PID_EXTRA_VBITELETEXT;
				}else if(pmt_find_descriptor(0x56,pmt->data_full+i+PMT_INFO_LEN,descr_section_len, NULL)){
					log_message( log_module,  MSG_DEBUG,"  Teletext \tPID %d\n",pid);
					pid_type=PID_EXTRA_TELETEXT;
				}else if(pmt_find_descriptor(0x59,pmt->data_full+i+PMT_INFO_LEN,descr_section_len, &pos)){
					log_message( log_module,  MSG_DEBUG,"  Subtitling \tPID %d\n",pid);
					pid_type=PID_EXTRA_SUBTITLE;
					char * lng=(char *)(pmt->data_full+i+PMT_INFO_LEN+pos+2);
					language[0]=lng[0];
					language[1]=lng[1];
					language[2]=lng[2];
					language[3]=0;
				}else if(pmt_find_descriptor(0x6a,pmt->data_full+i+PMT_INFO_LEN,descr_section_len, NULL)){
					log_message( log_module,  MSG_DEBUG,"  AC3 (audio) \tPID %d\n",pid);
					pid_type=PID_AUDIO_AC3;
				}else if(pmt_find_descriptor(0x7a,pmt->data_full+i+PMT_INFO_LEN,descr_section_len, NULL)){
					log_message( log_module,  MSG_DEBUG,"  Enhanced AC3 (audio) \tPID %d\n",pid);
					pid_type=PID_AUDIO_EAC3;
				}else if(pmt_find_descriptor(0x7b,pmt->data_full+i+PMT_INFO_LEN,descr_section_len, NULL)){
					log_message( log_module,  MSG_DEBUG,"  DTS (audio) \tPID %d\n",pid);
					pid_type=PID_AUDIO_DTS;
				}else if(pmt_find_descriptor(0x7c,pmt->data_full+i+PMT_INFO_LEN,descr_section_len, NULL)){
					log_message( log_module,  MSG_DEBUG,"  AAC (audio) \tPID %d\n",pid);
					pid_type=PID_AUDIO_AAC;
				}else
				{
					log_message( log_module,  MSG_DEBUG,"Unknown descriptor see EN 300 468 v1.9.1 table 12, PID %d descriptor tags : ", pid);
					pmt_print_descriptor_tags(pmt->data_full+i+PMT_INFO_LEN,descr_section_len);
					log_message( log_module,  MSG_DEBUG,"\n");
					continue;
				}
			}
			else
			{
				log_message( log_module,  MSG_DEBUG,"PMT read : stream type 0x06 without descriptor\n");
				continue;
			}
			break;

			//Now, the list of what we drop
		case 0x05:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : 0x05, ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections \n",pid);
			continue;
			//Digital Storage Medium Command and Control (DSM-CC) cf H.222.0 | ISO/IEC 13818-1 annex B
		case 0x0a:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : 0x0A ISO/IEC 13818-6 type A (DSM-CC)\n",pid);
			continue;
		case 0x0b:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : 0x0B ISO/IEC 13818-6 type B (DSM-CC)\n",pid);
			continue;
		case 0x0c:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : 0x0C ISO/IEC 13818-6 type C (DSM-CC)\n",pid);
			continue;
		case 0x0D:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : ISO/IEC 13818-6 type D",pid);
			continue;
		case 0x0E:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary",pid);
			continue;
		case 0x12:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets",pid);
			continue;
		case 0x13:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC 14496_sections",pid);
			continue;
		case 0x14:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : ISO/IEC 13818-6 Synchronized Download Protocol",pid);
			continue;
		case 0x15:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : Metadata carried in PES packets",pid);
			continue;
		case 0x16:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : Metadata carried in metadata_sections",pid);
			continue;
		case 0x17:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : Metadata carried in ISO/IEC 13818-6 Data Carousel",pid);
			continue;
		case 0x18:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : Metadata carried in ISO/IEC 13818-6 Object Carousel",pid);
			continue;
		case 0x19:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : Metadata carried in ISO/IEC 13818-6 Synchronized Download Protocol",pid);
			continue;
		case 0x1A:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : IPMP stream (defined in ISO/IEC 13818-11, MPEG-2 IPMP)",pid);
			continue;
		case 0x7F:
			log_message( log_module,  MSG_DEBUG, "Dropped PID %d, type : IPMP stream",pid);
			continue;
		default:
			if(descr_header->stream_type >= 0x1C && descr_header->stream_type <= 0x7E)
				log_message( log_module,  MSG_DEBUG, "Dropped PID %d, stream type : 0x%02x : ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved",pid,descr_header->stream_type);
			else if(descr_header->stream_type >= 0x80)
				log_message( log_module,  MSG_DEBUG, "Dropped PID %d, stream type : 0x%02x : User Private",pid,descr_header->stream_type);
			else
				log_message( log_module,  MSG_INFO, "!!!!Unknown stream type : 0x%02x, PID : %d cf ITU-T Rec. H.222.0 | ISO/IEC 13818\n",descr_header->stream_type,pid);
			continue;
		}

		//We keep this pid

		// We try to find a 0x0a (ISO639) descriptor to have language information about the stream
		pos=0;
		if(pmt_find_descriptor(0x0a,pmt->data_full+i+PMT_INFO_LEN,descr_section_len, &pos)){
			char * lng=(char *)(pmt->data_full+i+PMT_INFO_LEN+pos+2);
			language[0]=lng[0];
			language[1]=lng[1];
			language[2]=lng[2];
			language[3]=0;
		}
		log_message( log_module,  MSG_DEBUG,"  PID Language Code = %s\n",language);


		pids[*num_pids]=pid;
		pids_type[*num_pids]=pid_type;
		snprintf(temp_pids_language[*num_pids],4,"%s",language);
		(*num_pids)++;
		if (*num_pids >= MAX_PIDS)
		{
			log_message( log_module,  MSG_ERROR,
					"Too many PIDs : %d\n",
					*num_pids);
			(*num_pids)--;
		}

	}
	memcpy(pids_language,temp_pids_language,MAX_PIDS*4*sizeof(char));
}


/** @brief Reads the program map table
 *
 * It's used to get the different "useful" pids of the channel
 * @param pmt the pmt packet
 * @param channel the associated channel
 */
int autoconf_read_pmt(auto_p_t *auto_p, mumudvb_channel_t *channel, mumudvb_ts_packet_t *pmt)
{
	pmt_t *header;

	header=(pmt_t *)pmt->data_full;

	if(channel->service_id && (channel->service_id != HILO(header->program_number)) )
	{
		log_message( log_module,  MSG_DETAIL,"The PMT %d does not belongs to channel \"%s\"\n", pmt->pid, channel->name);
		return 0;
	}




	log_message( log_module,  MSG_DEBUG,"PMT (PID %d) read for configuration of channel \"%s\" with SID %d", pmt->pid, channel->name,channel->service_id);

	// Reset of the CA SYS saved for the channel
	for (int i=0; i<32; i++)
		channel->ca_sys_id[i]=0;


	int temp_pids[MAX_PIDS];
	int temp_pids_type[MAX_PIDS];
	char temp_pids_language[MAX_PIDS][4];
	//For channel update
	int temp_num_pids=0;


	temp_pids[0]=pmt->pid;
	temp_num_pids++;




	//We get the PIDs contained in the PMT
	autoconf_get_pmt_pids(auto_p, pmt, temp_pids, &temp_num_pids, temp_pids_type, temp_pids_language, channel->ca_sys_id);



	/**************************
	 * PCR PID
	 **************************/
	channel->pid_i.pcr_pid=HILO(header->PCR_PID); //The PCR pid.
	//we check if it's not already included (ie the pcr is carried with the video)
	int found=0;
	for(int i=0;i<channel->pid_i.num_pids;i++)
	{
		if(temp_pids[i]==channel->pid_i.pcr_pid)
			found=1;
	}
	if(!found)
	{
		temp_pids[temp_num_pids]=channel->pid_i.pcr_pid;
		temp_pids_type[temp_num_pids]=PID_PCR;
		snprintf(temp_pids_language[temp_num_pids],4,"%s","---");
		temp_num_pids++;
	}
	log_message( log_module,  MSG_DEBUG, "PCR pid %d\n",channel->pid_i.pcr_pid);

	/**************************
	 * PCR PID - END
	 **************************/

	log_message( log_module,  MSG_DEBUG,"Detected PIDs");
	for(int curr_pid=0;curr_pid<temp_num_pids;curr_pid++)
	{
		log_message( log_module,  MSG_DEBUG,"PID %d type %s language %s",
				temp_pids[curr_pid],
				pid_type_to_str(temp_pids_type[curr_pid]),
				temp_pids_language[curr_pid]);
	}


	//MAYBE: copy ca sys id in the channel structure



	/**************************
	 * Channel update
	 **************************/
	//If it's a channel update we will have to update the filters

	log_message( log_module,  MSG_DEBUG,"Channel update number of PIDs detected %d old %d we check for changes", temp_num_pids+1, channel->pid_i.num_pids);

	if(channel->pid_i.pid_f!=F_USER)
	{
		log_message( log_module,  MSG_DETAIL, "PIDs update");

		//We display it just for information the filter update is done elsewhere
		//We search for added PIDs
		for(int i=0,found=0;i<temp_num_pids;i++)
		{
			for(int j=0;j<channel->pid_i.num_pids;j++)
				if(channel->pid_i.pids[j]==temp_pids[i])
					found=1;
			if(!found)
				log_message( log_module,  MSG_DETAIL, "PID %d added type %s lang %s",
						temp_pids[i],
						pid_type_to_str(temp_pids_type[i]),
						temp_pids_language[i]);
		}
		//We search for suppressed pids
		for(int i=0,found=0;i<channel->pid_i.num_pids;i++)
		{
			for(int j=0;j<temp_num_pids;j++)
				if(channel->pid_i.pids[i]==temp_pids[j] || channel->pid_i.pids[i] == channel->pid_i.pmt_pid )
					found=1;
			if(!found)
				log_message( log_module,  MSG_DETAIL, "PID %d removed type %s lang %s",
						channel->pid_i.pids[i],
						pid_type_to_str(channel->pid_i.pids_type[i]),
						channel->pid_i.pids_language[i]);
		}

		for(int i=0;i<temp_num_pids;i++)
		{
			//+1 because PMT is already set
			channel->pid_i.pids[i+1]=temp_pids[i];
			channel->pid_i.pids_type[i+1]=temp_pids_type[i];
			snprintf(channel->pid_i.pids_language[i+1],4,"%s",temp_pids_language[i]);
		}
		channel->pid_i.num_pids=temp_num_pids+1;


		log_message( log_module,  MSG_DETAIL, "        pids : \n");/**@todo Generate a strind and call log_message after, in syslog it generates one line per pid : use the toolbox unicast*/
		int ipid;
		for (ipid = 0; ipid < channel->pid_i.num_pids; ipid++)
			log_message( log_module,  MSG_DETAIL, "              %d (%s) \n", channel->pid_i.pids[ipid], pid_type_to_str(channel->pid_i.pids_type[ipid]));
	}
	else
		log_message( log_module,  MSG_DETAIL, "PIDs user set we keep intact");
	/**************************
	 * Channel update END
	 **************************/

	//Never more than one section in the PMT
	log_message( log_module, MSG_DEBUG,"It seems that we have finished to get the Program Map Table");
	log_message( log_module,  MSG_DEBUG,"Number of PIDs after autoconf %d\n", channel->pid_i.num_pids);
	return 1;
}


/** @brief Tells if the descriptor with tag in present in buf
 *
 * for more information see ITU-T Rec. H.222.0 | ISO/IEC 13818
 *
 * @param tag the descriptor tag, cf EN 300 468
 * @param buf the decriptors buffer (part of the PMT)
 * @param descriptors_loop_len the length of the descriptors
 * @param pos the position in the buffer
 */
int pmt_find_descriptor(uint8_t tag, unsigned char *buf, int descriptors_loop_len, int *pos)
{
	if(pos!=NULL)
	{
		buf+=*pos;
		descriptors_loop_len -= *pos;
	}
	while (descriptors_loop_len > 0)
	{
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		if (tag == descriptor_tag)
			return descriptor_len;

		if(pos!=NULL)
			*pos += descriptor_len;
		buf += descriptor_len;
		descriptors_loop_len -= descriptor_len;
	}
	return 0;
}

/** @brief Debugging function, Print the tags present in the descriptor
 *
 * for more information see ITU-T Rec. H.222.0 | ISO/IEC 13818
 * @param buf the decriptors buffer (part of the PMT)
 * @param descriptors_loop_len the length of the descriptors
 */
void pmt_print_descriptor_tags(unsigned char *buf, int descriptors_loop_len)
{
	while (descriptors_loop_len > 0)
	{
		unsigned char descriptor_tag = buf[0];
		unsigned char descriptor_len = buf[1] + 2;

		log_message( log_module,  MSG_DEBUG,"0x%02x - \n", descriptor_tag);
		buf += descriptor_len;
		descriptors_loop_len -= descriptor_len;
	}
}


