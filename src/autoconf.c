/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for Autoconfiguration
 *
 * (C) 2008-2010 Brice DUBOST <mumudvb@braice.net>
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
 *  @brief This file contain the code related to the autoconfiguration of mumudvb
 * 
 *  It contains the functions to extract the relevant informations from the PAT,PMT,SDT pids and from ATSC PSIP table
 * 
 *  The PAT contains the list of the channels in the actual stream, their service number and the PMT pid
 * 
 *  The SDT contains the name of the channels associated to a certain service number and the type of service
 *
 *  The PSIP (ATSC only) table contains the same kind of information as the SDT
 *
 *  The pmt contains the Pids (audio video etc ...) of the channels,
 *
 *  The idea is the following (for full autoconf),
 *  once we find a sdt, we add the service to a service list (ie we add the name and the service number)
 *  if we find a pat, we check if we have seen the services before, if no we skip, if yes we update the pmt pids
 *
 *  Once we updated all the services or reach the timeout we create a channel list from the services list and we go
 *  in the autoconf=partial mode (and we add the filters for the new pmt pids)
 *
 *  In partial autoconf, we read the pmt pids to find the other pids of the channel. We add only pids wich seems relevant
 *  ie : audio, video, pcr, teletext, subtitle
 *
 *  once it's finished, we add the new filters
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
#include "autoconf.h"
#include "rtp.h"
#include "log.h"
#ifdef ENABLE_TRANSCODING
#include "transcode.h"
#endif

extern int Interrupted;
static char *log_module="Autoconf: ";
#ifdef ENABLE_TRANSCODING
extern transcode_options_t global_transcode_opt;
#endif


mumudvb_service_t *autoconf_find_service_for_add(mumudvb_service_t *services,int service_id);
mumudvb_service_t *autoconf_find_service_for_modify(mumudvb_service_t *services,int service_id);
void autoconf_free_services(mumudvb_service_t *services);
int autoconf_read_sdt(unsigned char *buf,int len, mumudvb_service_t *services);
int autoconf_read_psip(autoconf_parameters_t *parameters);
int autoconf_read_pmt(mumudvb_ts_packet_t *pmt, mumudvb_channel_t *channel, char *card_base_path, int tuner, uint8_t *asked_pid, uint8_t *number_chan_asked_pid,fds_t *fds);
void autoconf_sort_services(mumudvb_service_t *services);
int autoconf_read_nit(autoconf_parameters_t *parameters, mumudvb_channel_t *channels, int number_of_channels);

/** @brief Read a line of the configuration file to check if there is a autoconf parameter
 *
 * @param autoconf_vars the autoconfiguration parameters
 * @param substring The currrent line
 */
int read_autoconfiguration_configuration(autoconf_parameters_t *autoconf_vars, char *substring)
{

  char delimiteurs[] = CONFIG_FILE_SEPARATOR;

  if (!strcmp (substring, "autoconf_scrambled"))
  {
    substring = strtok (NULL, delimiteurs);
    autoconf_vars->autoconf_scrambled = atoi (substring);
  }
  else if (!strcmp (substring, "autoconf_pid_update"))
  {
    substring = strtok (NULL, delimiteurs);
    autoconf_vars->autoconf_pid_update = atoi (substring);
  }
  else if (!strcmp (substring, "autoconf_lcn"))
  {
    substring = strtok (NULL, delimiteurs);
    autoconf_vars->autoconf_lcn = atoi (substring);
    if(autoconf_vars->autoconf_lcn)
    {
      log_message( log_module, MSG_INFO,
                   "You enabled the search for the logical channel numbers. This should work well on european terestrial. For other the results are not guaranteed\n");
    }
  }
  else if (!strcmp (substring, "autoconfiguration"))
  {
    substring = strtok (NULL, delimiteurs);
    if(atoi (substring)==2)
      autoconf_vars->autoconfiguration = AUTOCONF_MODE_FULL;
    else if(atoi (substring)==1)
      autoconf_vars->autoconfiguration = AUTOCONF_MODE_PIDS;
    else if (!strcmp (substring, "full"))
      autoconf_vars->autoconfiguration = AUTOCONF_MODE_FULL;
    else if (!strcmp (substring, "partial"))
      autoconf_vars->autoconfiguration = AUTOCONF_MODE_PIDS;

    if(!((autoconf_vars->autoconfiguration==AUTOCONF_MODE_PIDS)||(autoconf_vars->autoconfiguration==AUTOCONF_MODE_FULL)))
    {
      log_message( log_module,  MSG_WARN,
                   "Bad value for autoconfiguration, autoconfiguration will not be run\n");
      autoconf_vars->autoconfiguration=0;
    }
  }
  else if (!strcmp (substring, "autoconf_radios"))
  {
    substring = strtok (NULL, delimiteurs);
    autoconf_vars->autoconf_radios = atoi (substring);
    if(!(autoconf_vars->autoconfiguration==AUTOCONF_MODE_FULL))
    {
      log_message( log_module,  MSG_INFO,
                   "You have to set autoconfiguration in full mode to use autoconf of the radios\n");
    }
  }
  else if (!strcmp (substring, "autoconf_ip_header"))
  {
    substring = strtok (NULL, delimiteurs);
    if(strlen(substring)>8)
    {
      log_message( log_module,  MSG_ERROR,
                   "The autoconf ip header is too long\n");
      return -1;
    }
    snprintf(autoconf_vars->autoconf_ip,79,"%s.%%card.%%number",substring);
  }
  else if (!strcmp (substring, "autoconf_ip"))
  {
    substring = strtok (NULL, delimiteurs);
    if(strlen(substring)>79)
    {
      log_message( log_module,  MSG_ERROR,
                   "The autoconf ip is too long\n");
      return -1;
    }
    sscanf (substring, "%s\n", autoconf_vars->autoconf_ip);
  }
  /**  option for the starting http unicast port (for autoconf full)*/
  else if (!strcmp (substring, "autoconf_unicast_start_port"))
  {
    substring = strtok (NULL, delimiteurs);
    sprintf(autoconf_vars->autoconf_unicast_port,"%d +%%number",atoi (substring));
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
    strcpy(autoconf_vars->autoconf_unicast_port,substring);
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
    strcpy(autoconf_vars->autoconf_multicast_port,substring);
  }
  else if ((!strcmp (substring, "autoconf_tsid_list"))||(!strcmp (substring, "autoconf_sid_list")))
  {
    if(!strcmp (substring, "autoconf_tsid_list"))
      log_message( log_module,  MSG_WARN, "Warning: The option autoconf_tsid_list is deprecated, use autoconf_sid_list instead\n");
    while ((substring = strtok (NULL, delimiteurs)) != NULL)
    {
      if (autoconf_vars->num_service_id >= MAX_CHANNELS)
      {
        log_message( log_module,  MSG_ERROR,
                     "Autoconfiguration : Too many ts id : %d\n",
                     autoconf_vars->num_service_id);
        return -1;
      }
      autoconf_vars->service_id_list[autoconf_vars->num_service_id] = atoi (substring);
      autoconf_vars->num_service_id++;
    }
  }
  else if (!strcmp (substring, "autoconf_name_template"))
    {
      // other substring extraction method in order to keep spaces
      substring = strtok (NULL, "=");
      if (!(strlen (substring) >= MAX_NAME_LEN - 1))
        strcpy(autoconf_vars->name_template,strtok(substring,"\n"));
      else
      {
        log_message( log_module,  MSG_WARN,"Autoconfiguration: Channel name template too long\n");
        strncpy(autoconf_vars->name_template,strtok(substring,"\n"),MAX_NAME_LEN-1);
        autoconf_vars->name_template[MAX_NAME_LEN-1]='\0';
      }
    }
  else
    return 0; //Nothing concerning autoconfiguration, we return 0 to explore the other possibilities

  return 1;//We found something for autoconfiguration, we tell main to go for the next line
}


