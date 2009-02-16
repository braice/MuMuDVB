/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Session Announcement Protocol Announces
 * 
 * (C) 2008-2009 Brice DUBOST
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
 * @brief Header File for Session Announcement Protocol Announces
 * @author Brice DUBOST
 * @date 2008-2009
 */

#ifndef _SAP_H
#define _SAP_H

#include "mumudvb.h"

/**refer to  RFC 2974 : sap IP address*/
#define SAP_IP  "224.2.127.254"
/**refer to  RFC 2974 : sap port*/
#define SAP_PORT 9875
/**refer to  RFC 2974 : sap time to live*/
#define SAP_TTL 255 

/**intervall between sap announces*/
#define SAP_DEFAULT_INTERVAL 5

#define SAP_HEADER 0x20 /**00100000 : version 1 and nothing else*/
#define SAP_HEADER2 0x00 /**No auth header*/

/**@brief sap_message*/
typedef struct{
  /**the buffer*/
  unsigned char buf[MAX_UDP_SIZE]; 
  /**Lenght of the sap message*/
  int len;
  /**the version of the sap message, MUST be changed when sap changes*/
  int version;
  /** Do we have to send this message ?*/
  int to_be_sent;
}mumudvb_sap_message_t;


/**@brief General parameter for sap announces*/
typedef struct sap_parameters_t{
  /**the sap messages array*/
  mumudvb_sap_message_t *sap_messages; 
  /**do we send sap announces ?*/
  int sap; 
  /**Interval between two sap announces in second*/
  int sap_interval;
  /** The ip adress of the server wich send the sap announces*/
  char sap_sending_ip[20];
  /**the x-plgroup default : ie the playlist group (mainly for vlc)*/
  char sap_default_group[20];
  /**The organisation wich made the announces*/
  char sap_organisation[256];
  /** The socket for sending the announces*/
  int sap_socketOut;
  /** The socket for sending the announces*/
  struct sockaddr_in sap_sOut;
  /** The serial number for the sap announces*/
  int sap_serial;
  /** The time when the last sap announces have been sent*/
  long sap_last_time_sent;
}sap_parameters_t;


void sap_send(mumudvb_sap_message_t *sap_messages, int num_messages);
int sap_update(mumudvb_channel_t channel, mumudvb_sap_message_t *sap_message);
int sap_add_program(mumudvb_channel_t channel, mumudvb_sap_message_t *sap_message);

#endif
