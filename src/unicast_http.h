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

/**@file
 * @brief HTML unicast headers
 */

#ifndef _UNICAST_H
#define _UNICAST_H

#include "mumudvb.h"
#include "unicast_queue.h"

/** @brief The different fd/socket types */
enum
  {
    UNICAST_MASTER=1,
    UNICAST_LISTEN_CHANNEL,
    UNICAST_CLIENT,
  };



#define RECV_BUFFER_MULTIPLE 100
/**@brief the timeout for disconnecting a client with only consecutive errors*/
#define UNICAST_CONSECUTIVE_ERROR_TIMEOUT 5


#define HTTP_OK_REPLY "HTTP/1.0 200 OK\r\n"\
                      "Content-type: video/mpeg\r\n"\
                      "\r\n"

#define HTTP_404_REPLY_HTML "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\r\n"\
                            "<html lang=\"en\">\r\n"\
                            "<head>\r\n"\
                            "<title>Not found</title>\r\n"\
                            "</head>\r\n"\
                            "<body>\r\n"\
                            "   <h1>404 Not found</h1>\r\n"\
                            "<hr />\r\n"\
                            "<a href=\"http://mumudvb.braice.net\">MuMuDVB</a> version %s\r\n"\
                            "</body>\r\n"\
                            "</html>\r\n"\
                            "\r\n"

#define HTTP_CHANNELS_REPLY_START "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml10/DTD/xhtml10strict.dtd\">\r\n"\
                            "<html lang=\"en\">\r\n"\
                            "<head>\r\n"\
                            "<title>Channels list</title>\r\n"\
                            "</head>\r\n"\
                            "<body>\r\n"\
                            "   <h1>Channel list</h1>\r\n"\
                            "<hr />\r\n"\
                            "This is the list of actually streamed channels by the MuMuDVB server. To open a channel copy the link to your client or use multicast.\r\n"\
                            "<hr />\r\n"\

#define HTTP_CHANNELS_REPLY_END "<hr />\r\n"\
                            "See <a href=\"http://mumudvb.braice.net\">MuMuDVB</a> website for more details.\r\n"\
                            "</body>\r\n"\
                            "</html>\r\n"\
                            "\r\n"

#define HTTP_501_REPLY "HTTP/1.0 501 Not implemented\r\n"\
                      "\r\n"

#define HTTP_503_REPLY "HTTP/1.0 503 Too many clients\r\n"\
                      "\r\n"


/** @brief A client connected to the unicast connection.
 *
 *There is two chained list of client : a global one wich contain all the clients. Another one in each channel wich contain the associated clients.
 */
typedef struct unicast_client_t{
  /**HTTP socket*/
  struct sockaddr_in SocketAddr;
  /**HTTP socket*/
  int Socket;
  /**Reception buffer*/
  char *buffer;
  /**Size of the buffer*/
  int buffersize;
  /**Position in the buffer*/
  int bufferpos;
  /**Is there consecutive errors ?*/
  int consecutive_errors;
  /**When the first consecutive error happeard*/
  long first_error_time;
  /**Channel : -1 if not associated yet*/
  int channel;
  /**Future channel : we will set the channel when we will receive the get*/
  int askedChannel;
  /**Next client*/
  struct unicast_client_t *next;
  /**Previous client*/
  struct unicast_client_t *prev;
  /**Next client in the channel*/
  struct unicast_client_t *chan_next;
  /**Previous client in the channel*/
  struct unicast_client_t *chan_prev;
  /** The packets in queue*/
  unicast_queue_header_t queue;
  /** The latest write error for this client*/
  int last_write_error;
}unicast_client_t;


/** @brief The information on the unicast file descriptors/sockets
 * There is three kind of descriptors :
  * The master connection : this connection will interpret the HTTP path asked, to give the channel, the channel list or debugging information
  * Client connections : This is the connections for connected clients
  * Channel listening connections : When a client connect to one of these sockets, the associated channel will be given directly without interpreting the PATH
 *
 * The numbering of this socket information is the same as the file descriptors numbering
 */
typedef struct unicast_fd_info_t{
  /**The fd/socket type*/
  int type;
  /** The channel if it's a channel socket*/
  int channel;
  /** The client if it's a client socket*/
  unicast_client_t *client;
}unicast_fd_info_t;


/** @brief The parameters for unicast
*/
typedef struct unicast_parameters_t{
  /** Do we activate unicast ?*/
  int unicast;
  /**The "HTTP" ip address*/
  char ipOut[20];
  /** The "HTTP" port*/
  int portOut;
  /** The "HTTP" port string version before parsing*/
  char *portOut_str;
  /** The HTTP input socket*/
  struct sockaddr_in sIn;
  /**  The HTTP input socket*/
  int socketIn;
  /** The clients, contains all the clients, associated to a channel or not*/
  unicast_client_t *clients;
  /** The number of connected clients*/
  int client_number;
  /** The maximum number of simultaneous clients allowed*/
  int max_clients;
  /** The timeout before disconnecting a client wich does only errors*/
  int consecutive_errors_timeout;
  /** The information on the file descriptors : ie the type of FD, the client associated if it's a client fd, the channel if it's a channel fd */
  unicast_fd_info_t *fd_info;
  /** The maximim size of the queue */
  int queue_max_size;
  /** The socket SO_SNDBUF size*/
  int socket_sendbuf_size;
}unicast_parameters_t;

int unicast_create_listening_socket(int socket_type, int socket_channel, char *ipOut, int port, struct sockaddr_in *sIn, int *socketIn, fds_t *fds, unicast_parameters_t *unicast_vars);

int unicast_handle_fd_event(unicast_parameters_t *unicast_vars, fds_t *fds, mumudvb_channel_t *channels, int number_of_channels);

int unicast_del_client(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels);

int channel_add_unicast_client(unicast_client_t *client,mumudvb_channel_t *channel);

void unicast_freeing(unicast_parameters_t *unicast_vars, mumudvb_channel_t *channels);

int read_unicast_configuration(unicast_parameters_t *unicast_vars, mumudvb_channel_t *current_channel, int ip_ok, char *substring);

void unicast_data_send(mumudvb_channel_t *actual_channel, mumudvb_channel_t *channels, fds_t *fds, unicast_parameters_t *unicast_vars);




#endif
