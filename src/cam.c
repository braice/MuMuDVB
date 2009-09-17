/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Conditionnal Access Modules support
 * 
 * (C) 2004-2009 Brice DUBOST <mumudvb@braice.net>
 *
 * The latest version can be found at http://mumudvb.braice.net
 *
 * Code inspired by libdvben50221 examples from dvb apps
 * Copyright (C) 2004, 2005 Manu Abraham <abraham.manu@gmail.com>
 * Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)
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

/**@file
 * @brief cam support
 * 
 * Code for talking with conditionnal acces modules. This code uses the libdvben50221 from dvb-apps
 */

#ifdef LIBDVBEN50221
#include <libucsi/section.h>


#include "errors.h"
#include "cam.h"
#include "ts.h"
#include "mumudvb.h"




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

static char *static_nom_fich_cam_info;


/**********************************Debug ******************************/
#ifdef CAMDEBUG

/**
 * The types of CA interface we support.
 */

#define DVBCA_INTERFACE_LINK 0
#define DVBCA_INTERFACE_HLCI 1

struct en50221_stdcam_llci {
  struct en50221_stdcam stdcam;

  int cafd;
  int slotnum;
  int state;
/** //I dont need these ones
  struct llci_resource resources[RESOURCE_IDS_COUNT];

  struct en50221_transport_layer *tl;
  struct en50221_session_layer *sl;
  struct en50221_app_send_functions sendfuncs;
  int tl_slot_id;

  struct en50221_app_rm *rm_resource;

  struct en50221_app_datetime *datetime_resource;
  int datetime_session_number;
  uint8_t datetime_response_interval;
  time_t datetime_next_send;
  time_t datetime_dvbtime;
 */

};

void cam_reset_cam(cam_parameters_t *cam_params)
{
  struct en50221_stdcam *stdcam=cam_params->stdcam;
  struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) stdcam;
  ioctl(llci->cafd, CA_RESET, (1 << llci->slotnum));
  //This variable only exist for low level CAMs so we check the type
  if(cam_params->cam_type==DVBCA_INTERFACE_LINK)
    llci->state = EN50221_STDCAM_CAM_NONE;
 
}

/**
 * States a CAM in a slot can be in.
 */

#define DVBCA_CAMSTATE_MISSING 0
#define DVBCA_CAMSTATE_INITIALISING 1
#define DVBCA_CAMSTATE_READY 2

int cam_debug_dvbca_get_cam_state(cam_parameters_t *cam_params)
{
  struct en50221_stdcam *stdcam=cam_params->stdcam;
  struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) stdcam;

  ca_slot_info_t info;
  info.num = llci->slotnum;

  if (ioctl(llci->cafd, CA_GET_SLOT_INFO, &info))
    return -1;

  if (info.flags == 0)
    return DVBCA_CAMSTATE_MISSING;
  if (info.flags & CA_CI_MODULE_READY)
    return DVBCA_CAMSTATE_READY;
  if (info.flags & CA_CI_MODULE_PRESENT)
    return DVBCA_CAMSTATE_INITIALISING;

  return -1;
}





int cam_debug_dvbca_get_interface_type(cam_parameters_t *cam_params)
{
  struct en50221_stdcam *stdcam=cam_params->stdcam;
  struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) stdcam;

  ca_slot_info_t info;

  info.num = llci->slotnum;

  if (ioctl(llci->cafd, CA_GET_SLOT_INFO, &info))
    return -1;

  if (info.type & CA_CI_LINK)
    return DVBCA_INTERFACE_LINK;
  if (info.type & CA_CI)
    return DVBCA_INTERFACE_HLCI;

  return -1;
}


#endif
/***************************end of debug ******************************/

/** @brief start the cam
 * This function will create the communication layers and set the callbacks*/
