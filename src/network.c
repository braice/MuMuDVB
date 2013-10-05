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
#include <net/if.h>
#include <unistd.h>


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
makesocket (char *szAddr, unsigned short port, int TTL, char *iface,
	    struct sockaddr_in *sSockAddr)
{
  int iRet, iLoop = 1;
  struct sockaddr_in sin;
  int iReuse = 1;
  int iSocket = socket (AF_INET, SOCK_DGRAM, 0);

  if (iSocket < 0)
    {
      log_message( log_module,  MSG_WARN, "socket() failed : %s\n",strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
      return -1;
    }

  sSockAddr->sin_family = sin.sin_family = AF_INET;
  sSockAddr->sin_port = sin.sin_port = htons (port);
  iRet=inet_aton (szAddr,&sSockAddr->sin_addr);
  if (iRet == 0)
    {
      log_message( log_module,  MSG_ERROR,"inet_aton failed : %s\n", strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
      close(iSocket);
      return -1;
    }

  iRet = setsockopt (iSocket, SOL_SOCKET, SO_REUSEADDR, &iReuse, sizeof (int));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt SO_REUSEADDR failed : %s\n",strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
      close(iSocket);
      return -1;
    }

  iRet =
    setsockopt (iSocket, IPPROTO_IP, IP_MULTICAST_TTL, &TTL, sizeof (int));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt IP_MULTICAST_TTL failed.  multicast in kernel? error : %s \n",strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
      close(iSocket);
      return -1;
    }

  iRet = setsockopt (iSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
		     &iLoop, sizeof (int));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt IP_MULTICAST_LOOP failed.  multicast in kernel? error : %s\n",strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
      close(iSocket);
      return -1;
    }
  if(strlen(iface))
  {
	  int iface_index;
	  iface_index = if_nametoindex(iface);
	  if(iface_index)
	  {
		  log_message( log_module,  MSG_DEBUG, "Setting IPv4 multicast iface to %s, index %d",iface,iface_index);
		  struct ip_mreqn iface_struct;
		  iface_struct.imr_multiaddr.s_addr=INADDR_ANY;
		  iface_struct.imr_address.s_addr=INADDR_ANY;
		  iface_struct.imr_ifindex=iface_index;
		  iRet = setsockopt (iSocket, IPPROTO_IP, IP_MULTICAST_IF, &iface_struct, sizeof (struct ip_mreqn));
		  if (iRet < 0)
		  {
			  log_message( log_module,  MSG_ERROR,"setsockopt IP_MULTICAST_IF failed.  multicast in kernel? error : %s \n",strerror(errno));
			  set_interrupted(ERROR_NETWORK<<8);
			  close(iSocket);
		      return -1;
		  }
	  }
	  else
		  log_message( log_module,  MSG_ERROR,"Setting IPV4 multicast interface: Interface %s does not exist",iface);
  }

  return iSocket;
}

/** @brief create an IPv6 sender socket.
 *
 * Create a socket for sending data, the socket is multicast, udp, with the options REUSE_ADDR et MULTICAST_LOOP set to 1
 */
int
makesocket6 (char *szAddr, unsigned short port, int TTL, char *iface,
	    struct sockaddr_in6 *sSockAddr)
{
  int iRet;
  int iReuse=1;
  struct sockaddr_in6 sin;

