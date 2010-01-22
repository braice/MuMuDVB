/*
* MuMuDVB - Stream a DVB transport stream.
*
* (C) 2009-2010 Brice DUBOST
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
* @brief File for HTTP unicast common fonctions
* @author Brice DUBOST
* @date 2009-2010
*/

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
#include <string.h>
#include <stdarg.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdlib.h>

#include "unicast_queue.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"
#include "unicast.h"
#include "unicast_common.h"
#include "unicast_http.h"
#include "unicast_rtsp.h"

extern int Interrupted;


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
      if(unicast_vars->unicast==0)
      {
        log_message( MSG_WARN,"You should use the option \"unicast\" to activate unicast instead of ip_http\n");
        unicast_vars->unicast=1;
        log_message( MSG_WARN,"You have enabled the support for HTTP Unicast. This feature is quite youg, please report any bug/comment\n");
      }
    }
  }
  else if (!strcmp (substring, "unicast"))
  {
    substring = strtok (NULL, delimiteurs);
    unicast_vars->unicast = atoi (substring);
    if(unicast_vars->unicast)
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
  else if (!strcmp (substring, "unicast_queue_size"))
  {
    substring = strtok (NULL, delimiteurs);
    unicast_vars->queue_max_size = atoi (substring);
  }
  else if (!strcmp (substring, "port_http"))
  {
    substring = strtok (NULL, "");
    if((strchr(substring,'*')!=NULL)||(strchr(substring,'+')!=NULL)||(strchr(substring,'%')!=NULL))
    {
      unicast_vars->http_portOut_str=malloc(sizeof(char)*(strlen(substring)+1));
      strcpy(unicast_vars->http_portOut_str,substring);
    }
    else
      unicast_vars->http_portOut = atoi (substring);
  }
  else if (!strcmp (substring, "port_rtsp"))
  {
    substring = strtok (NULL, "");
    if((strchr(substring,'*')!=NULL)||(strchr(substring,'+')!=NULL)||(strchr(substring,'%')!=NULL))
    {
      unicast_vars->rtsp_portOut_str=malloc(sizeof(char)*(strlen(substring)+1));
      strcpy(unicast_vars->rtsp_portOut_str,substring);
    }
    else
      unicast_vars->rtsp_portOut = atoi (substring);
  }
  else if (!strcmp (substring, "unicast_rtsp"))
  {
    substring = strtok (NULL, delimiteurs);
    unicast_vars->unicast_rtsp_enable = atoi (substring);
    if(unicast_vars->unicast_rtsp_enable)
      log_message(MSG_DETAIL, "RTSP enabled\n");
  }
  else if (!strcmp (substring, "unicast_port"))
  {
    if ( ip_ok == 0)
    {
      log_message( MSG_ERROR,
                   "unicast_port : You have to start a channel first (using ip= or channel_next)\n");
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
    fds->pfds[fds->pfdsnum-1].revents = 0;
    fds->pfds[fds->pfdsnum].fd = 0;
    fds->pfds[fds->pfdsnum].events = POLLIN | POLLPRI;
    fds->pfds[fds->pfdsnum].revents = 0;
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

/** @brief Close an unicast connection and delete the client
*
* @param unicast_vars the unicast parameters
* @param fds The polling file descriptors
* @param Socket The socket of the client we want to disconnect
* @param channels The channel list
*/
void unicast_close_connection(unicast_parameters_t *unicast_vars, fds_t *fds, int Socket, mumudvb_channel_t *channels)
{
  int delete_client=1;
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
  if(delete_client)
    unicast_del_client(unicast_vars, unicast_vars->fd_info[actual_fd].client, channels);
  /** @todo : deal with the keep alive, we can close the control connection but keep the client*/
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
    if((fds->pfds[actual_fd].revents&POLLHUP)&&((unicast_vars->fd_info[actual_fd].type==UNICAST_CLIENT_HTTP)||(unicast_vars->fd_info[actual_fd].type==UNICAST_CLIENT_RTSP)))
    {
      /** @todo RTSP : no keep alive */
      log_message(MSG_DEBUG,"Unicast : We've got a POLLHUP. Actual_fd %d socket %d we close the connection \n", actual_fd, fds->pfds[actual_fd].fd );
      unicast_close_connection(unicast_vars,fds,fds->pfds[actual_fd].fd,channels);
      //We check if we hage to parse fds->pfds[actual_fd].revents (the last fd moved to the actual one)
      if(fds->pfds[actual_fd].revents)
        actual_fd--;//Yes, we force the loop to see it
    }
    if((fds->pfds[actual_fd].revents&POLLIN)||(fds->pfds[actual_fd].revents&POLLPRI))
    {
      if((unicast_vars->fd_info[actual_fd].type==UNICAST_MASTER_HTTP)||
        (unicast_vars->fd_info[actual_fd].type==UNICAST_LISTEN_CHANNEL)||
        (unicast_vars->fd_info[actual_fd].type==UNICAST_MASTER_RTSP))
      {
        //Event on the master connection or listenin channel
        //New connection, we accept the connection
        if(unicast_vars->fd_info[actual_fd].type==UNICAST_MASTER_RTSP)
          log_message(MSG_DEBUG,"Unicast : New RTSP client\n");
        else
          log_message(MSG_DEBUG,"Unicast : New HTTP client\n");
        int tempSocket;
        unicast_client_t *tempClient;
        //we accept the incoming connection
        if(unicast_vars->fd_info[actual_fd].type==UNICAST_MASTER_RTSP)
          tempClient=unicast_accept_connection(unicast_vars, fds->pfds[actual_fd].fd,CLIENT_RTSP);
        else 
          tempClient=unicast_accept_connection(unicast_vars, fds->pfds[actual_fd].fd,CLIENT_HTTP);

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
          fds->pfds[fds->pfdsnum-1].revents = 0;
          fds->pfds[fds->pfdsnum].fd = 0;
          fds->pfds[fds->pfdsnum].events = POLLIN | POLLPRI;
          fds->pfds[fds->pfdsnum].revents = 0;

          //Information about the descriptor
          unicast_vars->fd_info=realloc(unicast_vars->fd_info,(fds->pfdsnum)*sizeof(unicast_fd_info_t));
          if (unicast_vars->fd_info==NULL)
          {
            log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
            Interrupted=ERROR_MEMORY<<8;
            return -1;
          }
          //client connection
          if(unicast_vars->fd_info[actual_fd].type==UNICAST_MASTER_RTSP)
            unicast_vars->fd_info[fds->pfdsnum-1].type=UNICAST_CLIENT_RTSP;
          else
            unicast_vars->fd_info[fds->pfdsnum-1].type=UNICAST_CLIENT_HTTP;
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
      else if(unicast_vars->fd_info[actual_fd].type==UNICAST_CLIENT_HTTP)
      {
        //Event on a client connectio i.e. the client asked something
        log_message(MSG_FLOOD,"Unicast : New HTTP message for socket %d\n", fds->pfds[actual_fd].fd);
        iRet=unicast_handle_http_message(unicast_vars,unicast_vars->fd_info[actual_fd].client, channels, number_of_channels, fds);
        if (iRet==CLOSE_CONNECTION ) //iRet==-2 --> 0 received data or error, we close the connection
        {
          unicast_close_connection(unicast_vars,fds,fds->pfds[actual_fd].fd,channels);
          //We check if we hage to parse fds->pfds[actual_fd].revents (the last fd moved to the actual one)
          if(fds->pfds[actual_fd].revents)
            actual_fd--;//Yes, we force the loop to see it again
        }
      }
      else if(unicast_vars->fd_info[actual_fd].type==UNICAST_CLIENT_RTSP)
      {
        //Event on a client connectio i.e. the client asked something
        log_message(MSG_FLOOD,"Unicast : New RTSP message for socket %d\n", fds->pfds[actual_fd].fd);
        iRet=unicast_handle_rtsp_message(unicast_vars,unicast_vars->fd_info[actual_fd].client, channels, number_of_channels, fds);
        if (iRet==CLOSE_CONNECTION ) //iRet==CLOSE_CONNECTION --> 0 received data or error, we close the connection
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
unicast_client_t *unicast_accept_connection(unicast_parameters_t *unicast_vars, int socketIn, int client_type)
{

  unsigned int l;
  int tempSocket;
  unicast_client_t *tempClient;
  struct sockaddr_in tempSocketAddrIn;

  l = sizeof(struct sockaddr);
  tempSocket = accept(socketIn, (struct sockaddr *) &tempSocketAddrIn, &l);
  if (tempSocket < 0 )
  {
    log_message(MSG_WARN,"Unicast : Error when accepting the incoming connection : %s\n", strerror(errno));
    return NULL;
  }
  struct sockaddr_in tempSocketAddr;
  l = sizeof(struct sockaddr);
  getsockname(tempSocket, (struct sockaddr *) &tempSocketAddr, &l);
  log_message(MSG_DETAIL,"Unicast : New connection from %s:%d to %s:%d \n",inet_ntoa(tempSocketAddrIn.sin_addr), ntohs(tempSocketAddrIn.sin_port),inet_ntoa(tempSocketAddr.sin_addr), ntohs(tempSocketAddr.sin_port));

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
    if(client_type==CLIENT_HTTP)
      iRet=write(tempSocket,HTTP_503_REPLY, strlen(HTTP_503_REPLY)); //iRet is to make the copiler happy we will close the connection anyways
    else
      iRet=write(tempSocket,RTSP_503_REPLY, strlen(RTSP_503_REPLY));
    close(tempSocket);
    return NULL;
  }

  tempClient=unicast_add_client(unicast_vars, tempSocketAddrIn, tempSocket, client_type);
  if( tempClient == NULL)
  {
    //We cannot create the client, we close the socket cleanly
    close(tempSocket);
    return NULL;
  }

  return tempClient;

}

/** @brief Store the new message on the socket
*
*/
int unicast_new_message(unicast_client_t *client)
{
  int received_len;
  /************ auto increasing buffer to receive the message **************/
  if((client->buffersize-client->bufferpos)<RECV_BUFFER_MULTIPLE)
  {
    client->buffer=realloc(client->buffer,(client->buffersize + RECV_BUFFER_MULTIPLE+1)*sizeof(char)); //the +1 if for the \0 at the end
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
    if(client->bufferpos==0)
      log_message(MSG_DEBUG,"Unicast : beginning of buffer %c%c%c%c%c\n",client->buffer[0],client->buffer[1],client->buffer[2],client->buffer[3],client->buffer[4]);
    client->bufferpos+=received_len;
    log_message(MSG_FLOOD,"Unicast : We received %d, buffer len %d new buffer pos %d\n",received_len,client->buffersize, client->bufferpos);
  }

  if(received_len==-1)
  {
    log_message(MSG_ERROR,"Unicast : Problem with recv : %s\n",strerror(errno));
    return -1;
  }
  if(received_len==0)
    return CLOSE_CONNECTION; //To say to the main program to close the connection

  /***************** Now we parse the message to see if something was asked  *****************/
  client->buffer[client->buffersize]='\0'; //For avoiding strlen to look too far (other option is to use the gnu extension strnlen)

  return 0;
}

void unicast_flush_client(unicast_client_t *client)
{

  if(client->buffer)
    {
      free(client->buffer);
      client->buffersize=0;
      client->bufferpos=0;
    }
  client->buffer=NULL;

}
////////////////////////
// HTTP/RTSP Toolbox  //
////////////////////////



/** @brief Init reply structure
*
*/
unicast_reply_t* unicast_reply_init()
{
  unicast_reply_t* reply = malloc(sizeof (unicast_reply_t));
  if (NULL == reply)
  {
    log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return NULL;
  }
  reply->buffer_header = malloc(REPLY_SIZE_STEP * sizeof (char));
  if (NULL == reply->buffer_header)
  {
    free(reply);
    log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return NULL;
  }
  reply->length_header = REPLY_SIZE_STEP;
  reply->used_header = 0;
  reply->buffer_body = malloc(REPLY_SIZE_STEP * sizeof (char));
  if (NULL == reply->buffer_body)
  {
    free(reply->buffer_header);
    free(reply);
    log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
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
int unicast_reply_free(unicast_reply_t *reply)
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
int unicast_reply_write(unicast_reply_t *reply, const char* msg, ...)
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
      log_message(MSG_WARN,"Unicast : unicast_reply_write with wrong type, please contact\n");
      return -1;
  }
  va_start(args, msg);
  int estimated_len = vsnprintf(NULL, 0, msg, args); /* !! imply gcc -std=c99 */
  //Since vsnprintf put the mess we reinitiate the args
  va_end(args);
  va_start(args, msg);
  while (*length - *used < estimated_len) {
    temp_buffer = realloc(*buffer, *length + REPLY_SIZE_STEP);
    if(temp_buffer == NULL)
    {
      log_message(MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      return -1;
    }
    *buffer=temp_buffer;
    *length += REPLY_SIZE_STEP;
  }
  int real_len = vsnprintf(*buffer+*used, *length - *used, msg, args);
  if (real_len != estimated_len) {
    log_message(MSG_WARN,"Unicast : Error when writing the HTTP reply\n");
  }
  *used += real_len;
  va_end(args);
  return 0;
}

/** @brief Dump the filled buffer on the socket adding HTTP header informations
*/
int unicast_reply_send(unicast_reply_t *reply, int socket, int code, const char* content_type)
{
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
      log_message(MSG_ERROR,"reply send with bad code please contact\n");
      return 0;
  }
  unicast_reply_write(reply, "Server: mumudvb/" VERSION "\r\n");
  unicast_reply_write(reply, "Content-type: %s\r\n", content_type);
  unicast_reply_write(reply, "Content-length: %d\r\n", reply->used_body);
  unicast_reply_write(reply, "\r\n"); /* end header */
  //we merge the header and the body
  reply->buffer_header = realloc(reply->buffer_header, reply->used_header+reply->used_body);
  memcpy(&reply->buffer_header[reply->used_header],reply->buffer_body,sizeof(char)*reply->used_body);
  reply->used_header+=reply->used_body;
  //now we write the data
  int size = write(socket, reply->buffer_header, reply->used_header);
  return size;
}


//////////////////////
// End HTTP Toolbox //
//////////////////////