/** @brief initialize the autoconfiguration : alloc the memory etc...
 *
 */
int autoconf_init(autoconf_parameters_t *autoconf_vars, mumudvb_channel_t *channels,int number_of_channels)
{
  int curr_channel;

  if(autoconf_vars->autoconfiguration==AUTOCONF_MODE_FULL)
    {
      autoconf_vars->autoconf_temp_pat=malloc(sizeof(mumudvb_ts_packet_t));
      if(autoconf_vars->autoconf_temp_pat==NULL)
	{
          log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
          Interrupted=ERROR_MEMORY<<8;
          return -1;
	}
      memset (autoconf_vars->autoconf_temp_pat, 0, sizeof( mumudvb_ts_packet_t));//we clear it
      pthread_mutex_init(&autoconf_vars->autoconf_temp_pat->packetmutex,NULL);
      autoconf_vars->autoconf_temp_sdt=malloc(sizeof(mumudvb_ts_packet_t));
      if(autoconf_vars->autoconf_temp_sdt==NULL)
	{
          log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
          Interrupted=ERROR_MEMORY<<8;
          return -1;
	}
      memset (autoconf_vars->autoconf_temp_sdt, 0, sizeof( mumudvb_ts_packet_t));//we clear it
      pthread_mutex_init(&autoconf_vars->autoconf_temp_sdt->packetmutex,NULL);

      autoconf_vars->autoconf_temp_psip=malloc(sizeof(mumudvb_ts_packet_t));
      if(autoconf_vars->autoconf_temp_psip==NULL)
	{
          log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
          Interrupted=ERROR_MEMORY<<8;
          return -1;
	}
      memset (autoconf_vars->autoconf_temp_psip, 0, sizeof( mumudvb_ts_packet_t));//we clear it
      pthread_mutex_init(&autoconf_vars->autoconf_temp_psip->packetmutex,NULL);

      autoconf_vars->services=malloc(sizeof(mumudvb_service_t));
      if(autoconf_vars->services==NULL)
	{
          log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
          Interrupted=ERROR_MEMORY<<8;
          return -1;
	}
      memset (autoconf_vars->services, 0, sizeof( mumudvb_service_t));//we clear it

    }

  if (autoconf_vars->autoconfiguration==AUTOCONF_MODE_PIDS)
    for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      //If there is more than one pid in one channel we mark it
      //For no autoconfiguration
      if(channels[curr_channel].num_pids>1)
	{
	  log_message( log_module,  MSG_DETAIL, "Autoconfiguration desactivated for channel \"%s\" \n", channels[curr_channel].name);
	  channels[curr_channel].autoconfigurated=1;
	}
      else if (channels[curr_channel].num_pids==1)
	{
	  //Only one pid with autoconfiguration=1, it's the PMT pid
	  channels[curr_channel].pmt_pid=channels[curr_channel].pids[0];
          channels[curr_channel].pids_type[0]=PID_PMT;
          snprintf(channels[curr_channel].pids_language[0],4,"%s","---");
	}
    }
    if (autoconf_vars->autoconfiguration)
    {
      if(autoconf_vars->autoconf_lcn)
      {
        autoconf_vars->autoconf_temp_nit=malloc(sizeof(mumudvb_ts_packet_t));
        if(autoconf_vars->autoconf_temp_nit==NULL)
        {
          log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
          Interrupted=ERROR_MEMORY<<8;
          return -1;
        }
        memset (autoconf_vars->autoconf_temp_nit, 0, sizeof( mumudvb_ts_packet_t));//we clear it
        pthread_mutex_init(&autoconf_vars->autoconf_temp_nit->packetmutex,NULL);
      }
    }
  return 0;

}


