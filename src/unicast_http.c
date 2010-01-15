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
* @brief File for HTTP unicast
* @author Brice DUBOST
* @date 2009-2010
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
#include "unicast.h"
#include "unicast_common.h"

extern int Interrupted;

//from unicast_client.c
unicast_client_t *unicast_add_client(unicast_parameters_t *unicast_vars, struct sockaddr_in SocketAddr, int Socket, int client_type);
int channel_add_unicast_client(unicast_client_t *client,mumudvb_channel_t *channel);

unicast_client_t *unicast_accept_http_connection(unicast_parameters_t *unicast_vars, int socketIn);


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

int unicast_handle_http_message(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels, int num_of_channels, fds_t *fds);


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
      if((unicast_vars->fd_info[actual_fd].type==UNICAST_MASTER_HTTP)||
        (unicast_vars->fd_info[actual_fd].type==UNICAST_LISTEN_CHANNEL))
      {
        //Event on the master connection or listenin channel
        //New connection, we accept the connection
        log_message(MSG_DEBUG,"Unicast : New client\n");
        int tempSocket;
        unicast_client_t *tempClient;
        //we accept the incoming connection
        tempClient=unicast_accept_http_connection(unicast_vars, fds->pfds[actual_fd].fd);

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
        log_message(MSG_FLOOD,"Unicast : New message for socket %d\n", fds->pfds[actual_fd].fd);
        iRet=unicast_handle_http_message(unicast_vars,unicast_vars->fd_info[actual_fd].client, channels, number_of_channels, fds);
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
unicast_client_t *unicast_accept_http_connection(unicast_parameters_t *unicast_vars, int socketIn)
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
  log_message(MSG_DETAIL,"Unicast : New connection from %s:%d to %s:%d \n",inet_ntoa(tempSocketAddrIn.sin_addr), tempSocketAddrIn.sin_port,inet_ntoa(tempSocketAddr.sin_addr), tempSocketAddr.sin_port);

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

  tempClient=unicast_add_client(unicast_vars, tempSocketAddrIn, tempSocket, CLIENT_HTTP);
  if( tempClient == NULL)
  {
    //We cannot create the client, we close the socket cleanly
    close(tempSocket);
    return NULL;
  }

  return tempClient;

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
int unicast_handle_http_message(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels, int number_of_channels, fds_t *fds)
{
  int received_len;
  (void) unicast_vars;

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
      unicast_reply_t* reply=NULL;

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

          pos+=strlen("/bynumber/");
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
        //Channel by autoconf_sid_list
        //GET /bytsid/tsid
        else if(strstr(client->buffer +pos ,"/bysid/")==(client->buffer +pos))
        {
          if(client->channel!=-1)
          {
            log_message(MSG_INFO,"Unicast : A channel (%d) is already streamed to this client, it shouldn't ask for a new one without closing the connection, error 501\n",client->channel);
            iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY)); //iRet is to make the copiler happy we will close the connection anyways
            return -2; //to delete the client
          }
          pos+=strlen("/bytsid/");
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
              log_message(MSG_DEBUG,"Unicast : Channel by service id,  service_id %d number %d\n", requested_sid, requested_channel);
            else
            {
              log_message(MSG_INFO,"Unicast : Channel by service id, service_id  %d not found\n",requested_sid);
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
          pos+=strlen("/byname/");
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
          log_message(MSG_DETAIL,"Channel list\n");
          unicast_send_streamed_channels_list (number_of_channels, channels, client->Socket, substring);
          return -2; //We close the connection afterwards
        }
        //playlist, m3u
        else if(strstr(client->buffer +pos ,"/playlist.m3u ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"play list\n");
          unicast_send_play_list_unicast (number_of_channels, channels, client->Socket, unicast_vars->portOut, 0 );
          return -2; //We close the connection afterwards
        }
        //playlist, m3u
        else if(strstr(client->buffer +pos ,"/playlist_port.m3u ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"play list\n");
          unicast_send_play_list_unicast (number_of_channels, channels, client->Socket, unicast_vars->portOut, 1 );
          return -2; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/playlist_multicast.m3u ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"play list\n");
          unicast_send_play_list_multicast (number_of_channels, channels, client->Socket, 0 );
          return -2; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/playlist_multicast_vlc.m3u ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"play list\n");
          unicast_send_play_list_multicast (number_of_channels, channels, client->Socket, 1 );
          return -2; //We close the connection afterwards
        }
        //statistics, text version
        else if(strstr(client->buffer +pos ,"/channels_list.json ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"Channel list Json\n");
          unicast_send_streamed_channels_list_js (number_of_channels, channels, client->Socket);
          return -2; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/monitor/signal_power.json ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"Signal power json\n");
          unicast_send_signal_power_js(client->Socket, fds);
          return -2; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/monitor/channels_traffic.json ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"Channel traffic json\n");
          unicast_send_channel_traffic_js(number_of_channels, channels, client->Socket);
          return -2; //We close the connection afterwards
        }
        //Not implemented path --> 404
        else
          err404=1;


        if(err404)
        {
          log_message(MSG_INFO,"Unicast : Path not found i.e. 404\n");
          reply = unicast_reply_init();
          if (NULL == reply) {
            log_message(MSG_INFO,"Unicast : Error when creating the HTTP reply\n");
            return -2;
          }
          unicast_reply_write(reply, HTTP_404_REPLY_HTML, VERSION);
          unicast_reply_send(reply, client->Socket, 404, "text/html");
          if (0 != unicast_reply_free(reply)) {
            log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
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
          log_message(MSG_INFO,"Unicast : Unhandled HTTP method : \"%s\", error 501\n",  strtok (client->buffer+pos, " "));
          iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY));//iRet is to make the copiler happy we will close the connection anyways
          return -2; //to delete the client
        }
        else
        {
          log_message(MSG_INFO,"Unicast : Unhandled HTTP method : \"%s\", error 501 but we keep the client connected\n",  strtok (client->buffer+pos, " "));
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

  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message(MSG_INFO,"Unicast : Error when creating the HTTP reply\n");
    return -1;
  }

  unicast_reply_write(reply, HTTP_CHANNELS_REPLY_START);

  for (int curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
    if (channels[curr_channel].streamed_channel_old)
    {
      if(host)
        unicast_reply_write(reply, "Channel number %d : %s<br>Unicast link : <a href=\"http://%s/bynumber/%d\">http://%s/bynumber/%d</a><br>Multicast ip : %s:%d<br><br>\r\n",
                            curr_channel,
                            channels[curr_channel].name,
                            host,curr_channel+1,
                            host,curr_channel+1,
                            channels[curr_channel].ipOut,channels[curr_channel].portOut);
      else
        unicast_reply_write(reply, "Channel number %d : \"%s\"<br>Multicast ip : %s:%d<br><br>\r\n",curr_channel,channels[curr_channel].name,channels[curr_channel].ipOut,channels[curr_channel].portOut);
    }
  unicast_reply_write(reply, HTTP_CHANNELS_REPLY_END);

  unicast_reply_send(reply, Socket, 200, "text/html");

  if (0 != unicast_reply_free(reply)) {
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }

  return 0;
}


