/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2014 Brice DUBOST
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




#include "mumudvb.h"
#include "log.h"
#include "errors.h"
#include "dvb.h"
#include "rtp.h"
#include "unicast_http.h"

#include <sys/poll.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "scam_common.h"


static char *log_module="Common chan: ";

int mumu_init_chan(mumudvb_channel_t *chan)
{
	chan->num_packet = 0;
	chan->has_traffic = 1;
	chan->num_scrambled_packets = 0;
	chan->scrambled_channel = 0;
	chan->generated_pat_version=-1;
	chan->generated_sdt_version=-1;
	//We alloc the channel pmt_packet (useful for autoconf and cam)
	if(chan->pmt_packet==NULL)
	{
		chan->pmt_packet=malloc(sizeof(mumudvb_ts_packet_t));
		if(chan->pmt_packet==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			set_interrupted(ERROR_MEMORY<<8);
			return -1;
		}
		memset (chan->pmt_packet, 0, sizeof( mumudvb_ts_packet_t));//we clear it
		pthread_mutex_init(&chan->pmt_packet->packetmutex,NULL);

	}

	//Add pmt pid to pid list if not done
	if(MU_F(chan->pid_i.pmt_pid)==F_USER)
	{
		int found=0;
		for(int i=0;i<chan->pid_i.num_pids;i++)
		{
			if(chan->pid_i.pids[i]==chan->pid_i.pmt_pid)
				found=1;
		}
		if(!found && chan->pid_i.num_pids<(MAX_PIDS-1))
		{
			chan->pid_i.pids[chan->pid_i.num_pids]=chan->pid_i.pmt_pid;
			chan->pid_i.pids_type[chan->pid_i.num_pids]=PID_PMT;
			chan->pid_i.num_pids++;
		}
	}

#ifdef ENABLE_SCAM_SUPPORT
	pthread_mutex_init(&chan->stats_lock, NULL);
	pthread_mutex_init(&chan->cw_lock, NULL);
	chan->camd_socket = -1;

#endif
	return 0;
}




void chan_pmt_need_update( mumudvb_channel_t *chan, unsigned char *buf)
{
	pmt_t       *pmt=(pmt_t*)(get_ts_begin(buf));
	if(pmt) //It's the beginning of a new packet
	{
		if(pmt->version_number!=chan->pmt_version && (pmt->table_id==0x02) && !chan->pmt_need_update)
		{
			/*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
        sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
        and shall be the next table to become valid.*/
			if(pmt->current_next_indicator == 0)
			{
				return;
			}
			log_message( log_module, MSG_DEBUG,"PMT Need update. stored version : %d, new: %d\n",chan->pmt_version,pmt->version_number);
			//We check if the PMT belongs to this channel
			if(chan->service_id && (chan->service_id != HILO(pmt->program_number)) )
			{
				return;
			}
			chan->pmt_need_update=1;
		}
	}
}


int chan_pmt_ok(mumudvb_channel_t *channel, mumudvb_ts_packet_t *pmt)
{
	pmt_t *header;

	header=(pmt_t *)pmt->data_full;

	if(header->table_id!=0x02)
	{
		log_message( log_module,  MSG_WARN,"Packet PID %d for channel \"%s\" is not a PMT PID. We remove the PMT PID for this channel\n", pmt->pid, channel->name);
		channel->pid_i.pmt_pid=0;
		return 0;
	}

	//We check if this PMT belongs to the current channel.
	if(channel->service_id && (channel->service_id != HILO(header->program_number)) )
	{
		log_message( log_module,  MSG_DETAIL,"The PMT %d does not belongs to channel \"%s\"\n", pmt->pid, channel->name);
		return 0;
	}


	//If everything seems fine we set update to 0 maybe a false alarm before
	if(header->version_number==channel->pmt_version &&
			channel->pmt_need_update)
	{
		channel->pmt_need_update=0;
		log_message( log_module, MSG_DETAIL,"False PMT update alarm for channel \"%s\" sid %d", channel->name,channel->service_id);
		return 0;
	}
	else//We update the PMT version stored
		channel->pmt_version=header->version_number;

	return 1;
}


