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
 *  @brief This file contain the code related to the NIT reading for autoconfiguration
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
void parse_nit_descriptors(unsigned char *buf,int descriptors_loop_len);
void parse_nit_ts_descriptor(unsigned char *buf,int ts_descriptors_loop_len, mumudvb_channel_t *channels, int number_of_channels);
int convert_en399468_string(char *string, int max_len);
void parse_network_name_descriptor(unsigned char *buf);
void parse_multilingual_network_name_descriptor(unsigned char *buf);
void parse_lcn_descriptor(unsigned char *buf, mumudvb_channel_t *channels, int number_of_channels);

/** @brief Read the network information table (cf EN 300 468)
 *
 */
int autoconf_read_nit(autoconf_parameters_t *parameters, mumudvb_channel_t *channels, int number_of_channels)
{
  mumudvb_ts_packet_t *nit_mumu;
  int delta=0;
  unsigned char *buf=NULL;

  //We get the packet
  nit_mumu=parameters->autoconf_temp_nit;
  buf=nit_mumu->packet;
  nit_t       *header=(nit_t*)(buf);

  //We look only for the following table Ox40 : network_information_section - actual_network
  if (header->table_id != 0x40)
  {
    log_message(MSG_FLOOD,"Autoconf : NIT :  Bad table %d\n", header->table_id);
    return 1;
  }


  log_message(MSG_DEBUG, "Autoconf : -- NIT : Network Information Table --\n");

  log_message(MSG_DEBUG, "Autoconf : Network id 0x%x\n", HILO(header->network_id));
  int network_descriptors_length = HILO(header->network_descriptor_length);

  //Loop over different descriptors in the NIT
  buf+=NIT_LEN;
  delta=0;

  //We read the descriptors
  parse_nit_descriptors(buf,network_descriptors_length);
  buf += network_descriptors_length;
  nit_mid_t *middle=(nit_mid_t *)buf;
  int ts_loop_length=HILO(middle->transport_stream_loop_length);
  buf +=SIZE_NIT_MID;
  parse_nit_ts_descriptor(buf,ts_loop_length, channels, number_of_channels);
  return 0;
}

/** @brief Parse the NIT Network descriptors
 * Loop over the sdt descriptors and call other parsing functions if necessary
 * @param buf the buffer containing the descriptors
 * @param descriptors_loop_len the len of buffer containing the descriptors
 * @param service the associated service
 */
