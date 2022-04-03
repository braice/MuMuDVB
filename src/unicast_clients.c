/*
 * MuMuDVB - UDP-ize a DVB transport stream.
 *
 * (C) 2009 Brice DUBOST
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
 * @brief File for HTTP unicast clients
 * @author Brice DUBOST
 * @date 2009
 */

#define _CRT_SECURE_NO_WARNINGS

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <unistd.h>
#endif
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>


#include "unicast_http.h"
#include "unicast_queue.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"



static char *log_module="Unicast : ";

/** @brief Add a client to the chained list of clients
 * Will allocate the memory and fill the structure
 *
 * @param unicast_vars the unicast parameters
 * @param SocketAddr The socket address
 * @param Socket The socket number
 */
unicast_client_t *unicast_add_client(unicast_parameters_t *unicast_vars, struct sockaddr_in SocketAddr, int Socket)
{

	unicast_client_t *client;
	unicast_client_t *prev_client;
	log_message( log_module, MSG_FLOOD,"We create a client associated with the socket %d\n",Socket);
	//We allocate a new client
	if(unicast_vars->clients==NULL)
	{
		log_message( log_module, MSG_FLOOD,"first client\n");
		client=unicast_vars->clients=calloc(1, sizeof(unicast_client_t));
		prev_client=NULL;
	}
	else
	{
		log_message( log_module, MSG_FLOOD,"there is already clients\n");
		client=unicast_vars->clients;
		while(client->next!=NULL)
			client=client->next;
		client->next=calloc(1, sizeof(unicast_client_t));
		prev_client=client;
		client=client->next;
	}
	if(client==NULL)
	{
		log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		close(Socket);
		return NULL;
	}


	// Disable the Nagle (TCP No Delay) algorithm
	int iRet;
	int on = 1;
	iRet=setsockopt(Socket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
	if (iRet < 0)
	{
		log_message( log_module,  MSG_WARN,"setsockopt TCP_NODELAY failed : %s\n", strerror(errno));
	}

	int buffer_size;
	socklen_t size;
	size=sizeof(buffer_size);
	iRet=getsockopt( Socket, SOL_SOCKET, SO_SNDBUF, &buffer_size, &size);
	if (iRet < 0)
	{
		log_message( log_module,  MSG_WARN,"get SO_SNDBUF failed : %s\n", strerror(errno));
	}
	else
		log_message( log_module,  MSG_FLOOD,"Actual SO_SNDBUF size : %d\n", buffer_size);
	if(unicast_vars->socket_sendbuf_size)
	{
		buffer_size = unicast_vars->socket_sendbuf_size;
		iRet=setsockopt(Socket, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
		if (iRet < 0)
		{
			log_message( log_module,  MSG_WARN,"setsockopt SO_SNDBUF failed : %s\n", strerror(errno));
		}
		else
		{
			size=sizeof(buffer_size);
			iRet=getsockopt( Socket, SOL_SOCKET, SO_SNDBUF, &buffer_size, &size);
			if (iRet < 0)
				log_message( log_module,  MSG_WARN,"2nd get SO_SNDBUF failed : %s\n", strerror(errno));
			else
				log_message( log_module,  MSG_DETAIL,"New SO_SNDBUF size : %d\n", buffer_size);
		}
	}

	//We fill the client data
	client->SocketAddr=SocketAddr;
	client->Socket=Socket;
	client->buffer=NULL;
	client->buffersize=0;
	client->bufferpos=0;
	client->chan_ptr=NULL;
	client->askedChannel=-1;
	client->consecutive_errors=0;
	client->next=NULL;
	client->prev=prev_client;
	client->chan_next=NULL;
	client->chan_prev=NULL;
	//We init the queue
	client->queue.data_bytes_in_queue=0;
	client->queue.packets_in_queue=0;
	client->queue.first=NULL;
	client->queue.last=NULL;

	unicast_vars->client_number++;

	return client;
}



/** @brief Delete a client to the chained list of clients and in the associated channel
 * This function close the socket of the client
 * remove it from the associated channel if there is one
 * and free the memory of the client (including the buffer)
 *
 * @param unicast_vars the unicast parameters
 * @param client the client we want to delete
 */
int unicast_del_client(unicast_parameters_t *unicast_vars, unicast_client_t *client)
{
	unicast_client_t *prev_client,*next_client;

	log_message( log_module, MSG_FLOOD,"We delete the client %s:%d, socket %d\n",inet_ntoa(client->SocketAddr.sin_addr), client->SocketAddr.sin_port, client->Socket);

	if (client->Socket >= 0)
	{
		close(client->Socket);
	}

	prev_client=client->prev;
	next_client=client->next;
	if(prev_client==NULL)
	{
		log_message( log_module, MSG_FLOOD,"We delete the first client\n");
		unicast_vars->clients=client->next;
	}
	else
		prev_client->next=client->next;

	if(next_client)
		next_client->prev=prev_client;

	//We delete the client in the channel
	if(client->chan_ptr!=NULL)
	{
		log_message( log_module, MSG_DEBUG,"We remove the client from the channel \"%s\"\n",client->chan_ptr->name);
		// decrement the number of client connections
        client->chan_ptr->num_clients--;

		if(client->chan_prev==NULL)
		{
			client->chan_ptr->clients=client->chan_next;
			if(client->chan_ptr->clients)
				client->chan_ptr->clients->chan_prev=NULL;
		}
		else
		{
			client->chan_prev->chan_next=client->chan_next;
			if(client->chan_next)
				client->chan_next->chan_prev=client->chan_prev;
		}
	}


	if(client->buffer)
		free(client->buffer);
	unicast_queue_clear(&client->queue);
	free(client);

	unicast_vars->client_number--;


	return 0;
}



/** @brief This function add an unicast client to a channel
 *
 * @param client the client
 * @param channel the channel
 */
int channel_add_unicast_client(unicast_client_t *client,mumudvb_channel_t *channel)
{
	unicast_client_t *last_client;
	int iRet;

	log_message( log_module, MSG_INFO,"We add the client %s:%d to the channel \"%s\"\n",inet_ntoa(client->SocketAddr.sin_addr), client->SocketAddr.sin_port,channel->name);

	iRet=write(client->Socket,HTTP_OK_REPLY, strlen(HTTP_OK_REPLY));
	if(iRet!=strlen(HTTP_OK_REPLY))
	{
		log_message( log_module, MSG_INFO,"Error when sending the HTTP reply\n");
		return -1;
	}

	client->chan_next=NULL;
	// Increment the number of client connections
    channel->num_clients++;

	if(channel->clients==NULL)
	{
		channel->clients=client;
		client->chan_prev=NULL;
	}
	else
	{
		last_client=channel->clients;
		while(last_client->chan_next!=NULL)
			last_client=last_client->chan_next;
		last_client->chan_next=client;
		client->chan_prev=last_client;
	}
	return 0;
}


/** @brief Delete all the clients
 *
 * @param unicast_vars the unicast parameters
 */
void unicast_freeing(unicast_parameters_t *unicast_vars)
{
	unicast_client_t *actual_client;
	unicast_client_t *next_client;

	for(actual_client=unicast_vars->clients; actual_client != NULL; actual_client=next_client)
	{
		next_client= actual_client->next;
		unicast_del_client(unicast_vars, actual_client);
	}
}

