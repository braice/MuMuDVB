/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Autoconfiguration
 * 
 * (C) 2008-2009 Brice DUBOST <mumudvb@braice.net>
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
#include <iconv.h>

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

extern autoconf_parameters_t autoconf_vars; //just for autoconf_ip_header

/**
The different encodings that can be used
Cf EN 300 468 Annex A (I used v1.9.1)
 */
char *encodings_en300468[] ={
  "ISO8859-1",
  "ISO8859-2",
  "ISO8859-3",
  "ISO8859-4",
  "ISO8859-5",
  "ISO8859-6",
  "ISO8859-7",
  "ISO8859-8",
  "ISO8859-9",
  "ISO8859-10",
  "ISO8859-11",
  "ISO8859-12",
  "ISO8859-13",
  "ISO8859-14",
  "ISO8859-15",
  "ISO-10646", //control char 0x11
  "GB2312",    //control char 0x13
  "BIG5",      //control char 0x14
  "ISO-10646/UTF8",      //control char 0x15
};


/****************************************************************************/
//Parts of this code from libdvb, strongly modified, with commentaries added
//read the pmt for autoconfiguration
/****************************************************************************/
/**
 * Reads the program map table
 * It's used to get the differents "useful" pids of the channel
 * @param pmt the pmt packet
 * @param channel the associated channel
 */
