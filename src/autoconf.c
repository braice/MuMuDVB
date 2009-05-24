/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Autoconfiguration
 * 
 * (C) 2008-2009 Brice DUBOST <mumudvb@braice.net>
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
 *  The pat contains the list of the channels in the actual stream, their service number and the PMT pid
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
 *  in the autoconf=1 mode (and we add the filters for the new pmt pids)
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
#include <iconv.h>

#include "errors.h"
#include "ts.h"
#include "mumudvb.h"
#include "dvb.h"
#include "autoconf.h"

//LIBUSCI for long channel names (ATSC only)
#ifdef LIBUCSI
#include <libucsi/atsc/types.h>
#endif

void parse_sdt_descriptor(unsigned char *buf,int descriptors_loop_len, mumudvb_service_t *services);
void parse_service_descriptor(unsigned char *buf, mumudvb_service_t *services);
void autoconf_show_CA_identifier_descriptor(unsigned char *buf);
mumudvb_service_t *autoconf_find_service_for_add(mumudvb_service_t *services,int service_id);
mumudvb_service_t *autoconf_find_service_for_modify(mumudvb_service_t *services,int service_id);
int pmt_find_descriptor(uint8_t tag, unsigned char *buf, int descriptors_loop_len);
void pmt_print_descriptor_tags(unsigned char *buf, int descriptors_loop_len);
int autoconf_parse_vct_channel(unsigned char *buf, autoconf_parameters_t *parameters);


/**
@brief The different encodings that can be used
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
//Parts of this code (read of the pmt and read of the pat)
// from libdvb, strongly modified, with commentaries added
/****************************************************************************/

/** @brief Reads the program map table
 *
 * It's used to get the differents "useful" pids of the channel
 * @param pmt the pmt packet
 * @param channel the associated channel
 */