int cam_start(cam_parameters_t *cam_params, int adapter_id, char *nom_fich_cam_info)
{
  // Copy the filename pointer into a static local pointer to be accessible from other threads
  static_nom_fich_cam_info=nom_fich_cam_info;

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
  } else {
    log_message( MSG_WARN,  "CAM : WARNING : No Application Information ressource\n");
  }

  // hook up the CA callbacks
  if (cam_params->stdcam->ca_resource) {
    en50221_app_ca_register_info_callback(cam_params->stdcam->ca_resource, mumudvb_cam_ca_info_callback, cam_params);
    en50221_app_ca_register_pmt_reply_callback(cam_params->stdcam->ca_resource, mumudvb_cam_app_ca_pmt_reply_callback, cam_params);
  } else {
    log_message( MSG_WARN,  "CAM : WARNING : No CA ressource\n");
  }


  
  // hook up the MMI callbacks
  if (cam_params->stdcam->mmi_resource) {
    en50221_app_mmi_register_close_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_close_callback, cam_params);
    en50221_app_mmi_register_display_control_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_display_control_callback, cam_params);
    en50221_app_mmi_register_enq_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_enq_callback, cam_params);
    en50221_app_mmi_register_menu_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_menu_callback, cam_params);
    en50221_app_mmi_register_list_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_menu_callback, cam_params);
  } else {
    log_message( MSG_WARN,  "CAM Menus are not supported by this interface hardware\n");
  }


  // any other stuff
  cam_params->moveca = 1; //see http://www.linuxtv.org/pipermail/linux-dvb/2007-May/018198.html
  /**********************************Debug ******************************/
#ifdef CAMDEBUG
  cam_params->cam_type = cam_debug_dvbca_get_interface_type(cam_params); //The reset procedure have only been tested on LLCI cams
  switch(cam_params->cam_type)
  {
    case DVBCA_INTERFACE_LINK: 
      log_message( MSG_DETAIL,  "CAM CAM type : low level interface\n");
      break;
    case DVBCA_INTERFACE_HLCI: 
      log_message( MSG_DETAIL,  "CAM CAM type : HIGH level interface\n");
      break;
  }
#endif
  /***************************end of debug ******************************/
  // start the cam thread
  pthread_create(&(cam_params->camthread), NULL, camthread_func, cam_params);
  return 0;
}

/**@brief Stops the CAM*/
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





#define MAX_WAIT_AFTER_RESET 30

/** @brief The thread for polling the cam */
static void *camthread_func(void* arg)
{
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
#ifdef CAMDEBUG
  int i;
  int camstate;
#endif 
  while(!cam_params->camthread_shutdown) { 
    usleep(500000); //some waiting
    cam_params->stdcam->poll(cam_params->stdcam);
    /**********************************Debug ******************************/
#ifdef CAMDEBUG
    if(cam_params->need_reset==1)
    {
      log_message( MSG_DETAIL,  "CAM We force the reset of the CAM\n");
      cam_reset_cam(cam_params);
      i=0;
      log_message( MSG_DEBUG,  "CAM We wait for the cam to be INITIALISING\n");
      do
      {
        camstate=cam_debug_dvbca_get_cam_state(cam_params);
        switch(camstate) 
        {
          case DVBCA_CAMSTATE_MISSING:
            log_message( MSG_DEBUG,  "CAM cam state : DVBCA_CAMSTATE_MISSING\n");
            break;
          case DVBCA_CAMSTATE_READY:
            log_message( MSG_DEBUG,  "CAM cam state : DVBCA_CAMSTATE_READY\n");
            break;
          case DVBCA_CAMSTATE_INITIALISING:
            log_message( MSG_DEBUG,  "CAM cam state : DVBCA_CAMSTATE_INITIALISING\n");
            break;
          case -1:
            log_message( MSG_DEBUG,  "CAM cam state : Eroor during the query\n");
            break;
        }
        usleep(10000);
        i++;
      } while(camstate!=DVBCA_CAMSTATE_INITIALISING && i < MAX_WAIT_AFTER_RESET);
      if(i==MAX_WAIT_AFTER_RESET)
        log_message( MSG_DEBUG,  "CAM The CAM isn't in a good state after reset, it will probably don't work :(\n");
      cam_params->need_reset=0;
      cam_params->reset_counts++;
    }
#endif
    /***************************end of debug ******************************/
  }

  return 0;
}






