/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Autoconfiguration
 * 
 * (C) Brice DUBOST <mumudvb@braice.net>
 *  
 * Parts of this code come from libdvbpsi, modified for mumudvb
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

void parsesdtdescriptor(unsigned char *buf,int descriptors_loop_len, mumudvb_service_t *services);
void parseservicedescriptor(unsigned char *buf, mumudvb_service_t *services);
mumudvb_service_t *autoconf_find_service_for_add(mumudvb_service_t *services,int service_id);
mumudvb_service_t *autoconf_find_service_for_modify(mumudvb_service_t *services,int service_id);
int pmt_find_descriptor(uint8_t tag, unsigned char *buf, int descriptors_loop_len);
void pmt_print_descriptor_tags(unsigned char *buf, int descriptors_loop_len);

/****************************************************************************/
//Code from libdvb, strongly modified, with commentaries added
//read the pmt for autoconfiguration
/****************************************************************************/

int autoconf_read_pmt(mumudvb_ts_packet_t *pmt, mumudvb_channel_t *channel)
{
	int slen, dslen, i;
	int pid;
	pmt_t *header;
	pmt_info_t *descr_header;

	int program_info_length;


	slen=pmt->len;
	header=(pmt_t *)pmt->packet;

	if(header->table_id!=0x02)
	  {
	    log_message( MSG_DETAIL,"Autoconf : == Packet PID %d for channel \"%s\" is not a PMT PID\n", pmt->pid, channel->name);
	    return 1;
	  }
	
	log_message( MSG_DEBUG,"Autoconf : ==PMT read for autoconfiguration of channel \"%s\"\n", channel->name);

	program_info_length=HILO(header->program_info_length); //program_info_length

	for (i=program_info_length+PMT_LEN; i<=slen-(PMT_INFO_LEN+4); i+=dslen+PMT_INFO_LEN)
	  {      //we parse the part after the descriptors
	    descr_header=(pmt_info_t *)(pmt->packet+i);
	    dslen=HILO(descr_header->ES_info_length);        //ES_info_length
	    pid=HILO(descr_header->elementary_PID);
	    switch(descr_header->stream_type){
	    case 0x01:
	    case 0x02:
	    case 0x1b: /* H.264 video stream */
	      log_message( MSG_DEBUG,"Autoconf :   Video ");
	      break;
	    case 0x03:
	    case 0x81: /* Audio per ATSC A/53B [2] Annex B */
	    case 0x0f: /* ADTS Audio Stream - usually AAC */
	    case 0x11: /* ISO/IEC 14496-3 Audio with LATM transport */
	    case 0x04:
	      log_message( MSG_DEBUG,"Autoconf :   Audio ");
	      break;
	    case 0x06:
	      if(dslen)
		{
		  if(pmt_find_descriptor(0x56,pmt->packet+i+PMT_INFO_LEN,dslen))
		    log_message( MSG_DEBUG,"Autoconf :   Teletext ");
		  else if(pmt_find_descriptor(0x59,pmt->packet+i+PMT_INFO_LEN,dslen))
		    log_message( MSG_DEBUG,"Autoconf :   Subtitling ");
		  else if(pmt_find_descriptor(0x6a,pmt->packet+i+PMT_INFO_LEN,dslen))
		    log_message( MSG_DEBUG,"Autoconf :   AC3 ");
		  else
		    {
		      log_message( MSG_DEBUG,"Unknow descriptor see EN 300 468 table 12, descriptor tags : ");
		      pmt_print_descriptor_tags(pmt->packet+i+PMT_INFO_LEN,dslen);
		      log_message( MSG_DEBUG,"\n");
		      continue;
		    }
		}
	      else
		{
		  log_message( MSG_DEBUG,"Autoconf : stream type 0x06 without descriptor\n");
		  continue;
		}
	      break;
	    default:
	      log_message( MSG_DEBUG, "Autoconf : \t!!!!Stream dropped, type : %d, PID : %d \n",pmt->packet[i],pid);
	      continue;
	    }
	    
	    log_message( MSG_DEBUG,"  PID %d added\n", pid);
	    channel->pids[channel->num_pids]=pid;
	    channel->num_pids++;
	  }
	log_message( MSG_DEBUG,"Autoconf : Number of pids after autoconf %d\n", channel->num_pids);
	return 0;
	  
}

