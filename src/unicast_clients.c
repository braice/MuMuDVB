/*
* MuMuDVB - UDP-ize a DVB transport stream.
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

/** @file
* @brief File for HTTP unicast clients
* @author Brice DUBOST
* @date 2009
*/



#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdlib.h>

#include "unicast_http.h"
#include "unicast_queue.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"
#include "unicast.h"
#include "unicast_common.h"


/** @brief Add a client to the chained list of clients
* Will allocate the memory and fill the structure
*
* @param unicast_vars the unicast parameters
* @param SocketAddr The socket address
* @param Socket The socket number
*/
unicast_client_t *unicast_add_client(unicast_parameters_t *unicast_vars, struct sockaddr_in SocketAddr, int Socket, int client_type)
{

  unicast_client_t *client;
  unicast_client_t *prev_client;
  log_message(MSG_DEBUG,"Unicast : We create a client associated with the socket %d\n",Socket);
  //We allocate a new client
  if(unicast_vars->clients==NULL)
  {
    log_message(MSG_DEBUG,"Unicast : first client\n");
    client=unicast_vars->clients=malloc(sizeof(unicast_client_t));
    prev_client=NULL;
  }
  else
  {
    log_message(MSG_DEBUG,"Unicast : there is already clients\n");
    client=unicast_vars->clients;
    while(client->next!=NULL)
      client=client->next;
    client->next=malloc(sizeof(unicast_client_t));
    prev_client=client;
    client=client->next;
  }
  if(client==NULL)
  {
    log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    close(Socket);
    return NULL;
  }

  //We fill the client data
  client->SocketAddr=SocketAddr;
  client->Socket=Socket;
  client->buffer=NULL;
  client->buffersize=0;
  client->bufferpos=0;
  client->channel=-1;
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
  client->client_type=client_type;
  client->rtsp_Socket=0;
  client->Control_socket_closed=0;
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
* @param channels the array of channels
*/
int unicast_del_client(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels)
{
  unicast_client_t *prev_client,*next_client;

  log_message(MSG_DETAIL,"Unicast : We delete the client %s:%d, socket %d\n",inet_ntoa(client->SocketAddr.sin_addr), ntohs(client->SocketAddr.sin_port), client->Socket);
  if(client->client_type==CLIENT_RTSP && client->rtsp_Socket)
    log_message(MSG_DETAIL,"Unicast : RTSP %s:%d, Unicast socket %d\n",inet_ntoa(client->rtsp_SocketAddr.sin_addr), ntohs(client->rtsp_SocketAddr.sin_port), client->rtsp_Socket);

  if (client->Socket >= 0)
  {
    close(client->Socket);
  }
  if (client->rtsp_Socket >= 0)
  {
    close(client->rtsp_Socket);
  }

  prev_client=client->prev;
  next_client=client->next;
  if(prev_client==NULL)
  {
    log_message(MSG_DEBUG,"Unicast : We delete the first client\n");
    unicast_vars->clients=client->next;
  }
  else
    prev_client->next=client->next;

  if(next_client)
    next_client->prev=prev_client;

  //We delete the client in the channel
  if(client->channel!=-1)
  {
    log_message(MSG_DEBUG,"Unicast : We remove the client from the channel \"%s\"\n",channels[client->channel].name);

    if(client->chan_prev==NULL)
    {
      channels[client->channel].clients=client->chan_next;
      if(channels[client->channel].clients)
        channels[client->channel].clients->chan_prev=NULL;
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

  log_message(MSG_INFO,"Unicast : We add the client %s:%d to the channel \"%s\"\n",inet_ntoa(client->SocketAddr.sin_addr), ntohs(client->SocketAddr.sin_port),channel->name);

  if(client->client_type==CLIENT_HTTP)
  {
    iRet=write(client->Socket,HTTP_OK_REPLY, strlen(HTTP_OK_REPLY));
    if(iRet!=strlen(HTTP_OK_REPLY))
    {
      log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
      return -1;
    }
  }
  if(client->client_type==CLIENT_RTSP)
  {
    log_message(MSG_INFO,"Unicast : We add the client (RTP) sin_addr : %s: ntohs(client->rtsp_SocketAddr.sin_port) %d to the channel \"%s\"\n",inet_ntoa(client->rtsp_SocketAddr.sin_addr), ntohs(client->rtsp_SocketAddr.sin_port),channel->name);
  }

  client->chan_next=NULL;

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
* @param channels : the channels structure
*/
void unicast_freeing(unicast_parameters_t *unicast_vars, mumudvb_channel_t *channels)
{
  unicast_client_t *actual_client;
  unicast_client_t *next_client;

  for(actual_client=unicast_vars->clients; actual_client != NULL; actual_client=next_client)
  {
    next_client= actual_client->next;
    unicast_del_client(unicast_vars, actual_client, channels);
  }
}