/****************************************************************************/
//Parts of this code (read of the pmt and read of the pat)
// from libdvb, strongly modified, with commentaries added
/****************************************************************************/

/** @brief read the PAT for autoconfiguration
 * This function extract the pmt from the pat 
 * before doing so it checks if the service is already initialised (sdt packet)
 *
 * @param autoconf_vars The autoconfiguration structure, containing all we need
 */
int autoconf_read_pat(autoconf_parameters_t *autoconf_vars)
{

  mumudvb_ts_packet_t *pat_mumu;
  mumudvb_service_t *services;
  unsigned char *buf=NULL;
  mumudvb_service_t *actual_service=NULL;
  pat_mumu=autoconf_vars->autoconf_temp_pat;
  services=autoconf_vars->services;
  buf=pat_mumu->packet;
  pat_t       *pat=(pat_t*)(buf);
  pat_prog_t  *prog;
  int delta=PAT_LEN;
  int section_length=0;
  int number_of_services=0;
  int channels_missing=0;
  int new_services=0;

  log_message( log_module, MSG_DEBUG,"---- New PAT ----\n");

  //PAT reading
  section_length=HILO(pat->section_length);

  log_message( log_module, MSG_DEBUG,  "pat info : transport stream id 0x%04x section_length %d version %i last_section_number %x current_next_indicator %d\n"
	      ,HILO(pat->transport_stream_id)
	      ,HILO(pat->section_length)
	      ,pat->version_number
	      ,pat->last_section_number
	      ,pat->current_next_indicator);

  /*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
  sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
  and shall be the next table to become valid.*/
  if(pat->current_next_indicator == 0)
  {
    log_message( log_module, MSG_DEBUG,"The current_next_indicator is set to 0, this PAT is not valid for the current stream\n");
    return 0;
  }

  //We store the transport stream ID
  autoconf_vars->transport_stream_id=HILO(pat->transport_stream_id);

  //We loop over the different programs included in the pat
  while((delta+PAT_PROG_LEN)<(section_length))
  {
    prog=(pat_prog_t*)((char*)buf+delta);
    if(HILO(prog->program_number)==0)
    {
      log_message( log_module, MSG_DEBUG,"Network pid %d\n", HILO(prog->network_pid));
    }
    else
    {
      //Do we have already this program in the service list ?
      //Ie : do we already know the channel name/type ?
      actual_service=autoconf_find_service_for_modify(services,HILO(prog->program_number));
      if(actual_service)
      {
        if(!actual_service->pmt_pid)
        {
          //We found a new service without the PMT, pid, we update this service
          new_services=1;
          actual_service->pmt_pid=HILO(prog->network_pid);
          log_message( log_module, MSG_DETAIL,"service updated  pmt pid : %d\t id 0x%x\t name \"%s\"\n",
                            actual_service->pmt_pid,
                            actual_service->id,
                            actual_service->name);
        }
      }
      else
      {
        log_message( log_module, MSG_DEBUG,"service missing  pmt pid : %d\t id 0x%x\t\n",
                        HILO(prog->network_pid),
                        HILO(prog->program_number));
        channels_missing++;
      }
    }
    delta+=PAT_PROG_LEN;
    number_of_services++;
  }

  log_message( log_module, MSG_DEBUG,"This pat contains %d services\n",number_of_services);

  if(channels_missing)
  {
    if(new_services)
      log_message( log_module, MSG_DETAIL,"PAT read %d channels on %d are missing, we wait for others SDT/PSIP for the moment.\n",channels_missing,number_of_services);
    return 0;
  }

  return 1;
}


/** @brief Try to find the service specified by id, if not found create a new one.
 * if the service is not foud, it returns a pointer to the new service, and NULL if 
 * the service is found or run out of memory.
 * 
 * @param services the chained list of services
 * @param service_id the identifier/program number of the searched service
 */
mumudvb_service_t *autoconf_find_service_for_add(mumudvb_service_t *services,int service_id)
{
  int found=0;
  mumudvb_service_t *actual_service;

  actual_service=services;

  if(actual_service->id==service_id)
    found=1;

  while(found==0 && actual_service->next!=NULL)
  {
    actual_service=actual_service->next;
    if(actual_service->id==service_id)
      found=1;
  }

  if(found)
    return NULL;

  actual_service->next=malloc(sizeof(mumudvb_service_t));
  if(actual_service->next==NULL)
  {
    log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return NULL;
  }
  memset (actual_service->next, 0, sizeof( mumudvb_service_t));//we clear it
  return actual_service->next;

}

/** @brief try to find the service specified by id
 * if not found return NULL, otherwise return the service
 *
 * @param services the chained list of services
 * @param service_id the identifier of the searched service
 */
mumudvb_service_t *autoconf_find_service_for_modify(mumudvb_service_t *services,int service_id)
{
  mumudvb_service_t *found=NULL;
  mumudvb_service_t *actual_service;

  actual_service=services;

  while(found==NULL && actual_service!=NULL)
  {
    if(actual_service->id==service_id)
      found=actual_service;
    actual_service=actual_service->next;
  }

  if(found)
    return found;

  return NULL;

}


/**@brief Free the autoconf parameters.
 *
 * @param autoconf_vars pointer to the autoconf structure
 */
