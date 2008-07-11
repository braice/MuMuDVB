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

#include "errors.h"
#include "cam.h"
#include "ts.h"
#include "mumudvb.h"



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
  descr_ca_t *descriptor;


  out[2]=cmd;                            //ca_pmt_cmd_id 01 ok_descrambling 02 ok_mmi 03 query 04 not_selected
  for (i=0; i<dslen; i+=dlen)           //loop on all the descriptors (for each descriptor we add its length)
    {
      descriptor=(descr_ca_t *)(buf+i);
      dlen=descriptor->descriptor_length+2;
      //dlen=buf[i+1]+2;                     //ca_descriptor len
      //if(!quiet)
      //log_message( MSG_INFO,"CAM : \tDescriptor tag %d\n",buf[i]);
      if ((descriptor->descriptor_tag==9)&&(dlen>2)&&(dlen+i<=dslen)) //descriptor_tag (9=ca_descriptor)
	{
	  //id=(buf[i+2]<<8)|buf[i+3];
	  id=HILO(descriptor->CA_type);
	  for (j=0; j<cai->sys_num; j++)
	    if (cai->sys_id[j]==id) //does the system id supported by the cam ?
	      break; //yes --> we leave the loop
	  if (j==cai->sys_num) // do we leaved the loop just because we reached the end ?
	    {
	      if(!bad_sysid && !quiet)
		log_message( MSG_WARN,"CAM : !!! The cam don't support the following system id : \nCAM : ");
	      if(!quiet && (bad_sysid!=id))
		log_message( MSG_WARN,"%d 0x%x - ", id, id);
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
    log_message( MSG_WARN,"\nCAM :  Check if the good descrambling algorithms are selected\n");
  //if(!quiet)
  //log_message( MSG_INFO,"CAM : \tOK CA descriptors len %d\n",olen);
  return olen+2;      //we return the total written len
}

int convert_pmt(struct ca_info *cai, mumudvb_ts_packet_t *pmt, 
		       uint8_t list, uint8_t cmd, int quiet)
{
	int slen, dslen, o, i;
	uint8_t *buf;
	uint8_t *out;
	int ds_convlen;
	pmt_t *header;
	int program_info_length=0;
	pmt_info_t *descr_header;

	if(!quiet)
	  log_message( MSG_DEBUG,"CAM : \t===PMT convert into CA_PMT\n");

        header=(pmt_t *)pmt->packet;

        if(header->table_id!=0x02)
          {
            log_message( MSG_WARN,"CAM : == Packet PID %d is not a PMT PID\n", pmt->pid);
            return 1;
          }

	pmt->need_descr=0;
	
	buf=pmt->packet;
	out=pmt->converted_packet;
	//slen=(((buf[1]&0x03)<<8)|buf[2])+3; //section len (deja contenu dans mon pmt)
	slen=pmt->len;
	out[0]=list;   //ca_pmt_list_mgmt 00 more 01 first 02 last 03 only 04 add 05 update
	out[1]=buf[3]; //program number and version number
	out[2]=buf[4]; //program number and version number
	out[3]=buf[5]; //program number and version number

	//dslen=((buf[10]&0x0f)<<8)|buf[11]; //program_info_length
	program_info_length=HILO(header->program_info_length); //program_info_length

	ds_convlen=convert_desc(cai, out+4, buf+PMT_LEN, program_info_length, cmd, quiet); //new index : 4 + the descriptor size
	o=4+ds_convlen;
	if(ds_convlen>2)
	  pmt->need_descr=1;
	for (i=program_info_length+PMT_LEN; i<=slen-(PMT_INFO_LEN+4); i+=dslen+PMT_INFO_LEN) {      //we parse the part after the descriptors
	  descr_header=(pmt_info_t *)(pmt->packet+i);
	  dslen=HILO(descr_header->ES_info_length);        //ES_info_length
	  switch(descr_header->stream_type){
	  case 1:
	  case 2:
	    if(!quiet)
	      log_message( MSG_DEBUG,"CAM : \t=====Stream type : video\n");
	    break;
	  case 3:
	  case 4:
	    if(!quiet)
	      log_message( MSG_DEBUG,"CAM : \t=====Stream type : audio\n");
	    break;
	  case 0x06:
	    if(!quiet)
	      log_message( MSG_DEBUG,"CAM : \t=====Stream type : teletex, AC3 or subtitling, don(t kno if we need to descramble, dropped\n");
	    continue;
	    //break;
	  default:
	    if(!quiet)
	      log_message( MSG_DEBUG, "CAM : \t=====Stream type throwed away : 0x%02x\n",buf[i]);
	    continue;
	  }
	  
	  out[o++]=buf[i];                            //stream_type
	  out[o++]=buf[i+1];                          //reserved and elementary_pid
	  out[o++]=buf[i+2];                          //reserved and elementary_pid
	  //log_message( MSG_INFO,"CAM : TEST, PID %d bytes : %d %x \n",((buf[i+1] & 0x1f)<<8) | buf[i+2]);
	  ds_convlen=convert_desc(cai, out+o, buf+i+PMT_INFO_LEN, dslen, cmd,quiet);//we look to the descriptors associated to this stream
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

    if( snprintf( ca, sizeof(ca), CA_DEV, i_adapter, i_device ) >= (int)sizeof(ca) )
    {
        log_message( MSG_INFO,"CAM : snprintf() truncated string for CA" );
        ca[sizeof(ca) - 1] = '\0';
    }
    memset( &caps, 0, sizeof( ca_caps_t ));

    log_message( MSG_INFO,"CAM : Opening device %s\n", ca );
    if( (p_sys->i_ca_handle = open(ca, O_RDWR | O_NONBLOCK)) < 0 )
    {
        log_message( MSG_WARN, "CAMInit: opening CAM device failed (%s)\n",
                  strerror(errno) );
        p_sys->i_ca_handle = 0;
        return ERROR_CAM;
    }

    if ( ioctl( p_sys->i_ca_handle, CA_GET_CAP, &caps ) != 0 )
    {
        log_message( MSG_WARN, "CAMInit: ioctl() error getting CAM capabilities\n" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return ERROR_CAM;
    }

    /* Output CA capabilities */
    log_message( MSG_INFO, "CAMInit: CA interface with %d %s\n", caps.slot_num, 
        caps.slot_num == 1 ? "slot" : "slots" );
    if ( caps.slot_type & CA_CI )
        log_message( MSG_INFO, "CAMInit: CI high level interface type\n" );
    if ( caps.slot_type & CA_CI_LINK )
        log_message( MSG_INFO, "CAMInit: CI link layer level interface type\n" );
    if ( caps.slot_type & CA_CI_PHYS )
        log_message( MSG_INFO, "CAMInit: CI physical layer level interface type (not supported) \n" );
    if ( caps.slot_type & CA_DESCR )
        log_message( MSG_INFO, "CAMInit: built-in descrambler detected\n" );
    if ( caps.slot_type & CA_SC )
        log_message( MSG_INFO, "CAMInit: simple smart card interface\n" );

    log_message( MSG_INFO, "CAMInit: %d available %s\n", caps.descr_num,
        caps.descr_num == 1 ? "descrambler (key)" : "descramblers (keys)" );
    if ( caps.descr_type & CA_ECD )
        log_message( MSG_INFO, "CAMInit: ECD scrambling system supported\n" );
    if ( caps.descr_type & CA_NDS )
        log_message( MSG_INFO, "CAMInit: NDS scrambling system supported\n" );
    if ( caps.descr_type & CA_DSS )
        log_message( MSG_INFO, "CAMInit: DSS scrambling system supported\n" );

    if ( caps.slot_num == 0 )
    {
        log_message( MSG_WARN, "CAMInit: CAM module with no slots\n" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return ERROR_CAM;
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
        log_message( MSG_WARN, "CAMInit: incompatible CAM interface\n" );
        close( p_sys->i_ca_handle );
        p_sys->i_ca_handle = 0;
        return ERROR_CAM;
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
    int i_ret = ERROR_CAM;

    if ( p_sys->i_ca_handle == 0 )
    {
        log_message( MSG_INFO, "CAMPoll: Cannot Poll the CAM\n" );
        return ERROR_CAM;
    }

    switch( p_sys->i_ca_type )
    {
    case CA_CI_LINK:
        i_ret = en50221_Poll( p_sys );
        break;
    case CA_CI:
        i_ret = 0;
        log_message( MSG_WARN, "CAMPoll: CAM link type not supported\n" );
        break;
    default:
        log_message( MSG_WARN, "CAMPoll: This should not happen\n" );
        break;
    }

    return i_ret;
}

/*****************************************************************************
 * CAMSet :
 *****************************************************************************/
int CAMSet( access_sys_t * p_sys, mumudvb_ts_packet_t *p_pmt )
{

    if( p_sys->i_ca_handle == 0 )
    {
      //dvbpsi_DeletePMT( p_pmt );
        return ERROR_CAM;
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