int autoconf_read_pmt(mumudvb_ts_packet_t *pmt, mumudvb_channel_t *channel)
{
	int section_len, descr_section_len, i;
	int pid,pcr_pid;
	int found=0;
	pmt_t *header;
	pmt_info_t *descr_header;

	int program_info_length;


	section_len=pmt->len;
	header=(pmt_t *)pmt->packet;

	if(header->table_id!=0x02)
	  {
	    log_message( MSG_DETAIL,"Autoconf : Packet PID %d for channel \"%s\" is not a PMT PID\n", pmt->pid, channel->name);
	    channel->pmt_pid=0;
	    return 1;
	  }

	//We check if this PMT belongs to the current channel. (Only works with autoconfiguration full for the moment because it stores the ts_id)
	if(channel->ts_id && (channel->ts_id != HILO(header->program_number)) )
	  {
	    log_message( MSG_DETAIL,"Autoconf : The PMT %d not belongs to channel \"%s\"\n", pmt->pid, channel->name);
	    return 1;
	  }
	
	log_message( MSG_DEBUG,"Autoconf : PMT (PID %d) read for autoconfiguration of channel \"%s\"\n", pmt->pid, channel->name);


	program_info_length=HILO(header->program_info_length); //program_info_length

	//we read the different descriptors included in the pmt
	//for more information see ITU-T Rec. H.222.0 | ISO/IEC 13818 table 2-34
	for (i=program_info_length+PMT_LEN; i<=section_len-(PMT_INFO_LEN+4); i+=descr_section_len+PMT_INFO_LEN)
	  {      
	    //we parse the part after the descriptors
	    //we map the descriptor header
	    descr_header=(pmt_info_t *)(pmt->packet+i);
	    //We get the length of the descriptor
	    descr_section_len=HILO(descr_header->ES_info_length);        //ES_info_length

	    pid=HILO(descr_header->elementary_PID);
	    //Depending of the stream type we'll take or not this pid
	    switch(descr_header->stream_type)
	      {
	      case 0x01:
	      case 0x02:
	      case 0x10: /* ISO/IEC 14496-2 Visual - MPEG4 video */	      
	      case 0x1b: /* AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC 14496-10 Video */
		log_message( MSG_DEBUG,"Autoconf :   Video \tpid %d\n",pid);
		break;

	      case 0x03:
	      case 0x04:
	      case 0x81: /* Audio per ATSC A/53B [2] Annex B */
	      case 0x0f: /* ISO/IEC 13818-7 Audio with ADTS transport syntax - usually AAC */
	      case 0x11: /* ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3 */
		log_message( MSG_DEBUG,"Autoconf :   Audio \tpid %d\n",pid);
		break;

	      case 0x06: /* Descriptor defined in EN 300 468 */
		if(descr_section_len) //If we have an accociated descriptor, we'll search inforation in it
		  {
		    if(pmt_find_descriptor(0x46,pmt->packet+i+PMT_INFO_LEN,descr_section_len))
		      log_message( MSG_DEBUG,"Autoconf :   VBI Teletext \tpid %d\n",pid);
		    else if(pmt_find_descriptor(0x56,pmt->packet+i+PMT_INFO_LEN,descr_section_len))
		      log_message( MSG_DEBUG,"Autoconf :   Teletext \tpid %d\n",pid);
		    else if(pmt_find_descriptor(0x59,pmt->packet+i+PMT_INFO_LEN,descr_section_len))
		      log_message( MSG_DEBUG,"Autoconf :   Subtitling \tpid %d\n",pid);
		    else if(pmt_find_descriptor(0x6a,pmt->packet+i+PMT_INFO_LEN,descr_section_len))
		      log_message( MSG_DEBUG,"Autoconf :   AC3 (audio) \tpid %d\n",pid);
		    else if(pmt_find_descriptor(0x7a,pmt->packet+i+PMT_INFO_LEN,descr_section_len))
		      log_message( MSG_DEBUG,"Autoconf :   Enhanced AC3 (audio) \tpid %d\n",pid);
		    else if(pmt_find_descriptor(0x7b,pmt->packet+i+PMT_INFO_LEN,descr_section_len))
		      log_message( MSG_DEBUG,"Autoconf :   DTS (audio) \tpid %d\n",pid);
		    else if(pmt_find_descriptor(0x7c,pmt->packet+i+PMT_INFO_LEN,descr_section_len))
		      log_message( MSG_DEBUG,"Autoconf :   AAC (audio) \tpid %d\n",pid);
		    else
		      {
			log_message( MSG_DEBUG,"Autoconf : Unknown descriptor see EN 300 468 v1.9.1 table 12, pid %d descriptor tags : ", pid);
			pmt_print_descriptor_tags(pmt->packet+i+PMT_INFO_LEN,descr_section_len);
			log_message( MSG_DEBUG,"\n");
			continue;
		      }
		  }
		else
		  {
		    log_message( MSG_DEBUG,"Autoconf : PMT read : stream type 0x06 without descriptor\n");
		    continue;
		  }
		break;
		//Now, the list of what we drop
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
	    
	    //log_message( MSG_DEBUG,"Autoconf  PID %d added\n", pid);
	    channel->pids[channel->num_pids]=pid;
	    channel->num_pids++;
	  }

	pcr_pid=HILO(header->PCR_PID); //The PCR pid.
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

	//We store the PMT version
	channel->pmt_version=header->version_number;

	log_message( MSG_DEBUG,"Autoconf : Number of pids after autoconf %d\n", channel->num_pids);
	return 0; 
}


/** @brief Tells if the descriptor with tag in present in buf
 *
 * for more information see ITU-T Rec. H.222.0 | ISO/IEC 13818
 *
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
      
      log_message( MSG_DEBUG,"0x%02x - ", descriptor_tag);
      buf += descriptor_len;
      descriptors_loop_len -= descriptor_len;
    }
}


/** @brief read the PAT for autoconfiguration
 * This function extract the pmt from the pat 
 * before doing so it checks if the service is already initialised (sdt packet)
 *
 * @param autconf_vars The autoconfiguration structure, containing all we need
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

  log_message(MSG_DEBUG,"Autoconf : ---- New PAT ----\n");

  //PAT reading
  section_length=HILO(pat->section_length);

  log_message(MSG_DEBUG,  "Autoconf : pat info : ts_id 0x%04x section_length %d version %i last_section_number %x \n"
	      ,HILO(pat->transport_stream_id)
	      ,HILO(pat->section_length)
	      ,pat->version_number
	      ,pat->last_section_number); 

  //We store the transport stream ID
  autoconf_vars->transport_stream_id=HILO(pat->transport_stream_id);

  //We loop over the different programs included in the pat
  while((delta+PAT_PROG_LEN)<(section_length))
    {
      prog=(pat_prog_t*)((char*)buf+delta);
      if(HILO(prog->program_number)==0)
	{
	  log_message(MSG_DEBUG,"Autoconf : Network pid %d\n", HILO(prog->network_pid));
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
		  actual_service->pmt_pid=HILO(prog->network_pid);
		  log_message(MSG_DEBUG,"Autoconf : service updated  pmt pid : %d\t id 0x%x\t name \"%s\"\n",
			      actual_service->pmt_pid,
			      actual_service->id,
			      actual_service->name);
		}
	    }
	  else
	    {
	      log_message(MSG_DEBUG,"Autoconf : service missing  pmt pid : %d\t id 0x%x\t\n",
			  HILO(prog->network_pid),
			  HILO(prog->program_number));
	      channels_missing++;
	    }
	}
      delta+=PAT_PROG_LEN;
      number_of_services++;
    }

  log_message(MSG_DEBUG,"Autoconf : This pat contains %d services\n",number_of_services);

  if(channels_missing)
    {
      log_message(MSG_DETAIL,"Autoconf : PAT read %d channels on %d are missing, we wait for others SDT/PSIP for the moment.\n",channels_missing,number_of_services);
      return 0;
    }

  return 1;
}

/** @brief Read the service description table (cf EN 300 468)
 *
 * This table is used to find the name of the services versus the service number
 * This function will fill the services chained list.
 *
 * @param buf the buffer containing the SDT
 * @param len the len of the buffer
 * @param services the chained list of services
 * @todo : give the parameters as read_pat
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
      log_message(MSG_DEBUG, "Autoconf : -- SDT : Service Description Table --\n");
  
      //Loop over different services in the SDT
      delta=SDT_LEN;
      while((len-delta)>(4+SDT_DESCR_LEN))
	{	  
	  descr_header=(sdt_descr_t *)(buf +delta );
	  
	  //we search if we already have service id
	  new_service=autoconf_find_service_for_add(services,HILO(descr_header->service_id));
	  
	  if(new_service)
	    {
	      log_message(MSG_DEBUG, "Autoconf : We discovered a new service, service_id : 0x%x  ", HILO(descr_header->service_id));

	      //For information only
	      switch(descr_header->running_status)
		{
		case 0:
		  log_message(MSG_DEBUG, "running_status : undefined\t");  break;
		case 1:
		  log_message(MSG_DEBUG, "running_status : not running\t");  break;
		case 2:
		  log_message(MSG_DEBUG, "running_status : starts in a few seconds\t");  break;
		case 3:
		  log_message(MSG_DEBUG, "running_status : pausing\t");  break;
		  /* 		case 4:  log_message(MSG_DEBUG, "running\t");  break; *///too usual to be printed
		case 5:
		  log_message(MSG_DEBUG, "running_status : service off-air\t");  break;
		}
	      
	      //we store the data
	      new_service->id=HILO(descr_header->service_id);
	      new_service->running_status=descr_header->running_status;
	      new_service->free_ca_mode=descr_header->free_ca_mode;
	      log_message(MSG_DEBUG, "free_ca_mode : 0x%x\n", descr_header->free_ca_mode);
	      //We read the descriptor
	      parse_sdt_descriptor(buf+delta+SDT_DESCR_LEN,HILO(descr_header->descriptors_loop_length),new_service);
	    }
	  delta+=HILO(descr_header->descriptors_loop_length)+SDT_DESCR_LEN;
	}
    }
  return 0;
}


