/* 
 * mumudvb - UDP-ize a DVB transport stream.
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
 * @brief File for HTTP unicast
 * @author Brice DUBOST
 * @date 2009
 */

/***********************
Todo list
 * implement channel change (should be easy ;-)
 * implement a measurement of the "load"

***********************/

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "unicast_http.h"
#include "mumudvb.h"



/** @brief Accept an incoming connection
 *
 *
 * @param unicast_vars the unicast parameters
 */
int unicast_accept_connection(unicast_parameters_t *unicast_vars)
{

  unsigned int l;
  int tempSocket;
  struct sockaddr_in tempSocketAddrIn;
  l = sizeof(struct sockaddr);
  tempSocket = accept(unicast_vars->socketIn, (struct sockaddr *) &tempSocketAddrIn, &l);
  if (tempSocket < 0)
    {
      log_message(MSG_ERROR,"Unicast : Error when accepting the incoming connection : %s\n", strerror(errno));
      return -1;
    }
  log_message(MSG_DETAIL,"Unicast : New connection from %s:%d\n",inet_ntoa(tempSocketAddrIn.sin_addr), tempSocketAddrIn.sin_port);

  if( unicast_add_client(unicast_vars, tempSocketAddrIn, tempSocket)<0)
    {
      //We cannot create the client, we close the socket cleanly
      shutdown(tempSocket, SHUT_RDWR);
      close(tempSocket);                 
      return -1;
    }
  return tempSocket;

}

/** @brief Add a client to the chained list of clients
 * Will allocate the memory and fill the structure
 *
 * @param unicast_vars the unicast parameters
 */
int unicast_add_client(unicast_parameters_t *unicast_vars, struct sockaddr_in SocketAddr, int Socket)
{

  unicast_client_t *client;
  unicast_client_t *prev_client;
  log_message(MSG_DEBUG,"Unicast : We create a client associated with %d\n",Socket);
  //We allocate a new client
  if(unicast_vars->clients==NULL)
    {
      client=unicast_vars->clients=malloc(sizeof(unicast_client_t));
      prev_client=NULL;
    }
  else
    {
      client=unicast_vars->clients;
      while(client->next!=NULL)
	client=client->next;
      client->next=malloc(sizeof(unicast_client_t));
      prev_client=client;
      client=client->next;
    }
  if(client==NULL)
    {
      log_message(MSG_ERROR,"Unicast : allocating a new client : MALLOC error");
      shutdown(Socket, SHUT_RDWR);
      close(Socket);                 
      return -1;
    }

  //We fill the client data
  client->SocketAddr=SocketAddr;
  client->Socket=Socket;
  client->buffer=NULL;
  client->buffersize=0;
  client->bufferpos=0;
  client->channel=-1;
  client->next=NULL;
  client->prev=prev_client;
  client->chan_next=NULL;
  client->chan_prev=NULL;


  return 0;
}



/** @brief Delete a client to the chained list of clients and in the associated channel
 * @todo document
 *
 * @param unicast_vars the unicast parameters
 */
int unicast_del_client(unicast_parameters_t *unicast_vars, int Socket, mumudvb_channel_t *channels)
{
  unicast_client_t *client;
  unicast_client_t *prev_client,*next_client;

  client=unicast_find_client(unicast_vars,Socket);
  if(client==NULL)
    {
      log_message(MSG_ERROR,"Unicast : This should never happend, please contact\n");
      return -1;
    }
  log_message(MSG_DEBUG,"Unicast : We delete the client %s:%d, socket %d\n",inet_ntoa(client->SocketAddr.sin_addr), client->SocketAddr.sin_port, Socket);

  if (client->Socket >= 0)
    {			    
      shutdown(client->Socket, SHUT_RDWR);
      close(client->Socket);                 
    }
  
  prev_client=client->prev;
  next_client=client->next;
  if(prev_client==NULL)
    {
      unicast_vars->clients=NULL;
    }
  else
    prev_client->next=client->next;

  //We delete the client in the channel
  if(client->channel)
    {
      if(client->chan_prev==NULL)
	channels[client->channel].clients=NULL;
      else
	client->chan_prev->chan_next=client->chan_next;
    }


  if(client->buffer)
    free(client->buffer);
  free(client);

  return 0;
}

/** @brief Find a client with a specified socket
 * return NULL if no clients are found
 *
 * @param unicast_vars the unicast parameters
 * @param Socket the socket for wich we search the client
 */
unicast_client_t *unicast_find_client(unicast_parameters_t *unicast_vars, int Socket)
{
  unicast_client_t *client;

  if(unicast_vars->clients==NULL)
    return NULL;
  else
    {
      client=unicast_vars->clients;
      while(client!=NULL)
	{
	  if (client->Socket==Socket)
	    return client;
	  client=client->next;
	}
    }

  return NULL;
}

/** @brief Deal with an incoming message on the unicast client connection
 * This function will store and answer the HTTP requests
 *
 *
 * @param unicast_vars the unicast parameters
 */
