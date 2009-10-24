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
 * @brief File for HTTP unicast
 * @author Brice DUBOST
 * @date 2009
 */

/***********************
Todo list
 * implement a measurement of the "load" -- almost done with the traffic display
 * implement channel by name
 * implement monitoring features

***********************/
//in order to use asprintf (extension gnu)
#define _GNU_SOURCE

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/time.h>



#include "unicast_http.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"

extern int Interrupted;

int 
unicast_send_streamed_channels_list (int number_of_channels, mumudvb_channel_t *channels, int Socket, char *host);
int 
unicast_send_streamed_channels_list_txt (int number_of_channels, mumudvb_channel_t *channels, int Socket);
int 
    unicast_send_statistics_txt(int number_of_channels, mumudvb_channel_t *channels, int Socket);

/**@todo : deal with the RTP over http case ie implement RTSP  --> is it useful over TCP ?*/
extern int rtp_header;


/** @brief Read a line of the configuration file to check if there is a unicast parameter
 *
 * @param unicast_vars the unicast parameters
 * @param substring The currrent line
 */
int read_unicast_configuration(unicast_parameters_t *unicast_vars, mumudvb_channel_t *current_channel, int ip_ok, char *substring)
{

  char delimiteurs[] = CONFIG_FILE_SEPARATOR;

  if (!strcmp (substring, "ip_http"))
  {
    substring = strtok (NULL, delimiteurs);
    if(strlen(substring)>19)
    {
      log_message( MSG_ERROR,
                   "The Ip address %s is too long.\n", substring);
      exit(ERROR_CONF);
    }
    sscanf (substring, "%s\n", unicast_vars->ipOut);
    if(unicast_vars->ipOut)
    {
      log_message( MSG_WARN,
                   "You have enabled the support for HTTP Unicast. This feature is quite youg, please report any bug/comment\n");
    }
  }
  else if (!strcmp (substring, "unicast_consecutive_errors_timeout"))
  {
    substring = strtok (NULL, delimiteurs);
    unicast_vars->consecutive_errors_timeout = atoi (substring);
    if(unicast_vars->consecutive_errors_timeout<=0)
      log_message( MSG_WARN,
                   "Warning : You have desactivated the unicast timeout for disconnecting clients, this can lead to an accumulation of zombie clients, this is unadvised, prefer a long timeout\n");
  }
  else if (!strcmp (substring, "unicast_max_clients"))
  {
    substring = strtok (NULL, delimiteurs);
    unicast_vars->max_clients = atoi (substring);
  }
  else if (!strcmp (substring, "port_http"))
  {
    substring = strtok (NULL, delimiteurs);
    unicast_vars->portOut = atoi (substring);
  }
  else if (!strcmp (substring, "unicast_port"))
  {
    if ( ip_ok == 0)
    {
      log_message( MSG_ERROR,
                   "unicast_port : You must precise ip first\n");
      exit(ERROR_CONF);
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->unicast_port = atoi (substring);
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
int unicast_create_listening_socket(int socket_type, int socket_channel, char *ipOut, int port, struct sockaddr_in *sIn, int *socketIn, fds_t *fds, unicast_parameters_t *unicast_vars)
{
  *socketIn= makeTCPclientsocket(ipOut, port, sIn);
      //We add them to the poll descriptors
  if(*socketIn>0)
  {
    fds->pfdsnum++;
    log_message(MSG_DEBUG, "unicast : fds->pfdsnum : %d\n", fds->pfdsnum);
    fds->pfds=realloc(fds->pfds,(fds->pfdsnum+1)*sizeof(struct pollfd));
    if (fds->pfds==NULL)
    {
      log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      return -1;
    }
    fds->pfds[fds->pfdsnum-1].fd = *socketIn;
    fds->pfds[fds->pfdsnum-1].events = POLLIN | POLLPRI;
    fds->pfds[fds->pfdsnum].fd = 0;
    fds->pfds[fds->pfdsnum].events = POLLIN | POLLPRI;
          //Information about the descriptor
    unicast_vars->fd_info=realloc(unicast_vars->fd_info,(fds->pfdsnum)*sizeof(unicast_fd_info_t));
    if (unicast_vars->fd_info==NULL)
    {
      log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      return -1;
    }
          //Master connection
    unicast_vars->fd_info[fds->pfdsnum-1].type=socket_type;
    unicast_vars->fd_info[fds->pfdsnum-1].channel=socket_channel;
    unicast_vars->fd_info[fds->pfdsnum-1].client=NULL;
  }
  else
  {
    log_message( MSG_WARN, "Problem creating the socket %s:%d : %s\n",ipOut,port,strerror(errno) );
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
int unicast_handle_fd_event(unicast_parameters_t *unicast_vars, fds_t *fds, mumudvb_channel_t *channels, int number_of_channels)
{
  int iRet;
  //We look what happened for which connection
  int actual_fd;


  for(actual_fd=1;actual_fd<fds->pfdsnum;actual_fd++)
    {
      iRet=0;
      if((fds->pfds[actual_fd].revents&POLLHUP)&&(unicast_vars->fd_info[actual_fd].type==UNICAST_CLIENT))
      {
        log_message(MSG_DEBUG,"Unicast : We've got a POLLHUP. Actual_fd %d socket %d we close the connection \n", actual_fd, fds->pfds[actual_fd].fd );
        unicast_close_connection(unicast_vars,fds,fds->pfds[actual_fd].fd,channels);
          //We check if we hage to parse fds->pfds[actual_fd].revents (the last fd moved to the actual one)
        if(fds->pfds[actual_fd].revents)
          actual_fd--;//Yes, we force the loop to see it 
      }
      if((fds->pfds[actual_fd].revents&POLLIN)||(fds->pfds[actual_fd].revents&POLLPRI))
        {
          if((unicast_vars->fd_info[actual_fd].type==UNICAST_MASTER)||
              (unicast_vars->fd_info[actual_fd].type==UNICAST_LISTEN_CHANNEL))
          {
            //Event on the master connection or listenin channel 
            //New connection, we accept the connection
            log_message(MSG_DEBUG,"Unicast : New client\n");
            int tempSocket;
            unicast_client_t *tempClient;
            //we accept the incoming connection
            tempClient=unicast_accept_connection(unicast_vars, fds->pfds[actual_fd].fd);

            if(tempClient!=NULL)
            {
              tempSocket=tempClient->Socket;
              fds->pfdsnum++;
              fds->pfds=realloc(fds->pfds,(fds->pfdsnum+1)*sizeof(struct pollfd));
              if (fds->pfds==NULL)
              {
                log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
                Interrupted=ERROR_MEMORY<<8;
                return -1;
              }
              //We poll the new socket
              fds->pfds[fds->pfdsnum-1].fd = tempSocket;
              fds->pfds[fds->pfdsnum-1].events = POLLIN | POLLPRI | POLLHUP; //We also poll the deconnections
              fds->pfds[fds->pfdsnum].fd = 0;
              fds->pfds[fds->pfdsnum].events = POLLIN | POLLPRI;

              //Information about the descriptor
              unicast_vars->fd_info=realloc(unicast_vars->fd_info,(fds->pfdsnum)*sizeof(unicast_fd_info_t));
              if (unicast_vars->fd_info==NULL)
              {
                log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
                Interrupted=ERROR_MEMORY<<8;
                return -1;
              }
              //client connection
              unicast_vars->fd_info[fds->pfdsnum-1].type=UNICAST_CLIENT;
              unicast_vars->fd_info[fds->pfdsnum-1].channel=-1;
              unicast_vars->fd_info[fds->pfdsnum-1].client=tempClient;


              log_message(MSG_DEBUG,"Unicast : Number of clients : %d\n", unicast_vars->client_number);

              if(unicast_vars->fd_info[actual_fd].type==UNICAST_LISTEN_CHANNEL)
              {
                //Event on a channel connection, we open a new socket for this client and 
                //we store the wanted channel for when we will get the GET
                log_message(MSG_DEBUG,"Unicast : Connection on a channel socket the client  will get the channel %d\n", unicast_vars->fd_info[actual_fd].channel);
                tempClient->askedChannel=unicast_vars->fd_info[actual_fd].channel;
              }
            }
          }
          else if(unicast_vars->fd_info[actual_fd].type==UNICAST_CLIENT)
          {
            //Event on a client connectio i.e. the client asked something
            log_message(MSG_DEBUG,"Unicast : New message for socket %d\n", fds->pfds[actual_fd].fd);
            iRet=unicast_handle_message(unicast_vars,unicast_vars->fd_info[actual_fd].client, channels, number_of_channels);
            if (iRet==-2 ) //iRet==-2 --> 0 received data or error, we close the connection
            {
              unicast_close_connection(unicast_vars,fds,fds->pfds[actual_fd].fd,channels);
              //We check if we hage to parse fds->pfds[actual_fd].revents (the last fd moved to the actual one)
              if(fds->pfds[actual_fd].revents)
                actual_fd--;//Yes, we force the loop to see it again
            }
          }
          else
          {
            log_message(MSG_WARN,"Unicast : File descriptor with bad type, please contact\n Debug information : actual_fd %d unicast_vars->fd_info[actual_fd].type %d\n",
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
  int tempSocket;
  unicast_client_t *tempClient;
  struct sockaddr_in tempSocketAddrIn;

  l = sizeof(struct sockaddr);
  tempSocket = accept(socketIn, (struct sockaddr *) &tempSocketAddrIn, &l);
  if (tempSocket < 0)
    {
      log_message(MSG_ERROR,"Unicast : Error when accepting the incoming connection : %s\n", strerror(errno));
      return NULL;
    }
  log_message(MSG_DETAIL,"Unicast : New connection from %s:%d\n",inet_ntoa(tempSocketAddrIn.sin_addr), tempSocketAddrIn.sin_port);

  //Now we set this socket to be non blocking because we poll it
  int flags;
  flags = fcntl(tempSocket, F_GETFL, 0);
  flags |= O_NONBLOCK;
  if (fcntl(tempSocket, F_SETFL, flags) < 0)
    {
      log_message(MSG_ERROR,"Set non blocking failed : %s\n",strerror(errno));
      return NULL;
    }

  /* if the maximum number of clients is reached, raise a temporary error*/
  if((unicast_vars->max_clients>0)&&(unicast_vars->client_number>=unicast_vars->max_clients))
    {
      int iRet;
      log_message(MSG_INFO,"Unicast : Too many clients connected, we raise an error to  %s\n", inet_ntoa(tempSocketAddrIn.sin_addr));
      iRet=write(tempSocket,HTTP_503_REPLY, strlen(HTTP_503_REPLY)); //iRet is to make the copiler happy we will close the connection anyways
      close(tempSocket);
      return NULL;
    }

  tempClient=unicast_add_client(unicast_vars, tempSocketAddrIn, tempSocket);
  if( tempClient == NULL)
    {
      //We cannot create the client, we close the socket cleanly
      close(tempSocket);
      return NULL;
    }

  return tempClient;

}

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
  unicast_client_t *clienttmp;

  log_message(MSG_DETAIL,"Unicast : We delete the client %s:%d, socket %d\n",inet_ntoa(client->SocketAddr.sin_addr), client->SocketAddr.sin_port, client->Socket);

  log_message(MSG_DEBUG,"Unicast : ---------------Before-------------\n");
  if(unicast_vars->clients==NULL)
    log_message(MSG_ERROR,"Unicast : 0 client\n");
  else
    {
      int i;
      i=0;
      clienttmp=unicast_vars->clients;
      while(clienttmp!=NULL)
	{
	  log_message(MSG_DEBUG,"We see socket %d\n",clienttmp->Socket);
	  clienttmp=clienttmp->next;
	  i++;
	}
    }
  


  if (client->Socket >= 0)
    {
      close(client->Socket);
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
  free(client);

  log_message(MSG_DEBUG,"Unicast : --------------- After -------------\n");
  if(unicast_vars->clients==NULL)
    log_message(MSG_ERROR,"Unicast : 0 client\n");
  else
    {
      int i;
      i=0;
      clienttmp=unicast_vars->clients;
      while(clienttmp!=NULL)
	{
	  log_message(MSG_DEBUG,"We see socket %d\n",clienttmp->Socket);
	  clienttmp=clienttmp->next;
	  i++;
	}
    }


  unicast_vars->client_number--;


  return 0;
}



/** @brief Close an unicast connection and delete the client
 *
 * @param unicast_vars the unicast parameters
 * @param fds The polling file descriptors
 * @param Socket The socket of the client we want to disconnect
 * @param channels The channel list
 */
void unicast_close_connection(unicast_parameters_t *unicast_vars, fds_t *fds, int Socket, mumudvb_channel_t *channels)
{

  int actual_fd;
  actual_fd=0;
  //We find the FD correspondig to this client
  while((actual_fd<fds->pfdsnum) && (fds->pfds[actual_fd].fd!=Socket))
    actual_fd++;

  if(actual_fd==fds->pfdsnum)
    {
      log_message(MSG_ERROR,"Unicast : close connection : we did't find the file descriptor this should never happend, please contact\n");
      actual_fd=0;
      //We find the FD correspondig to this client
      while(actual_fd<fds->pfdsnum) 
	{
	  log_message(MSG_ERROR,"Unicast : fds->pfds[actual_fd].fd %d Socket %d \n", fds->pfds[actual_fd].fd,Socket);
	  actual_fd++;
	}
      return;
    }

  log_message(MSG_DEBUG,"Unicast : We close the connection\n");
  //We delete the client
  unicast_del_client(unicast_vars, unicast_vars->fd_info[actual_fd].client, channels);
  //We move the last fd to the actual/deleted one, and decrease the number of fds by one
  fds->pfds[actual_fd].fd = fds->pfds[fds->pfdsnum-1].fd;
  fds->pfds[actual_fd].events = fds->pfds[fds->pfdsnum-1].events;
  fds->pfds[actual_fd].revents = fds->pfds[fds->pfdsnum-1].revents;
  //we move the file descriptor information
  unicast_vars->fd_info[actual_fd] = unicast_vars->fd_info[fds->pfdsnum-1];
  //last one set to 0 for poll()
  fds->pfds[fds->pfdsnum-1].fd=0;
  fds->pfds[fds->pfdsnum-1].events=POLLIN|POLLPRI;
  fds->pfds[fds->pfdsnum-1].revents=0; //We clear it to avoid nasty bugs ...
  fds->pfdsnum--;
  fds->pfds=realloc(fds->pfds,(fds->pfdsnum+1)*sizeof(struct pollfd));
  if (fds->pfds==NULL)
  {
    log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    Interrupted=ERROR_MEMORY<<8;
  }
  unicast_vars->fd_info=realloc(unicast_vars->fd_info,(fds->pfdsnum)*sizeof(unicast_fd_info_t));
  if (unicast_vars->fd_info==NULL)
  {
    log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    Interrupted=ERROR_MEMORY<<8;
  }
  log_message(MSG_DEBUG,"Unicast : Number of clients : %d\n", unicast_vars->client_number);

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
int unicast_handle_message(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels, int number_of_channels)
{
  int received_len;
  (void) unicast_vars;

  /************ auto increasing buffer to receive the message **************/
  if((client->buffersize-client->bufferpos)<RECV_BUFFER_MULTIPLE)
    {
      client->buffer=realloc(client->buffer,(client->buffersize + RECV_BUFFER_MULTIPLE)*sizeof(char));
      if(client->buffer==NULL)
	{
          log_message(MSG_ERROR,"Unicast : Problem with realloc for the client buffer : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
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
  if(strlen(client->buffer)>5 && strstr(client->buffer, "\n\r\n\0"))
    {
      int pos,err404;
      char *substring=NULL;
      int requested_channel;
      int iRet;
      requested_channel=0;
      pos=0;
      err404=0;

      log_message(MSG_DEBUG,"Unicast : End of HTTP request, we parse it\n");

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
            log_message(MSG_DEBUG,"Unicast : Channel by socket, number %d\n",requested_channel);
            client->askedChannel=-1;
          }
	  //Channel by number
	  //GET /bynumber/channelnumber
	  else if(strstr(client->buffer +pos ,"/bynumber/")==(client->buffer +pos))
	    {
	      if(client->channel!=-1)
		{
		  log_message(MSG_INFO,"Unicast : A channel (%d) is already streamed to this client, it shouldn't ask for a new one without closing the connection, error 501\n",client->channel);
		  iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY)); //iRet is to make the copiler happy we will close the connection anyways
		  return -2; //to delete the client
		}
		
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
	      if(client->channel!=-1)
		{
		  log_message(MSG_INFO,"Unicast : A channel is already streamed to this client, it shouldn't ask for a new one without closing the connection, error 501\n");
		  iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY));//iRet is to make the copiler happy we will close the connection anyways
		  return -2; //to delete the client
		}
	      pos+=8;
	      log_message(MSG_DEBUG,"Unicast : Channel by number\n");
	      substring = strtok (client->buffer+pos, " ");
	      if(substring == NULL)
		err404=1;
	      else
		{
		  log_message(MSG_DEBUG,"Unicast : Channel by name, name %s\n",substring);
		  //search the channel
		  err404=1;//Temporary
		  /** @todo implement the search without the spaces*/
		}
	    }
	  //Channels list
	  else if(strstr(client->buffer +pos ,"/channels_list.html")==(client->buffer +pos))
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

	      unicast_send_streamed_channels_list (number_of_channels, channels, client->Socket, substring);
	      return -2; //We close the connection afterwards
	    }
	  //Channels list, text version
          else if(strstr(client->buffer +pos ,"/channels_list.txt")==(client->buffer +pos))
            {
              unicast_send_streamed_channels_list_txt (number_of_channels, channels, client->Socket);
              return -2; //We close the connection afterwards
            }
            //statistics, text version
          else if(strstr(client->buffer +pos ,"/monitor/statistics.txt")==(client->buffer +pos))
            {
              unicast_send_statistics_txt (number_of_channels, channels, client->Socket);
              return -2; //We close the connection afterwards
            }
	  //Not implemented path --> 404
	  else
	      err404=1;




	  if(err404)
	    {
	      log_message(MSG_INFO,"Unicast : Path not found i.e. 404\n");
	      iRet=write(client->Socket,HTTP_404_REPLY, strlen(HTTP_404_REPLY));//iRet is to make the copiler happy we will close the connection anyways
	      iRet=write(client->Socket,HTTP_404_REPLY_HTML, strlen(HTTP_404_REPLY_HTML));//iRet is to make the copiler happy we will close the connection anyways
	      return -2; //to delete the client
	    }

	  //We have found a channel, we add the client
	  if(requested_channel)
	    {
	      if(!channel_add_unicast_client(client,&channels[requested_channel-1]))
		client->channel=requested_channel-1;
	      else
		return -2;
	    }

	}
      else
	{
	  //We don't implement this http method, but if the client is already connected, we keep the connection
	  if(client->channel==-1)
	    {
	      log_message(MSG_INFO,"Unicast : Unhandled HTTP method : \"%s\", error 501\n",  strtok (client->buffer+pos, " "));
	      iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY));//iRet is to make the copiler happy we will close the connection anyways
	      return -2; //to delete the client
	    }
	  else
	    {
	      log_message(MSG_INFO,"Unicast : Unhandled HTTP method : \"%s\", error 501 but we keep the client connected\n",  strtok (client->buffer+pos, " "));
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


/** @brief This function add an unicast client to a channel
 *
 * @param client the client
 * @param channel the channel
 */
int channel_add_unicast_client(unicast_client_t *client,mumudvb_channel_t *channel)
{
  unicast_client_t *last_client;
  int iRet;

  log_message(MSG_INFO,"Unicast : We add the client %s:%d to the channel \"%s\"\n",inet_ntoa(client->SocketAddr.sin_addr), client->SocketAddr.sin_port,channel->name);

  iRet=write(client->Socket,HTTP_OK_REPLY, strlen(HTTP_OK_REPLY));
  if(iRet!=strlen(HTTP_OK_REPLY))
    {
      log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
      return -1;
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


/** @brief Send a basic html file containig the list of streamed channels
 *
 * @param number_of_channels the number of channels
 * @param channels the channels array
 * @param Socket the socket on wich the information have to be sent
 * @param host The server ip address/name (got in the HTTP GET request)
 */
int 
unicast_send_streamed_channels_list (int number_of_channels, mumudvb_channel_t *channels, int Socket, char *host)
{
  int curr_channel;
  int iRet;
  char buffer[255];


  iRet=write(Socket,HTTP_OK_HTML_REPLY, strlen(HTTP_OK_HTML_REPLY));
  if(iRet!=strlen(HTTP_OK_HTML_REPLY))
    {
      log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
      return -1;
    }
  iRet=write(Socket,HTTP_CHANNELS_REPLY_START, strlen(HTTP_CHANNELS_REPLY_START));
  if(iRet!=strlen(HTTP_CHANNELS_REPLY_START))
    {
      log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
      return -1;
    }


  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    if (channels[curr_channel].streamed_channel_old)
      {
	if(host)
	  iRet=snprintf(buffer, 255, "Channel number %d : %s<br>Unicast link : <a href=\"http://%s/bynumber/%d\">http://%s/bynumber/%d</a><br>Multicast ip : %s:%d<br><br>\r\n",
			curr_channel,
			channels[curr_channel].name,
			host,curr_channel+1,
			host,curr_channel+1,
			channels[curr_channel].ipOut,channels[curr_channel].portOut);
	else
	  iRet=snprintf(buffer, 255, "Channel number %d : \"%s\"<br>Multicast ip : %s:%d<br><br>\r\n",curr_channel,channels[curr_channel].name,channels[curr_channel].ipOut,channels[curr_channel].portOut);
     
	iRet=write(Socket,buffer, strlen(buffer));
	if(iRet!=(int)strlen(buffer)) //Cast to make compiler happy
	  {
	    log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
	    return -1;
	  }
      }

  iRet=write(Socket,HTTP_CHANNELS_REPLY_END, strlen(HTTP_CHANNELS_REPLY_END));
  if(iRet!=strlen(HTTP_CHANNELS_REPLY_END))
    {
      log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
      return -1;
    }


  return 0;
}



/** @brief Send a basic text file containig the list of streamed channels
 *
 * @param number_of_channels the number of channels
 * @param channels the channels array
 * @param Socket the socket on wich the information have to be sent
 */
int 
unicast_send_streamed_channels_list_txt (int number_of_channels, mumudvb_channel_t *channels, int Socket)
{
  int curr_channel;
  int iRet;
  char buffer[1024];
  unicast_client_t *unicast_client=NULL;
  int clients=0;


  iRet=write(Socket,HTTP_OK_TEXT_REPLY, strlen(HTTP_OK_TEXT_REPLY));
  if(iRet!=strlen(HTTP_OK_TEXT_REPLY))
    {
      log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
      return -1;
    }
  iRet=write(Socket,HTTP_CHANNELS_REPLY_TEXT_START, strlen(HTTP_CHANNELS_REPLY_TEXT_START));
  if(iRet!=strlen(HTTP_CHANNELS_REPLY_TEXT_START))
    {
      log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
      return -1;
    }

  //"#Channel number::name::sap playlist group::multicast ip::multicast port::num unicast clients::scrambling ratio\r\n"
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    if (channels[curr_channel].streamed_channel_old)
      {
	clients=0;
	unicast_client=channels[curr_channel].clients;
	while(unicast_client!=NULL)
	  {
	    unicast_client=unicast_client->chan_next;
	    clients++;
	  }
	iRet=snprintf(buffer, 1024, "%d::%s::%s::%s::%d::%d::%d\r\n",
		      curr_channel,
		      channels[curr_channel].name,
		      channels[curr_channel].sap_group,
		      channels[curr_channel].ipOut,
		      channels[curr_channel].portOut,
		      clients,
		      channels[curr_channel].ratio_scrambled);
				  
	iRet=write(Socket,buffer, strlen(buffer));
	if(iRet!=(int)strlen(buffer)) //Cast to make compiler happy
	  {
	    log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
	    return -1;
	  }
      }

  iRet=write(Socket,"\r\n\r\n", strlen("\r\n\r\n"));
  if(iRet!=strlen("\r\n\r\n"))
    {
      log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
      return -1;
    }


  return 0;
}



/** @brief Send a basic text file containig statistics
 *
 * @param number_of_channels the number of channels
 * @param channels the channels array
 * @param Socket the socket on wich the information have to be sent
 */
int 
unicast_send_statistics_txt(int number_of_channels, mumudvb_channel_t *channels, int Socket)
{
  int curr_channel;
  int iRet;
  char buffer[1024];
  unicast_client_t *unicast_client=NULL;
  int clients=0;
  char *dest=NULL;
  char tempdest[81];
  char str_pidtype[81];
  char str_service_type[81];
  int len_dest;
  int i;
  struct timeval tv;
  extern long real_start_time;

  iRet=write(Socket,HTTP_OK_TEXT_REPLY, strlen(HTTP_OK_TEXT_REPLY));
  if(iRet!=strlen(HTTP_OK_TEXT_REPLY))
  {
    log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
    return -1;
  }
  iRet=write(Socket,HTTP_STATISTICS_TEXT_START, strlen(HTTP_STATISTICS_TEXT_START));
  if(iRet!=strlen(HTTP_STATISTICS_TEXT_START))
  {
    log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
    return -1;
  }

  /**@todo : send general statistics*/
  gettimeofday (&tv, (struct timezone *) NULL);
  iRet=snprintf(buffer, 1024, "Uptime  %d:%02d:%02d\r\n\r\n#Channel statistics \r\n",
                ((int)(tv.tv_sec - real_start_time)/3600),
                ((int)((tv.tv_sec - real_start_time) % 3600)/60),
                ((int)(tv.tv_sec - real_start_time) %60));
  iRet=write(Socket,buffer, strlen(buffer));
  if(iRet!=(int)strlen(buffer)) //Cast to make compiler happy
  {
    log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
    return -1;
  }
  
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    if (channels[curr_channel].streamed_channel_old)
  {
    clients=0;
    unicast_client=channels[curr_channel].clients;
    while(unicast_client!=NULL)
    {
      unicast_client=unicast_client->chan_next;
      clients++;
    }
    for(i=0;i<channels[curr_channel].num_pids;i++)
    {
      pid_type_to_str(str_pidtype,channels[curr_channel].pids_type[i]);
      if(snprintf(tempdest,80*sizeof(char),"  Pid %d type %s\r\n",channels[curr_channel].pids[i],str_pidtype)!=-1)
      {
        if(dest)
          len_dest=strlen(dest);
        else
          len_dest=0;
        dest=realloc(dest,(len_dest+strlen(tempdest)+1)*sizeof(char));
        strcat(dest,tempdest);
      }
    }
    service_type_to_str(str_service_type,channels[curr_channel].channel_type);
    iRet=snprintf(buffer, 1024, "Channel number %d\r\nName %s\r\nService type %s\r\nTs_id %d\r\nSap Group %s\r\nMulticast Ip %s\r\nMulticast Port %d\r\nUnicast port %d\r\nNumber of Unicast clients %d\r\nRatio of scrambled packets %d\r\nTraffic (kB/s) %f\r\nNumber of pids %d\r\nPids :\r\n%s\r\n\r\n",
                  curr_channel,
                  channels[curr_channel].name,
                  str_service_type,
                  channels[curr_channel].ts_id,
                  channels[curr_channel].sap_group,
                  channels[curr_channel].ipOut,
                  channels[curr_channel].portOut,
                  channels[curr_channel].unicast_port,
                  clients,
                  channels[curr_channel].ratio_scrambled,
                  channels[curr_channel].traffic,
                  channels[curr_channel].num_pids,
                  dest);
    free(dest);
    dest=NULL;
    iRet=write(Socket,buffer, strlen(buffer));
    if(iRet!=(int)strlen(buffer)) //Cast to make compiler happy
    {
      log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
      return -1;
    }
  }

  iRet=write(Socket,"\r\n\r\n", strlen("\r\n\r\n"));
  if(iRet!=strlen("\r\n\r\n"))
  {
    log_message(MSG_INFO,"Unicast : Error when sending the HTTP reply\n");
    return -1;
  }


  return 0;
}