void autoconf_freeing(autoconf_parameters_t *autoconf_vars)
{
  if(autoconf_vars->autoconf_temp_sdt)
  {
    free(autoconf_vars->autoconf_temp_sdt);
    autoconf_vars->autoconf_temp_sdt=NULL;
  }
  if(autoconf_vars->autoconf_temp_psip)
  {
    free(autoconf_vars->autoconf_temp_psip);
    autoconf_vars->autoconf_temp_psip=NULL;
  }
  if(autoconf_vars->autoconf_temp_pat)
  {
    free(autoconf_vars->autoconf_temp_pat);
    autoconf_vars->autoconf_temp_pat=NULL;
  }
  if(autoconf_vars->services)
  {
    autoconf_free_services(autoconf_vars->services);
    autoconf_vars->services=NULL;
  }
}

/**@brief Free the chained list of services.
 *
 * @param services the chained list of services
 */
void autoconf_free_services(mumudvb_service_t *services)
{

  mumudvb_service_t *actual_service;
  mumudvb_service_t *next_service;

  for(actual_service=services;actual_service != NULL; actual_service=next_service)
  {
    next_service= actual_service->next;
    free(actual_service);
  }
}

/**@brief Sort the chained list of services.
 *
 * This function sort the services using their service_id, this service doesn't sort the first one :( (but I think it's empty)
 * Unefficient sorting : O(n^2) but the number of services is never big and this function is called once
 * @param services the chained list of services
 */
void autoconf_sort_services(mumudvb_service_t *services)
{

  mumudvb_service_t *actual_service;
  mumudvb_service_t *next_service;
  mumudvb_service_t *actual_service_int;
  mumudvb_service_t *next_service_int;
  mumudvb_service_t *prev_service_int;
  mumudvb_service_t *temp_service_int;
  prev_service_int=NULL;
  log_message( log_module, MSG_DEBUG,"Service sorting\n");
  log_message( log_module, MSG_FLOOD,"Service sorting BEFORE\n");
  for(actual_service=services;actual_service != NULL; actual_service=next_service)
  {
    log_message( log_module, MSG_FLOOD,"Service sorting, services : %s id %d \n",actual_service->name,actual_service->id);
    next_service= actual_service->next;
  }
  for(actual_service=services;actual_service != NULL; actual_service=next_service)
  {
    for(actual_service_int=services;actual_service_int != NULL; actual_service_int=next_service_int)
    {
      next_service_int= actual_service_int->next;
      if((prev_service_int != NULL) &&(next_service_int != NULL) &&(next_service_int->id)&&(actual_service_int->id) && next_service_int->id < actual_service_int->id)
      {
	prev_service_int->next=next_service_int;
	actual_service_int->next=next_service_int->next;
	next_service_int->next=actual_service_int;
	if(actual_service_int==actual_service)
	  actual_service=next_service_int;
	temp_service_int=next_service_int;
	next_service_int=actual_service_int;
	actual_service_int=temp_service_int;
      }
      prev_service_int=actual_service_int;
    }
    next_service= actual_service->next;
  }
  log_message( log_module, MSG_FLOOD,"Service sorting AFTER\n");
  for(actual_service=services;actual_service != NULL; actual_service=next_service)
  {
    log_message( log_module, MSG_FLOOD,"Service sorting, services : %s id %d \n",actual_service->name,actual_service->id);
    next_service= actual_service->next;
  }
}

/** @brief Convert the chained list of services into channels
 *
 * This function is called when We've got all the services, we now fill the channels structure
 * After that we go in AUTOCONF_MODE_PIDS to get audio and video pids
 * @param parameters The autoconf parameters
 * @param channels Chained list of channels
 * @param port The mulicast port
 * @param card The card number for the ip address
 * @param unicast_vars The unicast parameters
 * @param fds The file descriptors (for filters and unicast)
 */