/** @brief PMT sending to the cam
 * This function if called when mumudvb receive a new PMT pid. 
 * This function will ask the cam to decrypt the associated channel
 */
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

  // parse PMT
  struct mpeg_pmt_section *pmt = mpeg_pmt_section_codec(section_ext);
  if (pmt == NULL) {
    log_message( MSG_WARN,"CAM : mpeg_pmt_section_codec parsing error\n");
    return -1;
  }

  if(pmt->head.table_id!=0x02)
    {
      log_message( MSG_WARN,"CAM : == Packet PID %d is not a PMT PID\n", cam_pmt_ptr->pid);
      return -1;
    }


  if (cam_params->stdcam == NULL)
    return -1;

  if (cam_params->ca_resource_connected) {
    log_message( MSG_INFO, "CAM : Received new PMT - sending to CAM...\n");

    // translate it into a CA PMT 
    // Concerning the list managment the simplest (since we don't want to remove channels is to do a CA_LIST_MANAGEMENT_ADD 
    //Always. Doing FIRST, MORE ,MORE ... LAST is more complicated because the CAM will wait for the LAST
    // If the an update is needed the Aston cams will be happy with a ADD (it detects that the channel is already present and updates
    //It seems that the power cam don't really follow the norm (ie accept almost everything)
    // Doing also only update should work
    if ((size = en50221_ca_format_pmt(pmt, capmt, sizeof(capmt), cam_params->moveca, CA_LIST_MANAGEMENT_UPDATE, //an update should be equivalent to an add when the channel is not present
				      CA_PMT_CMD_ID_OK_DESCRAMBLING)) < 0) {

      /*CA_PMT_CMD_ID_QUERY)) < 0) {
	We don't do query, the query is never working very well. This is because the CAM cannot ask the card if 
	you have the rights for the channel. So this answer is often not reliable.

	Much thanks to Aston www.aston-france.com for the explanation
      */
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

  // Write information to log
  log_message( MSG_INFO, "CAM Application type: %02x\n", application_type);
  log_message( MSG_INFO, "CAM Application manufacturer: %04x\n", application_manufacturer);
  log_message( MSG_INFO, "CAM Manufacturer code: %04x\n", manufacturer_code);
  log_message( MSG_INFO, "CAM Menu string: %.*s\n", menu_string_length, menu_string);

  // Try to append the information to the cam_info log file
  FILE *file_cam_info;
  file_cam_info = fopen (static_nom_fich_cam_info, "a");
  if (file_cam_info == NULL)
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   static_nom_fich_cam_info, strerror (errno));
    }
  else
    {
      fprintf (file_cam_info,"CAM_Application_Type=%02x\n",application_type);
      fprintf (file_cam_info,"CAM_Application_Manufacturer=%04x\n",application_manufacturer);
      fprintf (file_cam_info,"CAM_Manufacturer_Code=%04x\n",manufacturer_code);
      fprintf (file_cam_info,"CAM_Menu_String=%.*s\n",menu_string_length, menu_string);
      fclose (file_cam_info);
    }  

  return 0;
}

static int mumudvb_cam_ca_info_callback(void *arg, uint8_t slot_id, uint16_t session_number, uint32_t ca_id_count, uint16_t *ca_ids)
{
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  (void) slot_id;
  (void) session_number;

  // Write information to log
  log_message( MSG_DETAIL, "CAM supports the following ca system ids:\n");
  uint32_t i;
  for(i=0; i< ca_id_count; i++) {
    display_ca_sys_id(ca_ids[i]); //we display it with the description
  }


  // Try to append the information to the cam_info log file
  FILE *file_cam_info;
  file_cam_info = fopen (static_nom_fich_cam_info, "a");
  if (file_cam_info == NULL)
    {
      log_message( MSG_WARN,
		   "%s: %s\n",
		   static_nom_fich_cam_info, strerror (errno));
    }
  else
    {
      for(i=0; i< ca_id_count; i++) 
	fprintf (file_cam_info,"ID_CA_Supported=%04x\n",ca_ids[i]);

      fclose (file_cam_info);
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
  //  (void) session_number;
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

  cam_params->stdcam->mmi_session_number=session_number;
  //We leave

  en50221_app_mmi_menu_answ(cam_params->stdcam->mmi_resource, cam_params->stdcam->mmi_session_number, 0);


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

  log_message( MSG_INFO, "--- CAM MENU ----CLOSED-------\n");

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
