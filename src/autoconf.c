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
 *  @brief This file contain the code related to the autoconfiguration of MuMudvb
 * 
 *  It contains the functions to extract the relevant informations from the PAT,PMT,SDT PIDs and from ATSC PSIP table
 * 
 *  The PAT contains the list of the channels in the actual stream, their service number and the PMT PID
 * 
 *  The SDT contains the name of the channels associated to a certain service number and the type of service
 *
 *  The PSIP (ATSC only) table contains the same kind of information as the SDT
 *
 *  The PMT contains the PIDs (audio video etc ...) of the channels,
 *
 *  The idea is the following
 *  All the channels are uniquely identified with their SID
 *  For each parameter of the channel, it will not be updated by autoconf if user set (except templates)
 *  From the PAT we extract the channel list with the PMT PID, this list is too complete, some services in the PAT are not TV/radio channels
 *  The SDT table allow to extract the name of the service and the service type, from the service type we flag the channels we want
 *  On these channels once all the PAT and SDT sections are read, we open the sockets and the filters and wait for their PMT
 *  From the PMT PIDs we extract audio and video information.
 *  All these functions are intended to be triggered at any time in case of update.
 *
 *  When a channel is removed from the PAT, the information about the channel is kept, it is just marked as being removed.
 *  This allows to keep the IP if the channel comes back
 */


#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/poll.h>


#include "errors.h"
#include "ts.h"
#include "mumudvb.h"
#include "dvb.h"
#include "network.h"
#include "autoconf.h"
#include "rtp.h"
#include "log.h"
#ifdef ENABLE_SCAM_SUPPORT
#include "scam_capmt.h"
#include "scam_common.h"
#include "scam_getcw.h"
#include "scam_decsa.h"
#endif

static char *log_module="Autoconf: ";


int autoconf_read_pat(auto_p_t *auto_p,mumu_chan_p_t *chan_p);
int autoconf_read_sdt(auto_p_t *auto_p,mumu_chan_p_t *chan_p);
int autoconf_read_psip(auto_p_t *auto_p,mumu_chan_p_t *chan_p);
int autoconf_read_nit(auto_p_t *parameters,mumu_chan_p_t *chan_p);
int autoconf_read_pmt(mumudvb_channel_t *channel, mumudvb_ts_packet_t *pmt);
int autoconf_pat_need_update(auto_p_t *auto_p, unsigned char *buf);
void autoconf_sdt_need_update(auto_p_t *auto_p, unsigned char *buf);
int autoconf_nit_need_update(auto_p_t *auto_p, unsigned char *buf);
void autoconf_psip_need_update(auto_p_t *auto_p, unsigned char *buf);


/** Initialize Autoconf variables*/
void init_aconf_v(auto_p_t *aconf_p)
{
	memset(aconf_p,0,sizeof(auto_p_t));

	//Since we have 'memsetted' the structure we only set non zero values
	*aconf_p=(auto_p_t){
		.autoconf_pid_update=1,
		.autoconf_ip4="239.100.%card.%number",
		.autoconf_ip6="FF15:4242::%server:%card:%number",
		.transport_stream_id=-1,
		.pat_version=-1,
		.sdt_version=-1,
		.nit_version=-1,
	};
}



/** @brief Read a line of the configuration file to check if there is a autoconf parameter
 *
 * @param auto_p the autoconfiguration parameters
 * @param substring The currrent line
 */