/** @brief This function is called when a new PMT packet is there, the PMT will be downloaded,
 *  once done the proper flags will be set :  Autoconfiguration and CAM (SCAM just need the packet) */
void chan_new_pmt(unsigned char *ts_packet, mumu_chan_p_t *chan_p, int pid)
{

	for(int ichan=0;ichan<MAX_CHANNELS;ichan++)
	{
		//for the PMT we look only for channels with status READY
		if(pid &&
				(chan_p->channels[ichan].pid_i.pmt_pid==pid)&&
				(chan_p->channels[ichan].channel_ready>=READY))
		{
			if(!chan_p->channels[ichan].pmt_need_update)
				chan_pmt_need_update(&chan_p->channels[ichan],ts_packet);
			if(chan_p->channels[ichan].pmt_need_update)
				log_message( log_module, MSG_DEBUG,"We update the PMT for channel %d sid %d", ichan,chan_p->channels[ichan].service_id);
			//since we are looping on channels and modifing the packet pointer we need to copy it
			unsigned char *curr_ts_packet;
			curr_ts_packet=ts_packet;
			while(chan_p->channels[ichan].pmt_need_update && get_ts_packet(curr_ts_packet,chan_p->channels[ichan].pmt_packet))
			{
				curr_ts_packet=NULL; // next call we only POP packets from the stack
				//If everything ok, we set the proper flags
				if(chan_pmt_ok(&chan_p->channels[ichan], chan_p->channels[ichan].pmt_packet))
				{
					chan_p->channels[ichan].pmt_need_update=0;
					//We tell autoconf a new PMT is here
					chan_p->channels[ichan].autoconf_pmt_need_update=1;
					//We tell the CAM a new PMT is here
					if(chan_p->channels[ichan].need_cam_ask==CAM_ASKED)
						chan_p->channels[ichan].need_cam_ask=CAM_NEED_UPDATE; //We we send again this packet to the CAM
				}
			}
		}

	}
}


/** @brief Update the CAM information for the channels
 */
void chan_update_CAM(mumu_chan_p_t *chan_p, auto_p_t *auto_p,  void *scam_vars_v)
{
	int ichan=0;
#ifndef ENABLE_SCAM_SUPPORT
	(void) scam_vars_v; //to make compiler happy
#else
	scam_parameters_t *scam_vars=(scam_parameters_t *)scam_vars_v;
#endif

	for (ichan = 0; ichan < chan_p->number_of_channels; ichan++)
	{
		if(chan_p->channels[ichan].channel_ready!=READY)
			continue;
		//This is a scrambled channel, we will have to ask the cam for descrambling it
		if(auto_p->autoconf_scrambled && chan_p->channels[ichan].free_ca_mode)
		{
			//It was not asked before, we ask it
			if(chan_p->channels[ichan].need_cam_ask==CAM_NO_ASK)
				chan_p->channels[ichan].need_cam_ask=CAM_NEED_ASK;
			//If it was asked we ask for refresh
			if(chan_p->channels[ichan].need_cam_ask==CAM_ASKED)
				chan_p->channels[ichan].need_cam_ask=CAM_NEED_UPDATE;
		}


#ifdef ENABLE_SCAM_SUPPORT
		if (chan_p->channels[ichan].free_ca_mode && scam_vars->scam_support && chan_p->channels[ichan].scam_support == 0) {
		    auto_p->need_filter_chan_update = 1;
			chan_p->channels[ichan].scam_support=1;
			chan_p->channels[ichan].need_scam_ask=CAM_NEED_ASK;
			chan_p->channels[ichan].ring_buffer_size=scam_vars->ring_buffer_default_size;
			chan_p->channels[ichan].decsa_delay=scam_vars->decsa_default_delay;
			chan_p->channels[ichan].send_delay=scam_vars->send_default_delay;
		}
#endif
	}
}






/** @brief Set the networking for the channels almost ready
 */
