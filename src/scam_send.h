/* 
 * MuMuDVB - Stream a DVB transport stream.
 * Header file for software descrambling sending part
 * 
 * (C) 2004-2010 Brice DUBOST
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


#ifndef _SCAM_SEND_H
#define _SCAM_SEND_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/dvb/ca.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/time.h>

/**@file
 * @brief scam support
 * 
 * Header file for code concerning software descrambling
 */

void scam_send_start(mumudvb_channel_t *channel);
void scam_send_stop(mumudvb_channel_t *channel);
void *sendthread_func(void* arg); 

#if 0
typedef struct scam_sendthread_p_t{
	mumudvb_channel_t *channel
	struct unicast_parameters_t *unicast_vars, fds_t *fds
}scam_sendthread_p_t;
#endif


#endif
