/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Conditionnal Access Modules support
 * 
 * (C) 2004-2008 Brice DUBOST <mumudvb@braice.net>
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
 * Copyright (C) 1998-2005 the VideoLAN team
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


#ifdef LIBDVBEN50221
#include <libucsi/section.h>
#endif

#include "errors.h"
#include "cam.h"
#include "ts.h"
#include "mumudvb.h"



#ifndef LIBDVBEN50221
/****************************************************************************/
//Code from libdvbpsi, adapted and with commentaries added
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
 * CAMSet : //TODO use this function
 *****************************************************************************/
int CAMSet( access_sys_t * p_sys, mumudvb_ts_packet_t *p_pmt )
{

    if( p_sys->i_ca_handle == 0 )
    {
      //dvbpsi_DeletePMT( p_pmt );
        return ERROR_CAM;
    }

    en50221_SetCAPMT( p_sys, p_pmt , NULL); //TODO replace null by channels

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

#else
/*****************************************************************************
 * Code for dealing with cam using libdvben50221
 *****************************************************************************/
static void *camthread_func(void* arg); //The polling thread
static int mumudvb_cam_ai_callback(void *arg, uint8_t slot_id, uint16_t session_number,
			   uint8_t application_type, uint16_t application_manufacturer,
			   uint16_t manufacturer_code, uint8_t menu_string_length,
				   uint8_t *menu_string); //The application information callback
static int mumudvb_cam_ca_info_callback(void *arg, uint8_t slot_id, uint16_t session_number, uint32_t ca_id_count, uint16_t *ca_ids);
static int mumudvb_cam_app_ca_pmt_reply_callback(void *arg,
                                                  uint8_t slot_id,
                                                  uint16_t session_number,
                                                  struct en50221_app_pmt_reply *reply,
                                                  uint32_t reply_size);


static int mumudvb_cam_mmi_menu_callback(void *arg, uint8_t slot_id, uint16_t session_number,
					 struct en50221_app_mmi_text *title,
					 struct en50221_app_mmi_text *sub_title,
					 struct en50221_app_mmi_text *bottom,
					 uint32_t item_count, struct en50221_app_mmi_text *items,
					 uint32_t item_raw_length, uint8_t *items_raw);


static int mumudvb_cam_mmi_close_callback(void *arg, uint8_t slot_id, uint16_t session_number,
					  uint8_t cmd_id, uint8_t delay);



static int mumudvb_cam_mmi_display_control_callback(void *arg, uint8_t slot_id, uint16_t session_number,
						    uint8_t cmd_id, uint8_t mmi_mode);

static int mumudvb_cam_mmi_enq_callback(void *arg, uint8_t slot_id, uint16_t session_number,
					uint8_t blind_answer, uint8_t expected_answer_length,
					uint8_t *text, uint32_t text_size);


int cam_start(cam_parameters_t *cam_params, int adapter_id)
{
  // create transport layer
  cam_params->tl = en50221_tl_create(1, 16);
  if (cam_params->tl == NULL) {
    log_message( MSG_ERROR,"ERROR : CAM : Failed to create transport layer\n");
    return 1;
  }

  // create session layer
  cam_params->sl = en50221_sl_create(cam_params->tl, 16);
  if (cam_params->sl == NULL) {
    log_message( MSG_ERROR, "ERROR : CAM : Failed to create session layer\n");
    en50221_tl_destroy(cam_params->tl);
    return 1;
  }

  // create the stdcam instance
  cam_params->stdcam = en50221_stdcam_create(adapter_id, cam_params->cam_number, cam_params->tl, cam_params->sl);
  if (cam_params->stdcam == NULL) {
    log_message( MSG_ERROR, "ERROR : CAM : Failed to create the stdcam instance (no cam present ?)\n");
    en50221_sl_destroy(cam_params->sl);
    en50221_tl_destroy(cam_params->tl);
    return 1;
  }

  // hook up the AI callbacks
  if (cam_params->stdcam->ai_resource) {
    en50221_app_ai_register_callback(cam_params->stdcam->ai_resource, mumudvb_cam_ai_callback, cam_params->stdcam);
  }

  // hook up the CA callbacks
  if (cam_params->stdcam->ca_resource) {
    en50221_app_ca_register_info_callback(cam_params->stdcam->ca_resource, mumudvb_cam_ca_info_callback, cam_params);
    en50221_app_ca_register_pmt_reply_callback(cam_params->stdcam->ca_resource, mumudvb_cam_app_ca_pmt_reply_callback, cam_params);
  }


  
  // hook up the MMI callbacks
  if (cam_params->stdcam->mmi_resource) {
    en50221_app_mmi_register_close_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_close_callback, cam_params);
    en50221_app_mmi_register_display_control_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_display_control_callback, cam_params);
    en50221_app_mmi_register_enq_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_enq_callback, cam_params);
    en50221_app_mmi_register_menu_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_menu_callback, cam_params);
    en50221_app_mmi_register_list_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_menu_callback, cam_params);
  } else {
    fprintf(stderr, "CAM Menus are not supported by this interface hardware\n");
    exit(1);
  }
  

  // any other stuff
  cam_params->moveca = 1; //see http://www.linuxtv.org/pipermail/linux-dvb/2007-May/018198.html
  // start the cam thread
  pthread_create(&(cam_params->camthread), NULL, camthread_func, cam_params);
  return 0;
}

