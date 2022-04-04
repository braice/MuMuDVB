/*
 * MuMuDVB - Stream a DVB transport stream.
 *
 * (C) 2009-2013 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.net
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
 * @brief File for HTTP unicast
 * @author Brice DUBOST
 * @date 2009-2013
 */

//in order to use asprintf (extension gnu)
#define _GNU_SOURCE
#define _CRT_SECURE_NO_WARNINGS

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#else
#define write(sock, buf, size) send(sock, buf, size, 0)
#define close(sock) closesocket(sock)
#endif
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#include <strings.h>
#include <sys/time.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#include "unicast_http.h"
#include "unicast_queue.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"
#include "dvb.h"
#include "tune.h"
#include "autoconf.h"
#include "rewrite.h"
#ifdef ENABLE_CAM_SUPPORT
#include "cam.h"
#endif
#ifdef ENABLE_SCAM_SUPPORT
#include "scam_capmt.h"
#include "scam_common.h"
#include "scam_getcw.h"
#include "scam_decsa.h"
#endif

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

static char *log_module="Unicast : ";

//from unicast_client.c
unicast_client_t *unicast_add_client(unicast_parameters_t *unicast_vars, struct sockaddr_in SocketAddr, int Socket);
int channel_add_unicast_client(unicast_client_t *client,mumudvb_channel_t *channel);

unicast_client_t *unicast_accept_connection(unicast_parameters_t *unicast_vars, int socketIn);
void unicast_close_connection(unicast_parameters_t *unicast_vars, int Socket);

int
unicast_send_streamed_channels_list (int number_of_channels, mumudvb_channel_t *channels, int Socket, char *host);
int
unicast_send_index_page  (int Socket);
int
unicast_send_play_list_unicast (int number_of_channels, mumudvb_channel_t *channels, int Socket, int unicast_portOut, int perport, unicast_parameters_t *unicast_vars);
int
unicast_send_play_list_multicast (int number_of_channels, mumudvb_channel_t* channels, int Socket, int vlc, unicast_parameters_t *unicast_vars);
int
unicast_send_streamed_channels_list_js (int number_of_channels, mumudvb_channel_t *channels, void* cam_p_v, int Socket);
int
unicast_send_signal_power_js (int Socket, strength_parameters_t *strengthparams);
int
unicast_send_channel_traffic_js (int number_of_channels, mumudvb_channel_t *channels, int Socket);
int
unicast_send_json_state (int number_of_channels, mumudvb_channel_t* channels, int Socket, strength_parameters_t* strengthparams, auto_p_t* auto_p, void* cam_p_v, void* scam_vars_v);
int
unicast_send_prometheus (int number_of_channels, mumudvb_channel_t* channels, int Socket, strength_parameters_t* strengthparams);
int
unicast_send_xml_state (int number_of_channels, mumudvb_channel_t* channels, int Socket, strength_parameters_t* strengthparams, auto_p_t* auto_p, void* cam_p_v, void* scam_vars_v);
int
unicast_send_cam_menu (int Socket, void *cam_p);
int
unicast_send_cam_action (int Socket, char *Key, void *cam_p);
int
unicast_send_EIT (eit_packet_t *eit_packets, int Socket);


int unicast_handle_message(unicast_parameters_t* unicast_vars,
		unicast_client_t* client,
		mumudvb_channel_t* channels,
		int number_of_channels,
		strength_parameters_t* strengthparams,
		auto_p_t* auto_p,
		void* cam_p,
		void* scam_vars,
		eit_packet_t *eit_packets);

#define REPLY_HEADER 0
#define REPLY_BODY 1
#define REPLY_SIZE_STEP 256


/** Initialize unicast variables*/
void init_unicast_v(unicast_parameters_t *unicast_vars)
{
	memset(unicast_vars,0,sizeof(unicast_parameters_t));
	 *unicast_vars=(unicast_parameters_t){
				.unicast=0,
				.ipOut="0.0.0.0",
				.portOut=4242,
				.portOut_str=NULL,
				.consecutive_errors_timeout=UNICAST_CONSECUTIVE_ERROR_TIMEOUT,
				.max_clients=-1,
				.queue_max_size=UNICAST_DEFAULT_QUEUE_MAX,
				.socket_sendbuf_size=0,
				.flush_on_eagain=0,
				.pfdsnum=0,
				.playlist_ignore_dead=0,
				.playlist_ignore_scrambled_ratio=0,
	 };
	 unicast_vars->pfds=NULL;
	 //+1 for closing the pfd list, see man poll
	 unicast_vars->pfds=malloc(sizeof(struct pollfd));
	 if (unicast_vars->pfds==NULL)
	 {
		 log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		 set_interrupted(ERROR_MEMORY<<8);
		 return;
	 }
	 unicast_vars->pfds[0].fd = 0;
	 unicast_vars->pfds[0].events = POLLIN | POLLPRI;
	 unicast_vars->pfds[0].revents = 0;

}



/** @brief Read a line of the configuration file to check if there is a unicast parameter
 *
 * @param unicast_vars the unicast parameters
 * @param substring The currrent line
 */
