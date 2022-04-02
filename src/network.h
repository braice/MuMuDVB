/*
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 *
 * (C) 2004-2009 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.net/
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

#ifndef _NETWORK_H
#define _NETWORK_H

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <Ntddndis.h>
#include <Iphlpapi.h>
#include <Ws2def.h>
#include <ws2ipdef.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif
#include <sys/types.h>
#ifndef _WIN32
#include <syslog.h>
#endif
#include <stdio.h>
#include <stdlib.h>


/** The default time to live*/
#define DEFAULT_TTL		2


int makeclientsocket (char *szAddr, unsigned short port, int TTL, char *iface, struct sockaddr_in *sSockAddr);
void sendudp (int fd, struct sockaddr_in *sSockAddr, unsigned char *data, int len);
int makesocket (char *szAddr, unsigned short port, int TTL, char *iface, struct sockaddr_in *sSockAddr);
int makeTCPclientsocket (char *szAddr, unsigned short port, struct sockaddr_in *sSockAddr);
int makeclientsocket6 (char *szAddr, unsigned short port, int TTL, char *iface, struct sockaddr_in6 *sSockAddr);
void sendudp6 (int fd, struct sockaddr_in6 *sSockAddr, unsigned char *data, int len);
int makesocket6 (char *szAddr, unsigned short port, int TTL, char *iface, struct sockaddr_in6 *sSockAddr);

#endif


