/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Conditionnal Access Modules support
 * 
 * (C) Brice DUBOST <mumudvb@braice.net>
 *
 * Parts of this code is from the VLC project, modified  for mumudvb
 * by Brice DUBOST 
 * 
 * Authors of the VLC part: Damien Lucas <nitrox@via.ecp.fr>
 *                          Johan Bilien <jobi@via.ecp.fr>
 *                          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
 *                          Christopher Ross <chris@tebibyte.org>
 *                          Christophe Massiot <massiot@via.ecp.fr>
 * 
 * Parts of this code come from libdvbpsi, modified for mumudvb
 * by Brice DUBOST 
 * Libdvb part : Copyright (C) 2000 Klaus Schmidinger
 * 
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
#include <sys/stat.h>
#include <sys/poll.h>

/* DVB Card Drivers */
#include <linux/dvb/version.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/ca.h>

#include "cam.h"
#include "mumudvb.h"

extern unsigned long       crc32_table[256];

int cam_parse_pmt(unsigned char *buf, mumudvb_pmt_t *pmt, struct ca_info *cai)
{
  //This function will join the 188 bytes packet until the PMT is full
  //Once it's full we check the CRC32 and say if it's ok or not
  //
  //There is two important mpeg2-ts fields to do that
  // * the continuity counter wich is incremented for each packet
  // * The payload_unit_start_indicator wich says if it's the first packet
  //
  //When a packet is cutted in 188 bytes packets, there should be no other pid between two sub packets
  //
  //Return 1 when the packet is full and OK


  ts_header_t *header;
  int ok=0;
  int parsed=0;
  int delta,pid;

  //mapping of the buffer onto the TS header
  header=(ts_header_t *)buf;
  pid=HILO(header->pid);

  //delta used to remove TS HEADER
  delta = TS_HEADER_LEN-1; 

  //Sometimes there is some more data in the header, the adaptation field say it
  if (header->adaptation_field_control & 0x2)
    {
      fprintf(stderr, "CAM : parse PMT : Adaptation field \n");
      delta += buf[delta] ;        // add adapt.field.len
    }
  else if (header->adaptation_field_control & 0x1)
    {
      if (buf[delta]==0x00 && buf[delta+1]==0x00 && buf[delta+2]==0x01) 
	{
	  // -- PES/PS                                                                                                                               
	  //tspid->id   = buf[j+3];                                                                                                                  
	  fprintf(stderr, "CAM : parse PMT : #PES/PS ----- We ignore \n");
	  ok=0;
	}
      else
	  ok=1;
    }

  if (header->adaptation_field_control == 3)
    ok=0;

  if(header->payload_unit_start_indicator) //It's the beginning of a new packet
    {
      if(ok)
	{
	  pmt->empty=0;
	  pmt->continuity_counter=header->continuity_counter;
	  pmt->pid=pid;
	  pmt->len=AddPacketStart(pmt->packet,buf+delta+1,188-delta-1); //on ajoute le paquet //NOTE len
	}
    }
  else if(header->payload_unit_start_indicator==0) //Not the first, we check if che already registered packet corresponds
    {
      // -- pid change in stream? (without packet start). This is not supported
      if (pmt->pid != pid)
	{
	  //fprintf(stderr,"CAM : PMT parse. ERROR : PID change\n");
	  pmt->empty=1;
	}
      // -- discontinuity error in packet ?                                                                                                          
      if  ((pmt->continuity_counter+1)%16 != header->continuity_counter) 
	{
	  fprintf(stderr,"CAM : PMT parse : Continuity ERROR\n\n");
	  pmt->empty=1;
	}
      pmt->continuity_counter=header->continuity_counter;
      pmt->len=AddPacketContinue(pmt->packet,buf+delta,188-delta,pmt->len); //on ajoute le paquet 

      //fprintf(stderr,"CAM : \t\t Len %d PMT_len %d\n",pmt->len,HILO(((pmt_t *)pmt->packet)->section_length));
      
      //We check if the PMT is full
      if (pmt->len > ((HILO(((pmt_t *)pmt->packet)->section_length))+3)) //+3 is for the header
      {
	//Yes, it's full, I check the CRC32 to say it's valid
	parsed=cam_ca_pmt_check_CRC(pmt); //TEST CRC32
      }
    }
  return parsed;
}



