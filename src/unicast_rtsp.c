/*
* MuMuDVB - Stream a DVB transport stream.
*
* (C) 2010 Brice DUBOST
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
* @brief File for RTSP unicast
* @author Brice DUBOST
* @date 2010
*/

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

#include "unicast_rtsp.h"
#include "unicast_queue.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"
#include "unicast.h"
#include "unicast_common.h"

extern int Interrupted;


/** @todo handle non keepalive clients, and auto disconnect to old clients without streaming */

int unicast_send_rtsp_describe (int Socket, int CSeq);
int unicast_send_rtsp_options (int Socket, int CSeq);
int unicast_send_rtsp_setup (unicast_client_t *client, int CSeq, int Tsprt_type);
int unicast_send_rtsp_play (int Socket, int CSeq, unicast_client_t *client, mumudvb_channel_t *channels);
int unicast_send_rtsp_setup (unicast_client_t *client, int CSeq, int Tsprt_type);
int unicast_send_rtsp_teardown (int Socket, int CSeq, unicast_client_t *client , mumudvb_channel_t *channels, unicast_parameters_t *unicast_vars, fds_t *fds);

/** @brief Deal with an incoming message on the unicast client connection
* This function will store and answer the RTSP requests
*
*
* @param unicast_vars the unicast parameters
* @param client The client from which the message was received
* @param channels the channel array
* @param number_of_channels quite explicit ...
*/
int unicast_handle_rtsp_message(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels, int number_of_channels, fds_t *fds)
{
  (void) unicast_vars;
  (void) number_of_channels;
  int iRet;

  iRet=unicast_new_message(client);
  if(iRet)
    return iRet;

  //We search for the end of the HTTP request
  if(!(strlen(client->buffer)>5 && strstr(client->buffer, "\n\r\n\0")))
    return 0;

  //      char *substring=NULL;
  //char *substringcseq=NULL;
  //char *urlPreSuffix=NULL;
  //char *urlSuffix=NULL;
  int requested_channel;
  //int iRet;
  requested_channel=0;
  //unicast_reply_t* reply=NULL;

  log_message(MSG_DEBUG,"Unicast : End of RTSP request, we parse it\n");
  // We implement RTSP protocol
  /* The client is initially supplied with an RTSP URL, on the form:
      rtsp://server.address:port/object.sdp
  */

  char *poscseq;
  int CSeq=0;
  int pos;
  pos=0;
  poscseq=strstr(client->buffer ,"CSeq:");
  log_message(MSG_FLOOD,"Buffer : %s\n",client->buffer);
  if(poscseq==NULL)
  {
    log_message(MSG_DEBUG,"CSeq not found\n");
    /** @todo Implement an error following the RTSP protocol*/
    return CLOSE_CONNECTION; //tu n'est pas content ...
  }
  log_message(MSG_DEBUG,"CSeq a la position %d\n", (int)(poscseq-(client->buffer)));
  pos=(int)(poscseq-(client->buffer));
  pos+=strlen("Cseq: ");
  CSeq = atoi(client->buffer+pos); //replace by CSeq = atoi(poscseq+strlen("Cseq: "));
  log_message(MSG_INFO,"Cseq: %d\n", CSeq);

  if(strstr(client->buffer,"DESCRIBE")==client->buffer)
  {
    unicast_send_rtsp_describe(client->Socket, CSeq);
    unicast_flush_client(client);
    return 0;
  }
  else if(strstr(client->buffer,"OPTIONS")==client->buffer)
  {
    unicast_send_rtsp_options(client->Socket, CSeq);
    unicast_flush_client(client);
    return 0;
  }
  else if(strstr(client->buffer,"SETUP")==client->buffer)
  {
    pos=0;
    char *pos_search;
    /** @todo put this in a function */
    char session[16];
    for (int i=0;i<15;i++)
    {
      float randomnum;
      randomnum = rand();
      randomnum/=RAND_MAX;
      session[i]='a'+(int)(randomnum*25);
    }
    session[15]='\0';
    strcpy(client->session, session);
    log_message(MSG_DEBUG,"Session : %s\n", session );
    // Parse le type de transport
    int TransportType=TRANSPORT_UNDEFINED; // RTP/AVP | RTP/AVP/UDP = 0 , RTP/AVP/TCP=1
    // Transport: RTP/AVP
    /* transport-protocol  =    "RTP"
       profile             =    "AVP"
       lower-transport     =    "TCP" | "UDP" */
    // Parse le type de transport
    pos_search=strstr(client->buffer ,"Transport:");
    if(pos_search==NULL)
    {
      log_message(MSG_DEBUG,"Type de transport non trouve\n");
      return CLOSE_CONNECTION; //tu n'est pas content ...
    }
    log_message(MSG_DEBUG,"Type de transport a la position %d\n", (int)(pos_search-(client->buffer)));
    pos=(int)(pos_search-(client->buffer));
    pos+=strlen("Transport: ");
    if (!strncmp (client->buffer+pos, "RTP/AVP/TCP", strlen("RTP/AVP/TCP"))) {
      TransportType=RTP_AVP_TCP;
      log_message(MSG_INFO,"Transport: RTP/AVP/TCP\n");
    }
    else if (!strncmp (client->buffer+pos, "RTP/AVP/UDP", strlen("RTP/AVP/UDP")))
    {
      TransportType=RTP_AVP_UDP;
      log_message(MSG_INFO,"Transport: RTP/AVP/UDP\n");
    }
    else if (!strncmp (client->buffer+pos, "RTP/AVP", strlen("RTP/AVP")))
    {
      TransportType=RTP_AVP_UDP;
      log_message(MSG_INFO,"Transport: RTP/AVP\n");
    }
    else
    {
      log_message(MSG_INFO,"Transport unknown");
      return CLOSE_CONNECTION; /** @todo implement a RTSP error*/
    }

    // Parse le port client
    pos_search=strstr(client->buffer ,"client_port=");
    log_message(MSG_FLOOD,"Buffer : %s\n",client->buffer);
    if(pos_search==NULL)
    {
      log_message(MSG_DEBUG,"Port client non trouve\n");
      /** @todo implement RTSP error */
      return CLOSE_CONNECTION; //tu n'est pas content ...
    }
    pos=(int)(pos_search-(client->buffer));
    pos+=strlen("client_port=");
    client->rtsp_client_port = atoi(client->buffer+pos);
    log_message(MSG_INFO,"client_port= %d\n", client->rtsp_client_port);

    struct sockaddr_in tempSocketAddr;
    unsigned int l;
    l = sizeof(struct sockaddr);
    getsockname(client->Socket, (struct sockaddr *) &tempSocketAddr, &l);
    strcpy(client->rtsp_client_ip,inet_ntoa(tempSocketAddr.sin_addr));
    log_message(MSG_DETAIL,"Unicast : Client ip %s  \n",client->rtsp_client_ip);

    client->rtsp_Socket=makeUDPsocket (client->rtsp_client_ip, client->rtsp_client_port,&client->rtsp_SocketAddr);
    log_message(MSG_DETAIL,"Unicast : New RTSP socket d_IP %s, d_port:%d n°%d\n",inet_ntoa(client->rtsp_SocketAddr.sin_addr), client->rtsp_SocketAddr.sin_port, client->rtsp_Socket);
    l = sizeof(struct sockaddr);
    connect(client->rtsp_Socket, &client->rtsp_SocketAddr, l);
    client->rtsp_SocketAddr.sin_port=4242;
    bind(client->rtsp_Socket,&client->rtsp_SocketAddr,l);
    //l = sizeof(struct sockaddr);
    //getsockname(client->rtsp_Socket, (struct sockaddr *) &tempSocketAddr, &l);
    //log_message(MSG_DETAIL,"Unicast : after getsockname RTSP socket d_IP %s, d_port:%d n°%d\n",inet_ntoa(client->rtsp_SocketAddr.sin_addr), client->rtsp_SocketAddr.sin_port, client->rtsp_Socket);
    //client->rtsp_server_port=client->rtsp_SocketAddr.sin_port;
    client->rtsp_server_port=4242;
    unicast_send_rtsp_setup(client, CSeq, TransportType);
    unicast_flush_client(client);
    return 0;
  }
  else if(strstr(client->buffer,"PLAY")==client->buffer)
  {
    //char *inPort=NULL;
    unicast_send_rtsp_play(client->Socket, CSeq, client, channels);
    unicast_flush_client(client);
    return 0;
  }
  else if(strstr(client->buffer,"TEARDOWN")==client->buffer)
  {
    unicast_send_rtsp_teardown(client->Socket, CSeq, client, channels, unicast_vars, fds);
    return CLOSE_CONNECTION;
  }
  else
  {
    log_message(MSG_WARN,"rate ... \n");
    unicast_flush_client(client);
  }

  return 0;
}


