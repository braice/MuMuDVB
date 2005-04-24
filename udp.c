/* 
udp.h fonction in order to send a multicast stream
mumudvb - UDP-ize a DVB transport stream.
(C) Dave Chapman <dave@dchapman.com> 2001, 2002.
Modified By Brice DUBOST
 * 
The latest version can be found at http://www.crans.org

Copyright notice:

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
    
*/

#include "udp.h"

int
sendudp (int fd, struct sockaddr_in *sSockAddr, char *data, int len)
{
  return sendto (fd, data, len, 0, (struct sockaddr *) sSockAddr,
		 sizeof (*sSockAddr));
}



/* create a sender socket. */
int
makesocket (char *szAddr, unsigned short port, int TTL,
	    struct sockaddr_in *sSockAddr, int no_daemon)
{
  int iRet, iLoop = 1;
  struct sockaddr_in sin;
  char cTtl = (char) TTL;
  char cLoop = 0;

  int iSocket = socket (AF_INET, SOCK_DGRAM, 0);

  if (iSocket < 0)
    {
      if(!no_daemon)
	syslog(LOG_USER, "socket() failed.\n");
      else
	fprintf (stderr, "socket() failed.\n");
      exit (1);
    }

  sSockAddr->sin_family = sin.sin_family = AF_INET;
  sSockAddr->sin_port = sin.sin_port = htons (port);
  sSockAddr->sin_addr.s_addr = inet_addr (szAddr);

  iRet = setsockopt (iSocket, SOL_SOCKET, SO_REUSEADDR, &iLoop, sizeof (int));
  if (iRet < 0)
    {
      if(!no_daemon)
	syslog(LOG_USER,"setsockopt SO_REUSEADDR failed\n");
      else
	fprintf (stderr, "setsockopt SO_REUSEADDR failed\n");
      exit (1);
    }

  iRet =
    setsockopt (iSocket, IPPROTO_IP, IP_MULTICAST_TTL, &cTtl, sizeof (char));
  if (iRet < 0)
    {
      if(!no_daemon)
	syslog(LOG_USER,"setsockopt IP_MULTICAST_TTL failed.  multicast in kernel?\n");
      else
	fprintf (stderr,
		 "setsockopt IP_MULTICAST_TTL failed.  multicast in kernel?\n");
      exit (1);
    }

  cLoop = 1;			/* !? */
  iRet = setsockopt (iSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
		     &cLoop, sizeof (char));
  if (iRet < 0)
    {
      if(!no_daemon)
	syslog(LOG_USER,"setsockopt IP_MULTICAST_LOOP failed.  multicast in kernel?\n");
      else
	fprintf (stderr,
		 "setsockopt IP_MULTICAST_LOOP failed.  multicast in kernel?\n");
      exit (1);
    }

  return iSocket;
}

/* create a receiver socket, i.e. join the multicast group. */
int
makeclientsocket (char *szAddr, unsigned short port, int TTL,
		  struct sockaddr_in *sSockAddr, int no_daemon)
{
  int socket = makesocket (szAddr, port, TTL, sSockAddr, no_daemon);
  struct ip_mreq blub;
  struct sockaddr_in sin;
  unsigned int tempaddr;
  sin.sin_family = AF_INET;
  sin.sin_port = htons (port);
  sin.sin_addr.s_addr = inet_addr (szAddr);
  if (bind (socket, (struct sockaddr *) &sin, sizeof (sin)))
    {
      if(!no_daemon)
	syslog(LOG_USER,"bind failed");
      perror ("bind failed");
      exit (1);
    }
  tempaddr = inet_addr (szAddr);
  if ((ntohl (tempaddr) >> 28) == 0xe)
    {
      blub.imr_multiaddr.s_addr = inet_addr (szAddr);
      blub.imr_interface.s_addr = 0;
      if (setsockopt
	  (socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &blub, sizeof (blub)))
	{
	  if(!no_daemon)
	    syslog(LOG_USER,"setsockopt IP_ADD_MEMBERSHIP failed (multicast kernel?)");
	  perror ("setsockopt IP_ADD_MEMBERSHIP failed (multicast kernel?)");
	  exit (1);
	}
    }
  return socket;
}