void update_chan_net(mumu_chan_p_t *chan_p, auto_p_t *auto_p, multi_p_t *multi_p, unicast_parameters_t *unicast_vars, int server_id, int card, int tuner)
{
	pthread_mutex_lock(&chan_p->lock);
	int ichan;
	char tempstring[256];
	int unicast_port_per_channel;
	unicast_port_per_channel=strlen(auto_p->autoconf_unicast_port)?1:0;

	for (ichan = 0; ichan < chan_p->number_of_channels; ichan++)
	{
		if(chan_p->channels[ichan].channel_ready!=ALMOST_READY)
			continue;
		chan_p->channels[ichan].channel_ready=READY;
		//RTP init (even if no RTP, costs nothing)
		if(chan_p->channels[ichan].buf_with_rtp_header[0]!=128)
			init_rtp_header(&chan_p->channels[ichan]); //We init the RTP header in all cases
		//We update the unicast port, the connection will be created in autoconf_finish_full
		if(unicast_port_per_channel && unicast_vars->unicast && MU_F(chan_p->channels[ichan].unicast_port)!=F_USER)
		{
			strcpy(tempstring,auto_p->autoconf_unicast_port);
			int len;len=256;
			char number[10];
			sprintf(number,"%d",ichan);
			mumu_string_replace(tempstring,&len,0,"%number",number);
			sprintf(number,"%d",card);
			mumu_string_replace(tempstring,&len,0,"%card",number);
			sprintf(number,"%d",tuner);
			mumu_string_replace(tempstring,&len,0,"%tuner",number);
			sprintf(number,"%d",server_id);
			mumu_string_replace(tempstring,&len,0,"%server",number);
			//SID
			sprintf(number,"%d",chan_p->channels[ichan].service_id);
			mumu_string_replace(tempstring,&len,0,"%sid",number);
			chan_p->channels[ichan].unicast_port=string_comput(tempstring);
			log_message( log_module, MSG_DEBUG,"Channel (direct) unicast port  %d\n",chan_p->channels[ichan].unicast_port);
		}


		if(multi_p->multicast)
		{
			char number[10];
			char ip[80];
			int len=80;
			//We store if we send this channel with RTP, later it can be made channel dependent.
			chan_p->channels[ichan].rtp=multi_p->rtp_header;

			if(auto_p->autoconfiguration && strlen(auto_p->autoconf_multicast_port) && MU_F(chan_p->channels[ichan].portOut)!=F_USER)
			{
				strcpy(tempstring,auto_p->autoconf_multicast_port);
				sprintf(number,"%d",ichan);
				mumu_string_replace(tempstring,&len,0,"%number",number);
				sprintf(number,"%d",card);
				mumu_string_replace(tempstring,&len,0,"%card",number);
				sprintf(number,"%d",tuner);
				mumu_string_replace(tempstring,&len,0,"%tuner",number);
				sprintf(number,"%d",server_id);
				mumu_string_replace(tempstring,&len,0,"%server",number);
				//SID
				sprintf(number,"%d",chan_p->channels[ichan].service_id);
				mumu_string_replace(tempstring,&len,0,"%sid",number);
				chan_p->channels[ichan].portOut=string_comput(tempstring);
			}
			else if(MU_F(chan_p->channels[ichan].portOut)!=F_USER)
			{
				chan_p->channels[ichan].portOut=multi_p->common_port;//do here the job for evaluating the string
			}
			if(auto_p->autoconfiguration && multi_p->multicast_ipv4 && MU_F(chan_p->channels[ichan].ip4Out)!=F_USER)
			{
				strcpy(ip,auto_p->autoconf_ip4);
				sprintf(number,"%d",ichan);
				mumu_string_replace(ip,&len,0,"%number",number);
				sprintf(number,"%d",card);
				mumu_string_replace(ip,&len,0,"%card",number);
				sprintf(number,"%d",tuner);
				mumu_string_replace(ip,&len,0,"%tuner",number);
				sprintf(number,"%d",server_id);
				mumu_string_replace(ip,&len,0,"%server",number);
				//SID
				sprintf(number,"%d",(chan_p->channels[ichan].service_id&0xFF00)>>8);
				mumu_string_replace(ip,&len,0,"%sid_hi",number);
				sprintf(number,"%d",chan_p->channels[ichan].service_id&0x00FF);
				mumu_string_replace(ip,&len,0,"%sid_lo",number);
				// Compute the string, ex: 239.255.130+0*10+2.1
				log_message( log_module, MSG_DEBUG,"Computing expressions in string \"%s\"\n",ip);
				//Splitting and computing. use of strtok_r because it's safer
				int tn[4];
				char *sptr;
				tn[0]=string_comput(strtok_r (ip,".",&sptr));
				tn[1]=string_comput(strtok_r (NULL,".",&sptr));
				tn[2]=string_comput(strtok_r (NULL,".",&sptr));
				tn[3]=string_comput(strtok_r (NULL,".",&sptr));
				sprintf(chan_p->channels[ichan].ip4Out,"%d.%d.%d.%d",tn[0],tn[1],tn[2],tn[3]); // In C the evaluation order of arguments in a fct  is undefined, no more easy factoring
			}
			if(auto_p->autoconfiguration && multi_p->multicast_ipv6  && MU_F(chan_p->channels[ichan].ip6Out)!=F_USER )
			{
				strcpy(ip,auto_p->autoconf_ip6);
				sprintf(number,"%d",ichan);
				mumu_string_replace(ip,&len,0,"%number",number);
				sprintf(number,"%d",card);
				mumu_string_replace(ip,&len,0,"%card",number);
				sprintf(number,"%d",tuner);
				mumu_string_replace(ip,&len,0,"%tuner",number);
				sprintf(number,"%d",server_id);
				mumu_string_replace(ip,&len,0,"%server",number);
				//SID
				sprintf(number,"%04x",chan_p->channels[ichan].service_id);
				mumu_string_replace(ip,&len,0,"%sid",number);
				strncpy(chan_p->channels[ichan].ip6Out,ip,IPV6_CHAR_LEN);
				chan_p->channels[ichan].ip6Out[IPV6_CHAR_LEN-1]='\0';
			}
		}


		/** open the unicast listening connections for the channels which don't have one */
		if(chan_p->channels[ichan].unicast_port && unicast_vars->unicast && (chan_p->channels[ichan].socketIn <=0))
		{
			log_message( log_module, MSG_INFO,"Unicast : We open the channel %d http socket address %s:%d\n",
					ichan,
					unicast_vars->ipOut,
					chan_p->channels[ichan].unicast_port);
			unicast_create_listening_socket(UNICAST_LISTEN_CHANNEL,
					ichan,
					unicast_vars->ipOut,
					chan_p->channels[ichan].unicast_port,
					&chan_p->channels[ichan].sIn,
					&chan_p->channels[ichan].socketIn,
					unicast_vars);
		}


		//Open the multicast socket for the new channel which don't have them opened
		if(multi_p->multicast && multi_p->multicast_ipv4 && (chan_p->channels[ichan].socketOut4 <=0))
		{
			log_message( log_module, MSG_INFO,"We open the channel %d multicast IPv4 socket address %s:%d\n",
								ichan,
								chan_p->channels[ichan].ip4Out,
								chan_p->channels[ichan].portOut);

			if(multi_p->auto_join) //See the README for the reason of this option
				chan_p->channels[ichan].socketOut4 =
						makeclientsocket (chan_p->channels[ichan].ip4Out,
								chan_p->channels[ichan].portOut,
								multi_p->ttl,
								multi_p->iface4,
								&chan_p->channels[ichan].sOut4);
			else
				chan_p->channels[ichan].socketOut4 =
						makesocket (chan_p->channels[ichan].ip4Out,
								chan_p->channels[ichan].portOut,
								multi_p->ttl,
								multi_p->iface4,
								&chan_p->channels[ichan].sOut4);

		}
		if(multi_p->multicast && multi_p->multicast_ipv6 && (chan_p->channels[ichan].socketOut6 <=0))
		{
			log_message( log_module, MSG_INFO,"We open the channel %d multicast IPv6 socket address %s:%d\n",
								ichan,
								chan_p->channels[ichan].ip6Out,
								chan_p->channels[ichan].portOut);

			if(multi_p->auto_join) //See the README for the reason of this option
				chan_p->channels[ichan].socketOut6 =
						makeclientsocket6 (chan_p->channels[ichan].ip6Out,
								chan_p->channels[ichan].portOut,
								multi_p->ttl,
								multi_p->iface6,
								&chan_p->channels[ichan].sOut6);
			else
				chan_p->channels[ichan].socketOut6 =
						makesocket6 (chan_p->channels[ichan].ip6Out,
								chan_p->channels[ichan].portOut,
								multi_p->ttl,
								multi_p->iface6,
								&chan_p->channels[ichan].sOut6);
		}

		/******************************************************/
		//   SCAM START PART
		/******************************************************/
#ifdef ENABLE_SCAM_SUPPORT
		if (chan_p->channels[ichan].scam_support && !chan_p->channels[ichan].scam_support_started)
		{
			set_interrupted(scam_channel_start(&chan_p->channels[ichan], unicast_vars));
			chan_p->channels[ichan].scam_support_started=1;
		}
#endif
		/******************************************************/
		//   SCAM START PART FINISHED
		/******************************************************/


	}


	pthread_mutex_unlock(&chan_p->lock);

}





