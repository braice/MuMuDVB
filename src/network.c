/* 
 * udp.h fonction in order to send a multicast stream
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) 2004-2009 Brice DUBOST
 * (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 * 
 *  The latest version can be found at http://mumudvb.braice.net
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

#include "network.h"
#include "errors.h"
#include <string.h>
#include <errno.h> 
#include <fcntl.h>
#include "log.h"

extern int Interrupted;
static char *log_module="Network: ";

/**@brief Send data
 * just send the data over the socket fd
 */
int
sendudp (int fd, struct sockaddr_in *sSockAddr, unsigned char *data, int len)
{
  return sendto (fd, data, len, 0, (struct sockaddr *) sSockAddr,
		 sizeof (*sSockAddr));
}

/**@brief Send data
 * just send the data over the socket fd
 */
int
sendudp6 (int fd, struct sockaddr_in6 *sSockAddr, unsigned char *data, int len)
{
  return sendto (fd, data, len, 0, (struct sockaddr *) sSockAddr,
		 sizeof (*sSockAddr));
}



/** @brief create a sender socket.
 *
 * Create a socket for sending data, the socket is multicast, udp, with the options REUSE_ADDR et MULTICAST_LOOP set to 1
 */
int
makesocket (char *szAddr, unsigned short port, int TTL,
	    struct sockaddr_in *sSockAddr)
{
  int iRet, iLoop = 1;
  struct sockaddr_in sin;
  char cTtl = (char) TTL;
  int iReuse = 1;
  int iSocket = socket (AF_INET, SOCK_DGRAM, 0);