void cam_stop(cam_parameters_t *cam_params)
{
  if (cam_params->stdcam == NULL)
    return;

  // shutdown the cam thread
  cam_params->camthread_shutdown = 1;
  pthread_join(cam_params->camthread, NULL);

  // destroy the stdcam
  if (cam_params->stdcam->destroy)
    cam_params->stdcam->destroy(cam_params->stdcam, 1);

  // destroy session layer
  en50221_sl_destroy(cam_params->sl);

  // destroy transport layer
  en50221_tl_destroy(cam_params->tl);


}

static void *camthread_func(void* arg)
{
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  while(!cam_params->camthread_shutdown) { 
    usleep(100000); //some waiting
    cam_params->stdcam->poll(cam_params->stdcam);
  }

  return 0;
}






int mumudvb_cam_new_pmt(cam_parameters_t *cam_params, mumudvb_ts_packet_t *cam_pmt_ptr)
{
  uint8_t capmt[4096];
  int size;

  // parse section
  struct section *section = section_codec(cam_pmt_ptr->packet,cam_pmt_ptr->len);
  if (section == NULL) {
    log_message( MSG_WARN,"CAM : section_codec parsing error\n");
    return -1;
  }

  // parse section_ext
  struct section_ext *section_ext = section_ext_decode(section, 0);
  if (section_ext == NULL) {
    log_message( MSG_WARN,"CAM : section_ext parsing error\n");
    return -1;
  }

#if 0
  if ((section_ext->table_id_ext != cam_pmt_ptr->i_program_number) || //program number "already checked" by the pmt pid attribution
      (section_ext->version_number == cam_params->ca_pmt_version)) { //cam_pmt_version allow to see if there is new information, not implemented for the moment (to be attached to the channel)
    return;
  }
#endif

  // parse PMT
  struct mpeg_pmt_section *pmt = mpeg_pmt_section_codec(section_ext);
  if (pmt == NULL) {
    log_message( MSG_WARN,"CAM : mpeg_pmt_section_codec parsing error\n");
    return -1;
  }

  if(pmt->head.table_id!=0x02)
    {
      log_message( MSG_WARN,"CAM : == Packet PID %d is not a PMT PID\n", cam_pmt_ptr->pid);
      return 1;
    }


  if (cam_params->stdcam == NULL)
    return -1;

  if (cam_params->ca_resource_connected) {
    log_message( MSG_INFO, "CAM : Received new PMT - sending to CAM...\n");

    // translate it into a CA PMT
    int listmgmt = CA_LIST_MANAGEMENT_ONLY;
    if (cam_params->seenpmt) {
      listmgmt = CA_LIST_MANAGEMENT_UPDATE;
    }
    cam_params->seenpmt = 1;

    if ((size = en50221_ca_format_pmt(pmt, capmt, sizeof(capmt), cam_params->moveca, listmgmt,
				      CA_PMT_CMD_ID_OK_DESCRAMBLING)) < 0) {
      //CA_PMT_CMD_ID_QUERY)) < 0) {// We don't do query, My cam (powercam PRO) never give good answers
      log_message( MSG_WARN, "Failed to format PMT\n");
      return -1;
    }

    // set it
    if (en50221_app_ca_pmt(cam_params->stdcam->ca_resource, cam_params->stdcam->ca_session_number, capmt, size)) {
      log_message( MSG_WARN, "Failed to send PMT\n");
      return -1;
    }

    // we've seen this PMT
    return 1;
  }

  return 0;
}