//Tells if the descriptor with tag in present in buf
int pmt_find_descriptor(uint8_t tag, unsigned char *buf, int descriptors_loop_len)
{
  while (descriptors_loop_len > 0) 
    {
      unsigned char descriptor_tag = buf[0];
      unsigned char descriptor_len = buf[1] + 2;
      
      if (tag == descriptor_tag) 
	return 1;

      buf += descriptor_len;
      descriptors_loop_len -= descriptor_len;
    }
  return 0;
}

//Print the tags present in the descriptor
void pmt_print_descriptor_tags(unsigned char *buf, int descriptors_loop_len)
{
  while (descriptors_loop_len > 0) 
    {
      unsigned char descriptor_tag = buf[0];
      unsigned char descriptor_len = buf[1] + 2;
      
      log_message( MSG_DEBUG,"0x%02x - ", descriptor_tag);
      buf += descriptor_len;
      descriptors_loop_len -= descriptor_len;
    }
}


/****************************************************************************/
//read the PAT for autoconfiguration
/****************************************************************************/

int autoconf_read_pat(mumudvb_ts_packet_t *pat_mumu, mumudvb_service_t *services)
{
  unsigned char *buf=NULL;
  mumudvb_service_t *actual_service=NULL;

  buf=pat_mumu->packet;
  pat_t       *pat=(pat_t*)(buf);
  pat_prog_t  *prog;
  int delta=PAT_LEN;
  int section_length=0;
  int found=0;

  log_message(MSG_DEBUG,"---- PAT ----\n");

  //PAT reading
  section_length=HILO(pat->section_length);

  log_message(MSG_DEBUG,  "ts_id 0x%04x ",HILO(pat->transport_stream_id)); 
  log_message(MSG_DEBUG,  "section_length %d ",HILO(pat->section_length)); 
  log_message(MSG_DEBUG,  "version %i ",pat->version_number); 
  log_message(MSG_DEBUG,  "last_section_number %x ",pat->last_section_number); 
  log_message(MSG_DEBUG,  "last_section_number %x ",pat->last_section_number); 
  log_message(MSG_DEBUG,  "\n"); 


  while((delta+PAT_PROG_LEN)<(section_length))
    {
      prog=(pat_prog_t*)((char*)buf+delta);
      if(HILO(prog->program_number)==0)
	{
	  log_message(MSG_DEBUG,"Network pid %d\n", HILO(prog->network_pid));
	}
      else
	{
	  actual_service=autoconf_find_service_for_modify(services,HILO(prog->program_number));
	  if(!actual_service)
	    {
	      //log_message(MSG_DEBUG,"Prog mumber %d pmt pid %d\n", HILO(prog->program_number), HILO(prog->network_pid));
	    }
	  else
	    {
	      found=1;
	      if(!actual_service->pmt_pid)
		{
		  actual_service->pmt_pid=HILO(prog->network_pid);
		  log_message(MSG_DEBUG,"Pat : service updated  pmt %d\t id %d\t name \"%s\"\n",
			      actual_service->pmt_pid, actual_service->id, actual_service->name);
		}
	    }
	}
      delta+=PAT_PROG_LEN;
    }

  if(!found)
    {
      log_message(MSG_DEBUG,"Pat : No services not found, porbably no SDT for the moment\n");
    }

  return found;
}

/****************************************************************************/
//read the SDT for autoconfiguration
/****************************************************************************/