int read_unicast_configuration(unicast_parameters_t *unicast_vars, mumudvb_channel_t *c_chan, char *substring)
{

	char delimiteurs[] = CONFIG_FILE_SEPARATOR;

	if (!strcmp (substring, "ip_http"))
	{
		substring = strtok (NULL, delimiteurs);
		if(strlen(substring)>19)
		{
			log_message( log_module,  MSG_ERROR,
					"The Ip address %s is too long.\n", substring);
			exit(ERROR_CONF);
		}
		sscanf (substring, "%s\n", unicast_vars->ipOut);
		if(unicast_vars->ipOut[0]!='\0')
		{
			if(unicast_vars->unicast==0)
			{
				log_message( log_module,  MSG_WARN,"You should use the option \"unicast=1\" before to activate unicast instead of ip_http\n");
				unicast_vars->unicast=1;
			}
		}
	}
	else if (!strcmp (substring, "unicast"))
	{
		substring = strtok (NULL, delimiteurs);
		unicast_vars->unicast = atoi (substring);
	}
	else if (!strcmp (substring, "unicast_consecutive_errors_timeout"))
	{
		substring = strtok (NULL, delimiteurs);
		unicast_vars->consecutive_errors_timeout = atoi (substring);
		if(unicast_vars->consecutive_errors_timeout<=0)
			log_message( log_module,  MSG_WARN,
					"Warning : You have deactivated the unicast timeout for disconnecting clients, this can lead to an accumulation of zombie clients, this is unadvised, prefer a long timeout\n");
	}
	else if (!strcmp (substring, "unicast_max_clients"))
	{
		substring = strtok (NULL, delimiteurs);
		unicast_vars->max_clients = atoi (substring);
	}
	else if (!strcmp (substring, "unicast_queue_size"))
	{
		substring = strtok (NULL, delimiteurs);
		unicast_vars->queue_max_size = atoi (substring);
	}
	else if (!strcmp (substring, "port_http"))
	{
		substring = strtok (NULL, "=");

		// next we replace all the spaces as too many spaces are messing with the further parsing
		// like: "     port_http            = 2000 + %card"
		int len = strlen(substring)+1;
		substring = mumu_string_replace(substring, &len, 1, " ", "");

		// if the string is empty after the replacement, we need to tokenize one more time to get the actual setting
		if (strlen(substring) == 0) {
			substring = strtok(NULL, "=");
		}

		if((strchr(substring,'*')!=NULL)||(strchr(substring,'+')!=NULL)||(strchr(substring,'%')!=NULL))
		{
			unicast_vars->portOut_str=malloc(sizeof(char)*(strlen(substring)+1));
			strcpy(unicast_vars->portOut_str,substring);
		}
		else
			unicast_vars->portOut = atoi (substring);
	}
	else if (!strcmp (substring, "unicast_port"))
	{
		if ( c_chan == NULL )
		{
			log_message( log_module,  MSG_ERROR,
					"unicast_port : You have to start a channel first (using new_channel)\n");
			exit(ERROR_CONF);
		}
		substring = strtok (NULL, delimiteurs);
		c_chan->unicast_port = atoi (substring);
		MU_F(c_chan->unicast_port) = F_USER;
	}
	else if (!strcmp (substring, "socket_sendbuf_size"))
	{
		substring = strtok (NULL, delimiteurs);
		unicast_vars->socket_sendbuf_size = atoi (substring);
	}
	else if (!strcmp (substring, "flush_on_eagain"))
	{
		substring = strtok (NULL, delimiteurs);
		unicast_vars->flush_on_eagain = atoi (substring);
		if(unicast_vars->flush_on_eagain)
			log_message( log_module,  MSG_INFO, "The unicast data WILL be dropped on eagain errors\n");
	}
	else if (!strcmp (substring, "playlist_ignore_dead"))
	{
		substring = strtok (NULL, delimiteurs);
		unicast_vars->playlist_ignore_dead = atoi (substring);
	}
	else if (!strcmp (substring, "playlist_ignore_scrambled_ratio"))
	{
		substring = strtok (NULL, delimiteurs);
		unicast_vars->playlist_ignore_scrambled_ratio = atoi (substring);
		if (unicast_vars->playlist_ignore_scrambled_ratio > 100) {
                        log_message( log_module,  MSG_WARN,"Scrambled ignore ratio \"%d\" is over 100 percent, forcing to 100!\n", unicast_vars->playlist_ignore_scrambled_ratio);
                        unicast_vars->playlist_ignore_scrambled_ratio = 100;
                }

	}

	else
		return 0; //Nothing concerning tuning, we return 0 to explore the other possibilities

	return 1;//We found something for tuning, we tell main to go for the next line

}



/** @brief Create a listening socket and add it to the list of polling file descriptors if success
 *
 *
 *
 */
int unicast_create_listening_socket(int socket_type, int socket_channel, char *ipOut, int port, struct sockaddr_in *sIn, int *socketIn, unicast_parameters_t *unicast_vars)
{
	*socketIn= makeTCPclientsocket(ipOut, port, sIn);
	//We add them to the poll descriptors
	if(*socketIn>0)
	{
		unicast_vars->pfdsnum++;
		log_message( log_module, MSG_DEBUG, "unicast : unicast_vars->pfdsnum : %d\n", unicast_vars->pfdsnum);
		unicast_vars->pfds=realloc(unicast_vars->pfds,(unicast_vars->pfdsnum+1)*sizeof(struct pollfd));
		if (unicast_vars->pfds==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			return -1;
		}
		unicast_vars->pfds[unicast_vars->pfdsnum-1].fd = *socketIn;
		unicast_vars->pfds[unicast_vars->pfdsnum-1].events = POLLIN | POLLPRI;
		unicast_vars->pfds[unicast_vars->pfdsnum-1].revents = 0;
		unicast_vars->pfds[unicast_vars->pfdsnum].fd = 0;
		unicast_vars->pfds[unicast_vars->pfdsnum].events = POLLIN | POLLPRI;
		unicast_vars->pfds[unicast_vars->pfdsnum].revents = 0;
		//Information about the descriptor
		unicast_vars->fd_info=realloc(unicast_vars->fd_info,(unicast_vars->pfdsnum)*sizeof(unicast_fd_info_t));
		if (unicast_vars->fd_info==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			return -1;
		}
		//Master connection
		unicast_vars->fd_info[unicast_vars->pfdsnum-1].type=socket_type;
		unicast_vars->fd_info[unicast_vars->pfdsnum-1].channel=socket_channel;
		unicast_vars->fd_info[unicast_vars->pfdsnum-1].client=NULL;
	}
	else
	{
		log_message( log_module,  MSG_WARN, "Problem creating the socket %s:%d : %s\n",ipOut,port,strerror(errno) );
		return -1;
	}

	return 0;

}

/** @brief Handle an "event" on the unicast file descriptors
 * If the event is on an already open client connection, it handle the message
 * If the event is on the master connection, it accepts the new connection
 * If the event is on a channel specific socket, it accepts the new connection and starts streaming
 *
 */
