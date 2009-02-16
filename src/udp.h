/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 * (C) 2004-2009 Brice DUBOST
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

/**@file
 * @brief Networking functions
 */

#ifndef _UDP_H
#define _UDP_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>

#include "mumudvb.h"

/* udp */
/* Output: {uni,multi,broad}cast socket */

/** The default time to live*/
#define DEFAULT_TTL		2

int makeclientsocket (char *szAddr, unsigned short port, int TTL, struct sockaddr_in *sSockAddr);
int sendudp (int fd, struct sockaddr_in *sSockAddr, unsigned char *data, int len);
int makesocket (char *szAddr, unsigned short port, int TTL, struct sockaddr_in *sSockAddr);


#endif


