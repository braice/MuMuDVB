/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for Conditionnal Access Modules support
 * 
 * (C) 2004-2011 Brice DUBOST <mumudvb@braice.net>
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

#include <libucsi/section.h>


#include "errors.h"
#include "cam.h"
#include "ts.h"
#include "mumudvb.h"
#include "log.h"

static char *log_module="CAM: ";

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


static int mumudvb_cam_mmi_list_callback(void *arg, uint8_t slot_id, uint16_t session_number,
					 struct en50221_app_mmi_text *title,
					 struct en50221_app_mmi_text *sub_title,
					 struct en50221_app_mmi_text *bottom,
					 uint32_t item_count, struct en50221_app_mmi_text *items,
					 uint32_t item_raw_length, uint8_t *items_raw);

					 
static int mumudvb_cam_mmi_menu_list_callback(void *arg, uint8_t slot_id, uint16_t session_number,
                                   struct en50221_app_mmi_text *title,
                                   struct en50221_app_mmi_text *sub_title,
                                   struct en50221_app_mmi_text *bottom,
                                   uint32_t item_count, struct en50221_app_mmi_text *items,
                                   uint32_t item_raw_length, uint8_t *items_raw, int object_type);
					 

static int mumudvb_cam_mmi_close_callback(void *arg, uint8_t slot_id, uint16_t session_number,
					  uint8_t cmd_id, uint8_t delay);



static int mumudvb_cam_mmi_display_control_callback(void *arg, uint8_t slot_id, uint16_t session_number,
						    uint8_t cmd_id, uint8_t mmi_mode);

static int mumudvb_cam_mmi_enq_callback(void *arg, uint8_t slot_id, uint16_t session_number,
					uint8_t blind_answer, uint8_t expected_answer_length,
					uint8_t *text, uint32_t text_size);

static char *cam_status[] ={
      "EN50221_STDCAM_CAM_NONE",
      "EN50221_STDCAM_CAM_INRESET",
      "EN50221_STDCAM_CAM_OK",
      "EN50221_STDCAM_CAM_BAD"
};


/** @brief Read a line of the configuration file to check if there is a cam parameter
 *
 * @param cam_vars the cam parameters
 * @param substring The currrent line
 */
int read_cam_configuration(cam_parameters_t *cam_vars, mumudvb_channel_t *current_channel, int ip_ok, char *substring)
{
  char delimiteurs[] = CONFIG_FILE_SEPARATOR;
  if (!strcmp (substring, "cam_support"))
  {
    substring = strtok (NULL, delimiteurs);
    cam_vars->cam_support = atoi (substring);
    if(cam_vars->cam_support)
    {
      log_message( log_module,  MSG_WARN,
                   "You have enabled the support for conditionnal acces modules (scrambled channels). Please report any bug/comment\n");
    }
  }
  else if (!strcmp (substring, "cam_reask_interval"))
  {
    substring = strtok (NULL, delimiteurs);
    cam_vars->cam_reask_interval = atoi (substring);
  }
  else if (!strcmp (substring, "cam_reset_interval"))
  {
    substring = strtok (NULL, delimiteurs);
    cam_vars->reset_interval = atoi (substring);
    cam_vars->timeout_no_cam_init = cam_vars->reset_interval;
  }
  else if (!strcmp (substring, "cam_number"))
  {
    substring = strtok (NULL, delimiteurs);
    cam_vars->cam_number = atoi (substring);
  }
  else if (!strcmp (substring, "cam_delay_pmt_send"))
  {
    substring = strtok (NULL, delimiteurs);
    cam_vars->cam_delay_pmt_send = atoi (substring);
  }
  else if (!strcmp (substring, "cam_interval_pmt_send"))
  {
     substring = strtok (NULL, delimiteurs);
     cam_vars->cam_interval_pmt_send = atoi (substring);
  }
  else if (!strcmp (substring, "cam_pmt_follow"))
  {
     substring = strtok (NULL, delimiteurs);
     cam_vars->cam_pmt_follow = atoi (substring);
  }
  else if (!strcmp (substring, "cam_pmt_pid"))
  {
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "cam_pmt_pid : You have to start a channel first (using ip= or channel_next)\n");
      return -1;
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->pmt_pid = atoi (substring);
    if (current_channel->pmt_pid < 10 || current_channel->pmt_pid > 8191){
      log_message( log_module,  MSG_ERROR,
                   "Config issue in pids, given pid : %d\n",
                    current_channel->pmt_pid);
      return -1;
    }
    current_channel->need_cam_ask=CAM_NEED_ASK;
  }
  else
    return 0; //Nothing concerning cam, we return 0 to explore the other possibilities

  return 1;//We found something for cam, we tell main to go for the next line

}


struct en50221_stdcam_llci {
  struct en50221_stdcam stdcam;

  int cafd;
  int slotnum;
  int state;
};