int unicast_handle_fd_event(unicast_parameters_t *unicast_vars,
		mumudvb_channel_t *channels,
		int number_of_channels,
		strength_parameters_t *strengthparams,
		auto_p_t *auto_p,
		void *cam_p,
		void *scam_vars,
		eit_packet_t *eit_packets)
{
	int iRet;
	//We look what happened for which connection
	int actual_fd;


	for(actual_fd=0;actual_fd<unicast_vars->pfdsnum;actual_fd++)
	{
		iRet=0;
		if(((unicast_vars->pfds[actual_fd].revents&POLLHUP)||(unicast_vars->pfds[actual_fd].revents&POLLERR))
				&&(unicast_vars->fd_info[actual_fd].type==UNICAST_CLIENT))
		{
			log_message( log_module, MSG_DEBUG,"We've got a POLLHUP or POLLERR. Actual_fd %d socket %d we close the connection \n", actual_fd, unicast_vars->pfds[actual_fd].fd );
			unicast_close_connection(unicast_vars,unicast_vars->pfds[actual_fd].fd);
			//We check if we have to parse unicast_vars->pfds[actual_fd].revents (the last fd moved to the actual one)
			if(unicast_vars->pfds[actual_fd].revents)
				actual_fd--;//Yes, we force the loop to see it
		}
		if((unicast_vars->pfds[actual_fd].revents&POLLIN)||(unicast_vars->pfds[actual_fd].revents&POLLPRI))
		{
			if((unicast_vars->fd_info[actual_fd].type==UNICAST_MASTER)||
					(unicast_vars->fd_info[actual_fd].type==UNICAST_LISTEN_CHANNEL))
			{
				//Event on the master connection or listening channel
				//New connection, we accept the connection
				log_message( log_module, MSG_FLOOD,"New client\n");
				int tempSocket;
				unicast_client_t *tempClient;
				//we accept the incoming connection
				tempClient=unicast_accept_connection(unicast_vars, unicast_vars->pfds[actual_fd].fd);

				if(tempClient!=NULL)
				{
					tempSocket=tempClient->Socket;
					unicast_vars->pfdsnum++;
					unicast_vars->pfds=realloc(unicast_vars->pfds,(unicast_vars->pfdsnum+1)*sizeof(struct pollfd));
					if (unicast_vars->pfds==NULL)
					{
						log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
						set_interrupted(ERROR_MEMORY<<8);
						return -1;
					}
					//We poll the new socket
					unicast_vars->pfds[unicast_vars->pfdsnum-1].fd = tempSocket;
					unicast_vars->pfds[unicast_vars->pfdsnum-1].events = POLLIN | POLLPRI | POLLHUP | POLLERR; //We also poll the deconnections
					unicast_vars->pfds[unicast_vars->pfdsnum-1].revents = 0;
					unicast_vars->pfds[unicast_vars->pfdsnum].fd = 0;
					unicast_vars->pfds[unicast_vars->pfdsnum].events = POLLIN | POLLPRI;
					unicast_vars->pfds[unicast_vars->pfdsnum].revents = 0;

					//Information about the descriptor
					unicast_vars->fd_info=realloc(unicast_vars->fd_info,(unicast_vars->pfdsnum)*sizeof(unicast_fd_info_t));
					if (unicast_vars->fd_info==NULL)
					{
						log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
						set_interrupted(ERROR_MEMORY<<8);
						return -1;
					}
					//client connection
					unicast_vars->fd_info[unicast_vars->pfdsnum-1].type=UNICAST_CLIENT;
					unicast_vars->fd_info[unicast_vars->pfdsnum-1].channel=-1;
					unicast_vars->fd_info[unicast_vars->pfdsnum-1].client=tempClient;


					log_message( log_module, MSG_FLOOD,"Number of clients : %d\n", unicast_vars->client_number);

					if(unicast_vars->fd_info[actual_fd].type==UNICAST_LISTEN_CHANNEL)
					{
						//Event on a channel connection, we open a new socket for this client and
						//we store the wanted channel for when we will get the GET
						log_message( log_module, MSG_DEBUG,"Connection on a channel socket the client  will get the channel %d\n", unicast_vars->fd_info[actual_fd].channel);
						tempClient->askedChannel=unicast_vars->fd_info[actual_fd].channel;
					}
				}
			}
			else if(unicast_vars->fd_info[actual_fd].type==UNICAST_CLIENT)
			{
				//Event on a client connection i.e. the client asked something
				log_message( log_module, MSG_FLOOD,"New message for socket %d\n", unicast_vars->pfds[actual_fd].fd);
				iRet=unicast_handle_message(unicast_vars,unicast_vars->fd_info[actual_fd].client, channels, number_of_channels, strengthparams, auto_p, cam_p, scam_vars,eit_packets);
				if (iRet==-2 ) //iRet==-2 --> 0 received data or error, we close the connection
				{
					unicast_close_connection(unicast_vars,unicast_vars->pfds[actual_fd].fd);
					//We check if we have to parse unicast_vars->pfds[actual_fd].revents (the last fd moved to the actual one)
					if(unicast_vars->pfds[actual_fd].revents)
						actual_fd--;//Yes, we force the loop to see it again
				}
			}
			else
			{
				log_message( log_module, MSG_WARN,"File descriptor with bad type, please contact\n Debug information : actual_fd %d unicast_vars->fd_info[actual_fd].type %d\n",
						actual_fd, unicast_vars->fd_info[actual_fd].type);
			}
		}
	}
	return 0;

}




/** @brief Accept an incoming connection
 *
 *
 * @param unicast_vars the unicast parameters
 * @param socketIn the socket on wich the connection was made
 */
unicast_client_t *unicast_accept_connection(unicast_parameters_t *unicast_vars, int socketIn)
{

	unsigned int l;
	int tempSocket,iRet;
	unicast_client_t *tempClient;
	struct sockaddr_in tempSocketAddrIn;
	char addr_buf[64];
	char addr_buf2[64];

	l = sizeof(struct sockaddr);
	tempSocket = accept(socketIn, (struct sockaddr *) &tempSocketAddrIn, &l);
	if (tempSocket < 0 )
	{
		log_message( log_module, MSG_WARN,"Error when accepting the incoming connection : %s\n", strerror(errno));
		return NULL;
	}
	struct sockaddr_in tempSocketAddr;
	l = sizeof(struct sockaddr);
	iRet=getsockname(tempSocket, (struct sockaddr *) &tempSocketAddr, &l);
	if (iRet < 0)
	{
		log_message( log_module,  MSG_ERROR,"getsockname failed : %s while accepting incoming connection", strerror(errno));
		close(tempSocket);
		return NULL;
	}
	inet_ntop(AF_INET, &tempSocketAddrIn.sin_addr, addr_buf, sizeof(addr_buf));
	inet_ntop(AF_INET, &tempSocketAddr.sin_addr, addr_buf2, sizeof(addr_buf2));
	log_message( log_module, MSG_FLOOD,"New connection from %s:%d to %s:%d \n", addr_buf, tempSocketAddrIn.sin_port, addr_buf2, tempSocketAddr.sin_port);

	//Now we set this socket to be non blocking because we poll it
	int flags;
#ifndef _WIN32
	flags = fcntl(tempSocket, F_GETFL, 0);
	flags |= O_NONBLOCK;
	if (fcntl(tempSocket, F_SETFL, flags) < 0)
	{
		log_message( log_module, MSG_ERROR,"Set non blocking failed : %s\n",strerror(errno));
		close(tempSocket);
		return NULL;
	}
#else
	uint32_t iMode = 0;
	flags = ioctlsocket(tempSocket, FIONBIO, &iMode);
	if (flags != NO_ERROR) {
		log_message(log_module, MSG_ERROR, "Set non blocking failed : %s\n", strerror(errno));
		close(tempSocket);
		return NULL;
	}
#endif

	/* if the maximum number of clients is reached, raise a temporary error*/
	if((unicast_vars->max_clients>0)&&(unicast_vars->client_number>=unicast_vars->max_clients))
	{
		int iRet;

		inet_ntop(AF_INET, &tempSocketAddrIn.sin_addr, addr_buf, sizeof(addr_buf));
		log_message( log_module, MSG_INFO,"Too many clients connected, we raise an error to  %s\n", addr_buf);
		iRet = write(tempSocket,HTTP_503_REPLY, strlen(HTTP_503_REPLY));
		if (iRet < 0) {
			inet_ntop(AF_INET, &tempSocketAddrIn.sin_addr, addr_buf, sizeof(addr_buf));
			log_message(log_module, MSG_INFO, "Error writing to %s\n", addr_buf);
		}
		close(tempSocket);
		return NULL;
	}

	tempClient=unicast_add_client(unicast_vars, tempSocketAddrIn, tempSocket);

	return tempClient;

}