//Les fonctions qui permettent de coller les paquets les uns aux autres
// -- add TS data
// -- return: 0 = fail
int AddPacketStart (unsigned char *packet, unsigned char *buf, unsigned int len)
{
  memset(packet,0,4096);
  memcpy(packet,buf,len);
  return len;
}

int AddPacketContinue  (unsigned char *packet, unsigned char *buf, unsigned int len, unsigned int act_len)
{
  memcpy(packet+act_len,buf,len);
  return len+act_len;
}


//Checking of the CRC32
int cam_ca_pmt_check_CRC( mumudvb_pmt_t *pmt)
{
  pmt_t *pmt_struct;
  unsigned long crc32;
  int i;

  pmt_struct=(pmt_t *)pmt->packet;

  //the real lenght
  pmt->len=HILO(pmt_struct->section_length)+3; //+3 pour les trois bits du début (compris le section_lenght)

  //CRC32 calculation mostly taken from the xine project
  //Test of the crc32
  crc32=0xffffffff;
  //we compute the CRC32
  //we have two ways: either we compute untill the end and it should be 0
  //either we exclude the 4 last bits and in should be equal to the 4 last bits
  for(i = 0; i < pmt->len; i++) {
    crc32 = (crc32 << 8) ^ crc32_table[(crc32 >> 24) ^ pmt->packet[i]];
  }
  
  if(crc32!=0)
    {
      //Bad CRC32
      fprintf(stderr,"CAM : \tBAD CRC32 PID : %d\n", pmt->pid);
      return 0; //We don't send this PMT
    }
  
  return 1;

}


/****************************************************************************/
//Code from libdvb with commentaries added
//convert the PMT into CA_PMT
/****************************************************************************/

//convert the PMT descriptors
int convert_desc(struct ca_info *cai, 
		 uint8_t *out, uint8_t *buf, int dslen, uint8_t cmd,int quiet)
{
  int i, j, dlen, olen=0;
  int id;
  int bad_sysid=0;

  out[2]=cmd;                            //ca_pmt_cmd_id 01 ok_descrambling 02 ok_mmi 03 query 04 not_selected
  for (i=0; i<dslen; i+=dlen)           //loop on all the descriptors (for each descriptor we add its length)
    {
      dlen=buf[i+1]+2;                     //ca_descriptor len
      //if(!quiet)
      //fprintf(stderr,"CAM : \tDescriptor tag %d\n",buf[i]);
      if ((buf[i]==9)&&(dlen>2)&&(dlen+i<=dslen)) //here buf[i]=descriptor_tag (9=ca_descriptor)
	{
	  id=(buf[i+2]<<8)|buf[i+3];
	  for (j=0; j<cai->sys_num; j++)
	    if (cai->sys_id[j]==id) //does the system id supported by the cam ?
	      break; //yes --> we leave the loop
	  if (j==cai->sys_num) // do we leaved the loop just because we reached the end ?
	    {
	      if(!bad_sysid && !quiet)
		fprintf(stderr,"CAM : !!! The cam don't support the following system id : \nCAM : ");
	      if(!quiet && (bad_sysid!=id))
		fprintf(stderr,"%d 0x%x - ", id, id);
	      bad_sysid=id;
	      continue;          //yes, so we dont record this descriptor
	    }
	  memcpy(out+olen+3, buf+i, dlen); //good descriptor supported by the cam, we copy it
	  olen+=dlen; //output let
	}
    }
  olen=olen?olen+1:0; //if not empty we add one
  out[0]=(olen>>8);   //we write the program info_len
  out[1]=(olen&0xff);
  if (bad_sysid && !quiet)
    fprintf(stderr,"\nCAM :  Check if the good descrambling algorithms are selected\n");
  //if(!quiet)
  //fprintf(stderr,"CAM : \tOK CA descriptors len %d\n",olen);
  return olen+2;      //we return the total written len
}