int autoconf_services_to_channels(autoconf_parameters_t parameters, mumudvb_channel_t *channels, int port, int card, int tuner, unicast_parameters_t *unicast_vars, int multicast_out, int server_id)
{

  mumudvb_service_t *actual_service;
  int channel_number=0;
  int found_in_service_id_list;
  int unicast_port_per_channel;
  char tempstring[256];
  actual_service=parameters.services;
  unicast_port_per_channel=strlen(parameters.autoconf_unicast_port)?1:0;

  do
  {
    if(parameters.autoconf_scrambled && actual_service->free_ca_mode)
        log_message( log_module, MSG_DETAIL,"Service scrambled. Name \"%s\"\n", actual_service->name);

    //If there is a service_id list we look if we find it (option autoconf_sid_list)
    if(parameters.num_service_id)
    {
      int actual_service_id;
      found_in_service_id_list=0;
      for(actual_service_id=0;actual_service_id<parameters.num_service_id && !found_in_service_id_list;actual_service_id++)
      {
        if(parameters.service_id_list[actual_service_id]==actual_service->id)
        {
          found_in_service_id_list=1;
          log_message( log_module, MSG_DEBUG,"Service found in the service_id list. Name \"%s\"\n", actual_service->name);
        }
      }
    }
    else //No ts id list so it is found
      found_in_service_id_list=1;

    if(!parameters.autoconf_scrambled && actual_service->free_ca_mode)
      log_message( log_module, MSG_DETAIL,"Service scrambled, no CAM support and no autoconf_scrambled, we skip. Name \"%s\"\n", actual_service->name);
    else if(!actual_service->pmt_pid)
      log_message( log_module, MSG_DETAIL,"Service without a PMT pid, we skip. Name \"%s\"\n", actual_service->name);
    else if(!found_in_service_id_list)
      log_message( log_module, MSG_DETAIL,"Service NOT in the service_id list, we skip. Name \"%s\", id %d\n", actual_service->name, actual_service->id);
    else //service is ok, we make it a channel
    {
      //Cf EN 300 468 v1.9.1 Table 81
      if((actual_service->type==0x01||
          actual_service->type==0x11||
          actual_service->type==0x16||
          actual_service->type==0x19)||
          ((actual_service->type==0x02||
          actual_service->type==0x0a)&&parameters.autoconf_radios))
      {
        log_message( log_module, MSG_DETAIL,"We convert a new service into a channel, sid %d pmt_pid %d name \"%s\" \n",
			  actual_service->id, actual_service->pmt_pid, actual_service->name);
        display_service_type(actual_service->type, MSG_DETAIL, log_module);

        channels[channel_number].channel_type=actual_service->type;
        channels[channel_number].num_packet = 0;
        channels[channel_number].num_scrambled_packets = 0;
        channels[channel_number].scrambled_channel = 0;
        channels[channel_number].streamed_channel = 1;
        channels[channel_number].nb_bytes=0;
        channels[channel_number].pids[0]=actual_service->pmt_pid;
        channels[channel_number].pids_type[0]=PID_PMT;
        channels[channel_number].num_pids=1;
        snprintf(channels[channel_number].pids_language[0],4,"%s","---");
        if(strlen(parameters.name_template))
        {
          strcpy(channels[channel_number].name,parameters.name_template);
          int len=MAX_NAME_LEN;
          char number[10];
          mumu_string_replace(channels[channel_number].name,&len,0,"%name",actual_service->name);
          sprintf(number,"%d",channel_number+1);
          mumu_string_replace(channels[channel_number].name,&len,0,"%number",number);
          //put LCN here
        }
        else
          strcpy(channels[channel_number].name,actual_service->name);
        if(multicast_out)
        {
          char number[10];
          char ip[80];
          int len=80;
          if(strlen(parameters.autoconf_multicast_port))
          {
            strcpy(tempstring,parameters.autoconf_multicast_port);
            sprintf(number,"%d",channel_number);
            mumu_string_replace(tempstring,&len,0,"%number",number);
            sprintf(number,"%d",card);
            mumu_string_replace(tempstring,&len,0,"%card",number);
            sprintf(number,"%d",tuner);
            mumu_string_replace(tempstring,&len,0,"%tuner",number);
            sprintf(number,"%d",server_id);
            mumu_string_replace(tempstring,&len,0,"%server",number);
            channels[channel_number].portOut=string_comput(tempstring);
          }
          else
          {
            channels[channel_number].portOut=port;//do here the job for evaluating the string
          }
          strcpy(ip,parameters.autoconf_ip);
          sprintf(number,"%d",channel_number);
          mumu_string_replace(ip,&len,0,"%number",number);
          sprintf(number,"%d",card);
          mumu_string_replace(ip,&len,0,"%card",number);
          sprintf(number,"%d",tuner);
          mumu_string_replace(ip,&len,0,"%tuner",number);
          sprintf(number,"%d",server_id);
          mumu_string_replace(ip,&len,0,"%server",number);
          strcpy(channels[channel_number].ipOut,ip);
          log_message( log_module, MSG_DEBUG,"Channel Ip : \"%s\" port : %d\n",channels[channel_number].ipOut,channels[channel_number].portOut);
        }

        //This is a scrambled channel, we will have to ask the cam for descrambling it
        if(parameters.autoconf_scrambled && actual_service->free_ca_mode)
          channels[channel_number].need_cam_ask=CAM_NEED_ASK;

        //We store the PMT and the service id in the channel
        channels[channel_number].pmt_pid=actual_service->pmt_pid;
        channels[channel_number].service_id=actual_service->id;
        init_rtp_header(&channels[channel_number]); //We init the rtp header in all cases

        if(channels[channel_number].pmt_packet==NULL)
        {
          channels[channel_number].pmt_packet=malloc(sizeof(mumudvb_ts_packet_t));
          if(channels[channel_number].pmt_packet==NULL)
          {
            log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
            Interrupted=ERROR_MEMORY<<8;
            return -1;
          }
          memset (channels[channel_number].pmt_packet, 0, sizeof( mumudvb_ts_packet_t));//we clear it
          pthread_mutex_init(&channels[channel_number].pmt_packet->packetmutex,NULL);
        }
        //We update the unicast port, the connection will be created in autoconf_finish_full
        if(unicast_port_per_channel && unicast_vars->unicast)
        {
          strcpy(tempstring,parameters.autoconf_unicast_port);
          int len;len=256;
          char number[10];
          sprintf(number,"%d",channel_number);
          mumu_string_replace(tempstring,&len,0,"%number",number);
          sprintf(number,"%d",card);
          mumu_string_replace(tempstring,&len,0,"%card",number);
          sprintf(number,"%d",tuner);
          mumu_string_replace(tempstring,&len,0,"%tuner",number);
          sprintf(number,"%d",server_id);
          mumu_string_replace(tempstring,&len,0,"%server",number);
          channels[channel_number].unicast_port=string_comput(tempstring);
          log_message( log_module, MSG_DEBUG,"Channel (direct) unicast port  %d\n",channels[channel_number].unicast_port);
        }
#ifdef ENABLE_TRANSCODING
        //We copy the common transcode options to the new channel
        transcode_copy_options(&global_transcode_opt,&channels[channel_number].transcode_options);
        transcode_options_apply_templates(&channels[channel_number].transcode_options,card,tuner,server_id,channel_number);
#endif
        channel_number++;
      }
      else if(actual_service->type==0x02||actual_service->type==0x0a) //service_type digital radio sound service
        log_message( log_module, MSG_DETAIL,"Service type digital radio sound service, no autoconfigure. (if you want add autoconf_radios=1 to your configuration file) Name \"%s\"\n",actual_service->name);
      else
      {
        //We show the service type
        log_message( log_module, MSG_DETAIL,"No autoconfigure due to service type : %s. Name \"%s\"\n",service_type_to_str(actual_service->type),actual_service->name);
      }
    }
    actual_service=actual_service->next;
  }
  while(actual_service && channel_number<MAX_CHANNELS);

  if(channel_number==MAX_CHANNELS)
    log_message( log_module, MSG_WARN,"Warning : We reached the maximum channel number, we drop other possible channels !\n");

  return channel_number;
}