int autoconf_read_pmt(mumudvb_ts_packet_t *pmt, mumudvb_channel_t *channel)
{
	int slen, dslen, i;
	int pid,pcr_pid;
	int found=0;
	pmt_t *header;
	pmt_info_t *descr_header;

	int program_info_length;


	slen=pmt->len;
	header=(pmt_t *)pmt->packet;

	if(header->table_id!=0x02)
	  {
	    log_message( MSG_DETAIL,"Autoconf : Packet PID %d for channel \"%s\" is not a PMT PID\n", pmt->pid, channel->name);
	    return 1;
	  }
	
	log_message( MSG_DEBUG,"Autoconf : PMT read for autoconfiguration of channel \"%s\"\n", channel->name);


	program_info_length=HILO(header->program_info_length); //program_info_length

	//we read the different descriptors included in the pmt
	//for more information see ITU-T Rec. H.222.0 | ISO/IEC 13818 table 2-34
	for (i=program_info_length+PMT_LEN; i<=slen-(PMT_INFO_LEN+4); i+=dslen+PMT_INFO_LEN)
	  {      //we parse the part after the descriptors
	    descr_header=(pmt_info_t *)(pmt->packet+i);
	    dslen=HILO(descr_header->ES_info_length);        //ES_info_length
	    pid=HILO(descr_header->elementary_PID);
	    switch(descr_header->stream_type){
	    case 0x01:
	    case 0x02:
	    case 0x10: /* ISO/IEC 14496-2 Visual - MPEG4 video */	      
	    case 0x1b: /* AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC 14496-10 Video */
	      log_message( MSG_DEBUG,"Autoconf :   Video ");
	      break;
	    case 0x03:
	    case 0x04:
	    case 0x81: /* Audio per ATSC A/53B [2] Annex B */
	    case 0x0f: /* ISO/IEC 13818-7 Audio with ADTS transport syntax - usually AAC */
	    case 0x11: /* ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3 */
	      log_message( MSG_DEBUG,"Autoconf :   Audio ");
	      break;
	    case 0x06: /* Descriptor defined in EN 300 468 */
	      if(dslen)
		{
		  if(pmt_find_descriptor(0x46,pmt->packet+i+PMT_INFO_LEN,dslen))
		    log_message( MSG_DEBUG,"Autoconf :   VBI Teletext ");
		  else if(pmt_find_descriptor(0x56,pmt->packet+i+PMT_INFO_LEN,dslen))
		    log_message( MSG_DEBUG,"Autoconf :   Teletext ");
		  else if(pmt_find_descriptor(0x59,pmt->packet+i+PMT_INFO_LEN,dslen))
		    log_message( MSG_DEBUG,"Autoconf :   Subtitling ");
		  else if(pmt_find_descriptor(0x6a,pmt->packet+i+PMT_INFO_LEN,dslen))
		    log_message( MSG_DEBUG,"Autoconf :   AC3 (audio) ");
		  else if(pmt_find_descriptor(0x7a,pmt->packet+i+PMT_INFO_LEN,dslen))
		    log_message( MSG_DEBUG,"Autoconf :   Enhanced AC3 (audio)");
		  else if(pmt_find_descriptor(0x7b,pmt->packet+i+PMT_INFO_LEN,dslen))
		    log_message( MSG_DEBUG,"Autoconf :   DTS (audio)");
		  else if(pmt_find_descriptor(0x7c,pmt->packet+i+PMT_INFO_LEN,dslen))
		    log_message( MSG_DEBUG,"Autoconf :   AAC (audio)");
		  else
		    {
		      log_message( MSG_DEBUG,"Autoconf : Unknown descriptor see EN 300 468 v1.9.1 table 12, pid %d descriptor tags : ", pid);
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
	    case 0x05:
	      log_message( MSG_DEBUG, "Autoconf : Dropped pid %d, type : 0x05, ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections \n",pid);
	      continue;
	      //Digital Storage Medium Command and Control (DSM-CC) cf H.222.0 | ISO/IEC 13818-1 annex B
	    case 0x0a:
	      log_message( MSG_DEBUG, "Autoconf : Dropped pid %d, type : 0x0A ISO/IEC 13818-6 type A (DSM-CC)\n",pid);
	      continue;
	    case 0x0b:
	      log_message( MSG_DEBUG, "Autoconf : Dropped pid %d, type : 0x0B ISO/IEC 13818-6 type B (DSM-CC)\n",pid);
	      continue;
	    case 0x0c:
	      log_message( MSG_DEBUG, "Autoconf : Dropped pid %d, type : 0x0C ISO/IEC 13818-6 type C (DSM-CC)\n",pid);
	      continue;
	    default:
	      log_message( MSG_INFO, "Autoconf : !!!!Unknown stream type : 0x%02x, PID : %d cf ITU-T Rec. H.222.0 | ISO/IEC 13818\n",descr_header->stream_type,pid);
	      continue;
	    }
	    
	    log_message( MSG_DEBUG,"  PID %d added\n", pid);
	    channel->pids[channel->num_pids]=pid;
	    channel->num_pids++;
	  }

	pcr_pid=HILO(header->PCR_PID); //the pcr pid
	//we check if it's not already included (ie the pcr is carried with the video)
	found=0;
	for(i=0;i<channel->num_pids;i++)
	  {
	    if(channel->pids[i]==pcr_pid)
	       found=1;
	  }
	if(!found)
	  {
	    channel->pids[channel->num_pids]=pcr_pid;
	    channel->num_pids++;
	    log_message( MSG_DEBUG, "Autoconf : Added PCR pid %d\n",pcr_pid);
	  }
	log_message( MSG_DEBUG,"Autoconf : Number of pids after autoconf %d\n", channel->num_pids);
	return 0; 
}


/**
 * Tells if the descriptor with tag in present in buf
 * for more information see ITU-T Rec. H.222.0 | ISO/IEC 13818
 * @param tag the descriptor tag, cf EN 300 468
 * @param buf the decriptors buffer (part of the PMT)
 * @param descriptors_loop_len the length of the descriptors
 */
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

/**
 * Debugging function, Print the tags present in the descriptor
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
      log_message(MSG_DEBUG,"Autoconf : No services not found, probably no SDT for the moment, we skip this PAT\n");
    }

  return found;
}

/****************************************************************************/
//read the SDT for autoconfiguration
/****************************************************************************/
/**
Read the service description table (cf EN 300 468)
This table is used to find the name of the services versus the service number
Thins function will fill the services chained list.
 * @param buf the buffer containing the SDT
 * @param len the len of the buffer
 * @param services the chained list of services
 */
int autoconf_read_sdt(unsigned char *buf,int len, mumudvb_service_t *services)
{
  int delta;
  sdt_t *header;
  sdt_descr_t *descr_header;
  mumudvb_service_t *new_service=NULL;

  header=(sdt_t *)buf; //we map the packet over the header structure

  //We look only for the following tables
  //0x42 service_description_section - actual_transport_stream
  //0x46 service_description_section - other_transport_stream
  if((header->table_id==0x42)||(header->table_id==0x46))
    {
      log_message(MSG_DEBUG, "-- SDT : Service Description Table --\n");
  
      //Loop over different services
      delta=SDT_LEN;
      while((len-delta)>(4+SDT_DESCR_LEN))
	{	  
	  descr_header=(sdt_descr_t *)(buf +delta );
	  
	  //we search if we already have service id
	  new_service=autoconf_find_service_for_add(services,HILO(descr_header->service_id));
	  
	  if(new_service)
	    {
	      log_message(MSG_DEBUG, "\tWe discovered a new service, service_id : 0x%x  ", HILO(descr_header->service_id));
	      switch(descr_header->running_status)
		{
		case 0:
		  log_message(MSG_DEBUG, "running_status : undefined\t");
		  break;
		case 1:
		  log_message(MSG_DEBUG, "running_status : not running\t");
		  break;
		case 2:
		  log_message(MSG_DEBUG, "running_status : starts in a few seconds\t");
		  break;
		case 3:
		  log_message(MSG_DEBUG, "running_status : pausing\t");
		  break;
		  /* 		case 4: */ //too usual to be printed
/* 		  log_message(MSG_DEBUG, "running\t"); */
/* 		  break; */
		case 5:
		  log_message(MSG_DEBUG, "running_status : service off-air\t");
		  break;
		}
	      //we store the data
	      new_service->id=HILO(descr_header->service_id);
	      new_service->running_status=descr_header->running_status;
	      new_service->free_ca_mode=descr_header->free_ca_mode;
	      log_message(MSG_DEBUG, "free_ca_mode : 0x%x\n", descr_header->free_ca_mode);
	      
	      //We read the descriptor
	      parsesdtdescriptor(buf+delta+SDT_DESCR_LEN,HILO(descr_header->descriptors_loop_length),new_service);
	    }
	  delta+=HILO(descr_header->descriptors_loop_length)+SDT_DESCR_LEN;
	}
    }
  else
    log_message(MSG_DEBUG, "\tread sdt, ignored table_id : 0x%x (cf EN 300 468 table 2)\n", header->table_id);
  return 0;
}




/**
 * Parse the SDT descriptors
 * loop over the sdt descriptors and call other parsing functions if necessary
 * @param buf the buffer containing the descriptors
 * @param descriptor_loop_len the len of buffer containing the descriptors
 * @param service the associated service
 */
void parsesdtdescriptor(unsigned char *buf,int descriptors_loop_len, mumudvb_service_t *service)
{

  while (descriptors_loop_len > 0) {
    unsigned char descriptor_tag = buf[0];
    unsigned char descriptor_len = buf[1] + 2;
    
    if (!descriptor_len) {
      log_message(MSG_DEBUG, "####SDT#####descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
      break;
    }
    
    //The service descriptor provides the names of the service provider and the service in text form together with the service_type.
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



/**
 * Parse the service descriptor
 * It's used to get the channel name
 * @param buf the buffer containing the descriptor
 * @param service the associated service
 */
void parseservicedescriptor(unsigned char *buf, mumudvb_service_t *service)
{
  int len;
  unsigned char *src;
  char *dest;
  char *tempdest, *tempbuf;
  unsigned char type;
  int encoding_control_char=8; //cf encodings_en300468 

  type=buf[2];
  service->type=type;
  //TODO utiliser lookup
  //Cf EN 300 468 v1.9.1 table 81
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
      case 0x06:
	log_message(MSG_DEBUG, "\t Mosaic service ");
      break;
      case 0x0c:
	log_message(MSG_DEBUG, "\t Data braodcast service ");
      break;
      case 0x11:
	log_message(MSG_DEBUG, "\t Television MPEG2-HD");
      break;
    default:
      log_message(MSG_DEBUG, "\tUnknow service type (0x%02x), look at EN 300 468 v1.9.1 table 81\n", type);
    }

  buf += 3;
  len = *buf; //provider name len
  buf++;
  //we jump the provider name
  buf += len;
  len = *buf;
  buf++;

  memcpy (service->name, buf, len);
  service->name[len] = '\0';

  /* remove control characters and convert to UTF-8 the channel name */
  //If no channel encoding is specified, it seems that most of the broadcasters
  //uses ISO/IEC 8859-9. But the norm (EN 300 468) said that it should be Latin-1 (ISO/IEC 6937 + euro)

  //temporary buffers allocation
  tempdest=tempbuf=malloc(sizeof(char)*MAX_NAME_LEN);

  len=0;
  for (src = (unsigned char *) service->name; *src; src++)
    if (*src >= 0x20 && (*src < 0x80 || *src > 0x9f)){  
      *tempdest++ = *src;
      len++;
    }
    else if(*src <= 0x20){
      //control character recognition based on EN 300 468 v1.9.1 Annex A
      if(*src<=0x0b){
	encoding_control_char=(int) *src+4-1;
      }
      else if(*src==0x10){ //ISO/IEC 8859 : See table A.4
	src++;//we skip the current byte
	src++;//This one is always set to 0
	if(*src >= 0x01 && *src <=0x0f)
	  encoding_control_char=(int) *src-1;
      }
      else if(*src==0x11){//ISO/IEC 10646 : Basic Multilingual Plane
	encoding_control_char=15;
      }
      else if(*src==0x12){//KSX1001-2004 : Korean Character Set
	log_message(MSG_WARN, "\t\t Encoding KSX1001-2004 (korean character set) not implemented yet by iconv, we'll use the default encoding for service name\n");
      }
      else if(*src==0x13){//GB-2312-1980 : Simplified Chinese Character
	encoding_control_char=16;
      }
      else if(*src==0x14){//Big5 subset of ISO/IEC 10646 : Traditional Chinese
	encoding_control_char=17;
      }
      else if(*src==0x15){//UTF-8 encoding of ISO/IEC 10646 : Basic Multilingual Plane
	encoding_control_char=18;
      }
      else{
	log_message(MSG_WARN, "\t\t Encoding not implemented yet (0x%02x), we'll use the default encoding for service name\n",*src);
      }
    }
    else if (*src >= 0x80 && *src <= 0x9f){
      //to encode in UTF-8 we add 0xc2 before this control character
      if(*src==0x86 || *src==0x87 || *src>=0x8a ){ 
	*tempdest++ = 0xc2;
	len++;
	*tempdest++ = *src;
	len++;
      }
      else{  
      log_message(MSG_DEBUG, "\tUnimplemented name control_character : %x ", *src);
      }
    }
  
  //Conversion to utf8
  iconv_t cd;
  //we open the conversion table
  cd = iconv_open( "UTF8", encodings_en300468[encoding_control_char] );

  size_t inSize, outSize=MAX_NAME_LEN;
  inSize=len;
  //pointers initialisation
  dest=service->name;
  tempdest=tempbuf;
  //conversion
  iconv(cd, &tempdest, &inSize, &dest, &outSize );
  *dest = '\0';
  free(tempbuf);
  iconv_close( cd );
  log_message(MSG_DEBUG, "\tservice_name : \"%s\" (name encoding : %s)\n", service->name,encodings_en300468[encoding_control_char]);

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

  log_message(MSG_DEBUG,"Service not found : ie new service\n");
  
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


//Free the services list
void autoconf_free_services(mumudvb_service_t *services)
{

  mumudvb_service_t *actual_service;
  mumudvb_service_t *next_service;

  actual_service=services;

  for(actual_service=services;actual_service != NULL; actual_service=next_service)
    {
      next_service= actual_service->next;
      free(actual_service);
    }
}

/**
   Convert the chained list of services into channels
   This function is called when We've got all the services, we now fill the channels structure
   After that we go in AUTOCONF_MODE_PIDS to get audio and video pids
   @param services Chained list of services
   @param channels Chained list of channels
   @param cam_support Do we care about scrambled channels ?
   @param port The mulicast port
   @param card The card number for the ip address
 */
int services_to_channels(mumudvb_service_t *services, mumudvb_channel_t *channels, int cam_support, int port, int card)
{

  mumudvb_service_t *actual_service;
  int channel_number=0;
  char ip[20];


  actual_service=services;

  do
    {
      if(cam_support && actual_service->free_ca_mode)
	  log_message(MSG_DETAIL,"Service scrambled. Name \"%s\"\n", actual_service->name);
      if(!cam_support && actual_service->free_ca_mode) //FIXME : Some providers does not repport correctly free_ca_mode, add an option to ignore this
	{
	  log_message(MSG_DETAIL,"Service scrambled and no cam support. Name \"%s\"\n", actual_service->name);
	}
      else
	{
	  //Cf EN 300 468 v1.9.1 Table 81
	  if(actual_service->type==1||actual_service->type==0x11) //service_type "digital television service" (0x01) || MPEG-2 HD digital television service (0x11)
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
	      sprintf(ip,"%s.%d.%d", autoconf_vars.autoconf_ip_header, card, channel_number);
	      strcpy(channels[channel_number].ipOut,ip);
	      log_message(MSG_DEBUG,"Ip : \"%s\" port : %d\n",channels[channel_number].ipOut,port);

	      if(cam_support && actual_service->free_ca_mode)
		channels[channel_number].cam_pmt_pid=actual_service->pmt_pid;

	      channel_number++;
	    }
	  else if(actual_service->type==0x02) //service_type digital radio sound service
	    {
	      //TODO : set this as an option (autoconfigure radios)
	      log_message(MSG_DETAIL,"Service type digital radio sound service, no autoconfigure. Name \"%s\"\n", actual_service->name);
	    }
	  else if(actual_service->type==0x0c) //service_type data broadcast service
	    {
	      log_message(MSG_DETAIL,"Service type data broadcast service, no autoconfigure. Name \"%s\"\n", actual_service->name);
	    }
	  else
	    log_message(MSG_DETAIL,"Service type 0x%02x, no autoconfigure. Name \"%s\"\n", actual_service->type, actual_service->name);
	}
      actual_service=actual_service->next;
    }
  while(actual_service&&channel_number<MAX_CHANNELS);
  
  if(channel_number==MAX_CHANNELS)
    log_message(MSG_WARN,"We reached the maximum channel number, we drop other possible channels !\n");


  return channel_number;
}

void autoconf_end(int card, int number_of_channels, mumudvb_channel_t *channels, uint8_t *asked_pid, fds_t *fds)
{
  //This function is called when autoconfiguration is finished
  //It open what is needed to stream the new channels
  int curr_channel;
  int curr_pid;

  log_message(MSG_DETAIL,"Autoconfiguration almost done\n");
  log_message(MSG_DETAIL,"Autoconf : We open the new descriptors\n");
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
	asked_pid[channels[curr_channel].pids[curr_pid]]=PID_ASKED;
    }
  // we open the file descriptors
  if (create_card_fd (card, asked_pid, fds) < 0)
    {
      log_message(MSG_ERROR,"Autoconf : ERROR : CANNOT open the new descriptors Some channels will probably not work\n");
      //return; //FIXME : whato do we do here ?
    }
  
  log_message(MSG_DETAIL,"Autoconf : Add the new filters\n");
  set_filters(asked_pid, fds);
  
  log_message(MSG_INFO,"Autoconfiguration done\n");

  log_streamed_channels(number_of_channels, channels);

}