/** @brief Send a basic text file containig the playlist
*
* @param number_of_channels the number of channels
* @param channels the channels array
* @param Socket the socket on wich the information have to be sent
* @param perport says if the channel have to be given by the url /bynumber or by their port
*/
int
unicast_send_play_list_unicast (int number_of_channels, mumudvb_channel_t *channels, int Socket, int unicast_portOut, int perport)
{
  int curr_channel;

  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message(MSG_INFO,"Unicast : Error when creating the HTTP reply\n");
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
    if (channels[curr_channel].streamed_channel_old)
    {
      if(!perport)
      {
        unicast_reply_write(reply, "#EXTINF:0,%s\r\nhttp://%s:%d/bynumber/%d\r\n",
                          channels[curr_channel].name,
                          inet_ntoa(tempSocketAddr.sin_addr) ,
                          unicast_portOut ,
                          curr_channel+1);
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
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
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

  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message(MSG_INFO,"Unicast : Error when creating the HTTP reply\n");
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
    if (channels[curr_channel].streamed_channel_old)
    {
      unicast_reply_write(reply, "#EXTINF:0,%s\r\n%s://%s%s:%d\r\n",
                          channels[curr_channel].name,
                          urlheader,
                          vlcchar,
                          channels[curr_channel].ipOut,
                          channels[curr_channel].portOut);
    }

    unicast_reply_send(reply, Socket, 200, "audio/x-mpegurl");

    if (0 != unicast_reply_free(reply)) {
      log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
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

  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message(MSG_INFO,"Unicast : Error when creating the HTTP reply\n");
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
                        curr_channel,
                        channels[curr_channel].logical_channel_number,
                        channels[curr_channel].name,
                        channels[curr_channel].sap_group,
                        channels[curr_channel].ipOut,
                        channels[curr_channel].portOut,
                        clients,
                        channels[curr_channel].ratio_scrambled,
                        channels[curr_channel].streamed_channel_old);

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
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
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
  int strength, ber, snr;
  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply)
  {
    log_message(MSG_INFO,"Unicast : Error when creating the HTTP reply\n");
    return -1;
  }

  strength = ber = snr = 0;
  if (ioctl (fds->fd_frontend, FE_READ_BER, &ber) >= 0)
    if (ioctl (fds->fd_frontend, FE_READ_SIGNAL_STRENGTH, &strength) >= 0)
      if (ioctl (fds->fd_frontend, FE_READ_SNR, &snr) >= 0)
        unicast_reply_write(reply, "{\"ber\":%d, \"strength\":%d, \"snr\":%d}\n", ber,strength,snr);

  unicast_reply_send(reply, Socket, 200, "application/json");

  if (0 != unicast_reply_free(reply)) {
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
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

  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message(MSG_INFO,"Unicast : Error when creating the HTTP reply\n");
    return -1;
  }

  if ((time((time_t*)0L) - real_start_time) >= 10) //10 seconds for the traffic calculation to be done
  {
    unicast_reply_write(reply, "[");
    for (curr_channel = 0; curr_channel < number_of_channels; curr_channel++)
      unicast_reply_write(reply, "{\"number\":%d, \"name\":\"%s\", \"traffic\":%.2f},\n", curr_channel, channels[curr_channel].name, channels[curr_channel].traffic);
    reply->used_body -= 2; // dirty hack to erase the last comma
    unicast_reply_write(reply, "]\n");
  }

  unicast_reply_send(reply, Socket, 200, "application/json");

  if (0 != unicast_reply_free(reply)) {
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }
  return 0;
}