  if (iSocket < 0)
    {
      log_message( log_module,  MSG_WARN, "socket() failed : %s\n",strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }

  sSockAddr->sin_family = sin.sin_family = AF_INET;
  sSockAddr->sin_port = sin.sin_port = htons (port);
  iRet=inet_aton (szAddr,&sSockAddr->sin_addr);
  if (iRet == 0)
    {
      log_message( log_module,  MSG_ERROR,"inet_aton failed : %s\n", strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }

  iRet = setsockopt (iSocket, SOL_SOCKET, SO_REUSEADDR, &iReuse, sizeof (int));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt SO_REUSEADDR failed : %s\n",strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }

  iRet =
    setsockopt (iSocket, IPPROTO_IP, IP_MULTICAST_TTL, &cTtl, sizeof (char));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt IP_MULTICAST_TTL failed.  multicast in kernel? error : %s \n",strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }

  iRet = setsockopt (iSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
		     &iLoop, sizeof (int));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt IP_MULTICAST_LOOP failed.  multicast in kernel? error : %s\n",strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }

  return iSocket;
}

/** @brief create an IPv6 sender socket.
 *
 * Create a socket for sending data, the socket is multicast, udp, with the options REUSE_ADDR et MULTICAST_LOOP set to 1
 */
int
makesocket6 (char *szAddr, unsigned short port, int TTL,
	    struct sockaddr_in6 *sSockAddr)
{
  int iRet;
  int iReuse=1;
  struct sockaddr_in6 sin;
  char cTtl = (char) TTL;

  int iSocket = socket (AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

  if (iSocket < 0)
    {
      log_message( log_module,  MSG_WARN, "socket() failed : %s\n",strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }

  sSockAddr->sin6_family = sin.sin6_family = AF_INET6;
  sSockAddr->sin6_port = sin.sin6_port = htons (port);
  iRet=inet_pton (AF_INET6, szAddr,&sSockAddr->sin6_addr); 
  if (iRet == 0)
    {
      log_message( log_module,  MSG_ERROR,"inet_pton failed : %s\n", strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }

  iRet = setsockopt (iSocket, SOL_SOCKET, SO_REUSEADDR, &iReuse, sizeof (int));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt SO_REUSEADDR failed : %s\n",strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }

  iRet =
    setsockopt (iSocket, IPPROTO_IP, IPV6_MULTICAST_HOPS, &cTtl, sizeof (char));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt IPV6_MULTICAST_HOPS failed.  multicast in kernel? error : %s \n",strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }

  return iSocket;
}

/** @brief create a receiver socket, i.e. join the multicast group. 
 *@todo document
*/
int
makeclientsocket (char *szAddr, unsigned short port, int TTL,
		  struct sockaddr_in *sSockAddr)
{
  int socket = makesocket (szAddr, port, TTL, sSockAddr);
  struct ip_mreq blub;
  struct sockaddr_in sin;
  unsigned int tempaddr;
  sin.sin_family = AF_INET;
  sin.sin_port = htons (port);
  sin.sin_addr.s_addr = inet_addr (szAddr);
  if (bind (socket, (struct sockaddr *) &sin, sizeof (sin)))
    {
      log_message( log_module,  MSG_ERROR, "bind failed : %s\n", strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }
  tempaddr = inet_addr (szAddr);
  if ((ntohl (tempaddr) >> 28) == 0xe)
    {
      blub.imr_multiaddr.s_addr = inet_addr (szAddr);
      blub.imr_interface.s_addr = 0;
      if (setsockopt
	  (socket, IPPROTO_IP, MCAST_JOIN_GROUP, &blub, sizeof (blub)))
	{
	  log_message( log_module,  MSG_ERROR, "setsockopt IP_ADD_MEMBERSHIP ipv4 failed (multicast kernel?) : %s\n", strerror(errno));
          Interrupted=ERROR_NETWORK<<8;
	}
    }
  return socket;
}


/** @brief create a receiver socket, i.e. join the multicast group. 
 *@todo document
*/
int
makeclientsocket6 (char *szAddr, unsigned short port, int TTL,
		  struct sockaddr_in6 *sSockAddr)
{
  int socket = makesocket6 (szAddr, port, TTL, sSockAddr);
  struct ipv6_mreq blub;
  struct sockaddr_in6 sin;
  int iRet;
  sin.sin6_family = AF_INET;
  sin.sin6_port = htons (port);
  iRet=inet_pton (AF_INET6, szAddr,&sin.sin6_addr); 
  if (iRet == 0)
    {
      log_message( log_module,  MSG_ERROR,"inet_pton failed : %s\n", strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
      return 0;
    }

  if (bind (socket, (struct sockaddr *) &sin, sizeof (sin)))
    {
      log_message( log_module,  MSG_ERROR, "bind failed : %s\n", strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }

  //join the group
  inet_pton (AF_INET6, szAddr,&blub.ipv6mr_multiaddr); 
  blub.ipv6mr_interface = 0;
  if (setsockopt
      (socket, IPPROTO_IP, MCAST_JOIN_GROUP, &blub, sizeof (blub)))
    {
      log_message( log_module,  MSG_ERROR, "setsockopt MCAST_JOIN_GROUP (ipv6) failed (multicast kernel?) : %s\n", strerror(errno));
      Interrupted=ERROR_NETWORK<<8;
    }
  return socket;
}


/** @brief create a TCP receiver socket.
 *
 * Create a socket for waiting the HTTP connection
 */
int
makeTCPclientsocket (char *szAddr, unsigned short port, 
	    struct sockaddr_in *sSockAddr)
{
  int iRet, iLoop = 1;

  int iSocket = socket (AF_INET, SOCK_STREAM, 0); //TCP

  if (iSocket < 0)
    {
      log_message( log_module,  MSG_ERROR, "socket() failed.\n");
      return -1;
    }

  sSockAddr->sin_family = AF_INET;
  sSockAddr->sin_port = htons (port);
  iRet=inet_aton (szAddr,&sSockAddr->sin_addr);
  if (iRet == 0)
    {
      log_message( log_module,  MSG_ERROR,"inet_aton failed : %s\n", strerror(errno));
      return -1;
    }

  iRet = setsockopt (iSocket, SOL_SOCKET, SO_REUSEADDR, &iLoop, sizeof (int));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt SO_REUSEADDR failed : %s\n", strerror(errno));
      return -1;
    }


  if (bind (iSocket, (struct sockaddr *) sSockAddr, sizeof (*sSockAddr)))
    {
      log_message( log_module,  MSG_ERROR, "bind failed : %s\n", strerror(errno));
      return -1;
    }

  iRet = listen(iSocket,10);
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"listen failed : %s\n",strerror(errno));
      return -1;
    }

  //Now we set this socket to be non blocking because we poll it
  int flags;
  flags = fcntl(iSocket, F_GETFL, 0);
  flags |= O_NONBLOCK;
  if (fcntl(iSocket, F_SETFL, flags) < 0)
    {
      log_message( log_module, MSG_ERROR,"Set non blocking failed : %s\n",strerror(errno));
      return -1;
    }


  return iSocket;
}