  int iSocket = socket (AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

  if (iSocket < 0)
    {
      log_message( log_module,  MSG_WARN, "socket() failed : %s\n",strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
    }

  sSockAddr->sin6_family = sin.sin6_family = AF_INET6;
  sSockAddr->sin6_port = sin.sin6_port = htons (port);
  iRet=inet_pton (AF_INET6, szAddr,&sSockAddr->sin6_addr); 
  if (iRet == 0)
    {
      log_message( log_module,  MSG_ERROR,"inet_pton failed : %s\n", strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
    }

  iRet = setsockopt (iSocket, SOL_SOCKET, SO_REUSEADDR, &iReuse, sizeof (int));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt SO_REUSEADDR failed : %s\n",strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
    }
  iRet =
    setsockopt (iSocket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &TTL, sizeof (int));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt IPV6_MULTICAST_HOPS failed.  multicast in kernel? error : %s \n",strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
    }
  if(strlen(iface))
  {
	  int iface_index;
	  iface_index = if_nametoindex(iface);
	  if(iface_index)
	  {
		  log_message( log_module,  MSG_DEBUG, "Setting IPv6 multicast iface to %s, index %d",iface,iface_index);
		  iRet = setsockopt (iSocket, IPPROTO_IPV6, IPV6_MULTICAST_IF, &iface_index, sizeof (int));
		  if (iRet < 0)
		  {
			  log_message( log_module,  MSG_ERROR,"setsockopt IPV6_MULTICAST_IF failed.  multicast in kernel? error : %s \n",strerror(errno));
			  set_interrupted(ERROR_NETWORK<<8);
		  }
	  }
	  else
		  log_message( log_module,  MSG_ERROR,"Setting IPv6 multicast interface: Interface %s does not exist",iface);

  }

  return iSocket;
}

/** @brief create a receiver socket, i.e. join the multicast group. 
 *@todo document
*/
int
makeclientsocket (char *szAddr, unsigned short port, int TTL, char *iface,
		  struct sockaddr_in *sSockAddr)
{
  int socket;
  struct ip_mreqn blub;
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof sin);
  unsigned int tempaddr;
  socket = makesocket (szAddr, port, TTL, iface, sSockAddr);
  if(socket<0)
	  return 0;
  sin.sin_family = AF_INET;
  sin.sin_port = htons (port);
  sin.sin_addr.s_addr = inet_addr (szAddr);
  if (bind (socket, (struct sockaddr *) &sin, sizeof (sin)))
    {
      log_message( log_module,  MSG_ERROR, "bind failed : %s\n", strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
    }
  tempaddr = inet_addr (szAddr);
  if ((ntohl (tempaddr) >> 28) == 0xe)
    {
      blub.imr_multiaddr.s_addr = inet_addr (szAddr);
      if(strlen(iface))
    	  blub.imr_ifindex=if_nametoindex(iface);
      else
    	  blub.imr_ifindex = 0;
      if (setsockopt (socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &blub, sizeof (blub)))
	{
	  log_message( log_module,  MSG_ERROR, "setsockopt IP_ADD_MEMBERSHIP ipv4 failed (multicast kernel?) : %s\n", strerror(errno));
          set_interrupted(ERROR_NETWORK<<8);
	}
    }
  return socket;
}


/** @brief create a receiver socket, i.e. join the multicast group. 
 *@todo document
*/
int
makeclientsocket6 (char *szAddr, unsigned short port, int TTL, char *iface,
		  struct sockaddr_in6 *sSockAddr)
{
  int socket = makesocket6 (szAddr, port, TTL, iface, sSockAddr);
  struct ipv6_mreq blub;
  struct sockaddr_in6 sin;
  memset(&sin, 0, sizeof sin);
  int iRet;
  sin.sin6_family = AF_INET6;
  sin.sin6_port = htons (port);
  iRet=inet_pton (AF_INET6, szAddr,&sin.sin6_addr); 
  if (iRet == 0)
    {
      log_message( log_module,  MSG_ERROR,"inet_pton failed : %s\n", strerror(errno));
      close(socket);
      set_interrupted(ERROR_NETWORK<<8);
      return 0;
    }

  if (bind (socket, (struct sockaddr *) &sin, sizeof (sin)))
    {
      log_message( log_module,  MSG_ERROR, "bind failed : %s\n", strerror(errno));
      close(socket);
      set_interrupted(ERROR_NETWORK<<8);
      return 0;
    }

  //join the group
  inet_pton (AF_INET6, szAddr,&blub.ipv6mr_multiaddr); 
  if(strlen(iface))
	  blub.ipv6mr_interface=if_nametoindex(iface);
  else
	  blub.ipv6mr_interface = 0;
  if (setsockopt
      (socket, IPPROTO_IPV6, IPV6_JOIN_GROUP, &blub, sizeof (blub)))
    {
      log_message( log_module,  MSG_ERROR, "setsockopt IPV6_JOIN_GROUP (ipv6) failed (multicast kernel?) : %s\n", strerror(errno));
      set_interrupted(ERROR_NETWORK<<8);
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
      close(iSocket);
      return -1;
    }

  iRet = setsockopt (iSocket, SOL_SOCKET, SO_REUSEADDR, &iLoop, sizeof (int));
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"setsockopt SO_REUSEADDR failed : %s\n", strerror(errno));
      close(iSocket);
      return -1;
    }


  if (bind (iSocket, (struct sockaddr *) sSockAddr, sizeof (*sSockAddr)))
    {
      log_message( log_module,  MSG_ERROR, "bind failed : %s\n", strerror(errno));
      close(iSocket);
      return -1;
    }

  iRet = listen(iSocket,10);
  if (iRet < 0)
    {
      log_message( log_module,  MSG_ERROR,"listen failed : %s\n",strerror(errno));
      close(iSocket);
      return -1;
    }

  //Now we set this socket to be non blocking because we poll it
  int flags;
  flags = fcntl(iSocket, F_GETFL, 0);
  flags |= O_NONBLOCK;
  if (fcntl(iSocket, F_SETFL, flags) < 0)
    {
      log_message( log_module, MSG_ERROR,"Set non blocking failed : %s\n",strerror(errno));
      close(iSocket);
      return -1;
    }


  return iSocket;
}