int convert_pmt(struct ca_info *cai, mumudvb_pmt_t *pmt, 
		       uint8_t list, uint8_t cmd, int quiet)
{
	int slen, dslen, o, i;
	uint8_t *buf;
	uint8_t *out;
	int ds_convlen;

	if(!quiet)
	  fprintf(stderr,"CAM : \t===PMT convert into CA_PMT\n");


	pmt->need_descr=0;
	
	buf=pmt->packet;
	out=pmt->converted_packet;
	//slen=(((buf[1]&0x03)<<8)|buf[2])+3; //section len (deja contenu dans mon pmt)
	slen=pmt->len;
	out[0]=list;   //ca_pmt_list_mgmt 00 more 01 first 02 last 03 only 04 add 05 update
	out[1]=buf[3]; //program number and version number
	out[2]=buf[4]; //program number and version number
	out[3]=buf[5]; //program number and version number
	dslen=((buf[10]&0x0f)<<8)|buf[11]; //program_info_length
	ds_convlen=convert_desc(cai, out+4, buf+12, dslen, cmd, quiet); //new index : 4 + the descriptor size
	o=4+ds_convlen;
	if(ds_convlen>2)
	  pmt->need_descr=1;
	for (i=dslen+12; i<slen-9; i+=dslen+5) {      //we parse the part after the descriptors
	  dslen=((buf[i+3]&0x0f)<<8)|buf[i+4];        //ES_info_length
	  if ((buf[i]==0)||(buf[i]>4))                //stream_type
	    {
	      if(!quiet)
		fprintf(stderr,"CAM : \t=====Stream type throwed away : %d\n",buf[i]);
	      continue;
	    }
	  if(!quiet)
	    {
	      switch(buf[i]){
	      case 1:
	      case 2:
		fprintf(stderr,"CAM : \t=====Stream type : video\n");
		break;
	      case 3:
	      case 4:
		fprintf(stderr,"CAM : \t=====Stream type : audio\n");
		break;
	      default:
		fprintf(stderr,"CAM : \t=====Stream type : %d\n",buf[i]);
	      }
	    }

	  out[o++]=buf[i];                            //stream_type
	  out[o++]=buf[i+1];                          //reserved and elementary_pid
	  out[o++]=buf[i+2];                          //reserved and elementary_pid
	  //fprintf(stderr,"CAM : TEST, PID %d bytes : %d %x \n",((buf[i+1] & 0x1f)<<8) | buf[i+2]);
	  ds_convlen=convert_desc(cai, out+o, buf+i+5, dslen, cmd,quiet);//we look to the descriptors associated to this stream
	  o+=ds_convlen;
	  if(ds_convlen>2)
	    pmt->need_descr=1;
	}
	return o;
}

/****************************************************************************/
/* VLC part */
/****************************************************************************/


/*****************************************************************************
 * CAMOpen :
 *****************************************************************************/