/** @brief Close an unicast connection and delete the client
 *
 * @param unicast_vars the unicast parameters
 * @param fds The polling file descriptors
 * @param Socket The socket of the client we want to disconnect
 */
void unicast_close_connection(unicast_parameters_t *unicast_vars, int Socket)
{

	int actual_fd;
	actual_fd=0;
	//We find the FD correspondig to this client
	while((actual_fd<unicast_vars->pfdsnum) && (unicast_vars->pfds[actual_fd].fd!=Socket))
		actual_fd++;

	if(actual_fd==unicast_vars->pfdsnum)
	{
		log_message( log_module, MSG_ERROR,"close connection : we did't find the file descriptor this should never happend, please contact\n");
		actual_fd=0;
		//We find the FD correspondig to this client
		while(actual_fd<unicast_vars->pfdsnum)
		{
			log_message( log_module, MSG_ERROR,"unicast_vars->pfds[actual_fd].fd %d Socket %d \n", unicast_vars->pfds[actual_fd].fd,Socket);
			actual_fd++;
		}
		return;
	}

	log_message( log_module, MSG_FLOOD,"We close the connection\n");
	//We delete the client
	unicast_del_client(unicast_vars, unicast_vars->fd_info[actual_fd].client);
	//We move the last fd to the actual/deleted one, and decrease the number of fds by one
	unicast_vars->pfds[actual_fd].fd = unicast_vars->pfds[unicast_vars->pfdsnum-1].fd;
	unicast_vars->pfds[actual_fd].events = unicast_vars->pfds[unicast_vars->pfdsnum-1].events;
	unicast_vars->pfds[actual_fd].revents = unicast_vars->pfds[unicast_vars->pfdsnum-1].revents;
	//we move the file descriptor information
	unicast_vars->fd_info[actual_fd] = unicast_vars->fd_info[unicast_vars->pfdsnum-1];
	//last one set to 0 for poll()
	unicast_vars->pfds[unicast_vars->pfdsnum-1].fd=0;
	unicast_vars->pfds[unicast_vars->pfdsnum-1].events=POLLIN|POLLPRI;
	unicast_vars->pfds[unicast_vars->pfdsnum-1].revents=0; //We clear it to avoid nasty bugs ...
	unicast_vars->pfdsnum--;
	unicast_vars->pfds=realloc(unicast_vars->pfds,(unicast_vars->pfdsnum+1)*sizeof(struct pollfd));
	if (unicast_vars->pfds==NULL)
	{
		log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		set_interrupted(ERROR_MEMORY<<8);
	}
	unicast_vars->fd_info=realloc(unicast_vars->fd_info,(unicast_vars->pfdsnum)*sizeof(unicast_fd_info_t));
	if (unicast_vars->fd_info==NULL)
	{
		log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		set_interrupted(ERROR_MEMORY<<8);
	}
	log_message( log_module, MSG_FLOOD,"Number of clients : %d\n", unicast_vars->client_number);

}




/** @brief Deal with an incoming message on the unicast client connection
 * This function will store and answer the HTTP requests
 *
 *
 * @param unicast_vars the unicast parameters
 * @param client The client from which the message was received
 * @param channels the channel array
 * @param number_of_channels quite explicit ...
 */