void parse_nit_descriptors(unsigned char *buf,int descriptors_loop_len)
{

  while (descriptors_loop_len > 0)
  {
    unsigned char descriptor_tag = buf[0];
    unsigned char descriptor_len = buf[1] + 2;

    if (!descriptor_len)
    {
      log_message(MSG_DEBUG, " Autoconf : --- NIT descriptor --- descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
      break;
    }

    //The service descriptor provides the names of the service provider and the service in text form together with the service_type.
    if(descriptor_tag==0x40)
      parse_network_name_descriptor(buf);
    else if(descriptor_tag==0x5B)
      parse_multilingual_network_name_descriptor(buf);
    else
      log_message(MSG_FLOOD, "Autoconf NIT descriptor_tag : 0x%2x\n", descriptor_tag);

    buf += descriptor_len;
    descriptors_loop_len -= descriptor_len;
  }
}

void parse_nit_ts_descriptor(unsigned char* buf, int ts_descriptors_loop_len, mumudvb_channel_t* channels, int number_of_channels)
{
  int descriptors_loop_len;
  nit_ts_t *descr_header;
  int ts_id;
  while (ts_descriptors_loop_len > 0)
  {
    descr_header=(nit_ts_t *)(buf);
    descriptors_loop_len=HILO(descr_header->transport_descriptors_length);
    log_message(MSG_FLOOD, " Autoconf : --- NIT ts_descriptors_loop_len %d descriptors_loop_len %d\n", ts_descriptors_loop_len, descriptors_loop_len); 
    ts_id=HILO(descr_header->transport_stream_id);
    log_message(MSG_DEBUG, " Autoconf : --- NIT descriptor concerning the multiplex %d\n", ts_id);
    buf +=NIT_TS_LEN;
    ts_descriptors_loop_len -= (descriptors_loop_len+NIT_TS_LEN);
    while (descriptors_loop_len > 0)
    {
      unsigned char descriptor_tag = buf[0];
      unsigned char descriptor_len = buf[1] + 2;

      if (!descriptor_len)
      {
        log_message(MSG_DEBUG, " Autoconf : --- NIT descriptor --- descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
        break;
      }
      if(descriptor_tag==0x83)
	parse_lcn_descriptor(buf, channels, number_of_channels);
      else
        log_message(MSG_FLOOD, " Autoconf : --- NIT descriptor --- descriptor_tag == 0x%02x len %d descriptors_loop_len %d\n", descriptor_tag, descriptor_len, descriptors_loop_len);
      buf += descriptor_len;
      descriptors_loop_len -= descriptor_len;
    }

  }
}



/** @brief Parse the network name descriptor
 * It's used to get the network name
 * @param buf the buffer containing the descriptor
 */
void parse_multilingual_network_name_descriptor(unsigned char *buf)
{
  /* Service descriptor : 
     descriptor_tag			8
     descriptor_length			8
     for (i=0;i<N;I++){
       ISO_639_language_code		24
       network_name_length		8
       for (i=0;i<N;I++){
         char				8
       }
     }
   */
  char *dest;
  unsigned char descriptor_tag = buf[0];
  unsigned char descriptor_len = buf[1];
  int name_len;
  char language_code[4];
  buf += 2;

  log_message(MSG_FLOOD, "Autoconf NIT Multilingual network name descriptor  0x%x len %d\n",descriptor_tag,descriptor_len);

  while (descriptor_len > 0)
  {
    language_code[0]=*buf;buf++;
    language_code[1]=*buf;buf++;
    language_code[2]=*buf;buf++;
    language_code[3]='\0';
    name_len=*buf;buf++;
    log_message(MSG_FLOOD, "Autoconf NIT network descriptor_len %d, name_len %d\n",descriptor_len , name_len);
    dest=malloc(sizeof(char)*(name_len+1));
    memcpy (dest, buf, name_len);
    dest[name_len] = '\0';
    buf += name_len;
    convert_en399468_string(dest,name_len);
    log_message(MSG_DEBUG, "Autoconf : lang code %s network name : \"%s\"\n",language_code, dest);
    descriptor_len -= (name_len+4);
    free(dest);
    }

}


/** @brief Parse the network name descriptor
 * It's used to get the network name
 * @param buf the buffer containing the descriptor
 */
void parse_network_name_descriptor(unsigned char *buf)
{
  /* Service descriptor : 
     descriptor_tag			8
     descriptor_length			8
     for (i=0;i<N;I++){
       char				8
     }
   */
  char *dest;
  unsigned char descriptor_len = buf[1];
  buf += 2;

  log_message(MSG_DEBUG, "Autoconf NIT network name descriptor \n");
  log_message(MSG_FLOOD, "Autoconf NIT network descriptor_len %d\n",descriptor_len);
  dest=malloc(sizeof(char)*(descriptor_len+1));
  memcpy (dest, buf, descriptor_len);
  dest[descriptor_len] = '\0';
  convert_en399468_string(dest,descriptor_len);
  log_message(MSG_DEBUG, "Autoconf : network name : \"%s\"\n", dest);
  free(dest);

}


/** @brief Parse the lcn descriptor
 * It's used to get the logical channel number
 * @param buf the buffer containing the descriptor
 */
void parse_lcn_descriptor(unsigned char* buf, mumudvb_channel_t* channels, int number_of_channels)
{
  /* Service descriptor : 
     descriptor_tag			8
     descriptor_length			8
     for (i=0;i<N;I++){
       service_id			16
       visible_service_flag		1
       reserved				5
       logical_channel_number		10
     }
   */

  nit_lcn_t *lcn;
  int descriptor_len = buf[1];
  buf += 2;
  int service_id, i_lcn, curr_channel;
  log_message(MSG_DEBUG, "Autoconf NIT  0x83 descriptor (probably LCN) \n");
  log_message(MSG_FLOOD, "Autoconf NIT  0x83 descriptor (probably LCN) descriptor_len %d\n",descriptor_len);

  while (descriptor_len > 0)
  {
    lcn=(nit_lcn_t *)buf;
    buf+=NIT_LCN_LEN;
    service_id= HILO(lcn->service_id);
    i_lcn=HILO(lcn->logical_channel_number);
    log_message(MSG_DEBUG, "Autoconf NIT LCN channel number %d, service id %d visible %d\n",i_lcn ,service_id, lcn->visible_service_flag);
    for(curr_channel=0;curr_channel<number_of_channels;curr_channel++)
    {
      if(channels[curr_channel].service_id==service_id)
      {
	log_message(MSG_DETAIL, "Autoconf NIT LCN channel FOUND id %d, LCN %d name \"%s\"\n",service_id,i_lcn, channels[curr_channel].name);
	channels[curr_channel].logical_channel_number=i_lcn;
      }
    }
    descriptor_len -= NIT_LCN_LEN;
    }
}