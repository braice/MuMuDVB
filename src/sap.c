/* 
 * mumudvb - UDP-ize a DVB transport stream.
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
 * @brief File for Session Announcement Protocol Announces
 * @author Brice DUBOST
 * @date 2008-2009
 */

#include "sap.h"
#include "network.h"
#include <string.h>
#include <errno.h>


extern int multicast_ttl;
extern int multicast_auto_join;
extern int rtp_header;

int sap_add_program(mumudvb_channel_t channel, sap_parameters_t *sap_vars, mumudvb_sap_message_t *sap_message);


/** @brief init the sap
 * Alloc the memory for the messages, open the socket
 */
int init_sap(sap_parameters_t *sap_vars)
{
  if(sap_vars->sap)
    {
      sap_vars->sap_messages=malloc(sizeof(mumudvb_sap_message_t)*MAX_CHANNELS);
      if(sap_vars->sap_messages==NULL)
	{
          log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
	  return -1;
	}
      memset (sap_vars->sap_messages, 0, sizeof( mumudvb_sap_message_t)*MAX_CHANNELS);//we clear it
      //For sap announces, we open the socket
      //See the README about multicast_auto_join
      if(multicast_auto_join)
	sap_vars->sap_socketOut =  makeclientsocket (SAP_IP, SAP_PORT, sap_vars->sap_ttl, &sap_vars->sap_sOut);
      else
	sap_vars->sap_socketOut =  makesocket (SAP_IP, SAP_PORT, sap_vars->sap_ttl, &sap_vars->sap_sOut);
      sap_vars->sap_serial= 1 + (int) (424242.0 * (rand() / (RAND_MAX + 1.0)));
      sap_vars->sap_last_time_sent = 0;
      /** @todo : loop to create the version*/
    }
  return 0;
}


/** @brief Send the sap message
 * 
 * @param sap_vars the sap variables
 * @param num_messages the number of sap messages
 */
void sap_send(sap_parameters_t *sap_vars, int num_messages)
{
  int curr_message;
  int sent_messages=0;
  mumudvb_sap_message_t *sap_messages;
  sap_messages=sap_vars->sap_messages;
  
  for( curr_message=0; curr_message<num_messages;curr_message++)
    {
      if(sap_messages[curr_message].to_be_sent)
	{
	  sendudp (sap_vars->sap_socketOut, &sap_vars->sap_sOut, sap_messages[curr_message].buf, sap_messages[curr_message].len);
	  sent_messages++;
	}
    }
  return;
}


/** @brief update the contents of the sap message
 * This function read the informations of the channel and update the sap message
 * @param channel : the channel to be updated
 * @param sap_vars the sap variables
 * @param curr_channel the number of the updated channel
 */
int sap_update(mumudvb_channel_t channel, sap_parameters_t *sap_vars, int curr_channel)
{
  /** @todo check PACKET Size < MTU*/
  
  //This function is called when the channel changes so it increases the version and update the packet
  char temp_string[256];

  struct in_addr ip_struct;
  in_addr_t ip;
  mumudvb_sap_message_t *sap_message;
  sap_message=&(sap_vars->sap_messages[curr_channel]);
  //paranoia
  memset(sap_message->buf,0, MAX_UDP_SIZE * sizeof (unsigned char));

  sap_message->version++;
  sap_message->buf[0]=SAP_HEADER;
  sap_message->buf[1]=SAP_HEADER2;
  sap_message->buf[2]=(sap_message->version&0xff00)>>8;
  sap_message->buf[3]=sap_message->version&0xff;

  if( inet_aton(sap_vars->sap_sending_ip, &ip_struct))
    {
      ip=ip_struct.s_addr;
      /* Bytes 4-7 (or 4-19) byte: Originating source */
      log_message(MSG_DEBUG,"DEBUG : sap sending ip address : 0x%x\n", ip);
      memcpy (sap_message->buf + 4, &ip, 4);
    }
  else
    {
      log_message(MSG_WARN,"WARNING : Invalid SAP sending Ip address, using 0.0.0.0 as Ip adress\n");
      sap_message->buf[4]=0;
      sap_message->buf[5]=0;
      sap_message->buf[6]=0;
      sap_message->buf[7]=0;
    }


  //the mime type
  sprintf(temp_string,"application/sdp");
  memcpy(sap_message->buf + 8, temp_string, strlen(temp_string));
  sap_message->len=8+strlen(temp_string);
  sap_message->buf[sap_message->len]=0;
  sap_message->len++;

  // one program per message
  if(!sap_add_program(channel, sap_vars, sap_message))
    sap_message->to_be_sent=1;
  else
    sap_message->to_be_sent=0;

  return 0;

}

/** @brief Add a program to a sap message
 * When this function is called the header of the sap message is already done
 * it adds the payload (sdp). For mare information refer to RFC 2327 and RFC 1890
 * @param channel the channel
 * @param sap_vars the sap variables
 * @param sap_message the sap message
 */
