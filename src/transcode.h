/* 
 * MuMuDVB - UDP-ize a DVB transport stream.
 * Code for transcoding
 * 
 * Code written by Utelisys Communications B.V.
 * Copyright (C) 2009 Utelisys Communications B.V.
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
 *
 */
#ifndef _TRANSCODE_H
#define _TRANSCODE_H

#include "mumudvb.h"
#include "transcode_common.h"
#include <netinet/in.h>

void* transcode_start_thread(int socket, struct sockaddr_in *socket_addr,
    transcode_options_t *options);
void transcode_request_thread_end(void *transcode_handle);
void transcode_wait_thread_end(void *transcode_handle);
int transcode_enqueue_data(void *transcode_handle, void *data, int data_size);
int transcode_read_option(struct transcode_options_t *transcode_options, int ip_ok, char *delimiteurs, char **substring);
void transcode_copy_options(struct transcode_options_t *src, struct transcode_options_t *dst);

#endif