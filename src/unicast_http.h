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

#ifndef _UNICAST_HTTP_H
#define _UNICAST_HTTP_H

#include "mumudvb.h"
#include "unicast_queue.h"
#include "unicast.h"



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




int unicast_del_client(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels);

int channel_add_unicast_client(unicast_client_t *client,mumudvb_channel_t *channel);



int unicast_handle_http_message(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels, int num_of_channels, fds_t *fds);




#endif