/** @brief Finish full autoconfiguration (set everything needed to go to partial autoconf)
 * This function is called when FULL autoconfiguration is finished
 * It fill the asked pid array
 * It open the file descriptors for the new filters, and set the filters
 * It open the new sockets
 * It free autoconfiguration memory wich will be not used anymore
 *
 * @param card the card number
 * @param number_of_channels the number of channels
 * @param channels the array of channels
 * @param fds the file descriptors
*/
int autoconf_finish_full(mumudvb_chan_and_pids_t *chan_and_pids, autoconf_parameters_t *autoconf_vars, multicast_parameters_t *multicast_vars, tuning_parameters_t *tuneparams, fds_t *fds, unicast_parameters_t *unicast_vars, int server_id)
{
  int curr_channel,curr_pid;
  //We sort the services
  autoconf_sort_services(autoconf_vars->services);
  chan_and_pids->number_of_channels=autoconf_services_to_channels(*autoconf_vars, chan_and_pids->channels, multicast_vars->common_port, tuneparams->card, tuneparams->tuner, unicast_vars, multicast_vars->multicast, server_id); //Convert the list of services into channels
  //we got the pmt pids for the channels, we open the filters
  for (curr_channel = 0; curr_channel < chan_and_pids->number_of_channels; curr_channel++)
  {
    for (curr_pid = 0; curr_pid < chan_and_pids->channels[curr_channel].num_pids; curr_pid++)
    {
      if(chan_and_pids->asked_pid[chan_and_pids->channels[curr_channel].pids[curr_pid]]==PID_NOT_ASKED)
        chan_and_pids->asked_pid[chan_and_pids->channels[curr_channel].pids[curr_pid]]=PID_ASKED;
      chan_and_pids->number_chan_asked_pid[chan_and_pids->channels[curr_channel].pids[curr_pid]]++;
    }
  }

  // we open the file descriptors
  if (create_card_fd (tuneparams->card_dev_path, tuneparams->tuner, chan_and_pids->asked_pid, fds) < 0)
  {
    log_message( log_module, MSG_ERROR,"ERROR : CANNOT open the new descriptors. Some channels will probably not work\n");
  }
  // we set the new filters
  set_filters( chan_and_pids->asked_pid, fds);


  //Networking
  for (curr_channel = 0; curr_channel < chan_and_pids->number_of_channels; curr_channel++)
  {

    /** open the unicast listening connections fo the channels */
    if(chan_and_pids->channels[curr_channel].unicast_port && unicast_vars->unicast)
    {
      log_message( log_module, MSG_INFO,"Unicast : We open the channel %d http socket address %s:%d\n",
                  curr_channel,
                  unicast_vars->ipOut,
                  chan_and_pids->channels[curr_channel].unicast_port);
      unicast_create_listening_socket(UNICAST_LISTEN_CHANNEL,
                                      curr_channel,
                                      unicast_vars->ipOut,
                                      chan_and_pids->channels[curr_channel].unicast_port,
                                      &chan_and_pids->channels[curr_channel].sIn,
                                      &chan_and_pids->channels[curr_channel].socketIn,
                                      fds,
                                      unicast_vars);
    }

    //Open the multicast socket for the new channel
    if(multicast_vars->multicast && multicast_vars->auto_join) //See the README for the reason of this option
      chan_and_pids->channels[curr_channel].socketOut = 
          makeclientsocket (chan_and_pids->channels[curr_channel].ipOut,
                            chan_and_pids->channels[curr_channel].portOut,
                            multicast_vars->ttl,
                            &chan_and_pids->channels[curr_channel].sOut);
    else if(multicast_vars->multicast)
      chan_and_pids->channels[curr_channel].socketOut = 
          makesocket (chan_and_pids->channels[curr_channel].ipOut,
                      chan_and_pids->channels[curr_channel].portOut,
                      multicast_vars->ttl,
                      &chan_and_pids->channels[curr_channel].sOut);
  }

  log_message( log_module, MSG_DEBUG,"Step TWO, we get the video and audio PIDs\n");
  //We free autoconf memort
  autoconf_freeing(autoconf_vars);

  autoconf_vars->autoconfiguration=AUTOCONF_MODE_PIDS; //Next step add video and audio pids

  return 0;
}

