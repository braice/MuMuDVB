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



extern int Interrupted;
static char *log_module="Autoconf: ";

void parse_sdt_descriptor(unsigned char *buf,int descriptors_loop_len, mumudvb_service_t *services);
void parse_service_descriptor(unsigned char *buf, mumudvb_service_t *services);
void autoconf_show_country_avaibility_descriptor(unsigned char *buf);
mumudvb_service_t *autoconf_find_service_for_add(mumudvb_service_t *services,int service_id);

extern char *encodings_en300468[];

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

  /*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
  sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
  and shall be the next table to become valid.*/
  if(header->current_next_indicator == 0)
  {
    log_message( log_module, MSG_FLOOD,"SDT not yet valid, we get a new one (current_next_indicator == 0)\n");
    return 0;
  }

  //We look only for the following table
  //0x42 service_description_section - actual_transport_stream
  if(header->table_id==0x42)
  {
    log_message( log_module, MSG_DEBUG, "-- SDT : Service Description Table (id 0x%02x)--\n",header->table_id);

    log_message( log_module, MSG_FLOOD, "-- SDT: TSID %d Original network id %d version %d section number %d last section number %d  --\n",
                  HILO(header->transport_stream_id),
                  HILO(header->original_network_id),
                  header->version_number,
                  header->section_number,
                  header->last_section_number);
    //Loop over different services in the SDT
    delta=SDT_LEN;
    while((len-delta)>=(4+SDT_DESCR_LEN))
    {
      descr_header=(sdt_descr_t *)(buf +delta );
      //we search if we already have service id
      new_service=autoconf_find_service_for_add(services,HILO(descr_header->service_id));
      if(new_service)
      {
        log_message( log_module, MSG_DEBUG, "We discovered a new service, service_id : 0x%x\n", HILO(descr_header->service_id));
        //For information only
        switch(descr_header->running_status)
        {
          case 0:
            log_message( log_module, MSG_DEBUG, "\trunning_status : undefined\n");  break;
          case 1:
            log_message( log_module, MSG_DEBUG, "\trunning_status : not running\n");  break;
          case 2:
            log_message( log_module, MSG_DEBUG, "\trunning_status : starts in a few seconds\n");  break;
          case 3:
            log_message( log_module, MSG_DEBUG, "\trunning_status : pausing\n");  break;
          case 4:  log_message( log_module, MSG_FLOOD, "\trunning_status : running\n");  break; //too usual to be printed as debug
          case 5:
            log_message( log_module, MSG_DEBUG, "\trunning_status : service off-air\n");  break;
        }
        //we store the data
        new_service->id=HILO(descr_header->service_id);
        new_service->running_status=descr_header->running_status;
        new_service->free_ca_mode=descr_header->free_ca_mode;
        log_message( log_module, MSG_DEBUG, "\tfree_ca_mode : 0x%x\n", descr_header->free_ca_mode);
        //We read the descriptor
        parse_sdt_descriptor(buf+delta+SDT_DESCR_LEN,HILO(descr_header->descriptors_loop_length),new_service);
      }
    delta+=HILO(descr_header->descriptors_loop_length)+SDT_DESCR_LEN;
    }
  }
  else
    log_message( log_module, MSG_FLOOD, "-- SDT : bad table id 0x%02x--\n",header->table_id);
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

  while (descriptors_loop_len > 0)
  {
    unsigned char descriptor_tag = buf[0];
    unsigned char descriptor_len = buf[1] + 2;

    if (!descriptor_len)
    {
      log_message( log_module, MSG_DEBUG, "--- SDT descriptor --- descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
      break;
    }

    //The service descriptor provides the names of the service provider and the service in text form together with the service_type.
    if(descriptor_tag==0x48)
      parse_service_descriptor(buf,service);
    else if( descriptor_tag==0x53) //53 : CA identifier descriptor. This descriptor contains the CA_systems_id (the scrambling algorithms)
      show_CA_identifier_descriptor(buf);
    else if( descriptor_tag==0x49) //0x49 : Country availability descriptor.
      ts_display_country_avaibility_descriptor(log_module,buf);
    else /** @todo : Add descriptor 0x50 Component descriptor (multilingual 0x5E)*/
       /** @todo : Add descriptor 0x5D  multilingual_service_name_descriptor*/
      log_message( log_module, MSG_FLOOD, "SDT descriptor_tag : 0x%2x, descriptor_len %d\n", descriptor_tag, descriptor_len);

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

  int encoding_control_char;


  buf += 2;
  if(service)
    service->type=*buf;

  //We show the service type
  display_service_type(*buf, MSG_DEBUG,log_module);


  buf ++; //we skip the service_type
  len = *buf; //provider name len

  //we jump the provider_name + the provider_name_len
  buf += len + 1;

  //Channel name len
  len = *buf;
  buf++;  //we jump the channel_name_len

  char *name;
  char tempbuf[MAX_NAME_LEN];
  if(service)
    name=service->name;
  else
    name=tempbuf;

  //We store the channel name with the raw encoding
  memcpy (name, buf, len);
  name[len] = '\0';
  encoding_control_char=convert_en300468_string(name,MAX_NAME_LEN);
  if(encoding_control_char==-1)
    return;

  log_message( log_module, MSG_DEBUG, "service_name : \"%s\" (name encoding : %s)\n", name,encodings_en300468[encoding_control_char]);
}