/** @brief Dump the filled buffer on the socket adding RTSP header informations
*/
int rtsp_reply_prepare_headers(unicast_reply_t *reply, int code, int CSeq)
{
  //we add the header information
  reply->type = REPLY_HEADER;
  unicast_reply_write(reply, "RTSP/1.0 ");
  switch(code)
  {
    case 200:
      unicast_reply_write(reply, "200 OK\r\n");
      break;
    case 404:
      unicast_reply_write(reply, "404 Not found\r\n");
      break;
    case 503:
      unicast_reply_write(reply, "503 Too many clients\r\n");
      break;
    default:
      log_message(MSG_ERROR,"reply send with bad code please contact\n");
      return 0;
  }
  unicast_reply_write(reply, "CSeq: %d\r\n",CSeq);
  unicast_reply_write(reply, "Server: mumudvb/" VERSION "\r\n");
  unicast_reply_write(reply, "Public: DESCRIBE, OPTIONS, SETUP, TEARDOWN, PLAY\r\n");
  reply->type = REPLY_BODY;
  return 0;
}


int rtsp_reply_send(unicast_reply_t *reply, int socket, const char* content_type)
{
  //we add the header information
  reply->type = REPLY_HEADER;
  if(reply->used_body)
  {
    unicast_reply_write(reply, "Content-type: %s\r\n", content_type);
    unicast_reply_write(reply, "Content-length: %d\r\n", reply->used_body);
  }
  unicast_reply_write(reply, "\r\n"); /* end header */
  //we merge the header and the body
  reply->buffer_header = realloc(reply->buffer_header, reply->used_header+reply->used_body);
  memcpy(&reply->buffer_header[reply->used_header],reply->buffer_body,sizeof(char)*reply->used_body);
  reply->used_header+=reply->used_body;
  //now we write the data
  int size = write(socket, reply->buffer_header, reply->used_header);
  return size;
}