int read_autoconfiguration_configuration(auto_p_t *auto_p, char *substring)
{

	char delimiteurs[] = CONFIG_FILE_SEPARATOR;

	if (!strcmp (substring, "autoconf_scrambled"))
	{
		substring = strtok (NULL, delimiteurs);
		auto_p->autoconf_scrambled = atoi (substring);
	}
	else if (!strcmp (substring, "autoconf_pid_update"))
	{
		substring = strtok (NULL, delimiteurs);
		auto_p->autoconf_pid_update = atoi (substring);
	}
	else if (!strcmp (substring, "autoconfiguration"))
	{
		substring = strtok (NULL, delimiteurs);
		if(atoi (substring)==1)
			auto_p->autoconfiguration = AUTOCONF_MODE_FULL;
		else if (!strcmp (substring, "full"))
			auto_p->autoconfiguration = AUTOCONF_MODE_FULL;
		else if (!strcmp (substring, "none"))
			auto_p->autoconfiguration = AUTOCONF_MODE_NONE;


		if(!((auto_p->autoconfiguration==AUTOCONF_MODE_FULL)||(auto_p->autoconfiguration==AUTOCONF_MODE_NONE)))
		{
			log_message( log_module,  MSG_WARN,
					"Bad value for autoconfiguration, autoconfiguration will not be run\n");
			auto_p->autoconfiguration=AUTOCONF_MODE_NONE;
		}
	}
	else if (!strcmp (substring, "autoconf_radios"))
	{
		substring = strtok (NULL, delimiteurs);
		auto_p->autoconf_radios = atoi (substring);
		if(!(auto_p->autoconfiguration==AUTOCONF_MODE_FULL))
		{
			log_message( log_module,  MSG_INFO,
					"You have to set autoconfiguration in full mode to use autoconf of the radios\n");
		}
	}
	else if ((!strcmp (substring, "autoconf_ip4")))
	{
		substring = strtok (NULL, delimiteurs);
		if(strlen(substring)>79)
		{
			log_message( log_module,  MSG_ERROR,
					"The autoconf ip v4 is too long\n");
			return -1;
		}
		sscanf (substring, "%s\n", auto_p->autoconf_ip4);
	}
	else if (!strcmp (substring, "autoconf_ip6"))
	{
		substring = strtok (NULL, delimiteurs);
		if(strlen(substring)>79)
		{
			log_message( log_module,  MSG_ERROR,
					"The autoconf ip v6 is too long\n");
			return -1;
		}
		sscanf (substring, "%s\n", auto_p->autoconf_ip6);
	}
	/**  option for the starting http unicast port (for autoconf full)*/
	else if (!strcmp (substring, "autoconf_unicast_start_port"))
	{
		substring = strtok (NULL, delimiteurs);
		sprintf(auto_p->autoconf_unicast_port,"%d +%%number",atoi (substring));
	}
	/**  option for the http unicast port (for autoconf full) parsed version*/
	else if (!strcmp (substring, "autoconf_unicast_port"))
	{
		substring = strtok (NULL, "=");
		if(strlen(substring)>255)
		{
			log_message( log_module,  MSG_ERROR,
					"The autoconf_unicast_port is too long\n");
			return -1;
		}
		strcpy(auto_p->autoconf_unicast_port,substring);
	}
	/**  option for the http multicast port (for autoconf full) parsed version*/
	else if (!strcmp (substring, "autoconf_multicast_port"))
	{
		substring = strtok (NULL, "=");
		if(strlen(substring)>255)
		{
			log_message( log_module,  MSG_ERROR,
					"The autoconf_multicast_port is too long\n");
			return -1;
		}
		strcpy(auto_p->autoconf_multicast_port,substring);
	}
	else if (!strcmp (substring, "autoconf_sid_list"))
	{
		while ((substring = strtok (NULL, delimiteurs)) != NULL)
		{
			if (auto_p->num_service_id >= MAX_CHANNELS)
			{
				log_message( log_module,  MSG_ERROR,
						"Autoconfiguration : Too many ts id : %d\n",
						auto_p->num_service_id);
				return -1;
			}
			auto_p->service_id_list[auto_p->num_service_id] = atoi (substring);
			auto_p->num_service_id++;
		}
	}
	else if (!strcmp (substring, "autoconf_name_template"))
	{
		// other substring extraction method in order to keep spaces
		substring = strtok (NULL, "=");
		strncpy(auto_p->name_template,strtok(substring,"\n"),MAX_NAME_LEN-1);
		auto_p->name_template[MAX_NAME_LEN-1]='\0';
		if (strlen (substring) >= MAX_NAME_LEN - 1)
			log_message( log_module,  MSG_WARN,"Autoconfiguration: Channel name template too long\n");
	}
	else
		return 0; //Nothing concerning autoconfiguration, we return 0 to explore the other possibilities

	return 1;//We found something for autoconfiguration, we tell main to go for the next line
}


