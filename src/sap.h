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

/** Enum to tell if we send sap announces*/
typedef enum sap_status {
  SAP_UNDEFINED,
  SAP_OFF,
  SAP_ON
} sap_status_t;


/**refer to  RFC 2974 : sap IP address*/
#define SAP_IP  "224.2.127.254"
/**refer to  RFC 2974 : sap port*/
#define SAP_PORT 9875
/**refer to  RFC 2974 : sap time to live*/
#define SAP_DEFAULT_TTL 255 

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
  sap_status_t sap; 
  /**Interval between two sap announces in second*/
  int sap_interval;
  /** The ip address of the server that sends the sap announces*/
  char sap_sending_ip[20];
  /**the x-plgroup default : ie the playlist group (mainly for vlc)*/
  char sap_default_group[20];
  /**The URI The URI should be a pointer to additional information about the
  conference*/
  char sap_uri[256];
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
  /** The sap ttl (the norm ask it to be 255)*/
  int sap_ttl;
}sap_parameters_t;


int init_sap(sap_parameters_t *sap_vars, multicast_parameters_t multicast_vars);
void sap_send(sap_parameters_t *sap_vars, int num_messages);
int sap_update(mumudvb_channel_t channel, sap_parameters_t *sap_vars, int curr_channel, multicast_parameters_t multicast_vars);
int read_sap_configuration(sap_parameters_t *sap_vars, mumudvb_channel_t *current_channel, int ip_ok, char *substring);
void sap_poll(sap_parameters_t *sap_vars,int number_of_channels,mumudvb_channel_t  *channels, multicast_parameters_t multicast_vars, long now);

#endif