int sap_add_program(mumudvb_channel_t channel, sap_parameters_t *sap_vars, mumudvb_sap_message_t *sap_message)
{

  //See RFC 2327
  int payload_len=0;

  char temp_string[256];


  //we check if it's an alive channel
  if(!channel.streamed_channel_old)
    return 1;
  //Now we write the sdp part, in two times to avoid heavy code
  /** @section payload
  @subsection version
  v=0
  @subsection owner/creator and session identifier
  o=username session_id version network_type address_type address
  
  ex : o=mumudvb 123134 1 IN IP4 235.255.215.1

  @subsection session name (basically channel name)
  s= ...
   */
  sprintf(temp_string,"v=0\r\no=%s %d %d IN IP4 %s\r\ns=%s\r\n", 
          sap_vars->sap_organisation, sap_vars->sap_serial, sap_message->version, channel.ipOut,
          channel.name);
  if( (sap_message->len+payload_len+strlen(temp_string))>1024)
  {
    log_message(MSG_WARN,"Warning : SAP message too long for channel %s\n",channel.name);
    return 1;
  }
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, strlen(temp_string));
  payload_len+=strlen(temp_string);

    /**  @subsection URI
  ex : u=http://www.cs.ucl.ac.uk/staff/M.Handley/sdp.03.ps

  u=<URI>

  o A URI is a Universal Resource Identifier as used by WWW clients

  o The URI should be a pointer to additional information about the
  conference

  o This field is optional, but if it is present it should be specified
  before the first media field

  o No more than one URI field is allowed per session description
  */
  if(strlen(sap_vars->sap_uri))
  {
    sprintf(temp_string,"u=%s\r\n", 
            sap_vars->sap_uri);
    if( (sap_message->len+payload_len+strlen(temp_string))>1024)
    {
      log_message(MSG_WARN,"Warning : SAP message too long for channel %s\n",channel.name);
      return 1;
    }
    memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, strlen(temp_string));
    payload_len+=strlen(temp_string);

  }
  
  /**  @subsection connection information
  ex : c=IN IP4 235.214.225.1/2

  the /2 is the TTL of the media
   */
  sprintf(temp_string,"c=IN IP4 %s/%d\r\n",
          channel.ipOut, multicast_ttl);
  if( (sap_message->len+payload_len+strlen(temp_string))>1024)
  {
    log_message(MSG_WARN,"Warning : SAP message too long for channel %s\n",channel.name);
    return 1;
  }
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, strlen(temp_string));
  payload_len+=strlen(temp_string);


  /**@subsection time session : tell when the session is active
     
     t=...
     
     permanent program : t=0 0

     @subsection attributes : group and co, we'll take the minisapserver ones
     a=...

     a=tool:mumudvb-VERSION
     
     a=type:broadcast

     a=x-plgroup: //channel's group
  */
  //plopplop
  sprintf(temp_string,"t=0 0\r\na=tool:mumudvb-%s\r\na=type:broadcast\r\n", VERSION);
  if( (sap_message->len+payload_len+strlen(temp_string))>1024)
    {
      log_message(MSG_WARN,"Warning : SAP message too long for channel %s\n",channel.name);
      return 1;
    }
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, strlen(temp_string));
  payload_len+=strlen(temp_string);

  /**  @subsection media name and transport address See RFC 1890
     m=...

     Without RTP

     m=video channel_port udp 33     

     With RTP 

     m=video channel_port rtp/avp 33     

  */
  if(!rtp_header)
    sprintf(temp_string,"m=video %d udp 33\r\n", channel.portOut);
  else
    sprintf(temp_string,"m=video %d RTP/AVP 33\r\na=rtpmap:33 MP2T/90000\r\n",  channel.portOut);
  if( (sap_message->len+payload_len+strlen(temp_string))>1024)
    {
      log_message(MSG_WARN,"Warning : SAP message too long for channel %s\n",channel.name);
      return 1;
    }
  memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, strlen(temp_string));
  payload_len+=strlen(temp_string);


  if(strlen(channel.sap_group)||strlen(sap_vars->sap_default_group))
    {
      if(strlen(channel.sap_group))
	sprintf(temp_string,"a=x-plgroup:%s\r\n", channel.sap_group);
      else
	sprintf(temp_string,"a=x-plgroup:%s\r\n", sap_vars->sap_default_group);
      if( (sap_message->len+payload_len+strlen(temp_string))>1024)
	{
	  log_message(MSG_WARN,"Warning : SAP message too long for channel %s\n",channel.name);
	  return 1;
	}
      memcpy(sap_message->buf + sap_message->len + payload_len, temp_string, strlen(temp_string));
      payload_len+=strlen(temp_string);
    }
  log_message(MSG_DEBUG,"DEBUG : SAP payload\n");
  log_message(MSG_DEBUG, (char *) &sap_message->buf[sap_message->len]);
  log_message(MSG_DEBUG,"DEBUG : end of SAP payload\n\n");

  sap_message->len+=payload_len;

  return 0;

}
