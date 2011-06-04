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
int channel_add_unicast_client(unicast_client_t *client,mumudvb_channel_t *channel);




int
unicast_send_streamed_channels_list (int number_of_channels, mumudvb_channel_t *channels, int Socket, char *host);
int
unicast_send_play_list_unicast (int number_of_channels, mumudvb_channel_t *channels, int Socket, int unicast_portOut, int perport);
int
unicast_send_play_list_unicast_rtsp (int number_of_channels, mumudvb_channel_t *channels, int Socket, int unicast_portOut, int rtsp_portOut, int perport);
int
unicast_send_play_list_multicast (int number_of_channels, mumudvb_channel_t* channels, int Socket, int vlc);
int
unicast_send_streamed_channels_list_js (int number_of_channels, mumudvb_channel_t *channels, int Socket);
int
unicast_send_signal_power_js (int Socket, fds_t *fds);
int
unicast_send_channel_traffic_js (int number_of_channels, mumudvb_channel_t *channels, int Socket);
int
unicast_send_channel_clients_js (int number_of_clients, int Socket);











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
  (void) unicast_vars;

  int iRet;

  iRet=unicast_new_message(client);
  if(iRet)
    return iRet;

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

        requested_channel=parse_channel_path(client->buffer +pos,&err404, number_of_channels,channels);
        if(requested_channel&& (client->channel!=-1))
        {
          log_message(MSG_INFO,"Unicast : A channel (%d) is already streamed to this client, it shouldn't ask for a new one without closing the connection, error 501\n",client->channel);
          iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY)); //iRet is to make the copiler happy we will close the connection anyways
          return CLOSE_CONNECTION; //to delete the client
        }

        //Channels list
        if(strstr(client->buffer +pos ,"/channels_list.html ")==(client->buffer +pos))
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
          return CLOSE_CONNECTION; //We close the connection afterwards
        }
        //playlist, m3u
        else if(strstr(client->buffer +pos ,"/playlist.m3u ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"play list\n");
          unicast_send_play_list_unicast (number_of_channels, channels, client->Socket, unicast_vars->http_portOut, 0 );
          return CLOSE_CONNECTION; //We close the connection afterwards
        }
        //playlist, m3u
        else if(strstr(client->buffer +pos ,"/playlist_port.m3u ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"play list\n");
          unicast_send_play_list_unicast (number_of_channels, channels, client->Socket, unicast_vars->http_portOut, 1 );
          return CLOSE_CONNECTION; //We close the connection afterwards
        }
        //playlist RTSP, m3u
        else if( (strstr(client->buffer +pos ,"/playlist_rtsp.m3u ")==(client->buffer +pos)) && unicast_vars->unicast_rtsp_enable )
        {
          log_message(MSG_DETAIL,"play list\n");
          unicast_send_play_list_unicast_rtsp (number_of_channels, channels, client->Socket, unicast_vars->http_portOut, unicast_vars->rtsp_portOut, 0 );
          return CLOSE_CONNECTION; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/playlist_multicast.m3u ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"play list\n");
          unicast_send_play_list_multicast (number_of_channels, channels, client->Socket, 0 );
          return CLOSE_CONNECTION; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/playlist_multicast_vlc.m3u ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"play list\n");
          unicast_send_play_list_multicast (number_of_channels, channels, client->Socket, 1 );
          return CLOSE_CONNECTION; //We close the connection afterwards
        }
        //statistics, text version
        else if(strstr(client->buffer +pos ,"/channels_list.json ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"Channel list Json\n");
          unicast_send_streamed_channels_list_js (number_of_channels, channels, client->Socket);
          return CLOSE_CONNECTION; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/monitor/signal_power.json ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"Signal power json\n");
          unicast_send_signal_power_js(client->Socket, fds);
          return CLOSE_CONNECTION; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/monitor/channels_traffic.json ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"Channel traffic json\n");
          unicast_send_channel_traffic_js(number_of_channels, channels, client->Socket);
          return CLOSE_CONNECTION; //We close the connection afterwards
        }
        else if(strstr(client->buffer +pos ,"/monitor/clients.json ")==(client->buffer +pos))
        {
          log_message(MSG_DETAIL,"Unicast Clients\n");
          unicast_send_channel_clients_js(unicast_vars->client_number, client->Socket);
          return CLOSE_CONNECTION; //We close the connection afterwards
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
            return CLOSE_CONNECTION;
          }
          unicast_reply_write(reply, HTTP_404_REPLY_HTML, VERSION);
          unicast_reply_send(reply, client->Socket, 404, "text/html");
          if (0 != unicast_reply_free(reply)) {
            log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
            return CLOSE_CONNECTION;
          }
          return CLOSE_CONNECTION; //to delete the client
        }
        //We have found a channel, we add the client
        if(requested_channel)
        {
          if(!channel_add_unicast_client(client,&channels[requested_channel-1]))
            client->channel=requested_channel-1;
          else
            return CLOSE_CONNECTION;
        }

      }
      else
      {
        //We don't implement this http method, but if the client is already connected, we keep the connection
        if(client->channel==-1)
        {
          log_message(MSG_INFO,"Unicast : Unhandled HTTP method : \"%s\", error 501\n",  strtok (client->buffer+pos, " "));
          iRet=write(client->Socket,HTTP_501_REPLY, strlen(HTTP_501_REPLY));//iRet is to make the copiler happy we will close the connection anyways
          return CLOSE_CONNECTION; //to delete the client
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

/** @brief Send a basic text file containig the RTSP playlist
*
* @param number_of_channels the number of channels
* @param channels the channels array
* @param Socket the socket on wich the information have to be sent
* @param perport says if the channel have to be given by the url /bynumber or by their port
*/
int
unicast_send_play_list_unicast_rtsp (int number_of_channels, mumudvb_channel_t *channels, int Socket, int unicast_portOut, int rtsp_portOut, int perport)
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
        unicast_reply_write(reply, "#EXTINF:0,%s\r\nrtsp://%s:%d/bynumber/%d\r\n",
                          channels[curr_channel].name,
                          inet_ntoa(tempSocketAddr.sin_addr) ,
                          rtsp_portOut ,
                          curr_channel+1);
      }
      else if(channels[curr_channel].unicast_port)
      {
        unicast_reply_write(reply, "#EXTINF:0,%s\r\nrtsp://%s:%d/\r\n",
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




/** @brief Send a basic JSON file containig the clients
*
* @param number_of_clients the number of clients
* @param channels the channels array
* @param Socket the socket on wich the information have to be sent
*/
int
 unicast_send_channel_clients_js (int number_of_clients, int Socket)
{
  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply) {
    log_message(MSG_INFO,"Unicast : Error when creating the HTTP reply\n");
    return -1;
  }

  unicast_reply_write(reply, "{\"number\":%d},\n", number_of_clients);

  unicast_reply_send(reply, Socket, 200, "application/json");

  if (0 != unicast_reply_free(reply)) {
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }
  return 0;
}