/** @brief Send RTSP_DESCRIBE_REPLY
 *  The DESCRIBE method retrieves the description of a presentation or
 *  media object identified by the request URL from a server. It may use
 *  the Accept header to specify the description formats that the client
 *  understands. The server responds with a description of the requested
 *  resource. The DESCRIBE reply-response pair constitutes the media
 *  initialization phase of RTSP.
 * @param Socket the socket on wich the information have to be sent
 */
int unicast_send_rtsp_describe (int Socket, int CSeq)
 {
  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply)
  {
    log_message(MSG_INFO,"Unicast : Error when creating the RTSP reply\n");
    return -1;
  }
  log_message(MSG_INFO,"Requete DESCRIBE RTSP\n");

  char *TransportType="RTP/AVP";
  //if (TransportType) TransportType="RTP/AVP/TCP"; // =0:UDP, =1:TCP
  char *source_host = "MuMuDVB-server";

  unicast_reply_write(reply, "\r\n");
  unicast_reply_write(reply, "v=0\r\n");
  unicast_reply_write(reply, "o=- 14904518995472011776 14904518995472011776 IN IP4 %s\r\n", source_host);
  unicast_reply_write(reply, "s=unknown\r\n");
  unicast_reply_write(reply, "i=unknown\r\n");
  unicast_reply_write(reply, "c=IN IP4 0.0.0.0\r\n");
  //unicast_reply_write(reply, "c=IN IP4 239.100.1.0/2\r\n"); Pour le multicast RTSP
  unicast_reply_write(reply, "t=0 0\r\n");
  unicast_reply_write(reply, "m=video 0 %s 33\r\n", TransportType);
  //   unicast_reply_write(reply, "a=control:rtsp://192.168.15.151:4242/rtsp/stream?namespace=1&service=201&flavour=ld\r\n");

  rtsp_reply_prepare_headers(reply, 200, CSeq);
  rtsp_reply_send(reply, Socket,"application/sdp");

  if (0 != unicast_reply_free(reply))
  {
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }

  return 0;
}


/** @brief Send RTSP_OPTIONS_REPLY
 *
 * @param Socket the socket on wich the information have to be sent
 */