int unicast_handle_message(unicast_parameters_t *unicast_vars, int fd, mumudvb_channel_t *channels, int number_of_channels)
{
  unicast_client_t *client;
  int received_len;
  

  client=unicast_find_client(unicast_vars,fd);
  if(client==NULL)
    {
      log_message(MSG_ERROR,"Unicast : This should never happend, please contact\n");
      return -1;
    }

  if((client->buffersize-client->bufferpos)<RECV_BUFFER_MULTIPLE)
    {
      client->buffer=realloc(client->buffer,(client->buffersize + RECV_BUFFER_MULTIPLE)*sizeof(char));
      client->buffersize += RECV_BUFFER_MULTIPLE;
      if(client->buffer==NULL)
	{
	  log_message(MSG_ERROR,"Unicast : Problem with realloc for the client buffer : %s\n",strerror(errno));
	  client->buffersize=0;
	  client->bufferpos=0;
	  return -1;
	}
    }

  received_len=recv(client->Socket, client->buffer+client->bufferpos, RECV_BUFFER_MULTIPLE, 0);

  if(received_len>0)
    {
      client->bufferpos+=received_len;
      log_message(MSG_DEBUG,"Unicast : We received %d, buffer len %d new buffer pos %d\n",received_len,client->buffersize, client->bufferpos);
      //log_message(MSG_DEBUG,"Unicast : Actual message :\n--------------\n%s\n----------\n",client->buffer);
    }

  if(received_len==-1)
    {
      log_message(MSG_ERROR,"Unicast : Problem with recv : %s\n",strerror(errno));
      return -1;
    }
  if(received_len==0)
    return -2; //To say to the main program to close the connection

  /***************** Now we parse the message to see if something was asked  *****************/

  //We search for the end of the HTTP request
  if(strstr(client->buffer, "\n\r\n\0"))
    {
      int pos,err404;
      char *substring=NULL;
      int requested_channel;
      requested_channel=0;
      pos=0;
      err404=0;

      log_message(MSG_DEBUG,"Unicast : End of HTTP request, we parse it\n");
      
      if(strstr(client->buffer,"GET ")==client->buffer)
	{
	
	  //Channels list ?
	  //Return a index of the channels ?
	  //Information ???
	  //GET /monitor/???

	  pos=4;
	  //Channel by number
	  //GET /bynumber/channelnumber
	  if(strstr(client->buffer +pos ,"/bynumber/")==(client->buffer +pos))
	    {
	      pos+=10;
	      substring = strtok (client->buffer+pos, " ");
	      if(substring == NULL)
		err404=1;
	      else
		{
		  requested_channel=atoi(substring);
		  if(requested_channel && requested_channel<=number_of_channels)
		    log_message(MSG_DEBUG,"Unicast : Channel by number, number %d\n",requested_channel);
		  else
		    {
		      log_message(MSG_INFO,"Unicast : Channel by number, number %d out of range\n",requested_channel);
		      err404=1;
		      requested_channel=0;
		    }
		}
	    }
	  //Channel by name
	  //GET /byname/channelname
	  else if(strstr(client->buffer +pos ,"/byname/")==(client->buffer +pos))
	    {
	      pos+=8;
	      log_message(MSG_DEBUG,"Unicast : Channel by number\n");
	      substring = strtok (client->buffer+pos, " ");
	      if(substring == NULL)
		err404=1;
	      else
		{
		  log_message(MSG_DEBUG,"Unicast : Channel by name, name %s\n",substring);
		  //search the channel
		}
	    }
	  //Not implemented path --> 404
	  else
	      err404=1;

	  if(err404)
	    {
	      log_message(MSG_INFO,"Unicast : Path not found i.e. 404\n");
	      write(client->Socket,HTTP_404_REPLY, strlen(HTTP_404_REPLY));
	      return -2; //to delete the client
	    }

	  //We have found a channel, we add the client
	  if(requested_channel)
	    {
	      channel_add_unicast_client(client,&channels[requested_channel-1]);
	      client->channel=requested_channel-1;
	    }

	}
      else
	{
	  log_message(MSG_INFO,"Unicast : Unhandled HTTP method, error 501\n");
	  write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY));
	  return -2; //to delete the client
	}
      //We don't need the buffer anymore
      free(client->buffer);
      client->buffer=NULL;
      client->bufferpos=0;
      client->buffersize=0;
    }

  return 0;
}


/** @brief This function add an unicast client to a channel
 *
 * @param client the client
 * @param channel the channel
 */
void channel_add_unicast_client(unicast_client_t *client,mumudvb_channel_t *channel)
{
  unicast_client_t *last_client;

  write(client->Socket,HTTP_OK_REPLY, strlen(HTTP_OK_REPLY));


  if(channel->clients==NULL)
    channel->clients=client;
  else
    {
      last_client=channel->clients;
      while(last_client->chan_next!=NULL)
	last_client=last_client->chan_next;
      last_client->chan_next=client;
      client->chan_prev=last_client;
    }
    
}