int autoconf_read_sdt(unsigned char *buf,int len, mumudvb_service_t *services)
{
  int delta;
  sdt_t *header;
  sdt_descr_t *descr_header;
  mumudvb_service_t *new_service=NULL;

  header=(sdt_t *)buf; //on mappe l'en tete sur le paquet

  //On ne regarde que les tables
  //0x42 service_description_section - actual_transport_stream
  //0x46 service_description_section - other_transport_stream
  if((header->table_id==0x42)||(header->table_id==0x46))
    {
      log_message(MSG_DEBUG, "-- SDT --\n");
  
      //On boucle sur les différents services
      delta=SDT_LEN;
      while((len-delta)>(4+SDT_DESCR_LEN))
	{	  
	  descr_header=(sdt_descr_t *)(buf +delta );
	  
	  //we search if we already have service id
	  new_service=autoconf_find_service_for_add(services,HILO(descr_header->service_id));
	  
	  if(new_service)
	    {
	      log_message(MSG_DEBUG, "\tNe service, service_id : 0x%x  ", HILO(descr_header->service_id));
	      log_message(MSG_DEBUG, "\trunning_status : ");
	      switch(descr_header->running_status)
		{
		case 1:
		  log_message(MSG_DEBUG, "not running\t");
		case 2:
		  log_message(MSG_DEBUG, "starts in a few seconds\t");
		case 3:
		  log_message(MSG_DEBUG, "pausing\t");
		case 4:
		  log_message(MSG_DEBUG, "running\t");
		}
	      //we store the datas
	      new_service->id=HILO(descr_header->service_id);
	      new_service->running_status=descr_header->running_status;
	      new_service->free_ca_mode=descr_header->free_ca_mode;
	      log_message(MSG_DEBUG, "\tfree_ca_mode : 0x%x\n", descr_header->free_ca_mode);
	      
	      //log_message(MSG_DEBUG, "\t\tdescriptors_loop_length : %d\n", HILO(descr_header->descriptors_loop_length));
	      //On lit le descripteur contenu dans le paquet
	      parsesdtdescriptor(buf+delta+SDT_DESCR_LEN,HILO(descr_header->descriptors_loop_length),new_service);
	    }
	  delta+=HILO(descr_header->descriptors_loop_length)+SDT_DESCR_LEN;
	}
    }
  else
    log_message(MSG_DEBUG, "\ttable_id : 0x%x\n", header->table_id);
  return 0;
}




void parsesdtdescriptor(unsigned char *buf,int descriptors_loop_len, mumudvb_service_t *service)
{

  while (descriptors_loop_len > 0) {
    unsigned char descriptor_tag = buf[0];
    unsigned char descriptor_len = buf[1] + 2;
    
    if (!descriptor_len) {
      log_message(MSG_DEBUG, "############descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
      break;
    }

    if(descriptor_tag==0x48)
      {
	parseservicedescriptor(buf,service);
      }
    else if( descriptor_tag!=0x53) //53 : CA identifier descriptor
      log_message(MSG_DEBUG, "\t\tdescriptor_tag : 0x%2x\n", descriptor_tag);
    else
      log_message(MSG_DEBUG, "\t\tdescriptor_tag :53 : CA identifier descriptor\n", descriptor_tag);

    buf += descriptor_len;
    descriptors_loop_len -= descriptor_len;
  }
}



void parseservicedescriptor(unsigned char *buf, mumudvb_service_t *service)
{
  int len;
  unsigned char *src, *dest;
  unsigned char type;

  type=buf[2];
  service->type=type;
  //TODO utiliser lookup
  switch(type)
    {
      case 0x01:
	log_message(MSG_DEBUG, "\t Television ");
      break;
      case 0x02:
	log_message(MSG_DEBUG, "\t Radio ");
      break;
      case 0x03:
	log_message(MSG_DEBUG, "\t Teletext ");
      break;
      case 0x0c:
	log_message(MSG_DEBUG, "\t Data braodcast service ");
      break;
    default:
      log_message(MSG_DEBUG, "\t\ttype : 0x%02x ", type);
      log_message(MSG_DEBUG, "\t\ttype inconnu se référer a EN 300 468 table 75\n");
    }

  buf += 3;
  len = *buf; //provider name len
  buf++;
  //we jump the provider name
  //log_message(MSG_DEBUG, "\t\tlen : %d\n", len);
  buf += len;
  len = *buf;
  buf++;
  //log_message(MSG_DEBUG, "\t\tlen : %d\n", len);


  memcpy (service->name, buf, len);
  service->name[len] = '\0';

  /* remove control characters (FIXME: handle short/long name) */
  /* FIXME: handle character set correctly (e.g. via iconv)                                                                                            
   * c.f. EN 300 468 annex A */
  for (src = dest = (unsigned char *) service->name; *src; src++)
    if (*src >= 0x20 && (*src < 0x80 || *src > 0x9f))
      *dest++ = *src;
  *dest = '\0';
  if (!service->name[0]) {
    /* zap zero length names */
    //free (service->name);
    service->name[0] = 0;
  }
  log_message(MSG_DEBUG, "\t\tservice_name : %s\n", service->name);

}

//try to find the service specified by id, if not fount create a new one
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
    {
      return NULL;
    }

  log_message(MSG_DEBUG,"Service NOT found\n");
  
  actual_service->next=malloc(sizeof(mumudvb_service_t));
  if(actual_service->next==NULL)
    {
      log_message( MSG_ERROR,"MALLOC\n");
      return NULL;
    }
  memset (actual_service->next, 0, sizeof( mumudvb_service_t));//we clear it
  return actual_service->next;

}