static int mumudvb_cam_ai_callback(void *arg, uint8_t slot_id, uint16_t session_number,
			   uint8_t application_type, uint16_t application_manufacturer,
			   uint16_t manufacturer_code, uint8_t menu_string_length,
			   uint8_t *menu_string)
{
  (void) arg;
  (void) slot_id;
  (void) session_number;

  log_message( MSG_INFO, "CAM Application type: %02x\n", application_type);
  log_message( MSG_INFO, "CAM Application manufacturer: %04x\n", application_manufacturer);
  log_message( MSG_INFO, "CAM Manufacturer code: %04x\n", manufacturer_code);
  log_message( MSG_INFO, "CAM Menu string: %.*s\n", menu_string_length, menu_string);

  return 0;
}

static int mumudvb_cam_ca_info_callback(void *arg, uint8_t slot_id, uint16_t session_number, uint32_t ca_id_count, uint16_t *ca_ids)
{
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  (void) slot_id;
  (void) session_number;

  log_message( MSG_INFO, "CAM supports the following ca system ids:\n");
  uint32_t i;
  for(i=0; i< ca_id_count; i++) {
    log_message( MSG_INFO, "  0x%04x\n", ca_ids[i]);
  }
  cam_params->ca_resource_connected = 1; 
  return 0;
}



static int mumudvb_cam_app_ca_pmt_reply_callback(void *arg,
                                                  uint8_t slot_id,
                                                  uint16_t session_number,
                                                  struct en50221_app_pmt_reply *reply,
                                                  uint32_t reply_size)
{

  struct en50221_app_pmt_stream *pos;
  (void) arg;
  (void) slot_id;
  (void) session_number;
  log_message( MSG_INFO, "CAM PMT reply\n");
  log_message( MSG_INFO, "  Program number %d\n",reply->program_number);

  switch(reply->CA_enable)
    {
    case CA_ENABLE_DESCRAMBLING_POSSIBLE:
      log_message( MSG_INFO,"   Descrambling possible\n");
      break;
    case CA_ENABLE_DESCRAMBLING_POSSIBLE_PURCHASE:
      log_message( MSG_INFO,"   Descrambling possible under conditions (purchase dialogue)\n");
      break;
    case CA_ENABLE_DESCRAMBLING_POSSIBLE_TECHNICAL:
      log_message( MSG_INFO,"   Descrambling possible under conditions (technical dialogue)\n");
      break;
    case CA_ENABLE_DESCRAMBLING_NOT_POSSIBLE_NO_ENTITLEMENT:
      log_message( MSG_INFO,"   Descrambling not possible (because no entitlement)\n");
      break;
    case CA_ENABLE_DESCRAMBLING_NOT_POSSIBLE_TECHNICAL:
      log_message( MSG_INFO,"   Descrambling not possible (for technical reasons)\n");
      break;
    default:
      log_message( MSG_INFO,"   RFU\n");
    }


  en50221_app_pmt_reply_streams_for_each(reply, pos, reply_size)
    {
      log_message( MSG_INFO, "   ES pid %d\n",pos->es_pid);
      switch(pos->CA_enable)
	{
	case CA_ENABLE_DESCRAMBLING_POSSIBLE:
	  log_message( MSG_INFO,"     Descrambling possible\n");
	  break;
	case CA_ENABLE_DESCRAMBLING_POSSIBLE_PURCHASE:
	  log_message( MSG_INFO,"     Descrambling possible under conditions (purchase dialogue)\n");
	  break;
	case CA_ENABLE_DESCRAMBLING_POSSIBLE_TECHNICAL:
	  log_message( MSG_INFO,"     Descrambling possible under conditions (technical dialogue)\n");
	  break;
	case CA_ENABLE_DESCRAMBLING_NOT_POSSIBLE_NO_ENTITLEMENT:
	  log_message( MSG_INFO,"     Descrambling not possible (because no entitlement)\n");
	  break;
	case CA_ENABLE_DESCRAMBLING_NOT_POSSIBLE_TECHNICAL:
	  log_message( MSG_INFO,"     Descrambling not possible (for technical reasons)\n");
	  break;
	default:
	  log_message( MSG_INFO,"     RFU\n");
	}
    }

  return 0;
}