/** @brief Parse the SDT descriptors
 * Loop over the sdt descriptors and call other parsing functions if necessary
 * @param buf the buffer containing the descriptors
 * @param descriptors_loop_len the len of buffer containing the descriptors
 * @param service the associated service
 */
void parse_sdt_descriptor(unsigned char *buf,int descriptors_loop_len, mumudvb_service_t *service)
{

  while (descriptors_loop_len > 0) {
    unsigned char descriptor_tag = buf[0];
    unsigned char descriptor_len = buf[1] + 2;
    
    if (!descriptor_len) {
      log_message(MSG_DEBUG, " Autoconf : --- SDT descriptor --- descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
      break;
    }
    
    //The service descriptor provides the names of the service provider and the service in text form together with the service_type.
    if(descriptor_tag==0x48)
      parse_service_descriptor(buf,service);
    else if( descriptor_tag==0x53) //53 : CA identifier descriptor. This descriptor contains the CA_systems_id (the scrambling algorithms)
      autoconf_show_CA_identifier_descriptor(buf);
    //else
    //log_message(MSG_DEBUG, "Autoconf SDT descriptor_tag : 0x%2x\n", descriptor_tag);

    buf += descriptor_len;
    descriptors_loop_len -= descriptor_len;
  }
}



/** @brief Parse the service descriptor
 * It's used to get the channel name
 * @param buf the buffer containing the descriptor
 * @param service the associated service
 */
void parse_service_descriptor(unsigned char *buf, mumudvb_service_t *service)
{
  /* Service descriptor : 
     descriptor_tag			8
     descriptor_length			8
     service_type			8
     service_provider_name_length	8
     for (i=0;i<N;I++){
     char				8
     }
     service_name_length		8
     for (i=0;i<N;I++){
     Char				8
     }
   */
  int len;
  unsigned char *src;
  char *dest;
  char *tempdest, *tempbuf;
  int encoding_control_char=8; //cf encodings_en300468 
  
  buf += 2;
  service->type=*buf;
  /**\todo use lookup*/
  //Cf EN 300 468 v1.9.1 table 81
  switch(service->type)
    {
      case 0x01:
	log_message(MSG_DEBUG, "Autoconf : service type : Television\n"); break;
      case 0x02:
	log_message(MSG_DEBUG, "Autoconf : service type : Radio\n"); break;
      case 0x03:
	log_message(MSG_DEBUG, "Autoconf : service type : Teletext\n"); break;
      case 0x06:
	log_message(MSG_DEBUG, "Autoconf : service type : Mosaic service\n"); break;
      case 0x0c:
	log_message(MSG_DEBUG, "Autoconf : service type : Data braodcast service\n"); break;
      case 0x11:
	log_message(MSG_DEBUG, "Autoconf : service type : Television MPEG2-HD\n"); break;
    default:
      log_message(MSG_WARN, "Autoconf : Please report : Unknow service type (0x%02x), doc : EN 300 468 v1.9.1 table 81\n",
		  service->type);
    }

  buf ++; //we skip the service_type
  len = *buf; //provider name len

  //we jump the provider_name + the provider_name_len
  buf += len + 1;

  //Channel name len
  len = *buf;
  buf++;  //we jump the channel_name_len

  //We store the channel name with the raw encoding
  memcpy (service->name, buf, len);
  service->name[len] = '\0';

  /* remove control characters and convert to UTF-8 the channel name */
  //If no channel encoding is specified, it seems that most of the broadcasters
  //uses ISO/IEC 8859-9. But the norm (EN 300 468) said that it should be Latin-1 (ISO/IEC 6937 + euro)

  //temporary buffers allocation
  tempdest=tempbuf=malloc(sizeof(char)*MAX_NAME_LEN);
  if(tempdest==NULL)
    {
      log_message(MSG_WARN,"Warning : Malloc error in parse_service_descriptor\n");
      return;
    }

  len=0;
  for (src = (unsigned char *) service->name; *src; src++)
    {
      if (*src >= 0x20 && (*src < 0x80 || *src > 0x9f))
	{
	  //We copy non-control characters
	  *tempdest++ = *src;
	  len++;
	}
      else if(*src <= 0x20)
	{
	  //control character recognition based on EN 300 468 v1.9.1 Annex A
	  if(*src<=0x0b){
	    encoding_control_char=(int) *src+4-1;
	  }
	  else if(*src==0x10)
	    { //ISO/IEC 8859 : See table A.4
	      src++;//we skip the current byte
	      src++;//This one is always set to 0
	      if(*src >= 0x01 && *src <=0x0f)
		encoding_control_char=(int) *src-1;
	    }
	  else if(*src==0x11)//ISO/IEC 10646 : Basic Multilingual Plane
	    encoding_control_char=15;
	  else if(*src==0x12)//KSX1001-2004 : Korean Character Set
	    log_message(MSG_WARN, "\t\t Encoding KSX1001-2004 (korean character set) not implemented yet by iconv, we'll use the default encoding for service name\n");
	  else if(*src==0x13)//GB-2312-1980 : Simplified Chinese Character
	    encoding_control_char=16;
	  else if(*src==0x14)//Big5 subset of ISO/IEC 10646 : Traditional Chinese
	    encoding_control_char=17;
	  else if(*src==0x15)//UTF-8 encoding of ISO/IEC 10646 : Basic Multilingual Plane
	    encoding_control_char=18;
       	  else
	    log_message(MSG_WARN, "\t\t Encoding not implemented yet (0x%02x), we'll use the default encoding for service name\n",*src);
	}
      else if (*src >= 0x80 && *src <= 0x9f)
	{
	  //to encode in UTF-8 we add 0xc2 before this control character
	  if(*src==0x86 || *src==0x87 || *src>=0x8a )
	    { 
	      *tempdest++ = 0xc2;
	      len++;
	      *tempdest++ = *src;
	      len++;
	    }
	  else
	    log_message(MSG_DEBUG, "\tUnimplemented name control_character : %x ", *src);
	}
    }

  //Conversion to utf8
  iconv_t cd;
  //we open the conversion table
  cd = iconv_open( "UTF8", encodings_en300468[encoding_control_char] );

  size_t inSize, outSize=MAX_NAME_LEN;
  inSize=len;
  //pointers initialisation because iconv change them, we store
  dest=service->name;
  tempdest=tempbuf;
  //conversion
  iconv(cd, &tempdest, &inSize, &dest, &outSize );
  *dest = '\0';
  free(tempbuf);
  iconv_close( cd );

  log_message(MSG_DEBUG, "Autoconf : service_name : \"%s\" (name encoding : %s)\n", service->name,encodings_en300468[encoding_control_char]);

}


/** @Brief : show the contents of the CA identifier descriptor
 *
 * @param buf : the buffer containing the descriptor
 */
void autoconf_show_CA_identifier_descriptor(unsigned char *buf)
{

  int length,i;

  log_message(MSG_DEBUG, "Autoconf : --- SDT descriptor --- CA identifier descriptor\nAutoconf : CA_system_ids : ");

  length=buf[1];
  for(i=0;i<length;i++)
    log_message(MSG_DEBUG, "%d ", buf[i+2]);

  log_message(MSG_DEBUG, "\n");   

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
      log_message( MSG_WARN, "Warning : MALLOC error in autoconf_find_service_for_add\n");
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
void autoconf_freeing(autoconf_parameters_t *autoconf_vars,int flags)
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
  //We have the option not to free the PMT
  if(!(flags & DONT_FREE_PMT) && (autoconf_vars->autoconf_temp_pmt))
    {
      free(autoconf_vars->autoconf_temp_pmt);
      autoconf_vars->autoconf_temp_pmt=NULL;
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

/** @brief Convert the chained list of services into channels
 *
 * This function is called when We've got all the services, we now fill the channels structure
 * After that we go in AUTOCONF_MODE_PIDS to get audio and video pids
 * @param services Chained list of services
 * @param channels Chained list of channels
 * @param port The mulicast port
 * @param card The card number for the ip address
 */
int services_to_channels(autoconf_parameters_t parameters, mumudvb_channel_t *channels, int port, int card)
{

  mumudvb_service_t *actual_service;
  int channel_number=0;
  char ip[20];
  
  actual_service=parameters.services;

  do
    {
      if(parameters.autoconf_scrambled && actual_service->free_ca_mode)
	  log_message(MSG_DETAIL,"Service scrambled. Name \"%s\"\n", actual_service->name);

      if(!parameters.autoconf_scrambled && actual_service->free_ca_mode)
	log_message(MSG_DETAIL,"Service scrambled and no cam support. Name \"%s\"\n", actual_service->name);
      else if(!actual_service->pmt_pid)
	log_message(MSG_DETAIL,"Service without a PMT pid, we skip. Name \"%s\"\n", actual_service->name);
      else
	{
	  //Cf EN 300 468 v1.9.1 Table 81
	  if((actual_service->type==1||
	      actual_service->type==0x11)||
	     (actual_service->type==0x02&&parameters.autoconf_radios))
	    // service_type "digital television service" (0x01) ||
	    // MPEG-2 HD digital television service (0x11) || 
	    // service_type digital radio sound service  (0x02)
	    {
	      log_message(MSG_DETAIL,"Autoconf : We convert a new service into a channel, id %d pmt_pid %d name \"%s\" \n",
			  actual_service->id, actual_service->pmt_pid, actual_service->name);

	      channels[channel_number].streamed_channel = 0;
	      channels[channel_number].streamed_channel_old = 1;
	      channels[channel_number].nb_bytes=0;
	      channels[channel_number].pids[0]=actual_service->pmt_pid;
	      channels[channel_number].num_pids=1;
	      channels[channel_number].portOut=port;
	      strcpy(channels[channel_number].name,actual_service->name);
	      sprintf(ip,"%s.%d.%d", parameters.autoconf_ip_header, card, channel_number);
	      strcpy(channels[channel_number].ipOut,ip);
	      log_message(MSG_DEBUG,"Autoconf : Channel Ip : \"%s\" port : %d\n",channels[channel_number].ipOut,port);

	      //This is a scrambled channel, we will have to ask the cam for descrambling it
	      if(parameters.autoconf_scrambled && actual_service->free_ca_mode)
		channels[channel_number].need_cam_ask=1;
     
	      //We store the PMT and the service id in the channel
	      channels[channel_number].pmt_pid=actual_service->pmt_pid;
	      channels[channel_number].ts_id=actual_service->id;

	      channel_number++;
	    }
	  else if(actual_service->type==0x02) //service_type digital radio sound service
	    log_message(MSG_DETAIL,"Autoconf : Service type digital radio sound service, no autoconfigure. (if you want add autoconf_radios=1 to your configuration file) Name \"%s\"\n",
			actual_service->name);
	  else if(actual_service->type==0x0c) //service_type data broadcast service
	    log_message(MSG_DETAIL,"Autoconf : Service type data broadcast service, no autoconfigure. Name \"%s\"\n",
			  actual_service->name);
	  else
	    log_message(MSG_DETAIL,"Autoconf : Service type 0x%02x, no autoconfigure. Name \"%s\"\n",
			actual_service->type,
			actual_service->name);
	}
      actual_service=actual_service->next;
    }
  while(actual_service && channel_number<MAX_CHANNELS);
  
  if(channel_number==MAX_CHANNELS)
    log_message(MSG_WARN,"Autoconf : Warning : We reached the maximum channel number, we drop other possible channels !\n");

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
 * @param asked_pid the array containing the pids already asked
 * @param fds the file descriptors
*/
int autoconf_finish_full(int *number_of_channels, mumudvb_channel_t *channels, autoconf_parameters_t *autoconf_vars, int common_port, int card, fds_t *fds,uint8_t *asked_pid, int multicast_ttl)
{
  int curr_channel,curr_pid;
  *number_of_channels=services_to_channels(*autoconf_vars, channels, common_port, card); //Convert the list of services into channels
  //we got the pmt pids for the channels, we open the filters
  for (curr_channel = 0; curr_channel < *number_of_channels; curr_channel++)
    {
      for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
	asked_pid[channels[curr_channel].pids[curr_pid]]=PID_ASKED;
    }
  // we open the file descriptors
  if (create_card_fd (card, asked_pid, fds) < 0)
    {
      log_message(MSG_ERROR,"Autoconf : ERROR : CANNOT Open the new descriptors\n");
      return 666<<8; //the <<8 is to make difference beetween signals and errors;
    }
  // we set the new filters
  set_filters( asked_pid, fds);
  
  for (curr_channel = 0; curr_channel < *number_of_channels; curr_channel++)
    {
      // Init udp
      //Open the multicast socket for the new channel
      channels[curr_channel].socketOut = makesocket (channels[curr_channel].ipOut, channels[curr_channel].portOut, multicast_ttl, &channels[curr_channel].sOut);
    }
  
  log_message(MSG_DEBUG,"Autoconf : Step TWO, we get the video and audio PIDs\n");
  //We free autoconf memory except PMT
  autoconf_freeing(autoconf_vars,DONT_FREE_PMT);
  
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
 * @param fds the file descriptors
*/
void autoconf_end(int card, int number_of_channels, mumudvb_channel_t *channels, uint8_t *asked_pid, fds_t *fds)
{
  int curr_channel;
  int curr_pid;

  log_message(MSG_DETAIL,"Autoconfiguration almost done\n");
  log_message(MSG_DETAIL,"Autoconf : We open the new file descriptors\n");
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    {
      for (curr_pid = 0; curr_pid < channels[curr_channel].num_pids; curr_pid++)
	asked_pid[channels[curr_channel].pids[curr_pid]]=PID_ASKED;
    }
  // we open the file descriptors
  if (create_card_fd (card, asked_pid, fds) < 0)
    {
      log_message(MSG_ERROR,"Autoconf : ERROR : CANNOT open the new descriptors. Some channels will probably not work\n");
      //return; //FIXME : what do we do here ?
    }
  
  log_message(MSG_DETAIL,"Autoconf : Add the new filters\n");
  set_filters(asked_pid, fds);
  
  log_message(MSG_INFO,"Autoconfiguration done\n");

  log_streamed_channels(number_of_channels, channels);

  /**\todo : make an option to generate it or not ?*/
  gen_config_file(number_of_channels, channels, GEN_CONF_PATH);

}

/*******************************************************
  ATSC 
********************************************************/

/** @Brief read a PSIP table to find channels names 
 *
 * We read the master PSIP pid, search for a (T/C)VCT table
 * If it find this table, searches for channels within the transport (check
 * the transport id found in the PAT) and for the extended channel name descriptor
 * For the moment if the name of the channel is compressed, it will use the short channel
 * 
 * @param parameters : the structure containing autoconfiguration parameters
 */

int autoconf_read_psip(autoconf_parameters_t *parameters)
{
  mumudvb_ts_packet_t *psip_mumu;
  int number_of_channels_in_section=0;
  int delta=0;
  int i=0;
  unsigned char *buf=NULL;

  //We get the packet
  psip_mumu=parameters->autoconf_temp_psip;
  buf=psip_mumu->packet;
  psip_t       *psip=(psip_t*)(buf);

  //We look only for the following tables OxC8 : TVCT (Terrestrial Virtual Channel Table), 0XC9 : CVCT (Cable Virtual Channel Table)
  if (psip->table_id != 0xc8 && psip->table_id != 0xc9)  
      return 1;
  
  log_message(MSG_DEBUG,"Autoconf : ---- ATSC : PSIP TVCT ot CVCT----\n");

  if(parameters->transport_stream_id<0)
    {
      log_message(MSG_DEBUG,"Autoconf :We don't have a transport id from the pat, we skip this PSIP\n");
      return 1;
    }
    
  log_message(MSG_DEBUG,"Autoconf : PSIP transport_stream_id : 0x%x PAT TSID 0x%x\n",
	      HILO(psip->transport_stream_id),
	      parameters->transport_stream_id);

  if(HILO(psip->transport_stream_id)!=parameters->transport_stream_id)
    {
      log_message(MSG_DEBUG,"Autoconf : This table belongs to another transponder, we skip\n");
      return 1;
    }

  
  number_of_channels_in_section=buf[PSIP_HEADER_LEN]; //This field is the byte just after the PSIP header
  delta=PSIP_HEADER_LEN+1;
  log_message(MSG_DEBUG,"Autoconf : VCT : number_of_channels_in_section %d\n",
	      number_of_channels_in_section);

  //We parse the channels
  for(i=0;i<number_of_channels_in_section;i++)
    delta+=autoconf_parse_vct_channel(buf+delta,parameters);

  return 0;
}

/** @brief Parse the contents of a (CT)VCT channel descriptor
 *
 * This function parse the channel and add it to the services list
 * 
 * @param buf - The channel descriptor
 * @param parameters - The structure containing autoconf parameters
 */

int autoconf_parse_vct_channel(unsigned char *buf, autoconf_parameters_t *parameters)
{
  psip_vct_channel_t *vct_channel;
  char unconverted_short_name[15];//2*7 + 1 (for '\0')
  char *inbuf, *dest; //Pointers for iconv conversion
  char utf8_short_name[15];
  char *channel_name=NULL;
  char long_name[MAX_NAME_LEN];

  int mpeg2_service_type=0;
  vct_channel=(psip_vct_channel_t *)buf;
  long_name[0]='\0';

  mumudvb_service_t *new_service=NULL;

  // *********** We get the channel short name *******************
  memcpy (unconverted_short_name, vct_channel->short_name, 14*sizeof(uint8_t));
  unconverted_short_name[14] = '\0';

  //Conversion to utf8 of the short name
  iconv_t cd;
  //we open the conversion table
  cd = iconv_open( "UTF8", "UTF-16BE" );
  log_message(MSG_DEBUG,"Autoconf : We use big Endian UTF16 as source for channel name, if it give weird characters please contact\n");
  size_t inSize, outSize=14;
  inSize=14;
  //pointers initialisation
  dest=utf8_short_name;
  inbuf=unconverted_short_name;
  //conversion
  iconv(cd, &inbuf, &inSize, &dest, &outSize );
  *dest = '\0';
  iconv_close( cd );
  log_message(MSG_DEBUG, "Autoconf : \tchannel short_name : \"%s\"\n", utf8_short_name);


  //************ We skip "ininteresting" channels  ****************
  if(vct_channel->modulation_mode==0x01)
    {
      log_message(MSG_DEBUG, "Autoconf : \tAnalog channel, we skip\n");
      return PSIP_VCT_LEN + HILO(vct_channel->descriptor_length); //We return the length
    }
  if(HILO(vct_channel->channel_tsid)!=parameters->transport_stream_id)
    {
      log_message(MSG_DEBUG,"Autoconf : Channel for another transponder, we skip :  Channel TSID 0x%x , PAT TSID 0x%x\n",
		  HILO(vct_channel->channel_tsid),
		  parameters->transport_stream_id);
      return PSIP_VCT_LEN + HILO(vct_channel->descriptor_length); //We return the length
    }
  if(vct_channel->hidden)
    {
      log_message(MSG_DEBUG,"Autoconf : This channel is supposed to be hidden, we skip. Please contact if you want to bypass\n");
      return PSIP_VCT_LEN + HILO(vct_channel->descriptor_length); //We return the length
    }


  //We "convert" ATSC service type to the "equivalent" MPEG2 service type
  switch(vct_channel->service_type)
    {
    case 0x02://ATSC_digital_television — The virtual channel carries television programming (audio, video and
      //optional associated data) conforming to ATSC standards
      mpeg2_service_type=0x01; //service_type "digital television service" (0x01)
      log_message(MSG_DEBUG,"Autoconf :vct_channel->service_type ATSC_digital_television\n");
      break;
    case 0x03://ATSC_audio — The virtual channel carries audio programming (audio service and optional
      //associated data) conforming to ATSC standards.
      mpeg2_service_type=0x02;//service_type digital radio sound service  (0x02)
      log_message(MSG_DEBUG,"Autoconf :vct_channel->service_type ATSC_audio\n",vct_channel->service_type);
      break;
    case 0x04://ATSC_data_only_service — The virtual channel carries a data service conforming to ATSC
      //standards, but no video of stream_type 0x02 or audio of stream_type 0x81.
      mpeg2_service_type=0x0c;//service_type data broadcast service
      log_message(MSG_DEBUG,"Autoconf :vct_channel->service_type ATSC_data_only_service\n",vct_channel->service_type);
      break;
    default:
      log_message(MSG_DEBUG,"Autoconf : Unknown vct_channel->service_type 0x%02x\n",vct_channel->service_type);
      break;
    }

#ifdef LIBUCSI //used to decompress the atsc_text_descriptor
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
	  log_message(MSG_DEBUG, "Autoconf : Extended channel name descriptor, we try to decode long channel name\n");
	  dest8=(uint8_t *)long_name; //same type size, just the sign change but we don't care
	  //check 
	  delta_multiple_string_structure=PSIP_VCT_LEN+descriptor_delta+2;//+2 to skip descriptor tag and descriptor len
	  if (atsc_text_validate(((uint8_t*)(buf + delta_multiple_string_structure) ),
				 buf[PSIP_VCT_LEN+descriptor_delta+1]))
	    {
	      log_message(MSG_DEBUG, "Autoconf : Error when VALIDATING long channel name, we take the short one\n");
	    }
	  else
	    {
	      //If we have multiple strings for the channel name we ask people to report
	      if(buf[delta_multiple_string_structure]!=1 || buf[delta_multiple_string_structure+1+3] !=1)
		{
		  log_message(MSG_WARN, "Autoconf : !!!!! Please report : parsing of long name :  number strings : %d number segments : %d\n",
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
		  log_message(MSG_DEBUG, "Autoconf : Decoded long channel name : \"%s\"\n",dest8);
		  channel_name=long_name;	  
		}
	      else
		log_message(MSG_DEBUG, "Autoconf : Error when decoding long channel name, we take the short one\n");
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

  //************** We add this channel to the list of services *****************
  //we search if we already have service id
  new_service=autoconf_find_service_for_add(parameters->services,HILO(vct_channel->program_number)); //The service id IS the program number
  
  if(new_service)
    {
      log_message(MSG_DETAIL, "Autoconf : Adding new channel %s to the list of services , program number : 0x%x \n",
		  channel_name,
		  HILO(vct_channel->program_number));
      //we store the data
      new_service->id=HILO(vct_channel->program_number);
      new_service->running_status=0;
      new_service->type=mpeg2_service_type;
      new_service->free_ca_mode=vct_channel->access_controlled;
      log_message(MSG_DEBUG, "Autoconf : access_controlled : 0x%x\n", new_service->free_ca_mode);
      memcpy (new_service->name, channel_name, strlen(channel_name));
      new_service->name[strlen(channel_name)] = '\0';

    }

  //**************** Work done for this channel -- goodbye *********************
  return PSIP_VCT_LEN + HILO(vct_channel->descriptor_length); //We return the length

}


/********************************************************************
 * Autoconfiguration auto update
 ********************************************************************/

/** @Brief, tell if the pmt have a newer version than the one recorded actually
 * In the PMT pid there is a field to say if the PMT was updated
 * This function check if it has changed 
 *
 *@param channel the channel for which we have to check
 *@param buf : the received buffer
 */
int pmt_need_update(mumudvb_channel_t *channel, unsigned char *buf)
{
  pmt_t       *pmt=(pmt_t*)(buf+TS_HEADER_LEN);
  ts_header_t *header=(ts_header_t *)buf;

  if(header->payload_unit_start_indicator) //It's the beginning of a new packet
    if(pmt->version_number!=channel->pmt_version)
      {
	log_message(MSG_DEBUG,"Autoconfiguration : PMT version changed, channel %s . stored version : %d, new: %d. The CRC have to be checked\n",channel->name,channel->pmt_version,pmt->version_number);
	return 1;
      }
  return 0;

}


/** @brief update the version using the dowloaded pmt*/
void update_pmt_version(mumudvb_channel_t *channel)
{
  pmt_t       *pmt=(pmt_t*)(channel->pmt_packet->packet);
  if(channel->pmt_version!=pmt->version_number)
    log_message(MSG_INFO,"Autoconfiguration : New PMT version for channel %s. Old : %d, new: %d\n",channel->name,channel->pmt_version,pmt->version_number);

  channel->pmt_version=pmt->version_number;
}
