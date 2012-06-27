/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for Autoconfiguration
 *
 * (C) 2008-2011 Brice DUBOST <mumudvb@braice.net>
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

static char *log_module="Autoconf: ";

extern int Interrupted;
void parse_nit_descriptors(unsigned char *buf,int descriptors_loop_len);
void parse_nit_ts_descriptor(unsigned char *buf,int ts_descriptors_loop_len, mumudvb_channel_t *channels, int number_of_channels);
int convert_en399468_string(char *string, int max_len);
void parse_network_name_descriptor(unsigned char *buf);
void parse_multilingual_network_name_descriptor(unsigned char *buf);
void parse_lcn_descriptor(unsigned char *buf, mumudvb_channel_t *channels, int number_of_channels);
void parse_service_list_descriptor_descriptor(unsigned char *buf);
void parse_satellite_delivery_system_descriptor(unsigned char *buf);
void parse_terrestrial_delivery_system_descriptor(unsigned char *buf);

/** @brief Read the network information table (cf EN 300 468)
 *
 */
int autoconf_read_nit(autoconf_parameters_t *parameters, mumudvb_channel_t *channels, int number_of_channels)
{
  mumudvb_ts_packet_t *nit_mumu;
  unsigned char *buf=NULL;

  //We get the packet
  nit_mumu=parameters->autoconf_temp_nit;
  buf=nit_mumu->data_full;
  nit_t       *header=(nit_t*)(buf);

  //We look only for the following table Ox40 : network_information_section - actual_network
  if (header->table_id != 0x40)
  {
    log_message( log_module, MSG_FLOOD,"NIT :  Bad table %d\n", header->table_id);
    return 1;
  }

  /*current_next_indicator – A 1-bit indicator, which when set to '1' indicates that the Program Association Table
  sent is currently applicable. When the bit is set to '0', it indicates that the table sent is not yet applicable
  and shall be the next table to become valid.*/
  if(header->current_next_indicator == 0)
  {
    log_message( log_module, MSG_FLOOD,"NIT not yet valid, we get a new one (current_next_indicator == 0)\n");
    return 1;
  }


  log_message( log_module, MSG_DEBUG, "-- NIT : Network Information Table --\n");

  log_message( log_module, MSG_DEBUG, "Network id 0x%02x\n", HILO(header->network_id));
  int network_descriptors_length = HILO(header->network_descriptor_length);

  //Loop over different descriptors in the NIT
  buf+=NIT_LEN;

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
      log_message( log_module, MSG_DEBUG, " --- NIT descriptor --- descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
      break;
    }

    //The service descriptor provides the names of the service provider and the service in text form together with the service_type.
    if(descriptor_tag==0x40)
      parse_network_name_descriptor(buf);
    else if(descriptor_tag==0x5B)
      parse_multilingual_network_name_descriptor(buf);
    else
      log_message( log_module, MSG_FLOOD, "NIT descriptor_tag : 0x%2x\n", descriptor_tag);

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
    log_message( log_module, MSG_FLOOD, " --- NIT ts_descriptors_loop_len %d descriptors_loop_len %d\n", ts_descriptors_loop_len, descriptors_loop_len);
    ts_id=HILO(descr_header->transport_stream_id);
    log_message( log_module, MSG_DEBUG, " --- NIT descriptor concerning the multiplex %d\n", ts_id);
    buf +=NIT_TS_LEN;
    ts_descriptors_loop_len -= (descriptors_loop_len+NIT_TS_LEN);
    while (descriptors_loop_len > 0)
    {
      unsigned char descriptor_tag = buf[0];
      unsigned char descriptor_len = buf[1] + 2;

      if (!descriptor_len)
      {
        log_message( log_module, MSG_DEBUG, " --- NIT descriptor --- descriptor_tag == 0x%02x, len is 0\n", descriptor_tag);
        break;
      }
      if(descriptor_tag==0x83)
	parse_lcn_descriptor(buf, channels, number_of_channels);
      else if(descriptor_tag==0x41)
        parse_service_list_descriptor_descriptor(buf);
      else if(descriptor_tag==0x43)
        parse_satellite_delivery_system_descriptor(buf);
      else if(descriptor_tag==0x5A)
        parse_terrestrial_delivery_system_descriptor(buf);
      else
        log_message( log_module, MSG_FLOOD, " --- NIT descriptor --- descriptor_tag == 0x%02x len %d descriptors_loop_len %d ------------\n", descriptor_tag, descriptor_len, descriptors_loop_len);
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

  log_message( log_module, MSG_FLOOD, "NIT Multilingual network name descriptor  0x%02x len %d\n",descriptor_tag,descriptor_len);

  while (descriptor_len > 0)
  {
    language_code[0]=*buf;buf++;
    language_code[1]=*buf;buf++;
    language_code[2]=*buf;buf++;
    language_code[3]='\0';
    name_len=*buf;buf++;
    log_message( log_module, MSG_FLOOD, "NIT network descriptor_len %d, name_len %d\n",descriptor_len , name_len);
    dest=malloc(sizeof(char)*(name_len+1));
    memcpy (dest, buf, name_len);
    dest[name_len] = '\0';
    buf += name_len;
    convert_en399468_string(dest,name_len);
    log_message( log_module, MSG_DEBUG, "lang code %s network name : \"%s\"\n",language_code, dest);
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

  log_message( log_module, MSG_DEBUG, "NIT network name descriptor \n");
  log_message( log_module, MSG_FLOOD, "NIT network descriptor_len %d\n",descriptor_len);
  dest=malloc(sizeof(char)*(descriptor_len+1));
  memcpy (dest, buf, descriptor_len);
  dest[descriptor_len] = '\0';
  convert_en399468_string(dest,descriptor_len);
  log_message( log_module, MSG_DEBUG, "network name : \"%s\"\n", dest);
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
  log_message( log_module, MSG_DEBUG, "NIT  0x83 descriptor (probably LCN) \n");
  log_message( log_module, MSG_FLOOD, "NIT  0x83 descriptor (probably LCN) descriptor_len %d\n",descriptor_len);

  while (descriptor_len > 0)
  {
    lcn=(nit_lcn_t *)buf;
    buf+=NIT_LCN_LEN;
    service_id= HILO(lcn->service_id);
    i_lcn=HILO(lcn->logical_channel_number);
    log_message( log_module, MSG_DEBUG, "NIT LCN channel number %d, service id %d visible %d\n",i_lcn ,service_id, lcn->visible_service_flag);
    for(curr_channel=0;curr_channel<number_of_channels;curr_channel++)
    {
      if(channels[curr_channel].service_id==service_id)
      {
	log_message( log_module, MSG_DETAIL, "NIT LCN channel FOUND id %d, LCN %d name \"%s\"\n",service_id,i_lcn, channels[curr_channel].name);
	channels[curr_channel].logical_channel_number=i_lcn;
      }
    }
    descriptor_len -= NIT_LCN_LEN;
    }
}




void parse_service_list_descriptor_descriptor(unsigned char *buf)
{
  /* Service list descriptor :
     descriptor_tag                     8
     descriptor_length                  8
     for (i=0;i<N;I++){
       service_id                       8
       service_type                     16
     }
   */

  int i,service_id,service_type;
  unsigned char descriptor_len = buf[1];
  buf += 2;
  log_message( log_module, MSG_DETAIL, "--- NIT descriptor --- Service list descriptor\n");
  for(i=0;i<descriptor_len;i+=3)
  {
    service_id=(buf[i]<<8)+buf[i+1];;
    service_type=buf[i+2];
    log_message( log_module, MSG_DETAIL, "Service ID : 0x%02x service type: 0x%02x : %s \n",service_id, service_type, service_type_to_str(service_type));
  }
  log_message( log_module, MSG_DETAIL, "--- descriptor done ---\n");
}


/** 
  */
/** @brief display the contents of satellite_delivery_system_descriptor
  * EN 300 468 V1.10.1   6.2.13.2 Satellite delivery system descriptor
  */
void parse_satellite_delivery_system_descriptor(unsigned char *buf)
{
  descr_sat_delivery_t *descr;
  descr=(descr_sat_delivery_t *)buf;

  log_message( log_module, MSG_DETAIL, "--- NIT descriptor --- satellite delivery system descriptor\n");

  // The frequency is a 32-bit field giving the 4-bit BCD values specifying 8 characters of the frequency value.
  log_message( log_module, MSG_DETAIL, "Frequency: %x%02x%02x.%02x MHz", descr->frequency_4, descr->frequency_3, descr->frequency_2, descr->frequency_1);
  log_message( log_module, MSG_DETAIL, "Orbital position: %d%01d,%01d°", descr->orbital_position_hi,(descr->orbital_position_lo>>4)&0x0f, descr->orbital_position_lo&0x0f);
  if(descr->west_east_flag)
    log_message( log_module, MSG_DETAIL, "Estern position");
  else
    log_message( log_module, MSG_DETAIL, "Western position");
  switch(descr->polarization)
  {
    log_message( log_module, MSG_DETAIL, "Polarization: (0x%02x)", descr->polarization);
    case 0:
      log_message( log_module, MSG_DETAIL, "Polarization: linear - horizontal");
      break;
    case 1:
      log_message( log_module, MSG_DETAIL, "Polarization: linear - vertical");
      break;
    case 2:
      log_message( log_module, MSG_DETAIL, "Polarization: circular - left");
      break;
    case 3:
      log_message( log_module, MSG_DETAIL, "Polarization: circular - right");
      break;
    default:
      log_message( log_module, MSG_DETAIL, "Polarization: BUG");
      break;
  }
  if(descr->modulation_system)
    log_message( log_module, MSG_DETAIL, "Modulation system: DVB-S2");
  else
    log_message( log_module, MSG_DETAIL, "Modulation system: DVB-S");
  if(descr->modulation_system) {
	switch(descr->roll_off) {
		case 0:
		  log_message( log_module, MSG_DETAIL, "Roll-off factor: α = 0,35");
		  break;
		case 1:
		  log_message( log_module, MSG_DETAIL, "Roll-off factor: α = 0,25");
		  break;
		case 2:
		  log_message( log_module, MSG_DETAIL, "Roll-off factor: α = 0,20");
		  break;
		case 3:
		  log_message( log_module, MSG_DETAIL, "Roll-off factor: reserved");
		  break;
		default:
		  log_message( log_module, MSG_DETAIL, "Roll-off factor: BUG");
		  break;
	}
  }
  switch(descr->modulation_type)
  {
    case 0:
      log_message( log_module, MSG_DETAIL, "Constellation: Auto");
      break;
    case 1:
      log_message( log_module, MSG_DETAIL, "Constellation: QPSK");
      break;
    case 2:
      log_message( log_module, MSG_DETAIL, "Constellation: 8PSK");
      break;
    case 3:
      log_message( log_module, MSG_DETAIL, "Constellation: 16-QAM");
      break;
    default:
      log_message( log_module, MSG_DETAIL, "Constellation: BUG");
      break;
  }

  log_message( log_module, MSG_DETAIL, "Symbol rate: %d%d%d,%d%d%d%d Msymbol/s", BCDHI(descr->symbol_rate_12), BCDLO(descr->symbol_rate_12), BCDHI(descr->symbol_rate_34), BCDLO(descr->symbol_rate_34), BCDHI(descr->symbol_rate_56), BCDLO(descr->symbol_rate_56),  BCDLO(descr->symbol_rate_7) );

  switch(descr->FEC_inner)
  {
    case 0:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: not defined");
      break;
    case 1:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 1/2");
      break;
    case 2:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 2/3");
      break;
    case 3:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 3/4");
      break;
    case 4:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 5/6");
      break;
    case 5:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 7/8");
      break;
    case 6:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 8/9");
      break;
    case 7:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 3/5");
      break;
    case 8:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 4/5");
      break;
    case 9:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: 9/10");
      break;
    case 10:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: Reserved for future use");
      break;
    case 11:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: Reserved for future use");
      break;
    case 12:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: no convolutional coding");
      break;
    default:
      log_message( log_module, MSG_DETAIL, "Inner FEC scheme: BUG please contact");
      break;
  }
  log_message( log_module, MSG_DETAIL, "--- descriptor done ---\n");
}


/** @brief display the contents of terrestrial_delivery_system_descriptor
  * EN 300 468 V1.10.1   6.2.13.4 Terrestrial delivery system descriptor
  */
void parse_terrestrial_delivery_system_descriptor(unsigned char *buf)
{
  descr_terr_delivery_t *descr;
  descr=(descr_terr_delivery_t *)buf;

  log_message( log_module, MSG_DETAIL, "--- NIT descriptor --- terrestrial delivery system descriptor\n");

  log_message( log_module, MSG_DETAIL, "Frequency: %ld Hz", ((descr->frequency_4<<24)+(descr->frequency_3<<16)+(descr->frequency_2<<8)+descr->frequency_1) *10 );
  if(descr->bandwidth<=3)
    log_message( log_module, MSG_DETAIL, "Bandwidth: %d MHz",8-descr->bandwidth);
  else
    log_message( log_module, MSG_DETAIL, "Bandwidth: Reserved for future use");
  if(descr->priority)
    log_message( log_module, MSG_DETAIL, "Priority: HP (high priority)");
  else
    log_message( log_module, MSG_DETAIL, "Priority: LP (low priority)");
  log_message( log_module, MSG_DETAIL, "Time_Slicing_indicator: %d",descr->Time_Slicing_indicator);
  log_message( log_module, MSG_DETAIL, "MPE_FEC_indicator: %d",descr->MPE_FEC_indicator );
  switch(descr->constellation)
  {
    case 0:
      log_message( log_module, MSG_DETAIL, "Constellation: QPSK");
      break;
    case 1:
      log_message( log_module, MSG_DETAIL, "Constellation: 16-QAM");
      break;
    case 2:
      log_message( log_module, MSG_DETAIL, "Constellation: 64-QAM");
      break;
    case 3:
      log_message( log_module, MSG_DETAIL, "Constellation: RFU");
      break;
    default:
      log_message( log_module, MSG_DETAIL, "Constellation: BUG");
      break;
  }
  switch(descr->hierarchy_information)
  {
    case 0:
      log_message( log_module, MSG_DETAIL, "hierarchy_information: non-hierarchical, native interleaver");
      break;
    case 1:
      log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 1, native interleaver");
      break;
    case 2:
      log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 2, native interleaver");
      break;
    case 3:
      log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 4, native interleaver");
      break;
    case 4:
      log_message( log_module, MSG_DETAIL, "hierarchy_information: non-hierarchical, in-depth interleaver");
      break;
    case 5:
      log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 1, in-depth interleaver");
      break;
    case 6:
      log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 2, in-depth interleaver");
      break;
    case 7:
      log_message( log_module, MSG_DETAIL, "hierarchy_information: α = 4, in-depth interleaver");
      break;
    default:
      log_message( log_module, MSG_DETAIL, "hierarchy_information: BUG please contact");
      break;
  }

  switch(descr->code_rate_HP_stream)
  {
    case 0:
      log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: 1/2");
      break;
    case 1:
      log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: 2/3");
      break;
    case 2:
      log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: 3/4");
      break;
    case 3:
      log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: 5/6");
      break;
    case 4:
      log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: 7/8");
      break;
    case 5:
    case 6:
    case 7:
    default:
      log_message( log_module, MSG_DETAIL, "code_rate_HP_stream: RFU");
      break;
  }
  switch(descr->code_rate_LP_stream)
  {
    case 0:
      log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: 1/2");
      break;
    case 1:
      log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: 2/3");
      break;
    case 2:
      log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: 3/4");
      break;
    case 3:
      log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: 5/6");
      break;
    case 4:
      log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: 7/8");
      break;
    case 5:
    case 6:
    case 7:
    default:
      log_message( log_module, MSG_DETAIL, "code_rate_LP_stream: RFU");
      break;
  }


  switch(descr->guard_interval)
  {
    case 0:
      log_message( log_module, MSG_DETAIL, "guard_interval: 1/32");
      break;
    case 1:
      log_message( log_module, MSG_DETAIL, "guard_interval: 1/16");
      break;
    case 2:
      log_message( log_module, MSG_DETAIL, "guard_interval: 1/8");
      break;
    case 3:
      log_message( log_module, MSG_DETAIL, "guard_interval: 1/4");
      break;
    default:
      log_message( log_module, MSG_DETAIL, "guard_interval: BUG");
      break;
  }

  switch(descr->transmission_mode)
  {
    case 0:
      log_message( log_module, MSG_DETAIL, "transmission_mode: 2k");
      break;
    case 1:
      log_message( log_module, MSG_DETAIL, "transmission_mode: 8k");
      break;
    case 2:
      log_message( log_module, MSG_DETAIL, "transmission_mode: 4k");
      break;
    case 3:
      log_message( log_module, MSG_DETAIL, "transmission_mode: RFU");
      break;
    default:
      log_message( log_module, MSG_DETAIL, "transmission_mode: BUG");
      break;
  }

  if(descr->other_frequency_flag)
    log_message( log_module, MSG_DETAIL, "other_frequency_flag: one or more other frequencies are in use");
  else
    log_message( log_module, MSG_DETAIL, "other_frequency_flag: no other frequency is in use");

  log_message( log_module, MSG_DETAIL, "--- descriptor done ---\n");
}