/** @brief initialize the autoconfiguration : alloc the memory etc...
 *
 */
int autoconf_init(auto_p_t *auto_p)
{
	if(auto_p->autoconfiguration)
	{
		auto_p->autoconf_temp_pat=malloc(sizeof(mumudvb_ts_packet_t));
		if(auto_p->autoconf_temp_pat==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			set_interrupted(ERROR_MEMORY<<8);
			return -1;
		}
		memset (auto_p->autoconf_temp_pat, 0, sizeof( mumudvb_ts_packet_t));//we clear it
		pthread_mutex_init(&auto_p->autoconf_temp_pat->packetmutex,NULL);
		auto_p->autoconf_temp_sdt=malloc(sizeof(mumudvb_ts_packet_t));
		if(auto_p->autoconf_temp_sdt==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			set_interrupted(ERROR_MEMORY<<8);
			return -1;
		}
		memset (auto_p->autoconf_temp_sdt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
		pthread_mutex_init(&auto_p->autoconf_temp_sdt->packetmutex,NULL);

		auto_p->autoconf_temp_psip=malloc(sizeof(mumudvb_ts_packet_t));
		if(auto_p->autoconf_temp_psip==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			set_interrupted(ERROR_MEMORY<<8);
			return -1;
		}
		memset (auto_p->autoconf_temp_psip, 0, sizeof( mumudvb_ts_packet_t));//we clear it
		pthread_mutex_init(&auto_p->autoconf_temp_psip->packetmutex,NULL);

		auto_p->autoconf_temp_nit=malloc(sizeof(mumudvb_ts_packet_t));
		if(auto_p->autoconf_temp_nit==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			set_interrupted(ERROR_MEMORY<<8);
			return -1;
		}
		memset (auto_p->autoconf_temp_nit, 0, sizeof( mumudvb_ts_packet_t));//we clear it
		pthread_mutex_init(&auto_p->autoconf_temp_nit->packetmutex,NULL);
	}
	return 0;

}


/****************************************************************************/
//Parts of this code (read of the pmt and read of the pat)
// from libdvb, strongly modified, with commentaries added
/****************************************************************************/




/**@brief Free the autoconf parameters.
 *
 * @param auto_p pointer to the autoconf structure
 */
void autoconf_freeing(auto_p_t *auto_p)
{
	if(auto_p->autoconf_temp_sdt)
	{
		free(auto_p->autoconf_temp_sdt);
		auto_p->autoconf_temp_sdt=NULL;
	}
	if(auto_p->autoconf_temp_psip)
	{
		free(auto_p->autoconf_temp_psip);
		auto_p->autoconf_temp_psip=NULL;
	}
	if(auto_p->autoconf_temp_pat)
	{
		free(auto_p->autoconf_temp_pat);
		auto_p->autoconf_temp_pat=NULL;
	}
}







/** Update the status of the channels */
void autoconf_update_chan_status(auto_p_t *auto_p,mumu_chan_p_t *chan_p)
{
	//TODO: this function is a duplicate of what is done at the init of the global program : merge it
	log_message( log_module, MSG_INFO,"Looking through all channels to see if they are ready for streaming");
	pthread_mutex_lock(&chan_p->lock);
	for (int ichan = 0; ichan < chan_p->number_of_channels; ichan++)
	{
		//If service removed we let it like that
		if(chan_p->channels[ichan].channel_ready==REMOVED)
			continue;
		//If channel user specified, it's always up
		if( MU_F(chan_p->channels[ichan].service_id)!=F_DETECTED)
			continue;


		if(auto_p->autoconf_scrambled && chan_p->channels[ichan].free_ca_mode)
		{
				log_message( log_module, MSG_DETAIL,"Channel scrambled, no CAM support and no autoconf_scrambled, we skip. Name \"%s\"",
						chan_p->channels[ichan].name);
				chan_p->channels[ichan].channel_ready=NO_STREAMING;
				continue;
		}
		if(!chan_p->channels[ichan].pid_i.pmt_pid)
		{
				log_message( log_module, MSG_DETAIL,"Service without a PMT PID, we skip. Name \"%s\"",
						chan_p->channels[ichan].name);
				chan_p->channels[ichan].channel_ready=NO_STREAMING;
				continue;
		}
		//The service was autodetected, we check it's present in the SID list
		if(auto_p->num_service_id)
		{
			int sid_i;
			int found_in_service_id_list=0;
			for(sid_i=0;sid_i<auto_p->num_service_id && !found_in_service_id_list;sid_i++)
			{
				if(auto_p->service_id_list[sid_i]==chan_p->channels[ichan].service_id)
				{
					found_in_service_id_list=1;
					log_message( log_module, MSG_DEBUG,"Service found in the service_id list. Name \"%s\"",
							chan_p->channels[ichan].name);
				}
			}
			if(found_in_service_id_list==0)
			{
				log_message( log_module, MSG_DETAIL,"Service NOT in the service_id list, we skip. Name \"%s\", id %d\n",
						chan_p->channels[ichan].name,
						chan_p->channels[ichan].service_id);
				chan_p->channels[ichan].channel_ready=NO_STREAMING;
				continue;
			}
		}


		//Cf EN 300 468 v1.9.1 Table 81
		//Everything seems to be OK, we check if this is a radio or a TV channel
		if((chan_p->channels[ichan].service_type==0x01||
				chan_p->channels[ichan].service_type==0x11||
				chan_p->channels[ichan].service_type==0x16||
				chan_p->channels[ichan].service_type==0x19)||
				((chan_p->channels[ichan].service_type==0x02||
						chan_p->channels[ichan].service_type==0x0a)&&auto_p->autoconf_radios))
		{
			log_message( log_module, MSG_DETAIL,"Service OK becoming ready. Name \"%s\", id %d type %s",
					chan_p->channels[ichan].name,
					chan_p->channels[ichan].service_id, service_type_to_str(chan_p->channels[ichan].service_type));
			//We set it to almost ready because network is not up yet
			chan_p->channels[ichan].channel_ready=ALMOST_READY;
		}
		else if(chan_p->channels[ichan].service_type==0x02||chan_p->channels[ichan].service_type==0x0a) //service_type digital radio sound service
			log_message( log_module, MSG_DETAIL,"Service type digital radio sound service, no autoconfigure. (if you want add autoconf_radios=1 to your configuration file) Name \"%s\"\n",
					chan_p->channels[ichan].name);
		else if(chan_p->channels[ichan].service_type!=0) //0 is an empty service
		{
			//We show the service type
			log_message( log_module, MSG_DETAIL,"No autoconfigure due to service type : %s. Name \"%s\"\n",
					service_type_to_str(chan_p->channels[ichan].service_type),
					chan_p->channels[ichan].name);
		}
	}
	pthread_mutex_unlock(&chan_p->lock);
}


/** @brief Set the networking for the channels almost ready
 */
void autoconf_update_chan_name(mumu_chan_p_t *chan_p, auto_p_t *auto_p)
{
	//TODO: this function is a duplicate of what is done at the init of the global program : merge it
	for (int ichan = 0; ichan < chan_p->number_of_channels; ichan++)
	{
		//We copy the good variable to the current channel name depending is this was user set or not
		if(strlen(auto_p->name_template) && MU_F(chan_p->channels[ichan].name)!=F_USER)
		{
			strcpy(chan_p->channels[ichan].name,auto_p->name_template);
			MU_F(chan_p->channels[ichan].name)=F_DETECTED;
		}
		else if(MU_F(chan_p->channels[ichan].name)!=F_USER)
		{
			strcpy(chan_p->channels[ichan].name,chan_p->channels[ichan].service_name);
			MU_F(chan_p->channels[ichan].name)=F_DETECTED;
		}
		else
			strcpy(chan_p->channels[ichan].name,chan_p->channels[ichan].user_name);

		//No we apply the templates
		int len=MAX_NAME_LEN;
		char number[10];
		mumu_string_replace(chan_p->channels[ichan].name,&len,0,"%name",chan_p->channels[ichan].service_name);
		sprintf(number,"%d",ichan+1);
		mumu_string_replace(chan_p->channels[ichan].name,&len,0,"%number",number);

		char lcn[4];
		if(chan_p->channels[ichan].logical_channel_number)
		{
			sprintf(lcn,"%03d",chan_p->channels[ichan].logical_channel_number);
			mumu_string_replace(chan_p->channels[ichan].name,&len,0,"%lcn",lcn);
			sprintf(lcn,"%02d",chan_p->channels[ichan].logical_channel_number);
			mumu_string_replace(chan_p->channels[ichan].name,&len,0,"%2lcn",lcn);
		}
		else
		{
			mumu_string_replace(chan_p->channels[ichan].name,&len,0,"%lcn","");
			mumu_string_replace(chan_p->channels[ichan].name,&len,0,"%2lcn","");
		}

		/*************************
		 * Language template
		 **************************/
		int found =0;
		len=MAX_NAME_LEN;
		for(int i=0;i<chan_p->channels[ichan].pid_i.num_pids && !found;i++)
		{
			if(chan_p->channels[ichan].pid_i.pids_language[i][0]!='-')
			{
				log_message( log_module,  MSG_FLOOD, "Primary language for channel: %s",chan_p->channels[ichan].pid_i.pids_language[i]);
				mumu_string_replace(chan_p->channels[ichan].name,&len,0,"%lang",chan_p->channels[ichan].pid_i.pids_language[i]);
				found=1; //we exit the loop
			}
		}
		//If we don't find a lang we replace by our "usual" ---
		if(!found)
			mumu_string_replace(chan_p->channels[ichan].name,&len,0,"%lang",chan_p->channels[ichan].pid_i.pids_language[0]);
		/*************************
		 * Language template END
		 **************************/
		/*************************
		 * Show the result
		 **************************/
		log_message( log_module, MSG_DEBUG, "Channel SID %d service name: \"%s\" user name: \"%s\" channel name: \"%s\"",
				chan_p->channels[ichan].service_id,
				chan_p->channels[ichan].service_name,
				chan_p->channels[ichan].user_name,
				chan_p->channels[ichan].name);

		/*************************
		 * SAP update
		 **************************/
		chan_p->channels[ichan].sap_need_update=1;
	}
}




/********************************************************************
 * Autoconfiguration new packet functions
 ********************************************************************/
/** @brief This function is called when a new packet is there and the autoconf is not finished*/
int autoconf_new_packet(int pid, unsigned char *ts_packet, auto_p_t *auto_p, fds_t *fds, mumu_chan_p_t *chan_p, tune_p_t *tune_p, multi_p_t *multi_p,  unicast_parameters_t *unicast_vars, int server_id, void *scam_vars)
{
	if(auto_p->autoconfiguration==AUTOCONF_MODE_FULL) //Full autoconfiguration, we search the channels and their names
	{
		if(pid==0) //PAT : contains the services identifiers and the PMT PID for each service
		{
			if((auto_p->autoconfiguration==AUTOCONF_MODE_FULL))
			{
				//In case of wrong CRC32, at the next call it will go to 0
				autoconf_pat_need_update(auto_p,ts_packet);
				while(auto_p->pat_need_update && get_ts_packet(ts_packet,auto_p->autoconf_temp_pat))
				{
					ts_packet=NULL; // next call we only POP packets from the stack
					autoconf_read_pat(auto_p,chan_p);
				}
			}
		}
		else if(pid==17) //SDT : contains the names of the services
		{
			if(auto_p->pat_all_sections_seen)
			{
				autoconf_sdt_need_update(auto_p,ts_packet);
				while(auto_p->sdt_need_update && get_ts_packet(ts_packet,auto_p->autoconf_temp_sdt))
				{
					ts_packet=NULL; // next call we only POP packets from the stack
					autoconf_read_sdt(auto_p,chan_p);
				}
			}
		}
		else if(pid==PSIP_PID && tune_p->fe_type==FE_ATSC) //PSIP : contains the names of the services
		{
			if(auto_p->pat_all_sections_seen)
			{
				if(!auto_p->psip_need_update)
					autoconf_psip_need_update(auto_p,ts_packet);
				while(auto_p->psip_need_update && get_ts_packet(ts_packet,auto_p->autoconf_temp_psip))
				{
					ts_packet=NULL; // next call we only POP packets from the stack
					autoconf_read_psip(auto_p,chan_p);
				}
			}
		}
		else if(pid==16) //NIT : Network Information Table
		{
			if(auto_p->pat_all_sections_seen)
			{
				if(!auto_p->nit_need_update)
					autoconf_nit_need_update(auto_p,ts_packet);
				while(auto_p->nit_need_update && get_ts_packet(ts_packet,auto_p->autoconf_temp_nit))
				{
					ts_packet=NULL; // next call we only POP packets from the stack
					if(autoconf_read_nit(auto_p, chan_p))
					{
						//We update the names for the %lcn
						log_message( log_module, MSG_INFO,"We got the NIT, we update the channel names");
						autoconf_update_chan_name(chan_p, auto_p);
					}

				}
			}
		}
		if(auto_p->need_filter_chan_update)
		{
			//We update all aspects of the channels
			log_message( log_module, MSG_INFO,"We update the channel names");
			autoconf_update_chan_name(chan_p, auto_p);
			log_message( log_module, MSG_INFO,"We update the channel status");
			autoconf_update_chan_status(auto_p,chan_p);
			log_message( log_module, MSG_INFO,"We update the channel filters");
			update_chan_filters(chan_p, tune_p->card_dev_path, tune_p->tuner, fds);
			log_message( log_module, MSG_INFO,"We update the channel networking");
			chan_update_net(chan_p, auto_p, multi_p, unicast_vars, server_id, tune_p->card, tune_p->tuner,fds);
			auto_p->need_filter_chan_update=0;
		}
		//PMT PID analysis, only for channels being marked as ready
		if(auto_p->pat_all_sections_seen)
		{
			int ichan;
			int channel_updated=0;
			for(ichan=0;ichan<MAX_CHANNELS;ichan++)
			{
				if(pid &&
						(chan_p->channels[ichan].pid_i.pmt_pid==pid)&&
						(chan_p->channels[ichan].channel_ready>=READY) &&
						(chan_p->channels[ichan].autoconf_pmt_need_update))
				{
					if(autoconf_read_pmt(&chan_p->channels[ichan], chan_p->channels[ichan].pmt_packet))
					{
						chan_p->channels[ichan].autoconf_pmt_need_update=0;
						log_pids(log_module,&chan_p->channels[ichan],ichan);
						autoconf_update_chan_name(chan_p, auto_p);
						update_chan_filters(chan_p, tune_p->card_dev_path, tune_p->tuner, fds);
						log_message( log_module, MSG_INFO,"We update the channel CAM support");
						chan_update_CAM(chan_p, auto_p,  scam_vars);
						channel_updated=1;
					}
				}
			}
			if(channel_updated)
			{
				//check if all PMT PIDs seen and show channels
				int channel_left=0;
				for(ichan=0;ichan<MAX_CHANNELS;ichan++)
				{
					if(chan_p->channels[ichan].autoconf_pmt_need_update)
						channel_left=1;
				}
				if(!channel_left)
						log_streamed_channels(log_module,chan_p->number_of_channels, chan_p->channels, multi_p->multicast_ipv4, multi_p->multicast_ipv6, unicast_vars->unicast, unicast_vars->portOut, unicast_vars->ipOut);
			}
		}

	}
	//TODO : put PMT information in the pid_i structure of the channel

	return get_interrupted();
}

