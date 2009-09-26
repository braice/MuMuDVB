/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Conditionnal Access Modules support
 * 
 * (C) 2009 Brice DUBOST
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


#ifndef _CAM_H
#define _CAM_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/dvb/ca.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "ts.h"
#include "mumudvb.h"

/**@file
 * @brief cam support
 * 
 * Header file for cam support, contains mainly the structure for carrying cam parameters
 */

#include <libdvben50221/en50221_stdcam.h>
#include <pthread.h>

struct ca_info {
  int initialized; //are the cai complete ?
  int ready; //We wait a pool between each channel sending
  int sys_num;
  uint16_t sys_id[256];
  char app_name[256];
};



int cam_send_ca_pmt( mumudvb_ts_packet_t *pmt, struct ca_info *cai);
int convert_desc(struct ca_info *cai, uint8_t *out, uint8_t *buf, int dslen, uint8_t cmd, int quiet);
int convert_pmt(struct ca_info *cai, mumudvb_ts_packet_t *pmt, uint8_t list, uint8_t cmd,int quiet);


/** @brief the parameters for the cam
 * This structure contain the parameters needed for the CAM
 */
typedef struct cam_parameters_t{
  /**Do we activate the support for CAMs*/
  int cam_support;
  /**The came number (in case of multiple cams)*/
  int cam_number;
  int cam_type;
  int need_reset;
  int reset_counts;
  int max_reset_number;
  int reset_interval;
  struct en50221_transport_layer *tl;
  struct en50221_session_layer *sl;
  struct en50221_stdcam *stdcam;
  int ca_resource_connected;
  int camthread_shutdown;
  pthread_t camthread;
  int moveca;
  int delay; //used to get the menu answer
  int mmi_state;
  int mmi_enq_blind;
  int mmi_enq_length;

}cam_parameters_t;

/*****************************************************************************
 * Code for dealing with libdvben50221
 *****************************************************************************/

#define MMI_STATE_CLOSED 0
#define MMI_STATE_OPEN 1
#define MMI_STATE_ENQ 2
#define MMI_STATE_MENU 3

#define MAX_WAIT_AFTER_RESET 30
#define CAM_DEFAULT_MAX_RESET_NUM 5
#define CAM_DEFAULT_RESET_INTERVAL 30

/**
 * States a CAM in a slot can be in.
 */

#define DVBCA_CAMSTATE_MISSING 0
#define DVBCA_CAMSTATE_INITIALISING 1
#define DVBCA_CAMSTATE_READY 2

/**
 * The types of CA interface we support.
 */

#define DVBCA_INTERFACE_LINK 0
#define DVBCA_INTERFACE_HLCI 1

int cam_start(cam_parameters_t *, int, char *);
void cam_stop(cam_parameters_t *);
int mumudvb_cam_new_pmt(cam_parameters_t *cam_params, mumudvb_ts_packet_t *cam_pmt_ptr);

#endif