/*******************************
 * MMI
 *******************************/

static int mumudvb_cam_mmi_menu_callback(void *arg, uint8_t slot_id, uint16_t session_number,
                                   struct en50221_app_mmi_text *title,
                                   struct en50221_app_mmi_text *sub_title,
                                   struct en50221_app_mmi_text *bottom,
                                   uint32_t item_count, struct en50221_app_mmi_text *items,
                                   uint32_t item_raw_length, uint8_t *items_raw)
{
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  (void) slot_id;
  (void) session_number;
  (void) item_raw_length;
  (void) items_raw;

  log_message( MSG_INFO, "--- CAM MENU ----------------\n");

  if (title->text_length) {
    log_message( MSG_INFO, "%.*s\n", title->text_length, title->text);
  }
  if (sub_title->text_length) {
    log_message( MSG_INFO, "%.*s\n", sub_title->text_length, sub_title->text);
  }

  uint32_t i;
  for(i=0; i< item_count; i++) {
    log_message( MSG_INFO, "%.*s\n", items[i].text_length, items[i].text);
  }

  if (bottom->text_length) {
    log_message( MSG_INFO, "%.*s\n", bottom->text_length, bottom->text);
  }
  fflush(stdout);

  cam_params->mmi_state = MMI_STATE_MENU;

  //We leave
  en50221_app_mmi_answ(cam_params->stdcam->mmi_resource, cam_params->stdcam->mmi_session_number,
		       MMI_ANSW_ID_CANCEL, NULL, 0);


  return 0;
}



static int mumudvb_cam_mmi_close_callback(void *arg, uint8_t slot_id, uint16_t session_number,
                                    uint8_t cmd_id, uint8_t delay)
{
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  (void) slot_id;
  (void) session_number;
  (void) cmd_id;
  (void) delay;

  // note: not entirely correct as its supposed to delay if asked
  cam_params->mmi_state = MMI_STATE_CLOSED;
  return 0;
}

static int mumudvb_cam_mmi_display_control_callback(void *arg, uint8_t slot_id, uint16_t session_number,
                                              uint8_t cmd_id, uint8_t mmi_mode)
{
  struct en50221_app_mmi_display_reply_details reply;
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  (void) slot_id;

  // don't support any commands but set mode
  if (cmd_id != MMI_DISPLAY_CONTROL_CMD_ID_SET_MMI_MODE) {
    en50221_app_mmi_display_reply(cam_params->stdcam->mmi_resource, session_number,
				  MMI_DISPLAY_REPLY_ID_UNKNOWN_CMD_ID, &reply);
    return 0;
  }

  // we only support high level mode
  if (mmi_mode != MMI_MODE_HIGH_LEVEL) {
    en50221_app_mmi_display_reply(cam_params->stdcam->mmi_resource, session_number,
				  MMI_DISPLAY_REPLY_ID_UNKNOWN_MMI_MODE, &reply);
    return 0;
  }

  // ack the high level open
  reply.u.mode_ack.mmi_mode = mmi_mode;
  en50221_app_mmi_display_reply(cam_params->stdcam->mmi_resource, session_number,
				MMI_DISPLAY_REPLY_ID_MMI_MODE_ACK, &reply);
  cam_params->mmi_state = MMI_STATE_OPEN;
  return 0;
}

static int mumudvb_cam_mmi_enq_callback(void *arg, uint8_t slot_id, uint16_t session_number,
                                  uint8_t blind_answer, uint8_t expected_answer_length,
                                  uint8_t *text, uint32_t text_size)
{
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  (void) slot_id;
  (void) session_number;

  log_message( MSG_INFO, "ENQ");
  log_message( MSG_INFO, "%.*s: ", text_size, text);

  cam_params->mmi_enq_blind = blind_answer;
  cam_params->mmi_enq_length = expected_answer_length;
  cam_params->mmi_state = MMI_STATE_ENQ;

  //We leave
  en50221_app_mmi_answ(cam_params->stdcam->mmi_resource, cam_params->stdcam->mmi_session_number,
		       MMI_ANSW_ID_CANCEL, NULL, 0);

  return 0;
}















#endif
