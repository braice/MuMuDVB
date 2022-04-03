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
 *  @brief This file contain the code related to the PAT reading for autoconfiguration
 *
 */

#define _CRT_SECURE_NO_WARNINGS

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "errors.h"
#include "mumudvb.h"
#include "autoconf.h"
#include "log.h"


static char *log_module="Autoconf: ";
int autoconf_pat_update_chan(pat_prog_t  *prog,int pat_version,mumu_chan_p_t *chan_p);

void autoconf_pat_need_update(auto_p_t *auto_p, unsigned char *buf)
{
	pat_t       *pat=(pat_t*)(get_ts_begin(buf));
	if(pat) //It's the beginning of a new packet
	{
		if(pat->version_number!=auto_p->pat_version)
		{
			/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
        sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
        and shall be the next table to become valid.*/
			if(pat->current_next_indicator == 0)
			{
				return;
			}
			log_message( log_module, MSG_DEBUG,"PAT Need update. stored version : %d, new: %d\n",auto_p->pat_version,pat->version_number);
			auto_p->pat_need_update=1;
		}
		else if(auto_p->pat_all_sections_seen && auto_p->pat_need_update==1) //We can have a wrong need update if the packet was broken (the CRC32 is checked only if we think it's good)
		{
			log_message( log_module, MSG_DEBUG,"PAT Not needing update anymore (wrong CRC ?)");
			auto_p->pat_need_update=0;
		}
	}
}





/** @brief read the PAT for autoconfiguration
 * This function extract the pmt from the pat
 * before doing so it checks if the service is already initialised (sdt packet)
 *
 * @param auto_p The autoconfiguration structure, containing all we need
 */
int autoconf_read_pat(auto_p_t *auto_p,mumu_chan_p_t *chan_p)
{
	mumudvb_ts_packet_t *pat_mumu;
	unsigned char *buf=NULL;
	pat_mumu=auto_p->autoconf_temp_pat;
	buf=pat_mumu->data_full;
	pat_t       *pat=(pat_t*)(buf);
	pat_prog_t  *prog;
	int delta=PAT_LEN;
	int section_length=0;
	int i;

	if(pat->version_number==auto_p->pat_version)
	{
		//check if we saw this section
		if(auto_p->pat_sections_seen[pat->section_number])
			return 0;
	}
	else
	{
		//New PAT version so we didn't got all PAT
		auto_p->pat_all_sections_seen=0;
		//We also force a re read of the SDT
		auto_p->sdt_version=-1;

		//New version, no section seen
		for(i=0;i<256;i++)
			auto_p->pat_sections_seen[i]=0;
		auto_p->pat_version=pat->version_number;
		if(auto_p->pat_version!=-1)
		{
			log_message( log_module, MSG_INFO,"The PAT version changed, channels have changed");
		}
		log_message( log_module, MSG_INFO,"New PAT we force SDT update after all sections seen");
		//We mark previously existing autodetected channels for cleanup after all PAT parsing
		//this flag will be set to READY if we see the channel again in this new PAT, otherwise it means the channel went down
		//See the end of this function for more details
		for(i=0;i<chan_p->number_of_channels && i< MAX_CHANNELS;i++)
		{
			if(chan_p->channels[i].channel_ready==READY && MU_F(chan_p->channels[i].service_id)==F_DETECTED)
			{
				chan_p->channels[i].channel_ready=READY_EXISTING;
				log_message( log_module, MSG_DEBUG,"Channel %d SID %d autodetected before this new PAT, we mark it",
										i,
										chan_p->channels[i].service_id);
			}
		}
	}
	//we store the section
	auto_p->pat_sections_seen[pat->section_number]=1;

	log_message( log_module, MSG_DEBUG,"---- New PAT version %d section %d ----\n",pat->version_number, pat->section_number);
	//we display the contents
	ts_display_pat(log_module,buf);
	//PAT reading
	section_length=HILO(pat->section_length);


	/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
  sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
  and shall be the next table to become valid.*/
	if(pat->current_next_indicator == 0)
	{
		log_message( log_module, MSG_DEBUG,"The current_next_indicator is set to 0, this PAT is not valid for the current stream\n");
		return 0;
	}

	//We store the transport stream ID
	auto_p->transport_stream_id=HILO(pat->transport_stream_id);

	//We loop over the different programs included in the pat
	while((delta+PAT_PROG_LEN)<(section_length))
	{
		prog=(pat_prog_t*)((char*)buf+delta);
		if(HILO(prog->program_number)!=0)
		{
			//We check if we have to update a channel
			if(autoconf_pat_update_chan(prog,pat->version_number,chan_p))
			{
					log_message( log_module, MSG_DETAIL,"channel updated  ID 0x%d",
							HILO(prog->program_number));
			}
		}
		delta+=PAT_PROG_LEN;
	}

	int sections_missing=0;
	//We check if we saw all sections
	for(i=0;i<=pat->last_section_number;i++)
		if(auto_p->pat_sections_seen[i]==0)
			sections_missing++;
	if(sections_missing)
	{
		log_message( log_module, MSG_DETAIL,"PAT  %d sections on %d are missing",
				sections_missing,pat->last_section_number);
		return 0;
	}
	else
	{
		auto_p->pat_all_sections_seen=1;
		auto_p->pat_need_update=0;
		log_message( log_module, MSG_DEBUG,"It seems that we have finished to get the channel/services list");
		//We say we have seen all PAT to update SDT
		//we see the channel which were READY_EXISTING and which are not READY meaning that they were not updated
		for(i=0;i<chan_p->number_of_channels && i< MAX_CHANNELS;i++)
		{
			if(chan_p->channels[i].channel_ready==READY_EXISTING)
			{
				log_message( log_module, MSG_WARN,"Channel %d SID %d removed",
						i,
						chan_p->channels[i].service_id);
				chan_p->channels[i].channel_ready=REMOVED;

				//we don't clean everything up so if this is a blinking channel client will still be able to got it when it reappears
			}
			else
			{
				//Channel still here, we force PMT update
				log_message( log_module, MSG_WARN,"Channel %d SID %d Force PMT update",
						i,
						chan_p->channels[i].service_id);
				chan_p->channels[i].pmt_version=-1;
			}
		}
		if(auto_p->sdt_version!=-1)
		{
			//We force to re read the SDT as we updated the PAT
			log_message( log_module, MSG_DEBUG,"We force the SDT update as the PAT was updated");
			auto_p->sdt_version=-1;
		}

	}


	return 0;
}