/** @brief Finish autoconf
 * This function is called when autoconfiguration is finished
 * It opens what is needed to stream the new channels
 * It creates the file descriptors for the filters, set the filters
 * It also generates a config file with the data obtained during autoconfiguration
 *
 * @param card the card number
 * @param number_of_channels the number of channels
 * @param channels the array of channels
 * @param asked_pid the array containing the pids already asked
 * @param number_chan_asked_pid the number of channels who want this pid
 * @param fds the file descriptors
*/
void autoconf_set_channel_filt(char *card_base_path, int tuner, mumudvb_chan_and_pids_t *chan_and_pids, fds_t *fds)
{
  int curr_channel;
  int curr_pid;


  log_message( log_module, MSG_DETAIL,"Autoconfiguration almost done\n");
  log_message( log_module, MSG_DETAIL,"We open the new file descriptors\n");
  for (curr_channel = 0; curr_channel < chan_and_pids->number_of_channels; curr_channel++)
  {
    for (curr_pid = 0; curr_pid < chan_and_pids->channels[curr_channel].num_pids; curr_pid++)
    {
      if(chan_and_pids->asked_pid[chan_and_pids->channels[curr_channel].pids[curr_pid]]==PID_NOT_ASKED)
        chan_and_pids->asked_pid[chan_and_pids->channels[curr_channel].pids[curr_pid]]=PID_ASKED;
      chan_and_pids->number_chan_asked_pid[chan_and_pids->channels[curr_channel].pids[curr_pid]]++;
    }
  }
  // we open the file descriptors
  if (create_card_fd (card_base_path, tuner, chan_and_pids->asked_pid, fds) < 0)
  {
    log_message( log_module, MSG_ERROR,"ERROR : CANNOT open the new descriptors. Some channels will probably not work\n");
  }

  log_message( log_module, MSG_DETAIL,"Add the new filters\n");
  set_filters(chan_and_pids->asked_pid, fds);
}

void autoconf_definite_end(int card, int tuner, mumudvb_chan_and_pids_t *chan_and_pids, int multicast, unicast_parameters_t *unicast_vars)
{
  log_message( log_module, MSG_INFO,"Autoconfiguration done\n");

  log_streamed_channels(log_module,chan_and_pids->number_of_channels, chan_and_pids->channels, multicast, unicast_vars->unicast, unicast_vars->portOut, unicast_vars->ipOut);

  /**@todo : make an option to generate it or not ?*/
  char filename_gen_conf[256];
  sprintf (filename_gen_conf, GEN_CONF_PATH, card, tuner);
  gen_config_file(chan_and_pids->number_of_channels, chan_and_pids->channels, filename_gen_conf);

}

/********************************************************************
 * Autoconfiguration new packet and poll functions
 ********************************************************************/
/** @brief This function is called when a new packet is there and the autoconf is not finished*/
int autoconf_new_packet(int pid, unsigned char *ts_packet, autoconf_parameters_t *autoconf_vars, fds_t *fds, mumudvb_chan_and_pids_t *chan_and_pids, tuning_parameters_t *tuneparams, multicast_parameters_t *multicast_vars,  unicast_parameters_t *unicast_vars, int server_id)
{
  int iRet=0;
  if(autoconf_vars->autoconfiguration==AUTOCONF_MODE_FULL) //Full autoconfiguration, we search the channels and their names
  {
    if(pid==0) //PAT : contains the services identifiers and the PMT PID for each service
    {
      if(get_ts_packet(ts_packet,autoconf_vars->autoconf_temp_pat))
      {
        if(autoconf_read_pat(autoconf_vars))
        {
          log_message( log_module, MSG_DEBUG,"It seems that we have finished to get the services list\n");
          //we finish full autoconfiguration
          iRet = autoconf_finish_full(chan_and_pids, autoconf_vars, multicast_vars, tuneparams, fds, unicast_vars, server_id);
        }
        else
          autoconf_vars->autoconf_temp_pat->empty=1;//we clear it
      }
    }
    else if(pid==17) //SDT : contains the names of the services
    {
      if(get_ts_packet(ts_packet,autoconf_vars->autoconf_temp_sdt))
      {
        autoconf_read_sdt(autoconf_vars->autoconf_temp_sdt->packet,autoconf_vars->autoconf_temp_sdt->len,autoconf_vars->services);
        autoconf_vars->autoconf_temp_sdt->empty=1;//we clear it
      }
    }
    else if(pid==PSIP_PID && tuneparams->fe_type==FE_ATSC) //PSIP : contains the names of the services
    {
      if(get_ts_packet(ts_packet,autoconf_vars->autoconf_temp_psip))
      {
        autoconf_read_psip(autoconf_vars);
        autoconf_vars->autoconf_temp_psip->empty=1;//we clear it
      }
    }
  }
  else if(autoconf_vars->autoconfiguration==AUTOCONF_MODE_PIDS) //We have the channels and their PMT, we search the other pids
  {
    int curr_channel;
    for(curr_channel=0;curr_channel<MAX_CHANNELS;curr_channel++)
    {
      if((!chan_and_pids->channels[curr_channel].autoconfigurated) &&(chan_and_pids->channels[curr_channel].pmt_pid==pid)&& pid)
      {
        if(get_ts_packet(ts_packet,chan_and_pids->channels[curr_channel].pmt_packet))
	{
	  //Now we have the PMT, we parse it
          if(autoconf_read_pmt(chan_and_pids->channels[curr_channel].pmt_packet, &chan_and_pids->channels[curr_channel], tuneparams->card_dev_path, tuneparams->tuner, chan_and_pids->asked_pid, chan_and_pids->number_chan_asked_pid, fds)==0)
          {
            log_pids(log_module,&chan_and_pids->channels[curr_channel],curr_channel);

            chan_and_pids->channels[curr_channel].autoconfigurated=1;

            //We check if autoconfiguration is finished
	    if(autoconf_vars->autoconf_lcn)
	      autoconf_vars->autoconfiguration=AUTOCONF_MODE_NIT;
	    else
	      autoconf_vars->autoconfiguration=0;
            for (curr_channel = 0; curr_channel < chan_and_pids->number_of_channels; curr_channel++)
              if(!chan_and_pids->channels[curr_channel].autoconfigurated)
                autoconf_vars->autoconfiguration=AUTOCONF_MODE_PIDS;

            //if it's finished, we open the new descriptors and add the new filters
            if(autoconf_vars->autoconfiguration!=AUTOCONF_MODE_PIDS)
            {
              autoconf_set_channel_filt(tuneparams->card_dev_path, tuneparams->tuner, chan_and_pids, fds);
              //We free autoconf memory
              autoconf_freeing(autoconf_vars);
	      if(autoconf_vars->autoconfiguration==AUTOCONF_MODE_NIT)
		log_message( log_module, MSG_DETAIL,"We search for the NIT\n");
              else
                autoconf_definite_end(tuneparams->card, tuneparams->tuner, chan_and_pids, multicast_vars->multicast, unicast_vars);
            }
          }
        }
      }
    }
  }
  else if(autoconf_vars->autoconfiguration==AUTOCONF_MODE_NIT) //We search the NIT
  {
    if(autoconf_vars->autoconf_lcn && (pid==16)) //NIT : Network Information Table
    {
      if(get_ts_packet(ts_packet,autoconf_vars->autoconf_temp_nit))
      {
	log_message( log_module, MSG_FLOOD,"New NIT\n");
        if(autoconf_read_nit(autoconf_vars, chan_and_pids->channels, chan_and_pids->number_of_channels)==0)
	{
	  autoconf_vars->autoconfiguration=0;
          int curr_channel;
          char lcn[4];
          int len=MAX_NAME_LEN;
          for(curr_channel=0;curr_channel<MAX_CHANNELS;curr_channel++)
          {
            if(chan_and_pids->channels[curr_channel].logical_channel_number)
            {
              sprintf(lcn,"%03d",chan_and_pids->channels[curr_channel].logical_channel_number);
              mumu_string_replace(chan_and_pids->channels[curr_channel].name,&len,0,"%lcn",lcn);
              sprintf(lcn,"%02d",chan_and_pids->channels[curr_channel].logical_channel_number);
              mumu_string_replace(chan_and_pids->channels[curr_channel].name,&len,0,"%2lcn",lcn);
            }
            else
            {
              mumu_string_replace(chan_and_pids->channels[curr_channel].name,&len,0,"%lcn","");
              mumu_string_replace(chan_and_pids->channels[curr_channel].name,&len,0,"%2lcn","");
            }
          }
	  free(autoconf_vars->autoconf_temp_nit);
	  autoconf_vars->autoconf_temp_nit=NULL;
          autoconf_definite_end(tuneparams->card, tuneparams->tuner, chan_and_pids, multicast_vars->multicast, unicast_vars);
	}
      }
    }
  }
  return Interrupted;
}


