/* 
 * MuMuDVB - Stream a DVB transport stream.
 * 
 * (C) 2004-2010 Brice DUBOST
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
 * @brief This file contains the general functions for rewriting
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mumudvb.h"
#include "ts.h"
#include "rewrite.h"
#include "log.h"
#include "errors.h"
#include <stdint.h>

static char *log_module="Rewrite: ";


/** Initialize Rewrite variables*/
void init_rewr_v(rewrite_parameters_t *rewr_p)
{
	*rewr_p=(rewrite_parameters_t){
				.rewrite_pmt = OPTION_UNDEFINED,
				.rewrite_pat = OPTION_UNDEFINED,
				.pat_version=-1,
				.full_pat=NULL,
				.pat_needs_update=1,
				.full_pat_ok=0,
				.pat_continuity_counter=0,
				.rewrite_sdt = OPTION_UNDEFINED,
				.sdt_version=-1,
				.full_sdt=NULL,
				.sdt_needs_update=1,
				.full_sdt_ok=0,
				.sdt_continuity_counter=0,
				.rewrite_eit=OPTION_UNDEFINED,
				.store_eit=OPTION_UNDEFINED,
				.eit_version=-1,
				.full_eit=NULL,
				.eit_needs_update=0,
				.sdt_force_eit=OPTION_UNDEFINED,
				.eit_packets=NULL,
		};
}

int rewrite_init(rewrite_parameters_t *rewr_p)
{
	/*****************************************************/
	//Pat rewriting
	//memory allocation for MPEG2-TS
	//packet structures
	/*****************************************************/

	if(rewr_p->rewrite_pat == OPTION_ON)
	{
		rewr_p->full_pat=malloc(sizeof(mumudvb_ts_packet_t));
		if(rewr_p->full_pat==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			set_interrupted(ERROR_MEMORY<<8);
			return 1;
		}
		memset (rewr_p->full_pat, 0, sizeof( mumudvb_ts_packet_t));//we clear it
		pthread_mutex_init(&rewr_p->full_pat->packetmutex,NULL);
	}

	/*****************************************************/
	//SDT rewriting
	//memory allocation for MPEG2-TS
	//packet structures
	/*****************************************************/

	if(rewr_p->rewrite_sdt == OPTION_ON)
	{
		rewr_p->full_sdt=malloc(sizeof(mumudvb_ts_packet_t));
		if(rewr_p->full_sdt==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			set_interrupted(ERROR_MEMORY<<8);
			return 1;
		}
		memset (rewr_p->full_sdt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
		pthread_mutex_init(&rewr_p->full_sdt->packetmutex,NULL);
	}

	/*****************************************************/
	//EIT rewriting
	//memory allocation for MPEG2-TS
	//packet structures
	/*****************************************************/

	if(rewr_p->rewrite_eit == OPTION_ON || rewr_p->store_eit == OPTION_ON)
	{
		rewr_p->full_eit=malloc(sizeof(mumudvb_ts_packet_t));
		if(rewr_p->full_eit==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			set_interrupted(ERROR_MEMORY<<8);
			return 1;
		}
		memset (rewr_p->full_eit, 0, sizeof( mumudvb_ts_packet_t));//we clear it
		pthread_mutex_init(&rewr_p->full_eit->packetmutex,NULL);
	}

return 0;
}


/** @brief Read a line of the configuration file to check if there is a rewrite parameter
 *
 * @param rewrite_vars the autoconfiguration parameters
 * @param substring The currrent line
 */
int read_rewrite_configuration(rewrite_parameters_t *rewrite_vars, char *substring)
{
	char delimiteurs[] = CONFIG_FILE_SEPARATOR;
	if (!strcmp (substring, "rewrite_pmt"))
	{
		substring = strtok (NULL, delimiteurs);
		if(atoi (substring))
		{
			rewrite_vars->rewrite_pmt = OPTION_ON;
			log_message( log_module, MSG_INFO,
					"You have enabled the PMT Rewriting\n");
		}
		else
			rewrite_vars->rewrite_pmt = OPTION_OFF;
	}
	else if (!strcmp (substring, "rewrite_pat"))
	{
		substring = strtok (NULL, delimiteurs);
		if(atoi (substring))
		{
			rewrite_vars->rewrite_pat = OPTION_ON;
			log_message( log_module, MSG_INFO,
					"You have enabled the PAT Rewriting\n");
		}
		else
			rewrite_vars->rewrite_pat = OPTION_OFF;
	}
	else if (!strcmp (substring, "rewrite_sdt"))
	{
		substring = strtok (NULL, delimiteurs);
		if(atoi (substring))
		{
			rewrite_vars->rewrite_sdt = OPTION_ON;
			log_message( log_module, MSG_INFO,
					"You have enabled the SDT Rewriting\n");
		}
		else
			rewrite_vars->rewrite_sdt = OPTION_OFF;

	}
	else if ( (!strcmp (substring, "sort_eit")) || (!strcmp (substring, "rewrite_eit")))
	{
		substring = strtok (NULL, delimiteurs);
		if(atoi (substring))
		{
			rewrite_vars->rewrite_eit = OPTION_ON;
			log_message( log_module, MSG_INFO,
					"You have enabled the EIT rewriting\n");
		}
		else
			rewrite_vars->rewrite_eit = OPTION_OFF;
	}
	else if ((!strcmp (substring, "store_eit")))
	{
		substring = strtok (NULL, delimiteurs);
		if(atoi (substring))
		{
			rewrite_vars->store_eit = OPTION_ON;
			log_message( log_module, MSG_INFO,
					"You have enabled the EIT (EPG) storage for webservices");
		}
		else
			rewrite_vars->store_eit = OPTION_OFF;
	}
	else if (!strcmp (substring, "sdt_force_eit"))
	{
		substring = strtok (NULL, delimiteurs);
		if(atoi (substring))
		{
			rewrite_vars->sdt_force_eit = OPTION_ON;
			log_message( log_module, MSG_INFO,
					"You have enabled the forcing of the EIT flag in the SDT rewrite\n");
		}
		else
			rewrite_vars->sdt_force_eit = OPTION_OFF;
	}
	else
		return 0; //Nothing concerning rewrite, we return 0 to explore the other possibilities

	return 1;//We found something for rewrite, we tell main to go for the next line
}


/** @brief Just a small function to change the continuity counter of a packet
 * This function will overwrite the continuity counter of the packet with the one given in argument
 *
 */
void set_continuity_counter(unsigned char *buf,int continuity_counter)
{
	ts_header_t *ts_header=(ts_header_t *)buf;
	ts_header->continuity_counter=continuity_counter;
}