int unicast_handle_message(unicast_parameters_t *unicast_vars,
		unicast_client_t *client,
		mumudvb_channel_t *channels,
		int number_of_channels,
		strength_parameters_t *strengthparams,
		auto_p_t *auto_p,
		void *cam_p,
		void *scam_vars,
		eit_packet_t *eit_packets)
{
	int received_len;
	(void) unicast_vars;

	/************ auto increasing buffer to receive the message **************/
	if((client->buffersize-client->bufferpos)<RECV_BUFFER_MULTIPLE)
	{
		client->buffer=realloc(client->buffer,(client->buffersize + RECV_BUFFER_MULTIPLE+1)*sizeof(char)); //the +1 if for the \0 at the end
		if(client->buffer==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with realloc for the client buffer : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			client->buffersize=0;
			client->bufferpos=0;
			return -1;
		}
		memset (client->buffer+client->buffersize, 0, RECV_BUFFER_MULTIPLE*sizeof(char)); //We fill the buffer with zeros to be sure
		client->buffersize += RECV_BUFFER_MULTIPLE;
	}

	received_len=recv(client->Socket, client->buffer+client->bufferpos, RECV_BUFFER_MULTIPLE, 0);

	if(received_len>0)
	{
		if(client->bufferpos==0)
		{
			log_message( log_module, MSG_FLOOD,"beginning of buffer %c%c%c%c%c%c\n",client->buffer[0],client->buffer[1],client->buffer[2],client->buffer[3],client->buffer[4],client->buffer[5]);
			log_message( log_module, MSG_FLOOD,"beginning of buffer %d %d %d %d %d %d\n",client->buffer[0],client->buffer[1],client->buffer[2],client->buffer[3],client->buffer[4],client->buffer[5]);
		}
		client->bufferpos+=received_len;
		log_message( log_module, MSG_FLOOD,"We received %d, buffer len %d new buffer pos %d\n",received_len,client->buffersize, client->bufferpos);
	}

	if(received_len==-1)
	{
		log_message( log_module, MSG_ERROR,"Problem with recv : %s\n",strerror(errno));
		return -1;
	}
	if(received_len==0)
		return -2; //To say to the main program to close the connection

	/***************** Now we parse the message to see if something was asked  *****************/
	client->buffer[client->buffersize]='\0'; //For avoiding strlen to look too far (other option is to use the gnu extension strnlen)
	//We search for the end of the HTTP request
	if(strlen(client->buffer)>5 && strstr(client->buffer, "\n\r\n\0"))
	{
		int pos,err404;
		char *substring=NULL;
		int requested_channel;
		int iRet;
		requested_channel=0;
		pos=0;
		err404=0;
		struct unicast_reply* reply=NULL;

		log_message( log_module, MSG_FLOOD,"End of HTTP request, we parse it\n");

		if(strstr(client->buffer,"GET ")==client->buffer)
		{
			//to implement :
			//Information ???
			//GET /monitor/???

			pos=4;

			/* preselected channels via the port of the connection */
			//if the client have already an asked channel we don't parse the GET
			if(client->askedChannel!=-1)
			{
				requested_channel=client->askedChannel+1; //+1 because requested channel starts at 1 and asked channel starts at 0
				log_message( log_module, MSG_DEBUG,"Channel by socket, number %d\n",requested_channel);
				client->askedChannel=-1;
			}
			//Channel by number
			//GET /bynumber/channelnumber
			else if(strstr(client->buffer +pos ,"/bynumber/")==(client->buffer +pos))
			{
				if(client->chan_ptr!=NULL)
				{
					log_message( log_module, MSG_INFO,"A channel (%s) is already streamed to this client, it shouldn't ask for a new one without closing the connection, error 501\n",client->chan_ptr->name);
					iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY));
					if(iRet<0)
						log_message( log_module, MSG_INFO,"Error writing reply\n");

					return -2; //to delete the client
				}

				pos+=strlen("/bynumber/");
				substring = strtok (client->buffer+pos, " ");
				if(substring == NULL)
					err404=1;
				else
				{
					requested_channel=atoi(substring);
					if(requested_channel && requested_channel<=number_of_channels)
						log_message( log_module, MSG_DEBUG,"Channel by number, number %d\n",requested_channel);
					else
					{
						log_message( log_module, MSG_INFO,"Channel by number, number %d out of range\n",requested_channel);
						err404=1;
						requested_channel=0;
					}
				}
			}
			//Channel by autoconf_sid_list
			//GET /bysid/sid
			else if(strstr(client->buffer +pos ,"/bysid/")==(client->buffer +pos))
			{
				if(client->chan_ptr!=NULL)
				{
					log_message( log_module, MSG_INFO,"A channel (%s) is already streamed to this client, it shouldn't ask for a new one without closing the connection, error 501\n",client->chan_ptr->name);
					iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY)); //iRet is to make the copiler happy we will close the connection anyways
					return -2; //to delete the client
				}
				pos+=strlen("/bysid/");
				substring = strtok (client->buffer+pos, " ");
				if(substring == NULL)
					err404=1;
				else
				{
					int requested_sid;
					requested_sid=atoi(substring);
					for(int current_channel=0; current_channel<number_of_channels;current_channel++)
					{
						if(channels[current_channel].service_id == requested_sid)
							requested_channel=current_channel+1;
					}
					if(requested_channel)
						log_message( log_module, MSG_DEBUG,"Channel by service id,  service_id %d number %d\n", requested_sid, requested_channel);
					else
					{
						log_message( log_module, MSG_INFO,"Channel by service id, service_id  %d not found\n",requested_sid);
						err404=1;
						requested_channel=0;
					}
				}
			}
			//Channel by name
			//GET /byname/channelname
			else if(strstr(client->buffer +pos ,"/byname/")==(client->buffer +pos))
			{
				if(client->chan_ptr!=NULL)
				{
					log_message( log_module, MSG_INFO,"A channel (%s) is already streamed to this client, it shouldn't ask for a new one without closing the connection, error 501\n",client->chan_ptr->name);
					iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY));//iRet is to make the copiler happy we will close the connection anyways
					return -2; //to delete the client
				}
				pos+=strlen("/byname/");
                log_message( log_module, MSG_DEBUG,"Channel by name\n");

                char *substring = client->buffer+pos;
                char *end = strstr(substring, " HTTP"); // find end of channel name (this way channel name can contain spaces)

                if(*substring == 0) {
					err404=1;
                }
                else if(end == NULL) {
                    err404=1;
                    log_message( log_module, MSG_DEBUG,"Channel name was not found in the URL `%s`\n", substring);
                }
				else
				{
                    end[0] = '\0'; // add string terminator to be able to get channel name

                    char requested_channel_name[MAX_NAME_LEN];
                    char current_channel_name[MAX_NAME_LEN];
                    strncpy(requested_channel_name, substring,MAX_NAME_LEN);
                    requested_channel_name[MAX_NAME_LEN-1] = '\0';
                    process_channel_name(requested_channel_name);

                    for(int current_channel=0; current_channel<number_of_channels;current_channel++)
                    {
                        strcpy(current_channel_name, channels[current_channel].name);
                        process_channel_name(current_channel_name);

                        if(strcasecmp(current_channel_name, requested_channel_name) == 0)
                            requested_channel=current_channel+1;
                    }
                    if(requested_channel)
                        log_message( log_module, MSG_DEBUG,"Channel by name, name `%s` number `%d`\n", requested_channel_name, requested_channel);
                    else
                    {
                        log_message( log_module, MSG_INFO,"Channel by name, name `%s` not found in channel list\n", requested_channel_name);
                        err404=1;
                        requested_channel=0;
                    }
				}
			}
			//Channels list
			else if(strstr(client->buffer +pos ,"/channels_list.html ")==(client->buffer +pos))
			{
				//We get the host name if availaible
				char *hoststr;
				hoststr=strstr(client->buffer ,"Host: ");
				if(hoststr)
				{
					substring = strtok (hoststr+6, "\r");
				}
				else
					substring=NULL;
				log_message( log_module, MSG_DETAIL,"Channel list\n");
				unicast_send_streamed_channels_list (number_of_channels, channels, client->Socket, substring);
				return -2; //We close the connection afterwards
			}
			//playlist, m3u
			else if(strstr(client->buffer +pos ,"/playlist.m3u ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"play list\n");
				unicast_send_play_list_unicast (number_of_channels, channels, client->Socket, unicast_vars->portOut, 0, unicast_vars );
				return -2; //We close the connection afterwards
			}
			//playlist, m3u
			else if(strstr(client->buffer +pos ,"/playlist_port.m3u ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"play list\n");
				unicast_send_play_list_unicast (number_of_channels, channels, client->Socket, unicast_vars->portOut, 1, unicast_vars );
				return -2; //We close the connection afterwards
			}
			else if(strstr(client->buffer +pos ,"/playlist_multicast.m3u ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"play list\n");
				unicast_send_play_list_multicast (number_of_channels, channels, client->Socket, 0, unicast_vars );
				return -2; //We close the connection afterwards
			}
			else if(strstr(client->buffer +pos ,"/playlist_multicast_vlc.m3u ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"play list\n");
				unicast_send_play_list_multicast (number_of_channels, channels, client->Socket, 1, unicast_vars );
				return -2; //We close the connection afterwards
			}
			//statistics, text version
			else if(strstr(client->buffer +pos ,"/channels_list.json ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"Channel list Json\n");
				unicast_send_streamed_channels_list_js (number_of_channels, channels, scam_vars, client->Socket);
				return -2; //We close the connection afterwards
			}
			else if(strstr(client->buffer +pos ,"/monitor/state.json ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"HTTP request for state in Json\n");
				unicast_send_json_state(number_of_channels, channels, client->Socket, strengthparams, auto_p, cam_p, scam_vars);
				return -2; //We close the connection afterwards
			}
			else if(strstr(client->buffer +pos ,"/monitor/signal_power.json ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"Signal power json\n");
				unicast_send_signal_power_js(client->Socket, strengthparams);
				return -2; //We close the connection afterwards
			}
			else if(strstr(client->buffer +pos ,"/monitor/channels_traffic.json ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"Channel traffic json\n");
				unicast_send_channel_traffic_js(number_of_channels, channels, client->Socket);
				return -2; //We close the connection afterwards
			}
			else if(strstr(client->buffer +pos ,"/monitor/state.xml ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"HTTP request for XML State\n");
				unicast_send_xml_state(number_of_channels, channels, client->Socket, strengthparams, auto_p, cam_p, scam_vars);
				return -2; //We close the connection afterwards
			}
			//statistics, text version
			else if(strstr(client->buffer +pos ,"/monitor/EIT.json ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"EIT Json\n");
				unicast_send_EIT (eit_packets,  client->Socket);
				return -2; //We close the connection afterwards
			}
			else if(strstr(client->buffer +pos ,"/cam/menu.xml ")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"HTTP request for CAM menu display \n");
				unicast_send_cam_menu(client->Socket, cam_p);
				return -2; //We close the connection afterwards
			}
			else if(strstr(client->buffer +pos ,"/cam/action.xml?key=")==(client->buffer +pos))
			{
				log_message( log_module, MSG_DETAIL,"HTTP request for CAM menu action\n");
				pos+=strlen("/cam/action.xml?key=");
				unicast_send_cam_action(client->Socket,client->buffer+pos, cam_p);
				return -2; //We close the connection afterwards
			}
			else if((strstr(client->buffer +pos ,"/index.html")==(client->buffer +pos))||
					(strstr(client->buffer +pos ,"/index.htm")==(client->buffer +pos))||
					(strstr(client->buffer +pos ,"/ ")==(client->buffer +pos)))
			{
				log_message( log_module, MSG_DETAIL,"Index page\n");
				unicast_send_index_page(client->Socket);
				return -2; //We close the connection afterwards
			}
            //Prometheus exporter
            else if(strstr(client->buffer +pos ,"/metrics")==(client->buffer +pos))
            {
                log_message( log_module, MSG_DETAIL,"HTTP request for prometheus data\n");
                unicast_send_prometheus(number_of_channels, channels, client->Socket, strengthparams);
                return -2; //We close the connection afterwards
            }
			//Not implemented path --> 404
			else
				err404=1;


			if(err404)
			{
				log_message( log_module, MSG_INFO,"Path not found i.e. 404\n");
				reply = unicast_reply_init();
				if (NULL == reply) {
					log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
					return -2;
				}
				unicast_reply_write(reply, HTTP_404_REPLY_HTML, VERSION);
				unicast_reply_send(reply, client->Socket, 404, "text/html");
				if (0 != unicast_reply_free(reply)) {
					log_message( log_module, MSG_INFO,"Error when releasing the HTTP reply after sendinf it\n");
					return -2;
				}
				return -2; //to delete the client
			}
			//We have found a channel, we add the client
			if(requested_channel)
			{
				if(!channel_add_unicast_client(client,&channels[requested_channel-1]))
					client->chan_ptr=&channels[requested_channel-1];
				else
					return -2;
			}

		}
		else
		{
			//We don't implement this http method, but if the client is already connected, we keep the connection
			if(client->chan_ptr==NULL)
			{
				log_message( log_module, MSG_INFO,"Unhandled HTTP method : \"%s\", error 501\n",  strtok (client->buffer+pos, " "));
				iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY));//iRet is to make the copiler happy we will close the connection anyways
				return -2; //to delete the client
			}
			else
			{
				log_message( log_module, MSG_INFO,"Unhandled HTTP method : \"%s\", error 501 but we keep the client connected\n",  strtok (client->buffer+pos, " "));
				iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY));//iRet is to make the copiler happy we will close the connection anyways
				return 0;
			}
		}
		//We don't need the buffer anymore
		free(client->buffer);
		client->buffer=NULL;
		client->bufferpos=0;
		client->buffersize=0;
	}

	return 0;
}


//////////////////
// HTTP Toolbox //
//////////////////



/** @brief Init reply structure
 *
 */
struct unicast_reply* unicast_reply_init()
{
	struct unicast_reply* reply = malloc(sizeof (struct unicast_reply));
	if (NULL == reply)
	{
		log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		return NULL;
	}
	reply->buffer_header = malloc(REPLY_SIZE_STEP * sizeof (char));
	if (NULL == reply->buffer_header)
	{
		free(reply);
		log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		return NULL;
	}
	reply->length_header = REPLY_SIZE_STEP;
	reply->used_header = 0;
	reply->buffer_body = malloc(REPLY_SIZE_STEP * sizeof (char));
	if (NULL == reply->buffer_body)
	{
		free(reply->buffer_header);
		free(reply);
		log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		return NULL;
	}
	reply->length_body = REPLY_SIZE_STEP;
	reply->used_body = 0;
	reply->type = REPLY_BODY;
	return reply;
}

/** @brief Release the reply structure
 *
 */
int unicast_reply_free(struct unicast_reply *reply)
{
	if (NULL == reply)
		return 1;
	if ((NULL == reply->buffer_header)&&(NULL == reply->buffer_body))
		return 1;
	if(reply->buffer_header != NULL)
		free(reply->buffer_header);
	if(reply->buffer_body != NULL)
		free(reply->buffer_body);
	free(reply);
	return 0;
}

/** @brief Write data in a buffer using the same syntax that printf()
 *
 * auto-realloc buffer if needed
 */
int unicast_reply_write(struct unicast_reply *reply, const char* msg, ...)
{
	char **buffer;
	char *temp_buffer;
	int *length;
	int *used;
	buffer=NULL;
	va_list args;
	if (NULL == msg)
		return -1;
	switch(reply->type)
	{
	case REPLY_HEADER:
		buffer=&reply->buffer_header;
		length=&reply->length_header;
		used=&reply->used_header;
		break;
	case REPLY_BODY:
		buffer=&reply->buffer_body;
		length=&reply->length_body;
		used=&reply->used_body;
		break;
	default:
		log_message( log_module, MSG_WARN,"unicast_reply_write with wrong type, please contact\n");
		return -1;
	}
	va_start(args, msg);
	int estimated_len = vsnprintf(NULL, 0, msg, args); /* !! imply gcc -std=c99 */
	//Since vsnprintf put the mess we reinitiate the args
	va_end(args);
	va_start(args, msg);
	// Must add 1 byte more for the terminating zero (not counted)
	while (*length - *used < estimated_len + 1) {
		temp_buffer = realloc(*buffer, *length + REPLY_SIZE_STEP);
		if(temp_buffer == NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			va_end(args);
			return -1;
		}
		*buffer=temp_buffer;
		*length += REPLY_SIZE_STEP;
	}
	int real_len = vsnprintf(*buffer+*used, *length - *used, msg, args);
	if (real_len != estimated_len) {
		log_message( log_module, MSG_WARN,"Error when writing the HTTP reply\n");
	}
	*used += real_len;
	va_end(args);
	return 0;
}

/** @brief Dump the filled buffer on the socket adding HTTP header informations
 */
int unicast_reply_send(struct unicast_reply *reply, int socket, int code, const char* content_type)
{
	int size=0;
	int temp_size=0;
	//we add the header information
	reply->type = REPLY_HEADER;
	unicast_reply_write(reply, "HTTP/1.0 ");
	switch(code)
	{
	case 200:
		unicast_reply_write(reply, "200 OK\r\n");
		break;
	case 404:
		unicast_reply_write(reply, "404 Not found\r\n");
		break;
	default:
		log_message( log_module, MSG_ERROR,"reply send with bad code please contact\n");
		return 0;
	}
	unicast_reply_write(reply, "Access-Control-Allow-Origin: *\r\n");
	unicast_reply_write(reply, "Server: mumudvb/" VERSION "\r\n");
	unicast_reply_write(reply, "Content-type: %s; charset=utf-8\r\n", content_type);
	unicast_reply_write(reply, "Content-length: %d\r\n", reply->used_body);
	unicast_reply_write(reply, "\r\n"); /* end header */
	//we merge the header and the body
	reply->buffer_header = realloc(reply->buffer_header, reply->used_header+reply->used_body);
	memcpy(&reply->buffer_header[reply->used_header],reply->buffer_body,sizeof(char)*reply->used_body);
	reply->used_header+=reply->used_body;

	//now we write the data
	while (size<reply->used_header){
		temp_size = write(socket, reply->buffer_header+size, reply->used_header-size);
		if (temp_size != -1) {
			size += temp_size;
		} else {
			if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
				return -1;
			}
		}
	}
	return size;
}


//////////////////////
// End HTTP Toolbox //
//////////////////////



/** @brief Send a basic html file containing the list of streamed channels
 *
 * @param number_of_channels the number of channels
 * @param channels the channels array
 * @param Socket the socket on wich the information have to be sent
 * @param host The server ip address/name (got in the HTTP GET request)
 */
int
unicast_send_streamed_channels_list (int number_of_channels, mumudvb_channel_t *channels, int Socket, char *host)
{

	struct unicast_reply* reply = unicast_reply_init();
	if (NULL == reply) {
		log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
		return -1;
	}

	unicast_reply_write(reply, HTTP_CHANNELS_REPLY_START);

	for (int curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
		if (channels[curr_channel].channel_ready>=READY)
		{
			if(host)
				unicast_reply_write(reply, "Channel number %d : %s<br>Unicast link : <a href=\"http://%s/bysid/%d\">http://%s/bysid/%d</a><br>Multicast ip : %s:%d<br><br>\r\n",
						curr_channel+1,
						channels[curr_channel].name,
						host,channels[curr_channel].service_id,
						host,channels[curr_channel].service_id,
						channels[curr_channel].ip4Out,channels[curr_channel].portOut);
			else
				unicast_reply_write(reply, "Channel number %d : \"%s\"<br>Multicast ip : %s:%d<br><br>\r\n",
						curr_channel+1,
						channels[curr_channel].name,
						channels[curr_channel].ip4Out,channels[curr_channel].portOut);
		}
	unicast_reply_write(reply, HTTP_CHANNELS_REPLY_END);

	unicast_reply_send(reply, Socket, 200, "text/html");

	if (0 != unicast_reply_free(reply)) {
		log_message( log_module, MSG_INFO,"Error when releasing the HTTP reply after sendinf it\n");
		return -1;
	}

	return 0;
}


/** @brief Send a basic text file containig the playlist
 *
 * @param number_of_channels the number of channels
 * @param channels the channels array
 * @param Socket the socket on wich the information have to be sent
 * @param perport says if the channel have to be given by the url /bysid or by their port
 */
int
unicast_send_play_list_unicast (int number_of_channels, mumudvb_channel_t *channels, int Socket, int unicast_portOut, int perport, unicast_parameters_t *unicast_vars)
{
	int curr_channel,iRet;

	struct unicast_reply* reply = unicast_reply_init();
	if (NULL == reply) {
		log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
		return -1;
	}

	//We get the ip address on which the client is connected
	struct sockaddr_in tempSocketAddr;
	unsigned int l;
	l = sizeof(struct sockaddr);
	iRet=getsockname(Socket, (struct sockaddr *) &tempSocketAddr, &l);
	if (iRet < 0)
	{
		log_message( log_module,  MSG_ERROR,"getsockname failed : %s while making HTTP reply", strerror(errno));
		if (0 != unicast_reply_free(reply))
			log_message( log_module, MSG_INFO,"Error when releasing the HTTP reply");
		return -1;
	}
	//we write the playlist
	unicast_reply_write(reply, "#EXTM3U\r\n");

	//"#EXTINF:0,title\r\nURL"
	for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
		if (channels[curr_channel].channel_ready>=READY
		    && (channels[curr_channel].has_traffic == 1 || unicast_vars->playlist_ignore_dead == 0)
		    && (channels[curr_channel].ratio_scrambled < unicast_vars->playlist_ignore_scrambled_ratio || unicast_vars->playlist_ignore_scrambled_ratio == 0)
		)
		{
			char addr_buf[64];

			if(!perport)
			{
				inet_ntop(AF_INET, &tempSocketAddr.sin_addr, addr_buf, sizeof(addr_buf));

				unicast_reply_write(reply, "#EXTINF:0,%s\r\nhttp://%s:%d/bysid/%d\r\n",
						channels[curr_channel].name,
						addr_buf,
						unicast_portOut ,
						channels[curr_channel].service_id);
			}
			else if(channels[curr_channel].unicast_port)
			{
				inet_ntop(AF_INET, &tempSocketAddr.sin_addr, addr_buf, sizeof(addr_buf));

				unicast_reply_write(reply, "#EXTINF:0,%s\r\nhttp://%s:%d/\r\n",
						channels[curr_channel].name,
						addr_buf,
						channels[curr_channel].unicast_port);
			}
		}

	unicast_reply_send(reply, Socket, 200, "audio/x-mpegurl");

	if (0 != unicast_reply_free(reply)) {
		log_message( log_module, MSG_INFO,"Error when releasing the HTTP reply after sendinf it\n");
		return -1;
	}

	return 0;
}

/** @brief Send a basic index.html
 *
 * @param number_of_channels the number of channels
 * @param channels the channels array
 * @param Socket the socket on wich the information have to be sent
 * @param perport says if the channel have to be given by the url /bysid or by their port
 */
int
unicast_send_index_page (int Socket)
{
	struct unicast_reply* reply = unicast_reply_init();
	if (NULL == reply) {
		log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
		return -1;
	}

	unicast_reply_write(reply, HTTP_INDEX_REPLY_START);


	unicast_reply_write(reply, "<br>Channels by number : /bynumber/[channel number]<br><br>\r\n");
	unicast_reply_write(reply, "<br>Channels by service identifier : /bysid/[channel sid]<br><br>\r\n");
	unicast_reply_write(reply, "<br>Channels by number : /byname/[channel name]<br><br>\r\n");


	unicast_reply_write(reply, "<br>  <a href=\"/channels_list.html\">Channels list</a><br><br>\r\n");
	unicast_reply_write(reply, "<br>  <a href=\"/playlist.m3u\">Playlist (m3u)</a><br><br>\r\n");
	unicast_reply_write(reply, "<br>  <a href=\"/playlist_port.m3u\">Playlist by port(m3u)</a><br><br>\r\n");
	unicast_reply_write(reply, "<br>  <a href=\"/playlist_multicast.m3u\">Playlist multicast (m3u)</a><br><br>\r\n");
	unicast_reply_write(reply, "<br>  <a href=\"/playlist_multicast_vlc.m3u\">Playlist multicast for VLC(m3u)</a><br><br>\r\n");

	unicast_reply_write(reply, "<br>  <a href=\"/channels_list.json\">Channels list (json)</a><br><br>\r\n");
	unicast_reply_write(reply, "<br>  <a href=\"/monitor/signal_power.json\">Signal strength (json)</a><br><br>\r\n");
	unicast_reply_write(reply, "<br>  <a href=\"/monitor/channels_traffic.json\">Channels traffic (json)</a><br><br>\r\n");
	unicast_reply_write(reply, "<br>  <a href=\"/monitor/state.xml\">Server state : channel list, pids, traffic (XML)</a><br><br>\r\n");
	unicast_reply_write(reply, "<br>  <a href=\"/monitor/state.json\">Server state : channel list, pids, traffic (json)</a><br><br>\r\n");
	unicast_reply_write(reply, "<br>  <a href=\"/monitor/EIT.json\">Contents of the EIT tables (json)</a><br><br>\r\n");
	unicast_reply_write(reply, "<br>  <a href=\"/cam/menu.xml\">CAM menu</a><br><br>\r\n");
	unicast_reply_write(reply, "<br> make an action on the cam menu : /cam/action.xml?key=<br><br>\r\n");



	unicast_reply_write(reply, HTTP_INDEX_REPLY_END);

	unicast_reply_send(reply, Socket, 200, "text/html");

	if (0 != unicast_reply_free(reply)) {
		log_message( log_module, MSG_INFO,"Error when releasing the HTTP reply after sendinf it\n");
		return -1;
	}

	return 0;
}




/** @brief Send a basic text file containig the playlist
 *
 * @param number_of_channels the number of channels
 * @param channels the channels array
 * @param Socket the socket on wich the information have to be sent
 */
int
unicast_send_play_list_multicast (int number_of_channels, mumudvb_channel_t *channels, int Socket, int vlc, unicast_parameters_t *unicast_vars)
{
	int curr_channel;
	char urlheader[4];
	char vlcchar[2];


	struct unicast_reply* reply = unicast_reply_init();
	if (NULL == reply) {
		log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
		return -1;
	}

	unicast_reply_write(reply, "#EXTM3U\r\n");

	if(vlc)
		strcpy(vlcchar,"@");
	else
		vlcchar[0]='\0';


	//"#EXTINF:0,title\r\nURL"
	for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
		if (channels[curr_channel].channel_ready>=READY && (channels[curr_channel].has_traffic == 1 || unicast_vars->playlist_ignore_dead == 0))
		{
			if(channels[curr_channel].rtp)
				strcpy(urlheader,"rtp");
			else
				strcpy(urlheader,"udp");

			unicast_reply_write(reply, "#EXTINF:0,%s\r\n%s://%s%s:%d\r\n",
					channels[curr_channel].name,
					urlheader,
					vlcchar,
					channels[curr_channel].ip4Out,
					channels[curr_channel].portOut);
		}

	unicast_reply_send(reply, Socket, 200, "audio/x-mpegurl");

	if (0 != unicast_reply_free(reply)) {
		log_message( log_module, MSG_INFO,"Error when releasing the HTTP reply after sendinf it\n");
		return -1;
	}

	return 0;
}

/** @brief Trims name of a channel to remove leading and trailing spaces, and replaces all spaces by '-' character.
 * Note that the string will be modified so do a copy prior to running this function if modifications to the original shall be prevented.
 *
 * @param str Channel name to process
 */
void process_channel_name(char *str) {
    int i;
    int begin = 0;
    int end = strlen(str) - 1;

    while (isspace(str[begin]))
        begin++;
    while ((end >= begin) && isspace(str[end]))
        end--;

    // shift all characters back to the start of the string array
    for (i = begin; i <= end; i++) {
        if (isspace(str[i]))
            str[i - begin] = '-'; // replace spaces by '-'
        else
            str[i - begin] = str[i];
    }

    str[i - begin] = '\0';
}
