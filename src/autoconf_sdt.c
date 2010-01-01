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
 *  @brief This file contain the code related to the SDT reading for autoconfiguration
 *
 */


#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "errors.h"
#include "mumudvb.h"
#include "autoconf.h"
#include "log.h"

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

extern int Interrupted;

void parse_sdt_descriptor(unsigned char *buf,int descriptors_loop_len, mumudvb_service_t *services);
void parse_service_descriptor(unsigned char *buf, mumudvb_service_t *services);
void autoconf_show_CA_identifier_descriptor(unsigned char *buf);
mumudvb_service_t *autoconf_find_service_for_add(mumudvb_service_t *services,int service_id);

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
      while((len-delta)>=(4+SDT_DESCR_LEN))
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
		case 4:  log_message(MSG_FLOOD, "running_status : running\t");  break; //too usual to be printed as debug
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
    else
      log_message(MSG_FLOOD, "Autoconf SDT descriptor_tag : 0x%2x\n", descriptor_tag);

    buf += descriptor_len;
    descriptors_loop_len -= descriptor_len;
  }
}

int convert_en399468_string(char *string, int max_len)
{

  int encoding_control_char=8; //cf encodings_en300468 
  char *dest;
  char *tempdest, *tempbuf;
  unsigned char *src;
  /* remove control characters and convert to UTF-8 the channel name */
  //If no channel encoding is specified, it seems that most of the broadcasters
  //uses ISO/IEC 8859-9. But the norm (EN 300 468) said that it should be Latin-1 (ISO/IEC 6937 + euro)

  //temporary buffers allocation
  tempdest=tempbuf=malloc(sizeof(char)*2*strlen(string));
  if(tempdest==NULL)
  {
    log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    Interrupted=ERROR_MEMORY<<8;
    return -1;
  }

  int len=0;
  for (src = (unsigned char *) string; *src; src++)
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

#ifdef HAVE_ICONV
  //Conversion to utf8
  iconv_t cd;
  //we open the conversion table
  cd = iconv_open( "UTF8", encodings_en300468[encoding_control_char] );

  size_t inSize, outSize=max_len;
  inSize=len;
  //pointers initialisation because iconv change them, we store
  dest=string;
  tempdest=tempbuf;
  //conversion
  iconv(cd, &tempdest, &inSize, &dest, &outSize );
  *dest = '\0';
  free(tempbuf);
  iconv_close( cd );
#else
  log_message(MSG_DETAIL, "Autoconf : Iconv not present, no name encoding conversion \n");
#endif

  log_message(MSG_FLOOD, "Autoconf : Converted name : \"%s\" (name encoding : %s)\n", string,encodings_en300468[encoding_control_char]);
  return encoding_control_char;

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

  int encoding_control_char;


  buf += 2;
  service->type=*buf;

  //We show the service type
  display_service_type(service->type, MSG_DEBUG);


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
  encoding_control_char=convert_en399468_string(service->name,MAX_NAME_LEN);
  if(encoding_control_char==-1)
    return;

  log_message(MSG_DEBUG, "Autoconf : service_name : \"%s\" (name encoding : %s)\n", service->name,encodings_en300468[encoding_control_char]);

}


/** @brief : show the contents of the CA identifier descriptor
 *
 * @param buf : the buffer containing the descriptor
 */
void autoconf_show_CA_identifier_descriptor(unsigned char *buf)
{

  int length,i,ca_id;

  log_message(MSG_DETAIL, "Autoconf : --- SDT descriptor --- CA identifier descriptor\nAutoconf : CA_system_ids : ");

  length=buf[1];
  for(i=0;i<length;i+=2)
  {
    ca_id=(buf[i]<<8)+buf[i+1];
    log_message( MSG_DETAIL,"Autoconf : Ca system id 0x%04x : %s\n",ca_id, ca_sys_id_to_str(ca_id));
  }
}


