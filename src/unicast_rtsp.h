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

/**@file
 * @brief RTSP unicast headers
 */

#ifndef _UNICAST_RTSP_H
#define _UNICAST_RTSP_H

#include "mumudvb.h"
#include "unicast_queue.h"
#include "unicast.h"


#define RTSP_503_REPLY "RTSP/1.0 503 Too many clients\r\n"\
                      "\r\n"

int unicast_handle_rtsp_message(unicast_parameters_t *unicast_vars, unicast_client_t *client, mumudvb_channel_t *channels, int number_of_channels, fds_t *fds);


#endif