/** @brief Reset the CAM */
void cam_reset_cam(cam_parameters_t *cam_params)
{
  log_message( log_module,  MSG_DEBUG,"CAM Reset\n");
  struct en50221_stdcam *stdcam=cam_params->stdcam;
  struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) stdcam;
  if(ioctl(llci->cafd, CA_RESET, (1 << llci->slotnum))<0)
  {
    log_message( log_module,  MSG_WARN, "Reset IOCTL failed : %s", strerror (errno));
    return;
  }
  //This variable only exist for low level CAMs so we check the type
  if(cam_params->cam_type==DVBCA_INTERFACE_LINK)
    llci->state = EN50221_STDCAM_CAM_NONE;
 
}



/** @brief Get the CAM state */
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


/** @brief Get the CAM interface type */
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



/** @brief start the cam
 * This function will create the communication layers and set the callbacks*/
int cam_start(cam_parameters_t *cam_params, int adapter_id)
{

  // CAM Log
  log_message( log_module,  MSG_DEBUG,"CAM Initialization\n");
  log_message( log_module,  MSG_DEBUG,"CONF cam_reask_interval=%d\n",cam_params->cam_reask_interval);
  log_message( log_module,  MSG_DEBUG,"CONF cam_reset_interval=%d\n",cam_params->reset_interval);
  log_message( log_module,  MSG_DEBUG,"CONF cam_number=%d\n",cam_params->cam_number);
  log_message( log_module,  MSG_DEBUG,"CONF cam_delay_pmt_send=%d\n",cam_params->cam_delay_pmt_send);
  log_message( log_module,  MSG_DEBUG,"CONF cam_interval_pmt_send=%d\n",cam_params->cam_interval_pmt_send);

  // create transport layer - 1 Slot and 16 sessions maximum
  cam_params->tl = en50221_tl_create(1, 16);
  if (cam_params->tl == NULL) {
    log_message( log_module,  MSG_ERROR,"Failed to create transport layer\n");
    return 1;
  }

  // create session layer
  cam_params->sl = en50221_sl_create(cam_params->tl, SL_MAX_SESSIONS);
  if (cam_params->sl == NULL) {
    log_message( log_module,  MSG_ERROR, "Failed to create session layer\n");
    en50221_tl_destroy(cam_params->tl);
    return 1;
  }

  // create the stdcam instance
  cam_params->stdcam = en50221_stdcam_create(adapter_id, cam_params->cam_number, cam_params->tl, cam_params->sl);
  if (cam_params->stdcam == NULL) {
    log_message( log_module,  MSG_ERROR, "Failed to create the stdcam instance (no cam present ?)\n");
    en50221_sl_destroy(cam_params->sl);
    en50221_tl_destroy(cam_params->tl);
    return 1;
  }


  // hook up the AI callbacks
  if (cam_params->stdcam->ai_resource) {
    en50221_app_ai_register_callback(cam_params->stdcam->ai_resource, mumudvb_cam_ai_callback, cam_params);
  } else {
    log_message( log_module,  MSG_WARN,  "No Application Information resource\n");
  }

  // hook up the CA callbacks
  if (cam_params->stdcam->ca_resource) {
    en50221_app_ca_register_info_callback(cam_params->stdcam->ca_resource, mumudvb_cam_ca_info_callback, cam_params);
    en50221_app_ca_register_pmt_reply_callback(cam_params->stdcam->ca_resource, mumudvb_cam_app_ca_pmt_reply_callback, cam_params);
  } else {
    log_message( log_module,  MSG_WARN,  "No CA resource\n");
  }


  
  // hook up the MMI callbacks
  if (cam_params->stdcam->mmi_resource) {
    en50221_app_mmi_register_close_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_close_callback, cam_params);
    en50221_app_mmi_register_display_control_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_display_control_callback, cam_params);
    en50221_app_mmi_register_enq_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_enq_callback, cam_params);
    en50221_app_mmi_register_menu_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_menu_callback, cam_params);
    en50221_app_mmi_register_list_callback(cam_params->stdcam->mmi_resource, mumudvb_cam_mmi_list_callback, cam_params);
  } else {
    log_message( log_module,  MSG_WARN,  "CAM Menus are not supported by this interface hardware\n");
  }


  // any other stuff
  cam_params->moveca = 1; //see http://www.linuxtv.org/pipermail/linux-dvb/2007-May/018198.html

  cam_params->cam_type = cam_debug_dvbca_get_interface_type(cam_params); //The reset procedure have only been tested on LLCI cams
  switch(cam_params->cam_type)
  {
    case DVBCA_INTERFACE_LINK:
      log_message( log_module,  MSG_DETAIL,  "CAM type : low level interface\n");
      break;
    case DVBCA_INTERFACE_HLCI:
      log_message( log_module,  MSG_DETAIL,  "CAM type : HIGH level interface\n");
      break;
  }

  // start the cam thread
  pthread_create(&(cam_params->camthread), NULL, camthread_func, cam_params);
  return 0;
}

