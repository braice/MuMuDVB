/*
* MuMuDVB - Stream a DVB transport stream.
*
* (C) 2009-2011 Brice DUBOST
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
* @date 2009-2011
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
#include <string.h>
#include <stdarg.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdlib.h>

#include "unicast_http.h"
#include "unicast_queue.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"
#include "tune.h"
#include "autoconf.h"
#ifdef ENABLE_CAM_SUPPORT
#include "cam.h"
#endif

extern int Interrupted;
static char *log_module="Unicast : ";

//from unicast_client.c
unicast_client_t *unicast_add_client(unicast_parameters_t *unicast_vars, struct sockaddr_in SocketAddr, int Socket);
int channel_add_unicast_client(unicast_client_t *client,mumudvb_channel_t *channel);

unicast_client_t *unicast_accept_connection(unicast_parameters_t *unicast_vars, int socketIn);
void unicast_close_connection(unicast_parameters_t *unicast_vars, fds_t *fds, int Socket, mumudvb_channel_t *channels);

int
unicast_send_streamed_channels_list (int number_of_channels, mumudvb_channel_t *channels, int Socket, char *host);
int
unicast_send_play_list_unicast (int number_of_channels, mumudvb_channel_t *channels, int Socket, int unicast_portOut, int perport);
int
unicast_send_play_list_multicast (int number_of_channels, mumudvb_channel_t* channels, int Socket, int vlc);
int
unicast_send_streamed_channels_list_js (int number_of_channels, mumudvb_channel_t *channels, int Socket);
int
unicast_send_signal_power_js (int Socket, fds_t *fds);
int
unicast_send_channel_traffic_js (int number_of_channels, mumudvb_channel_t *channels, int Socket);
int
unicast_send_xml_state (int number_of_channels, mumudvb_channel_t *channels, int Socket, fds_t *fds, tuning_parameters_t *tuneparams, autoconf_parameters_t *autoconf_vars, void *cam_vars);
int
unicast_send_cam_menu (int Socket, void *cam_vars);
int
unicast_send_cam_action (int Socket, char *Key, void *cam_vars);

int unicast_handle_message(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels, int num_of_channels, fds_t *fds, tuning_parameters_t *tuneparams, autoconf_parameters_t *autoconf_vars, void *cam_vars);

#define REPLY_HEADER 0
#define REPLY_BODY 1
#define REPLY_SIZE_STEP 256

struct unicast_reply {
  char* buffer_header;
  char* buffer_body;
  int length_header;
  int length_body;
  int used_header;
  int used_body;
  int type;
};
static struct unicast_reply* unicast_reply_init();
static int unicast_reply_free(struct unicast_reply *reply);
static int unicast_reply_write(struct unicast_reply *reply, const char* msg, ...);
static int unicast_reply_send(struct unicast_reply *reply, int socket, int code, const char* content_type);


/**@todo : deal with the RTP over http case ie implement RTSP*/


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
      log_message( log_module,  MSG_ERROR,
                   "The Ip address %s is too long.\n", substring);
                   exit(ERROR_CONF);
    }
    sscanf (substring, "%s\n", unicast_vars->ipOut);
    if(unicast_vars->ipOut)
    {
      if(unicast_vars->unicast==0)
      {
        log_message( log_module,  MSG_WARN,"You should use the option \"unicast\" to activate unicast instead of ip_http\n");
        unicast_vars->unicast=1;
        log_message( log_module,  MSG_WARN,"You have enabled the support for HTTP Unicast. This feature is quite youg, please report any bug/comment\n");
      }
    }
  }
  else if (!strcmp (substring, "unicast"))
  {
    substring = strtok (NULL, delimiteurs);
    unicast_vars->unicast = atoi (substring);
    if(unicast_vars->unicast)
    {
      log_message( log_module,  MSG_WARN,
                   "You have enabled the support for HTTP Unicast. This feature is quite youg, please report any bug/comment\n");
    }
  }
  else if (!strcmp (substring, "unicast_consecutive_errors_timeout"))
  {
    substring = strtok (NULL, delimiteurs);
    unicast_vars->consecutive_errors_timeout = atoi (substring);
    if(unicast_vars->consecutive_errors_timeout<=0)
      log_message( log_module,  MSG_WARN,
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
      unicast_vars->portOut_str=malloc(sizeof(char)*(strlen(substring)+1));
      strcpy(unicast_vars->portOut_str,substring);
    }
    else
      unicast_vars->portOut = atoi (substring);
  }
  else if (!strcmp (substring, "unicast_port"))
  {
    if ( ip_ok == 0)
    {
      log_message( log_module,  MSG_ERROR,
                   "unicast_port : You have to start a channel first (using ip= or channel_next)\n");
                   exit(ERROR_CONF);
    }
    substring = strtok (NULL, delimiteurs);
    current_channel->unicast_port = atoi (substring);
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
    log_message( log_module, MSG_DEBUG, "unicast : fds->pfdsnum : %d\n", fds->pfdsnum);
    fds->pfds=realloc(fds->pfds,(fds->pfdsnum+1)*sizeof(struct pollfd));
    if (fds->pfds==NULL)
    {
      log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
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
      log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      return -1;
    }
    //Master connection
    unicast_vars->fd_info[fds->pfdsnum-1].type=socket_type;
    unicast_vars->fd_info[fds->pfdsnum-1].channel=socket_channel;
    unicast_vars->fd_info[fds->pfdsnum-1].client=NULL;
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
int unicast_handle_fd_event(unicast_parameters_t *unicast_vars, fds_t *fds, mumudvb_channel_t *channels, int number_of_channels, tuning_parameters_t *tuneparams, autoconf_parameters_t *autoconf_vars, void *cam_vars)
{
  int iRet;
  //We look what happened for which connection
  int actual_fd;


  for(actual_fd=1;actual_fd<fds->pfdsnum;actual_fd++)
  {
    iRet=0;
    if((fds->pfds[actual_fd].revents&POLLHUP)&&(unicast_vars->fd_info[actual_fd].type==UNICAST_CLIENT))
    {
      log_message( log_module, MSG_DEBUG,"We've got a POLLHUP. Actual_fd %d socket %d we close the connection \n", actual_fd, fds->pfds[actual_fd].fd );
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
        log_message( log_module, MSG_FLOOD,"New client\n");
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
            log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
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
            log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
            Interrupted=ERROR_MEMORY<<8;
            return -1;
          }
          //client connection
          unicast_vars->fd_info[fds->pfdsnum-1].type=UNICAST_CLIENT;
          unicast_vars->fd_info[fds->pfdsnum-1].channel=-1;
          unicast_vars->fd_info[fds->pfdsnum-1].client=tempClient;


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
        //Event on a client connectio i.e. the client asked something
        log_message( log_module, MSG_FLOOD,"New message for socket %d\n", fds->pfds[actual_fd].fd);
        iRet=unicast_handle_message(unicast_vars,unicast_vars->fd_info[actual_fd].client, channels, number_of_channels, fds, tuneparams, autoconf_vars, cam_vars);
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
  int tempSocket;
  unicast_client_t *tempClient;
  struct sockaddr_in tempSocketAddrIn;

  l = sizeof(struct sockaddr);
  tempSocket = accept(socketIn, (struct sockaddr *) &tempSocketAddrIn, &l);
  if (tempSocket < 0 )
  {
    log_message( log_module, MSG_WARN,"Error when accepting the incoming connection : %s\n", strerror(errno));
    return NULL;
  }
  struct sockaddr_in tempSocketAddr;
  l = sizeof(struct sockaddr);
  getsockname(tempSocket, (struct sockaddr *) &tempSocketAddr, &l);
  log_message( log_module, MSG_FLOOD,"New connection from %s:%d to %s:%d \n",inet_ntoa(tempSocketAddrIn.sin_addr), tempSocketAddrIn.sin_port,inet_ntoa(tempSocketAddr.sin_addr), tempSocketAddr.sin_port);

  //Now we set this socket to be non blocking because we poll it
  int flags;
  flags = fcntl(tempSocket, F_GETFL, 0);
  flags |= O_NONBLOCK;
  if (fcntl(tempSocket, F_SETFL, flags) < 0)
  {
    log_message( log_module, MSG_ERROR,"Set non blocking failed : %s\n",strerror(errno));
    return NULL;
  }

  /* if the maximum number of clients is reached, raise a temporary error*/
  if((unicast_vars->max_clients>0)&&(unicast_vars->client_number>=unicast_vars->max_clients))
  {
    int iRet;
    log_message( log_module, MSG_INFO,"Too many clients connected, we raise an error to  %s\n", inet_ntoa(tempSocketAddrIn.sin_addr));
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
    log_message( log_module, MSG_ERROR,"close connection : we did't find the file descriptor this should never happend, please contact\n");
    actual_fd=0;
    //We find the FD correspondig to this client
    while(actual_fd<fds->pfdsnum)
    {
      log_message( log_module, MSG_ERROR,"fds->pfds[actual_fd].fd %d Socket %d \n", fds->pfds[actual_fd].fd,Socket);
      actual_fd++;
    }
    return;
  }

  log_message( log_module, MSG_FLOOD,"We close the connection\n");
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
    log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    Interrupted=ERROR_MEMORY<<8;
  }
  unicast_vars->fd_info=realloc(unicast_vars->fd_info,(fds->pfdsnum)*sizeof(unicast_fd_info_t));
  if (unicast_vars->fd_info==NULL)
  {
    log_message( log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    Interrupted=ERROR_MEMORY<<8;
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
int unicast_handle_message(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels, int number_of_channels, fds_t *fds, tuning_parameters_t *tuneparams, autoconf_parameters_t *autoconf_vars, void *cam_vars)
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
      log_message( log_module, MSG_FLOOD,"beginning of buffer %c%c%c%c%c\n",client->buffer[0],client->buffer[1],client->buffer[2],client->buffer[3],client->buffer[4]);
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
          if(client->channel!=-1)
          {
            log_message( log_module, MSG_INFO,"A channel (%d) is already streamed to this client, it shouldn't ask for a new one without closing the connection, error 501\n",client->channel);
            iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY)); //iRet is to make the copiler happy we will close the connection anyways
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
          if(client->channel!=-1)
          {
            log_message( log_module, MSG_INFO,"A channel (%d) is already streamed to this client, it shouldn't ask for a new one without closing the connection, error 501\n",client->channel);
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
          if(client->channel!=-1)
          {
            log_message( log_module, MSG_INFO,"A channel is already streamed to this client, it shouldn't ask for a new one without closing the connection, error 501\n");
            iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY));//iRet is to make the copiler happy we will close the connection anyways
            return -2; //to delete the client
          }
          pos+=strlen("/byname/");
          log_message( log_module, MSG_DEBUG,"Channel by number\n");
          substring = strtok (client->buffer+pos, " ");
          if(substring == NULL)
            err404=1;
          else
          {
            log_message( log_module, MSG_DEBUG,"Channel by name, name %s\n",substring);
            //search the channel
            err404=1;//Temporary
            /** @todo implement the search without the spaces*/
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
          unicast_send_play_list_unicast (number_of_channels, channels, client->Socket, unicast_vars->portOut, 0 );
          return -2; //We close the connection afterwards
        }
        //playlist, m3u
        else if(strstr(client->buffer +pos ,"/playlist_port.m3u ")==(client->buffer +pos))
        {
          log_message( log_module, MSG_DETAIL,"play list\n");
          unicast_send_play_list_unicast (number_of_channels, channels, client->Socket, unicast_vars->portOut, 1 );
          return -2; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/playlist_multicast.m3u ")==(client->buffer +pos))
        {
          log_message( log_module, MSG_DETAIL,"play list\n");
          unicast_send_play_list_multicast (number_of_channels, channels, client->Socket, 0 );
          return -2; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/playlist_multicast_vlc.m3u ")==(client->buffer +pos))
        {
          log_message( log_module, MSG_DETAIL,"play list\n");
          unicast_send_play_list_multicast (number_of_channels, channels, client->Socket, 1 );
          return -2; //We close the connection afterwards
        }
        //statistics, text version
        else if(strstr(client->buffer +pos ,"/channels_list.json ")==(client->buffer +pos))
        {
          log_message( log_module, MSG_DETAIL,"Channel list Json\n");
          unicast_send_streamed_channels_list_js (number_of_channels, channels, client->Socket);
          return -2; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/monitor/signal_power.json ")==(client->buffer +pos))
        {
          log_message( log_module, MSG_DETAIL,"Signal power json\n");
          unicast_send_signal_power_js(client->Socket, fds);
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
          unicast_send_xml_state(number_of_channels, channels, client->Socket, fds, tuneparams, autoconf_vars, cam_vars);
          return -2; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/cam/menu.xml ")==(client->buffer +pos))
        {
          log_message( log_module, MSG_DETAIL,"HTTP request for CAM menu display \n");
          unicast_send_cam_menu(client->Socket, cam_vars);
          return -2; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/cam/action.xml?key=")==(client->buffer +pos))
        {
          log_message( log_module, MSG_DETAIL,"HTTP request for CAM menu action\n");
          pos+=strlen("/cam/action.xml?key=");
          unicast_send_cam_action(client->Socket,client->buffer+pos, cam_vars);
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

  struct unicast_reply* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
    return -1;
  }

  unicast_reply_write(reply, HTTP_CHANNELS_REPLY_START);

  for (int curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    if (channels[curr_channel].streamed_channel)
    {
      if(host)
        unicast_reply_write(reply, "Channel number %d : %s<br>Unicast link : <a href=\"http://%s/bysid/%d\">http://%s/bysid/%d</a><br>Multicast ip : %s:%d<br><br>\r\n",
                            curr_channel+1,
                            channels[curr_channel].name,
                            host,channels[curr_channel].service_id,
                            host,channels[curr_channel].service_id,
                            channels[curr_channel].ip4Out,channels[curr_channel].portOut);
                            else
                              unicast_reply_write(reply, "Channel number %d : \"%s\"<br>Multicast ip : %s:%d<br><br>\r\n",curr_channel+1,channels[curr_channel].name,channels[curr_channel].ip4Out,channels[curr_channel].portOut);
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
unicast_send_play_list_unicast (int number_of_channels, mumudvb_channel_t *channels, int Socket, int unicast_portOut, int perport)
{
  int curr_channel;

  struct unicast_reply* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
    return -1;
  }

  //We get the ip address on which the client is connected
  struct sockaddr_in tempSocketAddr;
  unsigned int l;
  l = sizeof(struct sockaddr);
  getsockname(Socket, (struct sockaddr *) &tempSocketAddr, &l);
  //we write the playlist
  unicast_reply_write(reply, "#EXTM3U\r\n");

  //"#EXTINF:0,title\r\nURL"
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    if (channels[curr_channel].streamed_channel)
    {
      if(!perport)
      {
        unicast_reply_write(reply, "#EXTINF:0,%s\r\nhttp://%s:%d/bysid/%d\r\n",
                          channels[curr_channel].name,
                          inet_ntoa(tempSocketAddr.sin_addr) ,
                          unicast_portOut ,
                          channels[curr_channel].service_id);
      }
      else if(channels[curr_channel].unicast_port)
      {
        unicast_reply_write(reply, "#EXTINF:0,%s\r\nhttp://%s:%d/\r\n",
                          channels[curr_channel].name,
                          inet_ntoa(tempSocketAddr.sin_addr) ,
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

/** @brief Send a basic text file containig the playlist
*
* @param number_of_channels the number of channels
* @param channels the channels array
* @param Socket the socket on wich the information have to be sent
*/
int
unicast_send_play_list_multicast (int number_of_channels, mumudvb_channel_t *channels, int Socket, int vlc)
{
  int curr_channel;
  char urlheader[4];
  char vlcchar[2];
  extern multicast_parameters_t multicast_vars;

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

  if(multicast_vars.rtp_header)
    strcpy(urlheader,"rtp");
  else
    strcpy(urlheader,"udp");

  //"#EXTINF:0,title\r\nURL"
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    if (channels[curr_channel].streamed_channel)
    {
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




/** @brief Send a basic JSON file containig the list of streamed channels
*
* @param number_of_channels the number of channels
* @param channels the channels array
* @param Socket the socket on wich the information have to be sent
*/
int unicast_send_streamed_channels_list_js (int number_of_channels, mumudvb_channel_t *channels, int Socket)
{
  int curr_channel;
  unicast_client_t *unicast_client=NULL;
  int clients=0;

  struct unicast_reply* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
    return -1;
  }

  unicast_reply_write(reply, "[");
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
  {
    clients=0;
    unicast_client=channels[curr_channel].clients;
    while(unicast_client!=NULL)
    {
      unicast_client=unicast_client->chan_next;
      clients++;
    }
    unicast_reply_write(reply, "{\"number\":%d, \"lcn\":%d, \"name\":\"%s\", \"sap_group\":\"%s\", \"ip_multicast\":\"%s\", \"port_multicast\":%d, \"num_clients\":%d, \"scrambling_ratio\":%d, \"is_up\":%d,",
                        curr_channel+1,
                        channels[curr_channel].logical_channel_number,
                        channels[curr_channel].name,
                        channels[curr_channel].sap_group,
                        channels[curr_channel].ip4Out,
                        channels[curr_channel].portOut,
                        clients,
                        channels[curr_channel].ratio_scrambled,
                        channels[curr_channel].streamed_channel);

    unicast_reply_write(reply, "\"unicast_port\":%d, \"service_id\":%d, \"service_type\":\"%s\", \"pids_num\":%d, \n",
                        channels[curr_channel].unicast_port,
                        channels[curr_channel].service_id,
                        service_type_to_str(channels[curr_channel].channel_type),
                        channels[curr_channel].num_pids);
                        unicast_reply_write(reply, "\"pids\":[");
                        for(int i=0;i<channels[curr_channel].num_pids;i++)
                          unicast_reply_write(reply, "{\"number\":%d, \"type\":\"%s\"},\n", channels[curr_channel].pids[i], pid_type_to_str(channels[curr_channel].pids_type[i]));
                        reply->used_body -= 2; // dirty hack to erase the last comma
                        unicast_reply_write(reply, "]");
                        unicast_reply_write(reply, "},\n");

  }
  reply->used_body -= 2; // dirty hack to erase the last comma
  unicast_reply_write(reply, "]\n");

  unicast_reply_send(reply, Socket, 200, "application/json");

  if (0 != unicast_reply_free(reply)) {
    log_message( log_module, MSG_INFO,"Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }
  return 0;
}


/** @brief Send a basic JSON file containig the reception power
*
* @param Socket the socket on wich the information have to be sent
*/
int
unicast_send_signal_power_js (int Socket, fds_t *fds)
{
  unsigned int strength, ber, snr;
  struct unicast_reply* reply = unicast_reply_init();
  if (NULL == reply)
  {
    log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
    return -1;
  }

  strength = ber = snr = 0;
  if (ioctl (fds->fd_frontend, FE_READ_BER, &ber) >= 0)
    if (ioctl (fds->fd_frontend, FE_READ_SIGNAL_STRENGTH, &strength) >= 0)
      if (ioctl (fds->fd_frontend, FE_READ_SNR, &snr) >= 0)
        unicast_reply_write(reply, "{\"ber\":%d, \"strength\":%d, \"snr\":%d}\n", ber,strength,snr);

  unicast_reply_send(reply, Socket, 200, "application/json");

  if (0 != unicast_reply_free(reply)) {
    log_message( log_module, MSG_INFO,"Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }
  return 0;
}



/** @brief Send a basic JSON file containig the channel traffic
*
* @param number_of_channels the number of channels
* @param channels the channels array
* @param Socket the socket on wich the information have to be sent
*/
int
unicast_send_channel_traffic_js (int number_of_channels, mumudvb_channel_t *channels, int Socket)
{
  int curr_channel;
  extern long real_start_time;

  struct unicast_reply* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
    return -1;
  }

  if ((time((time_t*)0L) - real_start_time) >= 10) //10 seconds for the traffic calculation to be done
  {
    unicast_reply_write(reply, "[");
    for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
      unicast_reply_write(reply, "{\"number\":%d, \"name\":\"%s\", \"traffic\":%.2f},\n", curr_channel+1, channels[curr_channel].name, channels[curr_channel].traffic);
    reply->used_body -= 2; // dirty hack to erase the last comma
    unicast_reply_write(reply, "]\n");
  }

  unicast_reply_send(reply, Socket, 200, "application/json");

  if (0 != unicast_reply_free(reply)) {
    log_message( log_module, MSG_INFO,"Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }
  return 0;
}


/** @brief Send a full XML state of the mumudvb instance
*
* @param number_of_channels the number of channels
* @param channels the channels array
* @param Socket the socket on wich the information have to be sent
* @param fds the frontend device structure
*/
int
unicast_send_xml_state (int number_of_channels, mumudvb_channel_t *channels, int Socket, fds_t *fds, tuning_parameters_t *tuneparams, autoconf_parameters_t *autoconf_vars, void *cam_vars_v)
{
  #ifndef ENABLE_CAM_SUPPORT
  (void) cam_vars_v; //to make compiler happy
  #else
  cam_parameters_t *cam_vars=(cam_parameters_t *)cam_vars_v;
  #endif
  // Prepare the HTTP reply
  struct unicast_reply* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message( log_module, MSG_INFO,"Error when creating the HTTP reply\n");
    return -1;
  }

  // UTF-8 Byte Order Mark (BOM)
  unicast_reply_write(reply, "\xef\xbb\xbf");

  // Date time formatting
  time_t rawtime;
  time (&rawtime);
  char sdatetime[25];
  snprintf(sdatetime,25,"%s",ctime(&rawtime));

  // XML header
  unicast_reply_write(reply, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");

  // Starting XML content
  unicast_reply_write(reply, "<mumudvb card=\"%d\" frontend=\"%d\">\n",tuneparams->card,tuneparams->tuner);
  
  // Mumudvb information
  unicast_reply_write(reply, "\t<global_version><![CDATA[%s]]></global_version>\n",VERSION);
  unicast_reply_write(reply, "\t<global_pid>%d</global_pid>\n",getpid ());
  
  // Uptime
  extern long real_start_time;
  struct timeval tv;
  gettimeofday (&tv, (struct timezone *) NULL);
  unicast_reply_write(reply, "\t<global_uptime>%d</global_uptime>\n",(tv.tv_sec - real_start_time));
  
  // Frontend setup
  unicast_reply_write(reply, "\t<frontend_name><![CDATA[%s]]></frontend_name>\n",tuneparams->fe_name);
  unicast_reply_write(reply, "\t<frontend_tuned>%d</frontend_tuned>\n",tuneparams->card_tuned);
  if (tuneparams->fe_type==FE_QPSK) // Do some test for always showing frequency in kHz
	unicast_reply_write(reply, "\t<frontend_frequency>%d</frontend_frequency>\n",tuneparams->freq);
  else
	unicast_reply_write(reply, "\t<frontend_frequency>%d</frontend_frequency>\n",(tuneparams->freq)/1000);
  if (tuneparams->pol==0)
    unicast_reply_write(reply, "\t<frontend_polarization><![CDATA[-]]></frontend_polarization>\n");
  else
    unicast_reply_write(reply, "\t<frontend_polarization><![CDATA[%c]]></frontend_polarization>\n",tuneparams->pol);
  unicast_reply_write(reply, "\t<frontend_symbolrate>%d</frontend_symbolrate>\n",tuneparams->srate);
  
  // Frontend type
  char fetype[10]="Unkonwn";
  if (tuneparams->fe_type==FE_OFDM) snprintf(fetype,10,"DVB-T");
  if (tuneparams->fe_type==FE_QAM)  snprintf(fetype,10,"DVB-C");
  if (tuneparams->fe_type==FE_ATSC) snprintf(fetype,10,"ATSC");
  if (tuneparams->fe_type==FE_QPSK)
  {
#if DVB_API_VERSION >= 5
    if (tuneparams->delivery_system==SYS_DVBS2)
      snprintf(fetype,10,"DVB-S2");  
    else
      snprintf(fetype,10,"DVB-S");  
#else
    snprintf(fetype,10,"DVB-S");  
#endif
  }
  unicast_reply_write(reply, "\t<frontend_system><![CDATA[%s]]></frontend_system>\n",fetype);

  // Frontend status
  char SCVYL[6]="-----";
  fe_status_t festatus;
  mumu_timing();
  if (ioctl (fds->fd_frontend, FE_READ_STATUS, &festatus) != -1)
  {
    if (festatus & FE_HAS_SIGNAL)  SCVYL[0]=83; // S
    if (festatus & FE_HAS_CARRIER) SCVYL[1]=67; // C
    if (festatus & FE_HAS_VITERBI) SCVYL[2]=86; // V
    if (festatus & FE_HAS_SYNC)    SCVYL[3]=89; // Y
    if (festatus & FE_HAS_LOCK)    SCVYL[4]=76; // L
  }
  SCVYL[5]=0;
  unicast_reply_write(reply, "\t<frontend_status><![CDATA[%s]]></frontend_status>\n",SCVYL);

  // Frontend signal
  unsigned int strength, ber, snr;
  strength = ber = snr = 0;
  if (ioctl (fds->fd_frontend, FE_READ_BER, &ber) == -1) ber=-1;
  if (ioctl (fds->fd_frontend, FE_READ_SIGNAL_STRENGTH, &strength) == -1) strength=-1;
  if (ioctl (fds->fd_frontend, FE_READ_SNR, &snr) == -1) snr=-1;
  unicast_reply_write(reply, "\t<frontend_ber>%d</frontend_ber>\n",ber);
  unicast_reply_write(reply, "\t<frontend_signal>%d</frontend_signal>\n",strength);
  unicast_reply_write(reply, "\t<frontend_snr>%d</frontend_snr>\n",snr);
  log_message( log_module,  MSG_FLOOD, "Timing : ioctls took %ld micro seconds\n",mumu_timing());
  // Autoconfiguration state
  if (autoconf_vars->autoconfiguration!=0)
    unicast_reply_write(reply, "\t<autoconf_end>%d</autoconf_end>\n",0);
  else
    unicast_reply_write(reply, "\t<autoconf_end>%d</autoconf_end>\n",1);

  // CAM information
  #ifdef ENABLE_CAM_SUPPORT
    unicast_reply_write(reply, "\t<cam_support>%d</cam_support>\n",cam_vars->cam_support);
    unicast_reply_write(reply, "\t<cam_number>%d</cam_number>\n",cam_vars->cam_number);
    unicast_reply_write(reply, "\t<cam_menustring><![CDATA[%s]]></cam_menustring>\n",cam_vars->cam_menu_string);
    unicast_reply_write(reply, "\t<cam_initialized>%d</cam_initialized>\n",cam_vars->ca_resource_connected);
  #else
    unicast_reply_write(reply, "\t<cam_support>%d</cam_support>\n",0);
    unicast_reply_write(reply, "\t<cam_number>%d</cam_number>\n",0);
    unicast_reply_write(reply, "\t<cam_menustring><![CDATA[No CAM support]]></cam_menustring>\n");
    unicast_reply_write(reply, "\t<cam_initialized>%d</cam_initialized>\n",0);
  #endif

  // Channels list
  int curr_channel;
  for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
  {
    unicast_reply_write(reply, "\t<channel number=\"%d\">\n",curr_channel+1);
    unicast_reply_write(reply, "\t\t<lcn>%d</lcn>\n",channels[curr_channel].logical_channel_number);
    unicast_reply_write(reply, "\t\t<name><![CDATA[%s]]></name>\n",channels[curr_channel].name);
    unicast_reply_write(reply, "\t\t<service_type type=\"%d\"><![CDATA[%s]]></service_type>\n",channels[curr_channel].channel_type,service_type_to_str(channels[curr_channel].channel_type));
	if (channels[curr_channel].portOut==0)
		unicast_reply_write(reply, "\t\t<ip_multicast><![CDATA[0.0.0.0]]></ip_multicast>\n");
	else
		unicast_reply_write(reply, "\t\t<ip_multicast><![CDATA[%s]]></ip_multicast>\n",channels[curr_channel].ip4Out);
    unicast_reply_write(reply, "\t\t<port_multicast>%d</port_multicast>\n",channels[curr_channel].portOut);
    unicast_reply_write(reply, "\t\t<is_up>%d</is_up>\n",channels[curr_channel].streamed_channel);
    unicast_reply_write(reply, "\t\t<traffic>%.0f</traffic>\n",channels[curr_channel].traffic);
    unicast_reply_write(reply, "\t\t<ratio_scrambled>%d</ratio_scrambled>\n",channels[curr_channel].ratio_scrambled);
    unicast_reply_write(reply, "\t\t<service_id>%d</service_id>\n",channels[curr_channel].service_id);
    unicast_reply_write(reply, "\t\t<pmt_pid>%d</pmt_pid>\n",channels[curr_channel].pmt_pid);
    unicast_reply_write(reply, "\t\t<unicast_port>%d</unicast_port>\n",channels[curr_channel].unicast_port);
    unicast_reply_write(reply, "\t\t<ca_sys>\n");
    for(int i=0;i<32;i++)
      if(channels[curr_channel].ca_sys_id[i]!=0)
        unicast_reply_write(reply, "\t\t\t<ca num=\"%d\"><![CDATA[%s]]></ca>\n",channels[curr_channel].ca_sys_id[i],ca_sys_id_to_str(channels[curr_channel].ca_sys_id[i]));
    unicast_reply_write(reply, "\t\t</ca_sys>\n");
    unicast_reply_write(reply, "\t\t<pids>\n");
    for(int i=0;i<channels[curr_channel].num_pids;i++)
      unicast_reply_write(reply, "\t\t\t<pid number=\"%d\" language=\"%s\" scrambled=\"%d\"><![CDATA[%s]]></pid>\n", channels[curr_channel].pids[i], channels[curr_channel].pids_language[i], channels[curr_channel].pids_scrambled[i], pid_type_to_str(channels[curr_channel].pids_type[i]));
    unicast_reply_write(reply, "\t\t</pids>\n");
    unicast_reply_write(reply, "\t</channel>\n");
  }

  // Ending XML content
  unicast_reply_write(reply, "</mumudvb>\n");

  // Cleaning all non acceptable characters for pseudo UTF-8 (in fact, US-ASCII) - Skipping BOM and last zero character
  unsigned char c;
  int j;
  for (j=3; j<reply->used_body; j++)
  {
    c=reply->buffer_body[j];
    if ((c<32 || c>127) && c!=9 && c!=10 && c!=13)
      reply->buffer_body[j]=32;
  }
  unicast_reply_send(reply, Socket, 200, "application/xml; charset=UTF-8");  

  // End of HTTP reply
  if (0 != unicast_reply_free(reply)) {
    log_message( log_module, MSG_INFO,"Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }
  return 0;
}

/** @brief Return the last MMI menu sent by CAM
*
* @param Socket the socket on wich the information have to be sent
*/
int
unicast_send_cam_menu (int Socket, void *cam_vars_v)
{
  #ifndef ENABLE_CAM_SUPPORT
  (void) cam_vars_v; //to make compiler happy
  #else
  cam_parameters_t *cam_vars=(cam_parameters_t *)cam_vars_v;
  #endif
  struct unicast_reply* reply = unicast_reply_init();
  if (NULL == reply)
  {
    log_message( log_module, MSG_INFO,"Unicast : Error when creating the HTTP reply\n");
    return -1;
  }
  
  // UTF-8 Byte Order Mark (BOM)
  unicast_reply_write(reply, "\xef\xbb\xbf");
  
  // Date time formatting
  time_t rawtime;
  time (&rawtime);
  char sdatetime[25];
  snprintf(sdatetime,25,"%s",ctime(&rawtime));

  // XML header
  unicast_reply_write(reply, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");

  // Starting XML content
  unicast_reply_write(reply, "<menu>\n");

  #ifdef ENABLE_CAM_SUPPORT
  // Sending the last menu if existing
  if (cam_vars->ca_resource_connected!=0)
  {
    if (cam_vars->cam_menulist_lines>0)
    {
      int i;
      for (i=0; i<cam_vars->cam_menulist_lines; i++)
        unicast_reply_write(reply, "%s",cam_vars->cam_menulist[i]);
    }
    else
    {
      unicast_reply_write(reply, "\t<datetime><![CDATA[%s]]></datetime>\n",sdatetime);
      unicast_reply_write(reply, "\t<cammenustring><![CDATA[%s]]></cammenustring>\n",cam_vars->cam_menu_string);
      unicast_reply_write(reply, "\t<object><![CDATA[NONE]]></object>\n");
      unicast_reply_write(reply, "\t<title><![CDATA[No menu to display]]></title>\n");
    }
  }
  else
  {
    unicast_reply_write(reply, "\t<datetime><![CDATA[%s]]></datetime>\n",sdatetime);
    unicast_reply_write(reply, "\t<object><![CDATA[NONE]]></object>\n");
    unicast_reply_write(reply, "\t<title><![CDATA[CAM not initialized!]]></title>\n");
  }
  #else
  unicast_reply_write(reply, "\t<datetime><![CDATA[%s]]></datetime>\n",sdatetime);
  unicast_reply_write(reply, "\t<object><![CDATA[NONE]]></object>\n");
  unicast_reply_write(reply, "\t<title><![CDATA[Compiled without CAM support]]></title>\n");
  #endif

  // Ending XML content
  unicast_reply_write(reply, "</menu>\n");

  // Cleaning all non acceptable characters for pseudo UTF-8 (in fact, US-ASCII) - Skipping BOM and last zero character
  unsigned char c;
  int j;
  for (j=3; j<reply->used_body; j++)
  {
    c=reply->buffer_body[j];
    if ((c<32 || c>127) && c!=9 && c!=10 && c!=13)
      reply->buffer_body[j]=32;
  }
  unicast_reply_send(reply, Socket, 200, "application/xml; charset=UTF-8");
  
  // End of HTTP reply
  if (0 != unicast_reply_free(reply)) {
    log_message( log_module, MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }
  return 0;
}
  
/** @brief Send an action to the CAM MMI menu
*
* @param Socket the socket on wich the information have to be sent
*/
int
unicast_send_cam_action (int Socket, char *Key, void *cam_vars_v)
{
  #ifndef ENABLE_CAM_SUPPORT
  (void) cam_vars_v; //to make compiler happy
  #else
  cam_parameters_t *cam_vars=(cam_parameters_t *)cam_vars_v;
  #endif
  struct unicast_reply* reply = unicast_reply_init();
  if (NULL == reply)
  {
    log_message( log_module, MSG_INFO,"Unicast : Error when creating the HTTP reply\n");
    return -1;
  }
  
  // UTF-8 Byte Order Mark (BOM)
  unicast_reply_write(reply, "\xef\xbb\xbf");
  
  // Date time formatting
  time_t rawtime;
  time (&rawtime);
  char sdatetime[25];
  snprintf(sdatetime,25,"%s",ctime(&rawtime));

  // XML header
  unicast_reply_write(reply, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");

  // Starting XML content
  unicast_reply_write(reply, "<action>\n");
  unicast_reply_write(reply, "\t<datetime><![CDATA[%.24s]]></datetime>\n",ctime(&rawtime));
  unicast_reply_write(reply, "\t<key><![CDATA[%c]]></key>\n",*Key);

  #ifdef ENABLE_CAM_SUPPORT
  // Check if valid action to be done [0-9] and 'M' and 'C' and 'O'
  int iKey=(int)*Key;
  if ((iKey>=48 && iKey<=57) || iKey==77 || iKey==67 || iKey==79)
  {
    // Check if CAM is initialized
    if (cam_vars->ca_resource_connected!=0)
    {
      // Disable auto response from now (as a manual action is asked)
      cam_vars->cam_mmi_autoresponse=0;
      // Numbers for MENU/LIST answer
      if (cam_vars->mmi_state==MMI_STATE_MENU && iKey>=48 && iKey<=57)
	  {
        log_message( log_module,  MSG_INFO, "Send CAM MENU key number %d\n",iKey-48);
        en50221_app_mmi_menu_answ(cam_vars->stdcam->mmi_resource, cam_vars->stdcam->mmi_session_number, iKey-48);
		cam_vars->mmi_state=MMI_STATE_OPEN;
	  }
      // 'M' = ask the menu - Always possible
      if (iKey==77)
	  {
	    log_message( log_module,  MSG_INFO, "Ask CAM to enter MENU\n");
        en50221_app_ai_entermenu(cam_vars->stdcam->ai_resource, cam_vars->stdcam->ai_session_number);
		cam_vars->mmi_state=MMI_STATE_OPEN;
	  }
      // Numbers for ENQUIRY answer
      if (cam_vars->mmi_state==MMI_STATE_ENQ && iKey>=48 && iKey<=57)
	  {
	    // We store the new key
		cam_vars->mmi_enq_answer[cam_vars->mmi_enq_entered]=iKey;
		cam_vars->mmi_enq_entered++;
        log_message( log_module,  MSG_INFO, "Received CAM ENQUIRY key number %d (%d of %d expected)\n", iKey-48, cam_vars->mmi_enq_entered, cam_vars->mmi_enq_length);
		// Test if the expected length is received
		if (cam_vars->mmi_enq_entered == cam_vars->mmi_enq_length)
		{
		  // We send the anwser
		  log_message( log_module,  MSG_INFO, "Sending ENQUIRY answer to CAM (answer has the expected length of %d)\n",cam_vars->mmi_enq_entered);
          en50221_app_mmi_answ(cam_vars->stdcam->mmi_resource, cam_vars->stdcam->mmi_session_number, MMI_ANSW_ID_ANSWER, (uint8_t*)cam_vars->mmi_enq_answer, cam_vars->mmi_enq_entered);
		  cam_vars->mmi_state=MMI_STATE_OPEN;
		}
	  }
      // 'C' = send CANCEL as an ENQUIRY answer
      if (cam_vars->mmi_state==MMI_STATE_ENQ && iKey==67)
	  {
	    log_message( log_module,  MSG_INFO, "Send CAM ENQUIRY key CANCEL\n");
        en50221_app_mmi_answ(cam_vars->stdcam->mmi_resource, cam_vars->stdcam->mmi_session_number, MMI_ANSW_ID_CANCEL, NULL, 0);
		cam_vars->mmi_state=MMI_STATE_OPEN;
	  }
      // OK
      unicast_reply_write(reply, "\t<result><![CDATA[OK]]></result>\n");
    }
    else
    {
      unicast_reply_write(reply, "\t<result><![CDATA[ERROR: CAM not initialized!]]></result>\n");
    }
  }
  else
  {
    unicast_reply_write(reply, "\t<result><![CDATA[ERROR: Unknown key!]]></result>\n");
  }
  #else
  unicast_reply_write(reply, "\t<result><![CDATA[Compiled without CAM support]]></result>\n");
  #endif
  
  // Ending XML content
  unicast_reply_write(reply, "</action>\n");

  // Cleaning all non acceptable characters for pseudo UTF-8 (in fact, US-ASCII) - Skipping BOM and last zero character
  unsigned char c;
  int j;
  for (j=3; j<reply->used_body; j++)
  {
    c=reply->buffer_body[j];
    if ((c<32 || c>127) && c!=9 && c!=10 && c!=13)
      reply->buffer_body[j]=32;
  }
  unicast_reply_send(reply, Socket, 200, "application/xml; charset=UTF-8");
  
  // End of HTTP reply
  if (0 != unicast_reply_free(reply)) {
    log_message( log_module, MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }
  return 0;
}