/** Update the filters of the channels, this function also searches for closed PIDs */
void update_chan_filters(mumu_chan_p_t *chan_p, char *card_base_path, int tuner, fds_t *fds)
{
	log_message( log_module, MSG_INFO,"Looking through all services to update their filters");
	pthread_mutex_lock(&chan_p->lock);
	uint8_t asked_pid[8193];
	//Clear
	memset(asked_pid,PID_NOT_ASKED,8193*sizeof(uint8_t));
	//We store the PIDs which are needed by the channels
	for (int ichan = 0; ichan < chan_p->number_of_channels; ichan++)
	{
		//We add PIDs only for channels almost ready at least
		if(chan_p->channels[ichan].channel_ready>=ALMOST_READY)
			for (int ipid = 0; ipid < chan_p->channels[ichan].pid_i.num_pids; ipid++)
			{
					asked_pid[chan_p->channels[ichan].pid_i.pids[ipid]]=PID_ASKED;
			}
	}


	//Now we compare with the ones for the channels
	for (int ipid = MAX_MANDATORY_PID_NUMBER; ipid < 8193; ipid++)
	{
		//Now we have the PIDs we look for those who disappeared
		if(chan_p->asked_pid[ipid]==PID_ASKED && asked_pid[ipid]!=PID_ASKED)
		{
			log_message( log_module,  MSG_DEBUG, "Update : PID %d does not belong to any channel anymore, we close the filter",
					ipid);
			close(fds->fd_demuxer[ipid]);
			fds->fd_demuxer[ipid]=0;
			chan_p->asked_pid[ipid]=PID_NOT_ASKED;
		}
		//And we look for the PIDs who are now asked
		else if(asked_pid[ipid]==PID_ASKED
				&& chan_p->asked_pid[ipid]!=PID_ASKED
				&& chan_p->asked_pid[ipid]!=PID_FILTERED)
		{

			log_message( log_module,  MSG_DETAIL, " pid %d added \n",ipid);
			//If the PID is not on the list we add it for the filters
			chan_p->asked_pid[ipid]=PID_ASKED;
		}

	}
	log_message( log_module, MSG_DETAIL,"Open the new filters");
	// we open the file descriptors
	if (create_card_fd (card_base_path, tuner, chan_p->asked_pid, fds) < 0)
	{
		log_message( log_module, MSG_ERROR,"ERROR : CANNOT open the new descriptors. Some channels will probably not work");
	}
	set_filters(chan_p->asked_pid, fds);

	pthread_mutex_unlock(&chan_p->lock);
}






































