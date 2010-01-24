/* 
 * MuMuDVB -Stream a DVB transport stream.
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

/**@file
 * @brief HTML unicast headers
 */

#ifndef _UNICAST_COMMON_H
#define _UNICAST_COMMON_H

#include "mumudvb.h"
#include "unicast_queue.h"
#include "unicast.h"

#define REPLY_HEADER 0
#define REPLY_BODY 1
#define REPLY_SIZE_STEP 256

typedef struct unicast_reply_t {
  char* buffer_header;
  char* buffer_body;
  int length_header;
  int length_body;
  int used_header;
  int used_body;
  int type;
}unicast_reply_t;

unicast_reply_t* unicast_reply_init();
int unicast_reply_free(unicast_reply_t *reply);
int unicast_reply_write(unicast_reply_t *reply, const char* msg, ...);
int unicast_reply_send(unicast_reply_t *reply, int socket, int code, const char* content_type);

void unicast_close_connection(unicast_parameters_t *unicast_vars, fds_t *fds, int Socket, mumudvb_channel_t *channels, int delete_client);
unicast_client_t *unicast_accept_connection(unicast_parameters_t *unicast_vars, int socketIn, int client_type);
unicast_client_t *unicast_add_client(unicast_parameters_t *unicast_vars, struct sockaddr_in SocketAddr, int Socket, int client_type);
int unicast_new_message(unicast_client_t *client);
void unicast_flush_client(unicast_client_t *client);




#endif