int unicast_send_rtsp_options (int Socket, int CSeq)
{
  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply)
  {
    log_message(MSG_INFO,"Unicast : Error when creating the RTSP reply\n");
    return -1;
  }

  log_message(MSG_INFO,"Requete OPTIONS RTSP\n");  ;

  rtsp_reply_prepare_headers(reply, 200, CSeq);
  rtsp_reply_send(reply, Socket,"text/plain");

  if (0 != unicast_reply_free(reply))
  {
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }

  return 0;
}


/** @brief Send RTSP_SETUP_REPLY
 *
 * @param Socket the socket on wich the information have to be sent
 */
int unicast_send_rtsp_setup (unicast_client_t *client, int CSeq, int Tsprt_type)
{
  int Socket=client->Socket;
  char TransportType[20], broadcast_type[20];
  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply)
  {
    log_message(MSG_INFO,"Unicast : Error when creating the RTSP reply\n");
    return -1;
  }

  log_message(MSG_INFO,"Requete SETUP RTSP\n");
  //unicast_reply_write(reply, RTSP_SETUP_REPLY);

  rtsp_reply_prepare_headers(reply, 200, CSeq);
  reply->type = REPLY_HEADER;
  unicast_reply_write(reply, "Session: %s\r\n", client->session);

  if (Tsprt_type==RTP_AVP_TCP)
    strcpy(TransportType,"RTP/AVP/TCP");
  else
    strcpy(TransportType,"RTP/AVP");

  //TODO Get the broadcast_type from the DESCRIBE request
  //if (Tsprt_type==2) broadcast_type="multicast";
  //  else broadcast_type="unicast";
  strcpy( broadcast_type,"unicast");
  log_message(MSG_DEBUG, "Transport: %s;%s;mode=play;destination=%s;client_port=%d-%d;server_port=%d\r\n", TransportType, broadcast_type, client->rtsp_client_ip, client->rtsp_client_port, client->rtsp_client_port+1, client->rtsp_server_port);
  unicast_reply_write(reply, "Transport: %s;%s;mode=play;destination=%s;client_port=%d-%d;server_port=%d\r\n", TransportType, broadcast_type, client->rtsp_client_ip, client->rtsp_client_port, client->rtsp_client_port+1, client->rtsp_server_port);
  rtsp_reply_send(reply, Socket,"text/plain");

  if (0 != unicast_reply_free(reply))
  {
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }

  return 0;
}

/** @brief Send RTSP_PLAY_REPLY
 *
 * @param Socket the socket on wich the information have to be sent
 */
int unicast_send_rtsp_play (int Socket, int CSeq, unicast_client_t *client, mumudvb_channel_t *channels)
{
  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply)
  {
    log_message(MSG_INFO,"Unicast : Error when creating the RTSP reply\n");
    return -1;
  }

  log_message(MSG_INFO,"Requete PLAY RTSP\n");
  unicast_reply_write(reply, "Session: %s\r\n", client->session);
  rtsp_reply_prepare_headers(reply, 200, CSeq);
  rtsp_reply_send(reply, Socket, "text/plain");

  // TODO Ajouter l'IP du client dans la liste des IP unicast.
  client->channel=0;
  //channel_add_unicast_client(client, &channels[0]); //plop ca ne va pas marcher, il faut le modifier pour prendre en compte le type de client


  if (0 != unicast_reply_free(reply))
  {
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }

  return 0;
}

/** @brief Send RTSP_PLAY_TEARDOWN
 *
 * @param Socket the socket on wich the information have to be sent
 */
int unicast_send_rtsp_teardown (int Socket, int CSeq, unicast_client_t *client , mumudvb_channel_t *channels, unicast_parameters_t *unicast_vars, fds_t *fds)
{
  unicast_reply_t* reply = unicast_reply_init();
  if (NULL == reply)
  {
    log_message(MSG_INFO,"Unicast : Error when creating the RTSP reply\n");
    return -1;
  }

  log_message(MSG_INFO,"Requete TEARDOWN RTSP\n");

  rtsp_reply_prepare_headers(reply, 200, CSeq);
  rtsp_reply_send(reply, Socket, "text/plain");

  //unicast_close_connection(unicast_vars, fds, Socket, channels);

  if (0 != unicast_reply_free(reply))
  {
    log_message(MSG_INFO,"Unicast : Error when releasing the HTTP reply after sendinf it\n");
    return -1;
  }

  return 0;
}