int autoconf_pat_update_chan(pat_prog_t  *prog,int pat_version,mumu_chan_p_t *chan_p)
{
	int i;
	int chan_num=-1;
	//we search if a channel already have the service_id

	for(i=0;i<chan_p->number_of_channels && i< MAX_CHANNELS;i++)
	{
		if(chan_p->channels[i].service_id==HILO(prog->program_number))
		{
			log_message( log_module, MSG_DEBUG,"Channel %d SID %d existing : %s",
					i,
					chan_p->channels[i].service_id,
					ready_f_to_str(chan_p->channels[i].channel_ready));
			chan_num=i;
		}
	}
	//if chan num == -1 we create a new channel and update channel number
	if(chan_num==-1)
	{
		if(chan_p->number_of_channels>=(MAX_CHANNELS-1))
		{
			//too many channels
			log_message( log_module, MSG_WARN,"PAT version %d program %d TOO MANY channel %d",
									pat_version,
									HILO(prog->program_number),
									chan_p->number_of_channels+1);
			return 0;
		}
		log_message( log_module, MSG_FLOOD,"PAT version %d program %d  NEW channel %d",
						pat_version,
						HILO(prog->program_number),
						chan_p->number_of_channels+1);
		//increase number of channels
		chan_num=chan_p->number_of_channels;
		chan_p->number_of_channels++;
		//set the service ID
		chan_p->channels[chan_num].service_id=HILO(prog->program_number);
		MU_F(chan_p->channels[chan_num].service_id)=F_DETECTED;
		//NEW channel we clear some stuff
		mumu_init_chan(&chan_p->channels[chan_num]);
		chan_p->channels[chan_num].channel_ready=NOT_READY;
	}
	i=chan_num;


	//if it was an existing channel we keep it up
	if(chan_p->channels[i].channel_ready==READY_EXISTING)
	{
		chan_p->channels[i].channel_ready=READY;
		log_message( log_module, MSG_DEBUG,"Channel %d SID %d is still here we mark it as being still READY",
								i,
								chan_p->channels[i].service_id);
	}
	else if(chan_p->channels[i].channel_ready==REMOVED)
	{
		chan_p->channels[i].channel_ready=NOT_READY;
		log_message( log_module, MSG_DEBUG,"Channel %d SID %d is BACK",
								i,
								chan_p->channels[i].service_id);
		mumu_init_chan(&chan_p->channels[i]);
	}

	//check if PMT PID user set, if not set the PMT
	if(MU_F(chan_p->channels[i].pid_i.pmt_pid)!=F_USER)
	{
		int pid_i=-1;
		//Set the PMT PID
		chan_p->channels[i].pid_i.pmt_pid=HILO(prog->network_pid);
		//if old PMT in the PID list we replace
		for (int ipid = 0; ipid < chan_p->channels[i].pid_i.num_pids; ipid++)
		{
			if(chan_p->channels[i].pid_i.pids[ipid]==chan_p->channels[i].pid_i.pmt_pid)
				pid_i=ipid;
		}
		//not found, we add a new PID
		if(pid_i==-1)
		{
			pid_i=chan_p->channels[i].pid_i.num_pids;
			chan_p->channels[i].pid_i.num_pids++;
		}

		chan_p->channels[i].pid_i.pids[pid_i]=chan_p->channels[i].pid_i.pmt_pid;
		chan_p->channels[i].pid_i.pids_type[pid_i]=PID_PMT;
		snprintf(chan_p->channels[i].pid_i.pids_language[pid_i],4,"%s","---");
		if(chan_p->channels[i].pmt_packet==NULL)
		{
			chan_p->channels[i].pmt_packet=malloc(sizeof(mumudvb_ts_packet_t));
			if(chan_p->channels[i].pmt_packet==NULL)
			{
				log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
				set_interrupted(ERROR_MEMORY<<8);
				return -1;
			}
			memset (chan_p->channels[i].pmt_packet, 0, sizeof( mumudvb_ts_packet_t));//we clear it
			pthread_mutex_init(&chan_p->channels[i].pmt_packet->packetmutex,NULL);
		}
	}
	else
		log_message( log_module, MSG_DEBUG,"PAT version %d program %d channel %d PMT user set to %d",
				pat_version,
				HILO(prog->program_number),
				i,
				chan_p->channels[i].pid_i.pmt_pid);


	//reset PMT version to force channel update
	chan_p->channels[chan_num].pmt_version=-1;


	return 0;
}

