/*
 * MuMuDVB - Stream a DVB transport stream.
 *
 * (C) 2008-2010 Brice DUBOST
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
 * @date 2008-2010
 */

#include "sap.h"
#include "network.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "log.h"

extern uint32_t       crc32_table[256];
static char *log_module="SAP: ";

int sap_add_program(mumudvb_channel_t *channel, sap_parameters_t *sap_vars, mumudvb_sap_message_t *sap_message4, mumudvb_sap_message_t *sap_message6, multicast_parameters_t multicast_vars);


/** @brief Read a line of the configuration file to check if there is a sap parameter
 *
 * @param sap_vars the sap parameters
 * @param substring The currrent line
 */
int read_sap_configuration(sap_parameters_t *sap_vars, mumudvb_channel_t *current_channel, int ip_ok, char *substring)
{
	char delimiteurs[] = CONFIG_FILE_SEPARATOR;
	if (!strcmp (substring, "sap"))
	{
		substring = strtok (NULL, delimiteurs);
		if(atoi (substring) != 0)
			sap_vars->sap = OPTION_ON;
		else
			sap_vars->sap = OPTION_OFF;
		if(sap_vars->sap == OPTION_ON)
		{
			log_message( log_module,  MSG_INFO,
					"Sap announces will be sent\n");
		}
	}
	else if (!strcmp (substring, "sap_interval"))
	{
		substring = strtok (NULL, delimiteurs);
		sap_vars->sap_interval = atoi (substring);
	}
	else if (!strcmp (substring, "sap_ttl"))
	{
		substring = strtok (NULL, delimiteurs);
		sap_vars->sap_ttl = atoi (substring);
	}
	else if (!strcmp (substring, "sap_organisation"))
	{
		// other substring extraction method in order to keep spaces
		substring = strtok (NULL, "=");
		strncpy(sap_vars->sap_organisation,strtok(substring,"\n"),255 - 1);
		sap_vars->sap_organisation[255]='\0';
		if ((strlen (substring) >= 255 - 1))
			log_message( log_module,  MSG_WARN,"SAP organization name too long\n");
	}
	else if (!strcmp (substring, "sap_uri"))
	{
		// other substring extraction method in order to keep spaces
		substring = strtok (NULL, "=");
		if (!(strlen (substring) >= 255 - 1))
			strcpy(sap_vars->sap_uri,strtok(substring,"\n"));
		else
		{
			log_message( log_module,  MSG_WARN,"Sap URI too long\n");
			strncpy(sap_vars->sap_uri,strtok(substring,"\n"),255 - 1);
		}
	}
	else if ((!strcmp (substring, "sap_sending_ip"))||(!strcmp (substring, "sap_sending_ip4")))
	{
		if(!strcmp (substring, "sap_sending_ip"))
			log_message( log_module,  MSG_WARN,
					"sap_sending_ip is Deprecated use sap_sending_ip4 instead");

		substring = strtok (NULL, delimiteurs);
		if(strlen(substring)>19)
		{
			log_message( log_module,  MSG_ERROR,
					"The sap sending ip is too long\n");
			return -1;
		}
		sscanf (substring, "%s\n", sap_vars->sap_sending_ip4);
	}
	else if (!strcmp (substring, "sap_sending_ip6"))
	{
		substring = strtok (NULL, delimiteurs);
		if(strlen(substring)>(IPV6_CHAR_LEN-1))
		{
			log_message( log_module,  MSG_ERROR,
					"The sap sending ipv6 is too long\n");
			return -1;
		}
		sscanf (substring, "%s\n", sap_vars->sap_sending_ip6);
	}
	else if (!strcmp (substring, "sap_group"))
	{
		if ( ip_ok == 0)
		{
			log_message( log_module,  MSG_ERROR,
					"sap_group : this is a channel option, You have to start a channel first (using ip= or channel_next)\n");
			return -1;
		}

		substring = strtok (NULL, "=");
		if(strlen(substring)>(SAP_GROUP_LENGTH-1))
		{
			log_message( log_module,  MSG_ERROR,
					"The sap group is too long\n");
			return -1;
		}
		strcpy (current_channel->sap_group, substring);
	}
	else if (!strcmp (substring, "sap_default_group"))
	{
		substring = strtok (NULL, "=");
		if(strlen(substring)>(SAP_GROUP_LENGTH-1))
		{
			log_message( log_module,  MSG_ERROR,
					"The sap default group is too long\n");
			return -1;
		}
		strcpy (sap_vars->sap_default_group, substring);
	}
	else
		return 0; //Nothing concerning sap, we return 0 to explore the other possibilities

	return 1;//We found something for sap, we tell main to go for the next line

}