int CAMOpen( access_sys_t * p_sys , int card, int device)
{
    char ca[128];
    int i_adapter, i_device;
    ca_caps_t caps;

    i_adapter = card;
    i_device = device;

    if( snprintf( ca, sizeof(ca), CA, i_adapter, i_device ) >= (int)sizeof(ca) )
    {
        fprintf(stderr,"CAM : snprintf() truncated string for CA" );
        ca[sizeof(ca) - 1] = '\0';
    }
    memset( &caps, 0, sizeof( ca_caps_t ));

    fprintf(stderr,"CAM : Opening device %s\n", ca );
    if( (p_sys->i_ca_handle = open(ca, O_RDWR | O_NONBLOCK)) < 0 )
    {
        fprintf(stderr, "CAMInit: opening CAM device failed (%s)\n",
                  strerror(errno) );
        p_sys->i_ca_handle = 0;
        return -666;
    }

    if ( ioctl( p_sys->i_ca_handle, CA_GET_CAP, &caps ) != 0 )
    {
        fprintf(stderr, "CAMInit: ioctl() error getting CAM capabilities\n" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return -666;
    }

    /* Output CA capabilities */
    fprintf(stderr, "CAMInit: CA interface with %d %s\n", caps.slot_num, 
        caps.slot_num == 1 ? "slot" : "slots" );
    if ( caps.slot_type & CA_CI )
        fprintf(stderr, "CAMInit: CI high level interface type\n" );
    if ( caps.slot_type & CA_CI_LINK )
        fprintf(stderr, "CAMInit: CI link layer level interface type\n" );
    if ( caps.slot_type & CA_CI_PHYS )
        fprintf(stderr, "CAMInit: CI physical layer level interface type (not supported) \n" );
    if ( caps.slot_type & CA_DESCR )
        fprintf(stderr, "CAMInit: built-in descrambler detected\n" );
    if ( caps.slot_type & CA_SC )
        fprintf(stderr, "CAMInit: simple smart card interface\n" );

    fprintf(stderr, "CAMInit: %d available %s\n", caps.descr_num,
        caps.descr_num == 1 ? "descrambler (key)" : "descramblers (keys)" );
    if ( caps.descr_type & CA_ECD )
        fprintf(stderr, "CAMInit: ECD scrambling system supported\n" );
    if ( caps.descr_type & CA_NDS )
        fprintf(stderr, "CAMInit: NDS scrambling system supported\n" );
    if ( caps.descr_type & CA_DSS )
        fprintf(stderr, "CAMInit: DSS scrambling system supported\n" );

    if ( caps.slot_num == 0 )
    {
        fprintf(stderr, "CAMInit: CAM module with no slots\n" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return -666;
    }

    if( caps.slot_type & CA_CI_LINK )
    {
        p_sys->i_ca_type = CA_CI_LINK;
    }
    else if( caps.slot_type & CA_CI )
    {
        p_sys->i_ca_type = CA_CI;
    }
    else {
        p_sys->i_ca_type = -1;
        fprintf(stderr, "CAMInit: incompatible CAM interface\n" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return -666;
    }

    p_sys->i_nb_slots = caps.slot_num;
    memset( p_sys->pb_active_slot, 0, sizeof(int) * MAX_CI_SLOTS );
    memset( p_sys->pb_slot_mmi_expected, 0, sizeof(int) * MAX_CI_SLOTS );
    memset( p_sys->pb_slot_mmi_undisplayed, 0,
            sizeof(int) * MAX_CI_SLOTS );

    return en50221_Init( p_sys );
}

/*****************************************************************************
 * CAMPoll :
 *****************************************************************************/
int CAMPoll( access_sys_t * p_sys )
{
    int i_ret = -666;

    if ( p_sys->i_ca_handle == 0 )
    {
        fprintf(stderr, "CAMPoll: Cannot Poll the CAM\n" );
        return -666;
    }

    switch( p_sys->i_ca_type )
    {
    case CA_CI_LINK:
        i_ret = en50221_Poll( p_sys );
        break;
    case CA_CI:
        i_ret = 0;
        break;
    default:
        fprintf(stderr, "CAMPoll: This should not happen" );
        break;
    }

    return i_ret;
}

/*****************************************************************************
 * CAMSet :
 *****************************************************************************/
int CAMSet( access_sys_t * p_sys, mumudvb_pmt_t *p_pmt )
{

    if( p_sys->i_ca_handle == 0 )
    {
      //dvbpsi_DeletePMT( p_pmt );
        return -666;
    }

    en50221_SetCAPMT( p_sys, p_pmt );

    return 0;
}

/*****************************************************************************
 * CAMClose :
 *****************************************************************************/
void CAMClose( access_sys_t * p_sys )
{

    en50221_End( p_sys );

    if ( p_sys->i_ca_handle )
    {
        close( p_sys->i_ca_handle );
    }
}