//try to find the service specified by id, if not found return NULL, otherwise return the service
mumudvb_service_t *autoconf_find_service_for_modify(mumudvb_service_t *services,int service_id)
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
    {
      return actual_service;
    }

  return NULL;

}

//Convert the chained list of services into channels
//Free the services and return the number of channels
int services_to_channels(mumudvb_service_t *services, mumudvb_channel_t *channels, int cam_support, int port, int card)
{

  mumudvb_service_t *actual_service;
  mumudvb_service_t *next_service=NULL;
  int channel_number=0;
  char ip[20];


  actual_service=services;

  do
    {
      if(cam_support && actual_service->free_ca_mode)
	  log_message(MSG_DETAIL,"Service scrambled. Name \"%s\"\n", actual_service->name);
      if(!cam_support && actual_service->free_ca_mode)
	{
	  log_message(MSG_DETAIL,"Service scrambled and no cam support. Name \"%s\"\n", actual_service->name);
	}
      else
	{
	  if(actual_service->type==1)
	    {
	      log_message(MSG_DETAIL,"Autoconf : We convert a new service, id %d pmt_pid %d type %d name \"%s\" \n",
			  actual_service->id, actual_service->pmt_pid, actual_service->type, actual_service->name);

	      channels[channel_number].streamed_channel = 0;
	      channels[channel_number].streamed_channel_old = 1;
	      channels[channel_number].nb_bytes=0;
	      channels[channel_number].pids[0]=actual_service->pmt_pid;
	      channels[channel_number].num_pids=1;
	      channels[channel_number].portOut=port;
	      strcpy(channels[channel_number].name,actual_service->name);
	      sprintf(ip,"239.100.%d.%d", card, channel_number);
	      strcpy(channels[channel_number].ipOut,ip);
	      log_message(MSG_DEBUG,"Ip : \"%s\" port : %d\n",channels[channel_number].ipOut,port);

	      if(cam_support && actual_service->free_ca_mode)
		channels[channel_number].cam_pmt_pid=actual_service->pmt_pid;

	      channel_number++;
	    }
	  else
	    log_message(MSG_DETAIL,"Service type %d, no autoconfigure. Name \"%s\"\n", actual_service->type, actual_service->name);
	}
      actual_service=actual_service->next;
    }
  while(actual_service);

  //FREE the services
  actual_service=services;
  do
    {
      next_service=actual_service->next;
      free(actual_service);
      actual_service=next_service;
    }
  while(actual_service);


  //TODO CHECK THE MAX CHANNELS

  return channel_number;
}

//TODO : explain
void autoconf_end(int card, int number_of_channels, mumudvb_channel_t *channels, fds_t *fds)
{
  int curr_channel;
  int curr_pid;

  log_message(MSG_DETAIL,"Autoconfiguration almost done\n");
  log_message(MSG_DETAIL,"Autoconf : We open the new descriptors\n");
  if (complete_card_fds(card, number_of_channels, channels, fds,1) < 0)
    {
      log_message(MSG_ERROR,"Autoconf : ERROR : CANNOT open the new descriptors\n");
      //return -1; //TODO !!!!!!!!! ADD AN ERROR
    }
  log_message(MSG_DETAIL,"Autoconf : Add the new filters\n");
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      for (curr_pid = 1; curr_pid < channels[curr_channel].num_pids; curr_pid++) //curr_pid = 1 --> why ? PMT opened ?
	set_ts_filt (fds->fd[curr_channel][curr_pid], channels[curr_channel].pids[curr_pid], DMX_PES_OTHER);
    }
  
  log_message(MSG_INFO,"Autoconfiguration done\n");

  log_streamed_channels(number_of_channels, channels);

}
