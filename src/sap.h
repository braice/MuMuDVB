/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Session Announcement Protocol Announces
 * 
 * (C) Brice DUBOST
 * 
 * The latest version can be found at http://mumudvb.braice.net
 * 
 * Parts of this code is from the VLC project, modified  for mumudvb
 * by Brice DUBOST 
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


#ifndef _SAP_H
#define _SAP_H

#include "mumudvb.h"

//From RFC 2974
#define SAP_IP  "224.2.127.254"
#define SAP_PORT 9875
#define SAP_TTL 255 

//intervall between sap announces
#define SAP_DEFAULT_INTERVAL 5

#define SAP_HEADER 0x20 //00100000 : version 1 and nothing else
#define SAP_HEADER2 0x00 //No auth header

//sap_message
typedef struct{
  unsigned char buf[MAX_UDP_SIZE]; //the buffer
  int len;                    //Lenght of the sap message
  int version; //the version of the sap message, MUST be changed when sap changes
  int to_be_sent;
}mumudvb_sap_message_t;

char sap_organisation[256];

struct sockaddr_in sap_sOut;
int sap_socketOut;

int sap_serial;
long sap_last_time_sent;

void sap_send(mumudvb_sap_message_t *sap_messages, int num_messages);
int sap_update(mumudvb_channel_t channel, mumudvb_sap_message_t *sap_message);
int sap_add_program(mumudvb_channel_t channel, mumudvb_sap_message_t *sap_message);

#endif