/** @brief Stops the CAM*/
void cam_stop(cam_parameters_t *cam_params)
{

  log_message( log_module,  MSG_DEBUG,  "CAM Stopping\n");
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


/** @brief The thread for polling the cam */
static void *camthread_func(void* arg)
{
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  int i;
  int camstate;
  struct timeval tv;
  long real_start_time;
  long now;
  long last_channel_check;

  extern mumudvb_chan_and_pids_t chan_and_pids; /** @todo ugly way to access channel data */
  //We record the starting time
  gettimeofday (&tv, (struct timezone *) NULL);
  real_start_time = tv.tv_sec;
  now = 0;
  last_channel_check=0;

  log_message( log_module,  MSG_DEBUG,"CAM Thread started\n");

  // Variables for detecting changes of status and error
  int status_old=0;
  int status_new=0;
  int error_old=0;
  int error_new=0;

  //Loop
  while(!cam_params->camthread_shutdown) {
    usleep(100*1000); //some waiting - 100ms (see specs)

    gettimeofday (&tv, (struct timezone *) NULL);
    now = tv.tv_sec - real_start_time;

    //If the CAM is initialised (ie we received the CA_info) we check if the "safety" delay is over
    //This behavior is made for some "cray" CAMs like powercam v4 which doesn't accept the PMT just after the ca_info_callback
    if(cam_params->ca_info_ok_time && cam_params->ca_resource_connected==0)
      if((tv.tv_sec - cam_params->ca_info_ok_time) > cam_params->cam_delay_pmt_send)
        cam_params->ca_resource_connected=1;

    /* Check for fully scrambled channels for a while, to re ask the CAM */
    if(cam_params->ca_resource_connected && (cam_params->cam_reask_interval>0))
    {
      //We don't check too often for the reasking of the highly scrambled channels
      if((now-last_channel_check)>2)
      {
        last_channel_check=now;
        for (int curr_channel = 0; curr_channel < chan_and_pids.number_of_channels; curr_channel++)
        {
          // Check if reasking (ie sending a CAM PMT UPDATE) is needed. IE channel hyghly/partially scrambled or down and asked a while ago
          if((chan_and_pids.channels[curr_channel].scrambled_channel == HIGHLY_SCRAMBLED || chan_and_pids.channels[curr_channel].scrambled_channel == PARTIALLY_UNSCRAMBLED || chan_and_pids.channels[curr_channel].streamed_channel == 0)&&
            (chan_and_pids.channels[curr_channel].need_cam_ask==CAM_ASKED)&&
            ((tv.tv_sec-chan_and_pids.channels[curr_channel].cam_asking_time)>cam_params->cam_reask_interval))
          {
            chan_and_pids.channels[curr_channel].need_cam_ask=CAM_NEED_UPDATE; //No race condition because need_cam_ask is not changed when it is at the value CAM_ASKED
            log_message( log_module,  MSG_DETAIL,
                          "Channel \"%s\" highly scrambled for more than %ds. We ask the CAM to update.\n",
                          chan_and_pids.channels[curr_channel].name,cam_params->cam_reask_interval);
            chan_and_pids.channels[curr_channel].cam_asking_time=tv.tv_sec;
          }
        }
      }
    }

    // Polling CAM and checking status change - List of possible status: (en50221_stdcam.h)
    // 0: EN50221_STDCAM_CAM_NONE
    // 1: EN50221_STDCAM_CAM_INRESET
    // 2: EN50221_STDCAM_CAM_OK
    // 3: EN50221_STDCAM_CAM_BAD
    status_new=cam_params->stdcam->poll(cam_params->stdcam);

    if (status_new!=status_old)
    {
      if(status_new>3)
        log_message( log_module,  MSG_WARN, "The CAM changed to an unknown status : %d, please contact\n",status_new);
      else if (status_old >3)
        log_message( log_module,  MSG_DEBUG, "Status change from UNKNOWN (%d) to %s.\n",status_old,cam_status[status_new]);
      else
        log_message( log_module,  MSG_DEBUG, "Status change from %s to %s.\n",cam_status[status_old],cam_status[status_new]);
      status_old=status_new;
    }

    // Try to get the Transport Layer structure from libdvben50221
    if (cam_params->tl!=NULL)
    {
      // Get the last error code
      error_new=en50221_tl_get_error(cam_params->tl);
    }
    // Check if error code has changed - List of error codes: (en50221_errno.h)
    if (error_new!=error_old)
    {
      log_message( log_module,  MSG_WARN, "Transport Layer Error change from %s (%s) to %s (%s)\n",
                   liben50221_error_to_str(error_old),liben50221_error_to_str_descr(error_old),
                   liben50221_error_to_str(error_new),liben50221_error_to_str_descr(error_new));
      error_old=error_new;
	  if(cam_params->ca_resource_connected)
	  {
		// This is probably a CAM crash, as after initialization, a Transport Layer error isn't good...
	    log_message( log_module,  MSG_ERROR,"Transport Layer error after CAM initialization: CAM may have crash, it's better to exit and restart...\n");
        set_interrupted(ERROR_CAM<<8); //the <<8 is to make difference beetween signals and errors
	  }
    }


    //check if we need reset
    if ( cam_params->ca_info_ok_time==0 && cam_params->timeout_no_cam_init>0 && now>cam_params->timeout_no_cam_init && cam_params->reset_interval>0)
    {
      if(cam_params->cam_type==DVBCA_INTERFACE_LINK)
      {
        if(cam_params->need_reset==0 && cam_params->reset_counts<cam_params->max_reset_number)
        {
          log_message( log_module,  MSG_INFO,
                       "No CAM initialization in %ds, WE FORCE A RESET. try %d on %d.\n",
                       cam_params->timeout_no_cam_init,
                       cam_params->reset_counts+1,
                       cam_params->max_reset_number);
          cam_params->need_reset=1;
          cam_params->timeout_no_cam_init=now+cam_params->reset_interval;
        }
        else if (cam_params->reset_counts>=cam_params->max_reset_number)
        {
          log_message( log_module,  MSG_INFO,
                       "No CAM initialization  in %ds,  the %d resets didn't worked. Exiting.\n",
                       cam_params->timeout_no_cam_init,cam_params->max_reset_number);
          set_interrupted(ERROR_NO_CAM_INIT<<8); //the <<8 is to make difference beetween signals and errors
        }
      }
      else
      {
        log_message( log_module,  MSG_INFO,
                     "No CAM initialization on in %ds and HLCI CAM, exiting.\n",
                     cam_params->timeout_no_cam_init);
        set_interrupted(ERROR_NO_CAM_INIT<<8); //the <<8 is to make difference beetween signals and errors
      }
    }
    //We do the reset if needed
    if(cam_params->need_reset==1)
    {
      cam_reset_cam(cam_params);
      i=0;
      log_message( log_module,  MSG_DEBUG,  "We wait for the cam to be INITIALISING\n");
      do
      {
        camstate=cam_debug_dvbca_get_cam_state(cam_params);
        switch(camstate) 
        {
          case DVBCA_CAMSTATE_MISSING:
            log_message( log_module,  MSG_DEBUG,  "cam state : DVBCA_CAMSTATE_MISSING\n");
            break;
          case DVBCA_CAMSTATE_READY:
            log_message( log_module,  MSG_DEBUG,  "cam state : DVBCA_CAMSTATE_READY\n");
            break;
          case DVBCA_CAMSTATE_INITIALISING:
            log_message( log_module,  MSG_DEBUG,  "cam state : DVBCA_CAMSTATE_INITIALISING\n");
            break;
          case -1:
            log_message( log_module,  MSG_DEBUG,  "cam state : Eroor during the query\n");
            break;
        }
        usleep(10000);
        i++;
      } while(camstate!=DVBCA_CAMSTATE_INITIALISING && i < MAX_WAIT_AFTER_RESET);
      if(i==MAX_WAIT_AFTER_RESET)
        log_message( log_module,  MSG_INFO, "The CAM isn't in a good state after reset, it will probably don't work :(\n");
      else
        log_message( log_module,  MSG_DEBUG, "state correct after reset\n");
      cam_params->need_reset=0;
      cam_params->reset_counts++;
    }

  }

  // As we can't get the state of the session, 
  // we try to close all of them with some polling to force communication
  log_message( log_module,  MSG_DEBUG,"Closing the CAM sessions\n");
  for (i=0;i<SL_MAX_SESSIONS;i++)
  {
    en50221_sl_destroy_session(cam_params->sl,i);
    usleep(50*1000);
    cam_params->stdcam->poll(cam_params->stdcam);
    usleep(50*1000);
    cam_params->stdcam->poll(cam_params->stdcam);
  }

  log_message( log_module,  MSG_DEBUG,"CAM Thread stopped\n");

  return 0;
}






/** @brief PMT sending to the cam
 * This function if called when mumudvb receive a new PMT pid. 
 * This function will ask the cam to decrypt the associated channel
 */
int mumudvb_cam_new_pmt(cam_parameters_t *cam_params, mumudvb_ts_packet_t *cam_pmt_ptr, int need_cam_ask)
{
  uint8_t capmt[MAX_TS_SIZE];
  int size,list_managment;

  // parse section
  struct section *section = section_codec(cam_pmt_ptr->data_full,cam_pmt_ptr->len_full);
  if (section == NULL) {
    log_message( log_module,  MSG_WARN,"section_codec parsing error\n");
    return -1;
  }

  // parse section_ext
  struct section_ext *section_ext = section_ext_decode(section, 0);
  if (section_ext == NULL) {
    log_message( log_module,  MSG_WARN,"section_ext parsing error\n");
    return -1;
  }

  // parse PMT
  struct mpeg_pmt_section *pmt = mpeg_pmt_section_codec(section_ext);
  if (pmt == NULL) {
    log_message( log_module,  MSG_WARN,"mpeg_pmt_section_codec parsing error\n");
    return -1;
  }

  if(pmt->head.table_id!=0x02)
    {
      log_message( log_module,  MSG_WARN,"Packet PID %d is not a PMT PID\n", cam_pmt_ptr->pid);
      return -1;
    }


  if (cam_params->stdcam == NULL)
    return -1;

  if (cam_params->ca_resource_connected) {
    log_message( log_module,  MSG_INFO, "Received new PMT - sending to CAM...\n");

    // translate it into a CA PMT 
    // Concerning the list managment the simplest (since we don't want to remove channels is to do a CA_LIST_MANAGEMENT_ADD 
    //Always. Doing FIRST, MORE ,MORE ... LAST is more complicated because the CAM will wait for the LAST
    // If the an update is needed the Aston cams will be happy with a ADD (it detects that the channel is already present and updates
    //It seems that the power cam don't really follow the norm (ie accept almost everything)
    // Doing also only update should work
    //an update should be equivalent to an add when the channel is not present
    //Note : The powercam HD V3.1 doesn't add channels with update, so we only do updates when the channel was added
    if(need_cam_ask==CAM_NEED_UPDATE)
      list_managment=CA_LIST_MANAGEMENT_UPDATE;
    else
      list_managment=CA_LIST_MANAGEMENT_ADD;
    if ((size = en50221_ca_format_pmt(pmt, capmt, sizeof(capmt), cam_params->moveca, list_managment, CA_PMT_CMD_ID_OK_DESCRAMBLING)) < 0) {

      /*CA_PMT_CMD_ID_QUERY)) < 0) {
	We don't do query, the query is never working very well. This is because the CAM cannot ask the card if 
	you have the rights for the channel. So this answer is often not reliable.

	Much thanks to Aston www.aston-france.com for the explanation
      */
      log_message( log_module,  MSG_WARN, "Failed to format PMT\n");
      return -1;
    }

    // set it
    if (en50221_app_ca_pmt(cam_params->stdcam->ca_resource, cam_params->stdcam->ca_session_number, capmt, size)) {
      log_message( log_module,  MSG_WARN, "Failed to send PMT\n");
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
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  (void) slot_id;
  (void) session_number;

  // Write information to log
  log_message( log_module,  MSG_DEBUG, "CAM Application_Info_Callback\n");
  log_message( log_module,  MSG_INFO, "CAM Application type: %02x\n", application_type);
  log_message( log_module,  MSG_INFO, "CAM Application manufacturer: %04x\n", application_manufacturer);
  log_message( log_module,  MSG_INFO, "CAM Manufacturer code: %04x\n", manufacturer_code);
  log_message( log_module,  MSG_INFO, "CAM Menu string: %.*s\n", menu_string_length, menu_string);

  // Store the CAM menu string for easy identification
  mumu_free_string(&cam_params->cam_menu_string);
  mumu_string_append(&cam_params->cam_menu_string, "%.*s", menu_string_length, menu_string);

  // Try to append the information to the cam_info log file
  FILE *file_cam_info;
  file_cam_info = fopen (cam_params->filename_cam_info, "a");
  if (file_cam_info == NULL)
    {
      log_message( log_module,  MSG_WARN,
		   "%s: %s\n",
		   cam_params->filename_cam_info, strerror (errno));
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
  struct timeval tv;

  // Write information to log
  log_message( log_module,  MSG_DEBUG,"CA_Info_Callback: %d CA systems supported\n",ca_id_count);
  log_message( log_module,  MSG_DETAIL, "CAM supports the following ca system ids:\n");
  uint32_t i;
  for(i=0; i< ca_id_count; i++) {
    log_message( log_module,  MSG_DETAIL,"Ca system id 0x%04x : %s\n",ca_ids[i], ca_sys_id_to_str(ca_ids[i])); //we display it with the description
  }


  // Try to append the information to the cam_info log file
  FILE *file_cam_info;
  file_cam_info = fopen (cam_params->filename_cam_info, "a");
  if (file_cam_info == NULL)
  {
    log_message( log_module,  MSG_WARN,
                  "%s: %s\n",
                  cam_params->filename_cam_info, strerror (errno));
  }
  else
  {
    for(i=0; i< ca_id_count; i++)
      fprintf (file_cam_info,"ID_CA_Supported=%04x\n",ca_ids[i]);
    fclose (file_cam_info);
  }
  gettimeofday (&tv, (struct timezone *) NULL);

  cam_params->ca_info_ok_time=tv.tv_sec;

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
  log_message( log_module,  MSG_INFO, "CAM PMT reply\n");
  log_message( log_module,  MSG_INFO, "  Program number %d\n",reply->program_number);

  switch(reply->CA_enable)
    {
    case CA_ENABLE_DESCRAMBLING_POSSIBLE:
      log_message( log_module,  MSG_INFO,"   Descrambling possible\n");
      break;
    case CA_ENABLE_DESCRAMBLING_POSSIBLE_PURCHASE:
      log_message( log_module,  MSG_INFO,"   Descrambling possible under conditions (purchase dialogue)\n");
      break;
    case CA_ENABLE_DESCRAMBLING_POSSIBLE_TECHNICAL:
      log_message( log_module,  MSG_INFO,"   Descrambling possible under conditions (technical dialogue)\n");
      break;
    case CA_ENABLE_DESCRAMBLING_NOT_POSSIBLE_NO_ENTITLEMENT:
      log_message( log_module,  MSG_INFO,"   Descrambling not possible (because no entitlement)\n");
      break;
    case CA_ENABLE_DESCRAMBLING_NOT_POSSIBLE_TECHNICAL:
      log_message( log_module,  MSG_INFO,"   Descrambling not possible (for technical reasons)\n");
      break;
    default:
      log_message( log_module,  MSG_INFO,"   RFU\n");
    }


  en50221_app_pmt_reply_streams_for_each(reply, pos, reply_size)
    {
      log_message( log_module,  MSG_INFO, "   ES pid %d\n",pos->es_pid);
      switch(pos->CA_enable)
	{
	case CA_ENABLE_DESCRAMBLING_POSSIBLE:
	  log_message( log_module,  MSG_INFO,"     Descrambling possible\n");
	  break;
	case CA_ENABLE_DESCRAMBLING_POSSIBLE_PURCHASE:
	  log_message( log_module,  MSG_INFO,"     Descrambling possible under conditions (purchase dialogue)\n");
	  break;
	case CA_ENABLE_DESCRAMBLING_POSSIBLE_TECHNICAL:
	  log_message( log_module,  MSG_INFO,"     Descrambling possible under conditions (technical dialogue)\n");
	  break;
	case CA_ENABLE_DESCRAMBLING_NOT_POSSIBLE_NO_ENTITLEMENT:
	  log_message( log_module,  MSG_INFO,"     Descrambling not possible (because no entitlement)\n");
	  break;
	case CA_ENABLE_DESCRAMBLING_NOT_POSSIBLE_TECHNICAL:
	  log_message( log_module,  MSG_INFO,"     Descrambling not possible (for technical reasons)\n");
	  break;
	default:
	  log_message( log_module,  MSG_INFO,"     RFU\n");
	}
    }

  return 0;
}


/*******************************
 * MMI
 *******************************/

// List object - DISPLAY_TYPE_LIST
static int mumudvb_cam_mmi_list_callback(void *arg, uint8_t slot_id, uint16_t session_number,
                                   struct en50221_app_mmi_text *title,
                                   struct en50221_app_mmi_text *sub_title,
                                   struct en50221_app_mmi_text *bottom,
                                   uint32_t item_count, struct en50221_app_mmi_text *items,
                                   uint32_t item_raw_length, uint8_t *items_raw)
{
  return(mumudvb_cam_mmi_menu_list_callback(arg, slot_id, session_number,
                                   title,
                                   sub_title,
                                   bottom,
                                   item_count, items,
                                   item_raw_length, items_raw, DISPLAY_TYPE_LIST));
}

// Menu object - DISPLAY_TYPE_MENU
static int mumudvb_cam_mmi_menu_callback(void *arg, uint8_t slot_id, uint16_t session_number,
                                   struct en50221_app_mmi_text *title,
                                   struct en50221_app_mmi_text *sub_title,
                                   struct en50221_app_mmi_text *bottom,
                                   uint32_t item_count, struct en50221_app_mmi_text *items,
                                   uint32_t item_raw_length, uint8_t *items_raw)
{
  return(mumudvb_cam_mmi_menu_list_callback(arg, slot_id, session_number,
                                   title,
                                   sub_title,
                                   bottom,
                                   item_count, items,
                                   item_raw_length, items_raw, DISPLAY_TYPE_MENU));
}

// Menu or List objects
static int mumudvb_cam_mmi_menu_list_callback(void *arg, uint8_t slot_id, uint16_t session_number,
                                   struct en50221_app_mmi_text *title,
                                   struct en50221_app_mmi_text *sub_title,
                                   struct en50221_app_mmi_text *bottom,
                                   uint32_t item_count, struct en50221_app_mmi_text *items,
                                   uint32_t item_raw_length, uint8_t *items_raw, int object_type)
{
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  (void) slot_id;
  //  (void) session_number;
  (void) item_raw_length;
  (void) items_raw;

  // New CAM menu received, we prepared its storage for future display
  mumu_free_string(&cam_params->cam_menulist_str);
  // We save the date/time when the menu was received
  time_t rawtime;
  time (&rawtime);
  // Add line to CAM menu storage - Date and Time
  mumu_string_append(&cam_params->cam_menulist_str,"\t<datetime><![CDATA[%s]]></datetime>\n",ctime(&rawtime));
  // Add line to CAM menu storage - CAM Menu String (model)
  mumu_string_append(&cam_params->cam_menulist_str,"\t<cammenustring><![CDATA[%s]]></cammenustring>\n",cam_params->cam_menu_string.string);
  // Add line to CAM menu storage - CAM Object type : LIST or MENU
  if (object_type==DISPLAY_TYPE_LIST)
    mumu_string_append(&cam_params->cam_menulist_str,"\t<object><![CDATA[LIST]]></object>\n");
  if (object_type==DISPLAY_TYPE_MENU)
    mumu_string_append(&cam_params->cam_menulist_str,"\t<object><![CDATA[MENU]]></object>\n");

  // Showing beginning of CAM menu
  if (object_type==DISPLAY_TYPE_LIST)
    log_message( log_module,  MSG_INFO, "------------------ NEW CAM LIST ------------------\n");
  if (object_type==DISPLAY_TYPE_MENU)
    log_message( log_module,  MSG_INFO, "------------------ NEW CAM MENU ------------------\n");

  // Title
  if (title->text_length)
  {
    log_message( log_module,  MSG_INFO, "Menu_Title    : %.*s\n", title->text_length, title->text);
    // Add line to CAM menu storage - Title
    mumu_string_append(&cam_params->cam_menulist_str,"\t<title><![CDATA[%.*s]]></title>\n", title->text_length, title->text);
  }

  // Subtitle
  if (sub_title->text_length)
  {
    log_message( log_module,  MSG_INFO, "Menu_Subtitle : %.*s\n", sub_title->text_length, sub_title->text);
    // Add line to CAM menu storage - Subtitle
    mumu_string_append(&cam_params->cam_menulist_str,"\t<subtitle><![CDATA[%.*s]]></subtitle>\n", sub_title->text_length, sub_title->text);
  }

  // Choice 0 is always for cancel/return/ok action in MENU and LIST
  log_message( log_module,  MSG_INFO, "Menu_Item 0   : Return\n");
  // Add line to CAM menu storage - Items
  mumu_string_append(&cam_params->cam_menulist_str,"\t<item num=\"0\"><![CDATA[Return]]></item>\n");

  // Items
  uint32_t i;
  for(i=0; i< item_count; i++)
  {
    log_message( log_module,  MSG_INFO, "Menu_Item %d   : %.*s\n", (i+1), items[i].text_length, items[i].text);
    // Add line to CAM menu storage - Items
    mumu_string_append(&cam_params->cam_menulist_str,"\t<item num=\"%d\"><![CDATA[%.*s]]></item>\n", (i+1), items[i].text_length, items[i].text);
  }

  // Bottom
  if (bottom->text_length)
  {
    log_message( log_module,  MSG_INFO, "Menu_Bottom   : %.*s\n", bottom->text_length, bottom->text);
    // Add line to CAM menu storage - Bottom
    mumu_string_append(&cam_params->cam_menulist_str,"\t<bottom><![CDATA[%.*s]]></bottom>\n", bottom->text_length, bottom->text);
  }

  // Showing end of CAM menu
  log_message( log_module,  MSG_INFO, "--------------------------------------------------\n");

  fflush(stdout);
  cam_params->stdcam->mmi_session_number=session_number;

  //We leave (action=0 => CANCEL) if autoresponse is active (default=yes if no menu asked)
  if (cam_params->cam_mmi_autoresponse==1)
  {
    // Autoresponse
    log_message( log_module,  MSG_INFO, "Menu autoresponse, send CANCEL\n");
    en50221_app_mmi_menu_answ(cam_params->stdcam->mmi_resource, cam_params->stdcam->mmi_session_number, 0);
    cam_params->mmi_state = MMI_STATE_OPEN;
  }
  else
    // We wait an answer from the user
    cam_params->mmi_state = MMI_STATE_MENU;

  return 0;
}



static int mumudvb_cam_mmi_close_callback(void *arg, uint8_t slot_id, uint16_t session_number,
                                    uint8_t cmd_id, uint8_t delay)
{
  cam_parameters_t *cam_params;
  cam_params= (cam_parameters_t *) arg;
  (void) slot_id;
  (void) cmd_id;
  (void) delay;

  // The CAM told us that the menu was closed
  log_message( log_module,  MSG_INFO, "Closing CAM Menu\n");

  // Remove last stored menu content
  mumu_free_string(&cam_params->cam_menulist_str);

  // Indicate that the menu was closed (for our own record)
  cam_params->mmi_state = MMI_STATE_CLOSED;

  // Close the session with the CAM or a new session MMI will not be allowed
  en50221_sl_destroy_session(cam_params->sl,session_number);

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

  // Show the enquiry informations
  log_message( log_module,  MSG_INFO, "----------------- NEW CAM ENQUIRY-----------------\n");
  log_message( log_module,  MSG_INFO, "Question: %.*s\n", text_size, text);
  log_message( log_module,  MSG_INFO, "Expected answer length: %d\n", expected_answer_length);
  log_message( log_module,  MSG_INFO, "--------------------------------------------------\n");

  // New CAM enquiry received, we prepared its storage for future display
  mumu_free_string(&cam_params->cam_menulist_str);
  // We save the date/time when the enquiry was received
  time_t rawtime;
  time(&rawtime);
  // Add line to CAM enquiry storage - Date and Time
  mumu_string_append(&cam_params->cam_menulist_str,"\t<datetime><![CDATA[%s]]></datetime>\n",ctime(&rawtime));

  // Add line to CAM menu storage - CAM Menu String (model)
  mumu_string_append(&cam_params->cam_menulist_str,"\t<cammenustring><![CDATA[%s]]></cammenustring>\n",cam_params->cam_menu_string.string);

  // Add line to CAM menu storage - CAM Object type : ENQUIRY
  mumu_string_append(&cam_params->cam_menulist_str,"\t<object><![CDATA[ENQUIRY]]></object>\n");

  // We put the question in the "Title" field
  mumu_string_append(&cam_params->cam_menulist_str,"\t<title><![CDATA[%.*s]]></title>\n", text_size, text);

  // We put the answer length in the "Subtitle" field
  mumu_string_append(&cam_params->cam_menulist_str,"\t<subtitle><![CDATA[Expected answer length: %d]]></subtitle>\n", expected_answer_length);

  // We don't care to hide or display the answer...
  cam_params->mmi_enq_blind = blind_answer;
  // The expected length of the answer (number of characters)
  cam_params->mmi_enq_length = expected_answer_length;
  // Limit the answer to MAX_ENQUIRY_ANSWER_LENGTH characters (enough for PIN code and changing maturity, for the most common usages)
  if (cam_params->mmi_enq_length>MAX_ENQUIRY_ANSWER_LENGTH) cam_params->mmi_enq_length=MAX_ENQUIRY_ANSWER_LENGTH;
  // The actual number of typed characters
  cam_params->mmi_enq_entered = 0;

  //We leave (CANCEL) if autoresponse is active (default=yes if no menu asked)
  if (cam_params->cam_mmi_autoresponse==1)
  {
    // Autoresponse
    log_message( log_module,  MSG_INFO, "Enquiry autoresponse, send CANCEL\n");
    en50221_app_mmi_answ(cam_params->stdcam->mmi_resource, cam_params->stdcam->mmi_session_number, MMI_ANSW_ID_CANCEL, NULL, 0);
	cam_params->mmi_state = MMI_STATE_OPEN;
  }
  else
    // We wait an answer from the user
    cam_params->mmi_state = MMI_STATE_ENQ;

  return 0;
}

/** @brief This function is called when a new PMT packet is there */
int cam_new_packet(int pid, int curr_channel, unsigned char *ts_packet, cam_parameters_t *cam_vars, mumudvb_channel_t *actual_channel)
{
  int iRet;
  struct timeval tv;
  gettimeofday (&tv, (struct timezone *) NULL);

  if (((actual_channel->need_cam_ask==CAM_NEED_ASK)||(actual_channel->need_cam_ask==CAM_NEED_UPDATE))&& (actual_channel->pmt_pid == pid))
  {
    if(get_ts_packet(ts_packet,actual_channel->cam_pmt_packet))
    {
      //We check the transport stream id of the packet
      if(check_pmt_service_id(actual_channel->cam_pmt_packet, actual_channel))
      {
        iRet=mumudvb_cam_new_pmt(cam_vars, actual_channel->cam_pmt_packet,actual_channel->need_cam_ask);
        if(iRet==1)
        {
          if(actual_channel->need_cam_ask==CAM_NEED_UPDATE)
            log_message( log_module,  MSG_INFO,"CA PMT (UPDATED) sent for channel %d : \"%s\"\n", curr_channel, actual_channel->name );
          else
            log_message( log_module,  MSG_INFO,"CA PMT (ADDED) sent for channel %d : \"%s\"\n", curr_channel, actual_channel->name );
          actual_channel->need_cam_ask=CAM_ASKED; //once we have asked the CAM for this PID, we don't have to ask anymore

          //For the feature of reasking we initalise the time
          actual_channel->cam_asking_time = tv.tv_sec;
          return 1;
        }
        else if(iRet==-1)
        {
          log_message( log_module,  MSG_DETAIL,"Problem sending CA PMT for channel %d : \"%s\"\n", curr_channel, actual_channel->name );
        }
      }
      //else //The service_id is bad, we will try to get another PMT packet
    }
  }
  return 0;
}







/*************************************************************************************************************************************
  This part of the code is a bit particular, it allows the PMT to be followed for non autoconfigurated channels
 *************************************************************************************************************************************/


//from autoconf_pmt.c
void update_pmt_version(mumudvb_channel_t *channel);
int pmt_need_update(mumudvb_channel_t *channel, unsigned char *packet);

/** @brief This function is called when a new PMT packet is there and we asked to check if there is updates*/
void cam_pmt_follow(unsigned char *ts_packet,  mumudvb_channel_t *actual_channel)
{
  /*Note : the pmt version is initialised during autoconfiguration*/
  /*Check the version stored in the channel*/
  if(!actual_channel->pmt_needs_update)
  {
    //Checking without crc32, it there is a change we get the full packet for crc32 checking
    actual_channel->pmt_needs_update=pmt_need_update(actual_channel,get_ts_begin(ts_packet));

  }
  /*We need to update the full packet, we download it*/
  if(actual_channel->pmt_needs_update)
  {
    if(get_ts_packet(ts_packet,actual_channel->pmt_packet))
    {
      if(pmt_need_update(actual_channel,actual_channel->pmt_packet->data_full))
      {
        log_message( log_module, MSG_DETAIL,"PMT packet updated, we now ask the CAM to update it\n");
        log_message( log_module, MSG_WARN,"The PMT version has changed but the PIDs are configured manually, use autoconfiguration if possible. If not, please tell me why so I can improve it\n");
        /*We've got the FULL PMT packet*/
        pmt_t *header;
        header=(pmt_t *)(actual_channel->pmt_packet->data_full);
        if(header->current_next_indicator == 0)
        {
          log_message( log_module, MSG_DEBUG,"The current_next_indicator is set to 0, this PMT is not valid for the current stream\n");
        }else{
          actual_channel->need_cam_ask=CAM_NEED_UPDATE; //We we resend this packet to the CAM
          update_pmt_version(actual_channel);
          actual_channel->pmt_needs_update=0;
        }
      }
      else
      {
        log_message( log_module, MSG_DEBUG,"False alert, nothing to do\n");
        actual_channel->pmt_needs_update=0;
      }
    }
  }
}