/** @brief init the sap
 * Alloc the memory for the messages, open the socket
 */
int init_sap(sap_parameters_t *sap_vars, multicast_parameters_t multicast_vars)
{
	if(sap_vars->sap == OPTION_ON)
	{
		if(multicast_vars.multicast_ipv4)
		{
			log_message( log_module,  MSG_DETAIL,  "init sap v4\n");
			sap_vars->sap_messages4=malloc(sizeof(mumudvb_sap_message_t)*MAX_CHANNELS);
			if(sap_vars->sap_messages4==NULL)
			{
				log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
				return -1;
			}
			memset (sap_vars->sap_messages4, 0, sizeof( mumudvb_sap_message_t)*MAX_CHANNELS);//we clear it
			//For sap announces, we open the socket
			//See the README about multicast_auto_join
			if(multicast_vars.auto_join)
				sap_vars->sap_socketOut4 =  makeclientsocket (SAP_IP4, SAP_PORT, sap_vars->sap_ttl, multicast_vars.iface4, &sap_vars->sap_sOut4);
			else
				sap_vars->sap_socketOut4 =  makesocket (SAP_IP4, SAP_PORT, sap_vars->sap_ttl, multicast_vars.iface4, &sap_vars->sap_sOut4);
		}
		if(multicast_vars.multicast_ipv6)
		{
			log_message( log_module,  MSG_DETAIL,  "init sap v6\n");
			sap_vars->sap_messages6=malloc(sizeof(mumudvb_sap_message_t)*MAX_CHANNELS);
			if(sap_vars->sap_messages6==NULL)
			{
				log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
				return -1;
			}
			memset (sap_vars->sap_messages6, 0, sizeof( mumudvb_sap_message_t)*MAX_CHANNELS);//we clear it
			//For sap announces, we open the socket
			//See the README about multicast_auto_join
			if(multicast_vars.auto_join)
				sap_vars->sap_socketOut6 =  makeclientsocket6 (SAP_IP6, SAP_PORT, sap_vars->sap_ttl, multicast_vars.iface6, &sap_vars->sap_sOut6);
			else
				sap_vars->sap_socketOut6 =  makesocket6 (SAP_IP6, SAP_PORT, sap_vars->sap_ttl, multicast_vars.iface6, &sap_vars->sap_sOut6);
		}
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
	mumudvb_sap_message_t *sap_messages4;
	mumudvb_sap_message_t *sap_messages6;
	sap_messages4=sap_vars->sap_messages4;
	sap_messages6=sap_vars->sap_messages6;

	for( curr_message=0; curr_message<num_messages;curr_message++)
	{
		if(sap_messages4 && sap_messages4[curr_message].to_be_sent)
			sendudp (sap_vars->sap_socketOut4, &sap_vars->sap_sOut4, sap_messages4[curr_message].buf, sap_messages4[curr_message].len);
		if(sap_messages6 && sap_messages6[curr_message].to_be_sent)
			sendudp6 (sap_vars->sap_socketOut6, &sap_vars->sap_sOut6, sap_messages6[curr_message].buf, sap_messages6[curr_message].len);
	}
	return;
}


/** @brief update the contents of the sap message
 * This function read the informations of the channel and update the sap message
 * @param channel : the channel to be updated
 * @param sap_vars the sap variables
 * @param curr_channel the number of the updated channel
 */
int sap_update(mumudvb_channel_t *channel, sap_parameters_t *sap_vars, int curr_channel, multicast_parameters_t multicast_vars)
{
	/** @todo check PACKET Size < MTU*/
	//This function is called when the channel changes so it increases the version and update the packet
	char temp_string[256];

	struct in_addr ip_struct4;
	struct sockaddr_in6 ip_struct6;
	in_addr_t ip4;
	struct in6_addr ip6;
	mumudvb_sap_message_t *sap_message4=NULL;
	mumudvb_sap_message_t *sap_message6=NULL;
	if(channel->socketOut4)
	{
		sap_message4=&(sap_vars->sap_messages4[curr_channel]);
		//paranoia
		memset(sap_message4->buf,0, MAX_UDP_SIZE * sizeof (unsigned char));
		sap_message4->version=(sap_message4->version+1)&0x000f;
		sap_message4->buf[0]=SAP_HEADER4_BYTE0;
		sap_message4->buf[1]=SAP_HEADER4_BYTE1;
		//Hash of SAP message: see end of this function
		sap_message4->buf[2]=0;
		sap_message4->buf[3]=0;
	}
	if(channel->socketOut6)
	{
		sap_message6=&(sap_vars->sap_messages6[curr_channel]);
		//paranoia
		memset(sap_message6->buf,0, MAX_UDP_SIZE * sizeof (unsigned char));
		sap_message6->version=(sap_message6->version+1)&0x000f;
		sap_message6->buf[0]=SAP_HEADER6_BYTE0;
		sap_message6->buf[1]=SAP_HEADER6_BYTE1;
		//Hash of SAP message: see end of this function
		sap_message6->buf[2]=0;
		sap_message6->buf[3]=0;
	}



	if(channel->socketOut4)
	{
		if( inet_aton(sap_vars->sap_sending_ip4, &ip_struct4))
		{
			ip4=ip_struct4.s_addr;
			/* Bytes 4-7 (or 4-19) byte: Originating source */
			log_message( log_module, MSG_DEBUG,"sap sending ipv4  address : %s (binary : 0x%x)\n",sap_vars->sap_sending_ip4, ip4);
			memcpy (sap_message4->buf + 4, &ip4, 4);
		}
		else
		{
			log_message( log_module, MSG_WARN,"Invalid SAP sending Ip address, using 0.0.0.0 as Ip address\n");
			sap_message4->buf[4]=0;
			sap_message4->buf[5]=0;
			sap_message4->buf[6]=0;
			sap_message4->buf[7]=0;
		}
	}
	if(channel->socketOut6)
	{
		if( inet_pton(AF_INET6, sap_vars->sap_sending_ip6, &ip_struct6))
		{
			ip6=ip_struct6.sin6_addr;
			log_message( log_module, MSG_DEBUG,"sap sending ipv6  address : %s\n",sap_vars->sap_sending_ip6);
			/* Bytes 4-7 (or 4-19) byte: Originating source */
			memcpy (sap_message6->buf + 4, &ip6.s6_addr, 16);
		}
		else
		{
			log_message( log_module, MSG_WARN,"Invalid SAP sending IPv6 address, using :: as IPv6 address\n");
			memset(sap_message6->buf+4,0,16*sizeof(char));
		}
	}

	//the mime type
	sprintf(temp_string,"application/sdp");
	if(channel->socketOut4)
	{
		memcpy(sap_message4->buf + SAP_HEAD_LEN4, temp_string, strlen(temp_string));
		sap_message4->len=SAP_HEAD_LEN4+strlen(temp_string);
		sap_message4->buf[sap_message4->len]=0;
		sap_message4->len++;
	}
	if(channel->socketOut6)
	{
		memcpy(sap_message6->buf + SAP_HEAD_LEN6, temp_string, strlen(temp_string));
		sap_message6->len=SAP_HEAD_LEN6+strlen(temp_string);
		sap_message6->buf[sap_message6->len]=0;
		sap_message6->len++;
	}
	// one program per message
	sap_add_program(channel, sap_vars, sap_message4, sap_message6, multicast_vars);


	//we compute the CRC32 of the message in order to generate a hash
	unsigned long crc32;
	int i;
	crc32=0xffffffff;
	if(channel->socketOut4)
	{
		for(i = 0; i < sap_message4->len-1; i++) {
			crc32 = (crc32 << 8) ^ crc32_table[((crc32 >> 24) ^ sap_message4->buf[i])&0xff];
		}
		//Hash of SAP message : we use the CRC32 that we merge onto 16bits
		sap_message4->buf[2]=(((crc32>>24) & 0xff)+((crc32>>16) & 0xff)) & 0xff;
		sap_message4->buf[3]=(((crc32>>8) & 0xff)+(crc32 & 0xff)) & 0xff;
	}
	crc32=0xffffffff;
	if(channel->socketOut6)
	{
		for(i = 0; i < sap_message6->len-1; i++) {
			crc32 = (crc32 << 8) ^ crc32_table[((crc32 >> 24) ^ sap_message6->buf[i])&0xff];
		}
		//Hash of SAP message : we use the CRC32 that we merge onto 16bits
		sap_message6->buf[2]=(((crc32>>24) & 0xff)+((crc32>>16) & 0xff)) & 0xff;
		sap_message6->buf[3]=(((crc32>>8) & 0xff)+(crc32 & 0xff)) & 0xff;
	}
	return 0;

}

/** @brief Add a program to a sap message
 * When this function is called the header of the sap message is already done
 * it adds the payload (sdp). For mare information refer to RFC 2327 and RFC 1890
 * @param channel the channel
 * @param sap_vars the sap variables
 * @param sap_message the sap message
 */
int sap_add_program(mumudvb_channel_t *channel, sap_parameters_t *sap_vars, mumudvb_sap_message_t *sap_message4, mumudvb_sap_message_t *sap_message6, multicast_parameters_t multicast_vars)
{

	//See RFC 2327
	mumu_string_t payload4;
	payload4.string=NULL;
	payload4.length=0;
	mumu_string_t payload6;
	payload6.string=NULL;
	payload6.length=0;

	if(sap_message4)
		sap_message4->to_be_sent=0;
	if(sap_message6)
		sap_message6->to_be_sent=0;
	//we check if it's an alive channel
	if(!channel->streamed_channel)
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
	if(channel->socketOut4)
		mumu_string_append(&payload4,
				"v=0\r\no=%s %d %d IN IP4 %s\r\ns=%s\r\n",
				sap_vars->sap_organisation, sap_vars->sap_serial, sap_message4->version, channel->ip4Out,
				channel->name);
	if(channel->socketOut6)
		mumu_string_append(&payload6,
				"v=0\r\no=%s %d %d IN IP6 %s\r\ns=%s\r\n",
				sap_vars->sap_organisation, sap_vars->sap_serial, sap_message6->version, channel->ip6Out,
				channel->name);


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

		if(channel->socketOut4)
			mumu_string_append(&payload4,
					"u=%s\r\n",
					sap_vars->sap_uri);
		if(channel->socketOut6)
			mumu_string_append(&payload6,
					"u=%s\r\n",
					sap_vars->sap_uri);

	}

	/**  @subsection connection information
  ex : c=IN IP4 235.214.225.1/2

  the /2 is the TTL of the media (IPv4 only)
	 */

	if(channel->socketOut4)
		mumu_string_append(&payload4,
				"c=IN IP4 %s/%d\r\n",
				channel->ip4Out, multicast_vars.ttl);
	if(channel->socketOut6)
		mumu_string_append(&payload6,
				"c=IN IP6 %s\r\n",
				channel->ip6Out);


	/**@subsection time session : tell when the session is active

     t=...

     permanent program : t=0 0

     @subsection attributes : group and co, we'll take the minisapserver ones
     a=...

     a=tool:mumudvb-VERSION

     a=type:broadcast

     a=x-plgroup: //channel's group
	 */

	if(channel->socketOut4)
		mumu_string_append(&payload4,"t=0 0\r\na=tool:mumudvb-%s\r\na=type:broadcast\r\n", VERSION);
	if(channel->socketOut6)
		mumu_string_append(&payload6,"t=0 0\r\na=tool:mumudvb-%s\r\na=type:broadcast\r\n", VERSION);


	/**  @subsection "source-filter" attribute
       RFC 4570 SDP Source Filters
       ex : a=source-filter: incl IN IP4 235.255.215.1 192.168.1.1
       only defined when the sending ip address is defined
	 */

	if(channel->socketOut4)
	{
		struct in_addr ip_struct4;
		if( inet_aton(sap_vars->sap_sending_ip4, &ip_struct4) && ip_struct4.s_addr)
			mumu_string_append(&payload4,
					"a=source-filter: incl IN IP4 %s %s\r\n", channel->ip4Out, sap_vars->sap_sending_ip4);
	}
	if(channel->socketOut6)
	{
		struct sockaddr_in6 ip_struct6;
		if( inet_pton(AF_INET6, sap_vars->sap_sending_ip6, &ip_struct6))
		{
			//ugly way to test non zero ipv6 addr but I didn found a better one
			u_int8_t  *s6;
			s6=ip_struct6.sin6_addr.s6_addr;
			if(s6[0]||s6[1]||s6[2]||s6[3]||s6[4]||s6[5]||s6[6]||s6[7]||s6[8]||s6[9]||s6[10]||s6[11]||s6[12]||s6[13]||s6[14]||s6[15])
				mumu_string_append(&payload6,
						"a=source-filter: incl IP6 %s %s\r\n", channel->ip6Out, sap_vars->sap_sending_ip6);
		}
	}

	/**@subsection channel's group
    a=cat channel's group
    a=x-plgroup backward compatibility
	 */
	if(strlen(channel->sap_group)||strlen(sap_vars->sap_default_group))
	{
		if(!strlen(channel->sap_group))
		{
			int len=SAP_GROUP_LENGTH;
			strcpy(channel->sap_group,sap_vars->sap_default_group);
			mumu_string_replace(channel->sap_group,&len,0,"%type",simple_service_type_to_str(channel->channel_type) );
		}
		if(channel->socketOut4)
			mumu_string_append(&payload4,"a=cat:%s\r\n", channel->sap_group);
		/* backward compatibility with VLC 0.7.3-2.0.0 senders */
		mumu_string_append(&payload4,"a=x-plgroup:%s\r\n", channel->sap_group);
		if(channel->socketOut6)
			mumu_string_append(&payload6,"a=cat:%s\r\n", channel->sap_group);
		/* backward compatibility with VLC 0.7.3-2.0.0 senders */
		mumu_string_append(&payload6,"a=x-plgroup:%s\r\n", channel->sap_group);
	}

	/**  @subsection media name and transport address See RFC 1890
     m=...

     Without RTP

     m=video channel_port udp 33     

     With RTP 

     m=video channel_port rtp/avp 33     

	 */
	if(!multicast_vars.rtp_header)
	{
		if(channel->socketOut4)
			mumu_string_append(&payload4,"m=video %d udp 33\r\n", channel->portOut);
		if(channel->socketOut6)
			mumu_string_append(&payload6,"m=video %d udp 33\r\n", channel->portOut);
	}
	else
	{
		if(channel->socketOut4)
			mumu_string_append(&payload4,"m=video %d RTP/AVP 33\r\na=rtpmap:33 MP2T/90000\r\n",  channel->portOut);
		if(channel->socketOut6)
			mumu_string_append(&payload6,"m=video %d RTP/AVP 33\r\na=rtpmap:33 MP2T/90000\r\n",  channel->portOut);
	}



	if(channel->socketOut4)
	{
		if( (sap_message4->len+payload4.length)>1024)
		{
			log_message( log_module, MSG_WARN,"SAP message v4 too long for channel %s\n",channel->name);
			goto epicfail;
		}
		memcpy(sap_message4->buf + sap_message4->len, payload4.string, payload4.length);
		log_message( log_module, MSG_DEBUG,"SAP payload v4");
		log_message( log_module, MSG_DEBUG, "%s", (char *) &sap_message4->buf[sap_message4->len]);
		log_message( log_module, MSG_DEBUG,"end of SAP payload v4");
		sap_message4->len+=payload4.length;
		sap_message4->to_be_sent=1;

	}
	if(channel->socketOut6)
	{
		if( (sap_message6->len+payload6.length)>1024)
		{
			log_message( log_module, MSG_WARN,"SAP message v4 too long for channel %s\n",channel->name);
			goto epicfail;
		}
		memcpy(sap_message6->buf + sap_message6->len, payload6.string, payload6.length);
		log_message( log_module, MSG_DEBUG,"SAP payload v6");
		log_message( log_module, MSG_DEBUG, "%s", (char *) &sap_message6->buf[sap_message6->len]);
		log_message( log_module, MSG_DEBUG,"end of SAP payload v6");
		sap_message6->len+=payload6.length;
		sap_message6->to_be_sent=1;
	}

	mumu_free_string(&payload4);
	mumu_free_string(&payload6);


	return 0;

	epicfail:
	mumu_free_string(&payload4);
	mumu_free_string(&payload6);
	return 1;

}


/** @brief Sap function called periodically
 * This function checks if there is sap messages to send
 * @param number_of_channels the number of channels
 * @param channels the channels
 * @param sap_vars the sap variables
 * @param multicast_vars the multicast variables
 * @param now the time
 */
void sap_poll(sap_parameters_t *sap_vars,int number_of_channels,mumudvb_channel_t  *channels, multicast_parameters_t multicast_vars, long now)
{
	int curr_channel;
	//we check if SAP is initialised
	if(sap_vars->sap_messages4==NULL && sap_vars->sap_messages6==NULL)
		return;
	if(sap_vars->sap == OPTION_ON)
	{
		if(!sap_vars->sap_last_time_sent)
		{
			// it's the first time we are here, we initialize all the channels
			for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
				sap_update(&channels[curr_channel], sap_vars, curr_channel, multicast_vars);
			sap_vars->sap_last_time_sent=now-sap_vars->sap_interval-1;
		}
		if((now-sap_vars->sap_last_time_sent)>=sap_vars->sap_interval)
		{
			sap_send(sap_vars, number_of_channels);
			sap_vars->sap_last_time_sent=now;
		}
	}
}
