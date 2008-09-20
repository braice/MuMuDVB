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

#include "sap.h"
#include "udp.h"
#include <string.h>


//SAP_init : define the version (rand 1,4242), clear the message and open the socket
int sap_init(mumudvb_sap_message_t *sap_messages, int num_messages)
{

  return 0;
}

//SAP_send : send the sap message
void sap_send(mumudvb_sap_message_t *sap_messages, int num_messages)
{

  return;
}


//SAP_update : update the contents of the sap message
int sap_update(mumudvb_channel_t channel, mumudvb_sap_message_t *sap_message)
{
  //TAILLE DU PAQUET < MTU
  //TODO : add debug messages 
  
  //This function is called when the channel changes so it increases the version and update the packet
  char temp_string[256];

  sap_message->version++;
  sap_message->buf[0]=SAP_HEADER;
  sap_message->buf[1]=SAP_HEADER2;
  sap_message->buf[2]=(sap_message->version&0xff00)>>8;
  sap_message->buf[3]=sap_message->version&0xff;

  //TODO TODO   //sap_message->buf[4 5 6 7]= IP;
  sap_message->buf[4]=0;
  sap_message->buf[5]=0;
  sap_message->buf[6]=0;
  sap_message->buf[7]=0;


  //the mime type
  sprintf(temp_string,"application/sdp");
  memcpy(sap_message->buf + 8, temp_string, sizeof(temp_string));
  sap_message->len=8+sizeof(temp_string);

  //boucle sur les chaines

  // one program per message
  if(!sap_add_program(channel, sap_message))
    sap_message->to_be_sent=1;
  else
    sap_message->to_be_sent=0;

  return 0;

}

int sap_add_program(mumudvb_channel_t channel, mumudvb_sap_message_t *sap_message)
{

  //See RFC 2327
  int payload_len=0;

  char temp_string[256];


  if(!channel.streamed_channel)
    return 1;

  //Now we write the sdp part

  //version
  //v=0

  sprintf(temp_string,"v=0\r\n");
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, sizeof(temp_string));
  payload_len+=sizeof(temp_string);
  
  //owner/creator and session identifier
  //o=username session_id version network_type address_type address
  //ex : o=mumudvb 123134 1 IN IP4 235.255.215.1
  //find a way ta create session id
  //for version we'll use sap version
  //o=....

  sprintf(temp_string,"o=mumudvb %d %d IN IP4 %s\r\n", sap_message->version, sap_message->version, channel.ipOut);
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, sizeof(temp_string));
  payload_len+=sizeof(temp_string);

  //session name (basically channel name)
  //s=...
  sprintf(temp_string,"s=%s\r\n", channel.name);
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, sizeof(temp_string));
  payload_len+=sizeof(temp_string);


  //connection information
  //Ex : c=IN IP4 235.214.225.1/2
  // the / is the TTL
  //c=...

  sprintf(temp_string,"o=%s/%d\r\n", channel.ipOut, DEFAULT_TTL);
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, sizeof(temp_string));
  payload_len+=sizeof(temp_string);

  //time session is active
  //t=...
  //permanent : t=0 0

  sprintf(temp_string,"t=0 0\r\n");
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, sizeof(temp_string));
  payload_len+=sizeof(temp_string);

  //attributes : group and co, we'll take the minisapserver ones
  //a=...
  //a=tool:mumudvb-VERSION
  //a=type:broadcast
  //a=x-plgroup: //channel's group

  sprintf(temp_string,"a=tool:mumudvb-VERSION\r\n");
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, sizeof(temp_string));
  payload_len+=sizeof(temp_string);

  sprintf(temp_string,"a=type:broadcast\r\n");
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, sizeof(temp_string));
  payload_len+=sizeof(temp_string);

  //media name and transport address
  //m=...
  //m=video channel_port udp mpeg

  sprintf(temp_string,"m=video %d udp mpeg\r\n", channel.portOut);
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, sizeof(temp_string));
  payload_len+=sizeof(temp_string);
  
  //TODO : display the message for debug

  log_message(MSG_DEBUG,"DEBUG : SAP payload\n");
  log_message(MSG_DEBUG,sap_message->buf + sap_message->len);
  log_message(MSG_DEBUG,"DEBUG : end of SAP payload\n");

  sap_message->len+=payload_len;

  //TODO : check packet size

  return 0;

}