/** @brief Autoconf function called periodically
 * This function check if we reached the timeout
 * if it's finished, we open the new descriptors and add the new filters
 */
int autoconf_poll(long now, autoconf_parameters_t *autoconf_vars, mumudvb_chan_and_pids_t *chan_and_pids, tuning_parameters_t *tuneparams, multicast_parameters_t *multicast_vars, fds_t *fds, unicast_parameters_t *unicast_vars, int server_id)
{
  int iRet=0;
  if(!autoconf_vars->time_start_autoconfiguration)
    autoconf_vars->time_start_autoconfiguration=now;
  else if (now-autoconf_vars->time_start_autoconfiguration>AUTOCONFIGURE_TIME)
  {
    if(autoconf_vars->autoconfiguration==AUTOCONF_MODE_PIDS)
    {
      log_message( log_module, MSG_WARN,"Not all the channels were configured before timeout\n");
      autoconf_vars->autoconfiguration=0;
      autoconf_set_channel_filt(tuneparams->card_dev_path, tuneparams->tuner, chan_and_pids, fds);
      //We free autoconf memory
      autoconf_freeing(autoconf_vars);
      if(autoconf_vars->autoconf_lcn)
      {
          autoconf_vars->autoconfiguration=AUTOCONF_MODE_NIT;
          autoconf_vars->time_start_autoconfiguration=now;
      }
      else
      {
        autoconf_definite_end(tuneparams->card, tuneparams->tuner, chan_and_pids, multicast_vars->multicast, unicast_vars);
        if(autoconf_vars->autoconf_temp_nit)
        {
	  free(autoconf_vars->autoconf_temp_nit);
          autoconf_vars->autoconf_temp_nit=NULL;
        }
      }
    }
    else if(autoconf_vars->autoconfiguration==AUTOCONF_MODE_FULL)
    {
      log_message( log_module, MSG_WARN,"We were not able to get all the services, we continue with the partial service list\n");
      //This happend when we are not able to get all the services of the PAT,
      //We continue with the partial list of services
      autoconf_vars->time_start_autoconfiguration=now;
      iRet = autoconf_finish_full(chan_and_pids, autoconf_vars, multicast_vars, tuneparams, fds, unicast_vars, server_id);
    }
    else if(autoconf_vars->autoconfiguration==AUTOCONF_MODE_NIT)
    {
      log_message( log_module, MSG_WARN,"Warning : No NIT found before timeout\n");
      autoconf_definite_end(tuneparams->card, tuneparams->tuner, chan_and_pids, multicast_vars->multicast, unicast_vars);
      if(autoconf_vars->autoconf_temp_nit)
      {
        free(autoconf_vars->autoconf_temp_nit);
        autoconf_vars->autoconf_temp_nit=NULL;
      }
      autoconf_vars->autoconfiguration=0;
    }
  }
  return iRet;
}

