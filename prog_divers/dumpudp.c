/* 

crans_dvbstream - UDP-ize a DVB transport stream.
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
// Linux includes:
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <resolv.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <values.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define MAX_IP_LENGTH 18

#define PACKET_SIZE 188


#define MAX_UDP_SIZE 188*7
#define MTU MAX_UDP_SIZE 


/* Todo */
/* Ce programme contient beaucoup de code sale a laver */
/* C'est un petit programme ecrit rapidement */

  int Interrupted=0;

int sendudp(int fd, struct sockaddr_in *sSockAddr, char *data, int len) {
  return sendto(fd,data,len,0,(struct sockaddr *)sSockAddr,sizeof(*sSockAddr));
}



// create a sender socket.
int makesocket(char *szAddr,unsigned short port,int TTL,struct sockaddr_in *sSockAddr) {
  int          iRet, iLoop = 1;
  struct       sockaddr_in sin;
  char         cTtl = (char)TTL;
  char         cLoop=0;

  int iSocket = socket( AF_INET, SOCK_DGRAM, 0 );

  if (iSocket < 0) {
    fprintf(stderr,"socket() failed.\n");
    exit(1);
  }

  sSockAddr->sin_family = sin.sin_family = AF_INET;
  sSockAddr->sin_port = sin.sin_port = htons(port);
  sSockAddr->sin_addr.s_addr = inet_addr(szAddr);

  iRet = setsockopt(iSocket, SOL_SOCKET, SO_REUSEADDR, &iLoop, sizeof(int));
  if (iRet < 0) {
    fprintf(stderr,"setsockopt SO_REUSEADDR failed\n");
    exit(1);
  }

  iRet = setsockopt(iSocket, IPPROTO_IP, IP_MULTICAST_TTL, &cTtl, sizeof(char));
  if (iRet < 0) {
    fprintf(stderr,"setsockopt IP_MULTICAST_TTL failed.  multicast in kernel?\n");
    exit(1);
  }

  cLoop = 1;	/* !? */
  iRet = setsockopt(iSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
                    &cLoop, sizeof(char));
  if (iRet < 0) {
    fprintf(stderr,"setsockopt IP_MULTICAST_LOOP failed.  multicast in kernel?\n");
    exit(1);
  }

  return iSocket;
}

// create a receiver socket, i.e. join the multicast group.
int makeclientsocket(char *szAddr,unsigned short port,int TTL,struct sockaddr_in *sSockAddr) {
  int socket=makesocket(szAddr,port,TTL,sSockAddr);
  struct ip_mreq blub;
  struct sockaddr_in sin;
  unsigned int tempaddr;
  sin.sin_family=AF_INET;
  sin.sin_port=htons(port);
  sin.sin_addr.s_addr=inet_addr(szAddr);
  if (bind(socket,(struct sockaddr *)&sin,sizeof(sin))) {
    perror("bind failed");
    exit(1);
  }
  tempaddr=inet_addr(szAddr);
  if ((ntohl(tempaddr) >> 28) == 0xe) {
    blub.imr_multiaddr.s_addr = inet_addr(szAddr);
    blub.imr_interface.s_addr = 0;
    if (setsockopt(socket,IPPROTO_IP,IP_ADD_MEMBERSHIP,&blub,sizeof(blub))) {
      perror("setsockopt IP_ADD_MEMBERSHIP failed (multicast kernel?)");
      exit(1);
    }
  }
  return socket;
}


void
Usage ()
{
  fprintf (stderr, "Usage: dumpudp source portsource [duree]\n\n");
}

static void
SignalHandler (int signum)
{
  if (signum == SIGALRM)
    {
      fprintf (stderr, "\nFin\n");
    }
  if (signum != SIGPIPE)
    {
      Interrupted = signum;
    }
  signal (signum, SignalHandler);
}


int
main (int argc, char **argv)
{

  /* sockets */
  int ttl;
  char ipIn[MAX_IP_LENGTH];
  int portIn;
  struct sockaddr_in sIn;
  int socketIn;
  int lengthPacket;
  int num=0;
  int num80=0;
  int signaux=0;
  int duree=0;

  unsigned char temp_buf[MTU];

  fprintf (stderr, "dumpudp\n Program used to dump an UDP multicast stream to stdout\n");
  fprintf (stderr, "Released under the GPL.\n");
  fprintf (stderr,
	   "Latest version available from http://www.crans.org/\n");
  fprintf (stderr,
	   "By Brice DUBOST (brice.dubost@crans.org)\n");

  if (argc != 3 &&argc != 4)
    {
      Usage ();
      exit (1);
    }
  else 
    {
      strncpy (ipIn, argv[1],MAX_IP_LENGTH);
      portIn = atoi(argv[2]);
      if(argc==4)
	duree = atoi(argv[3]);
    }


  ttl = 2;
  fprintf (stderr, "Debut du dump\nUn point est affiché tous les 500 paquets\n");
  
  /* Init udp */
  socketIn = makeclientsocket (ipIn, portIn, ttl, &sIn); //le makeclientsocket est pour joindre automatiquement le flux


  //pour la possibilite de programmer
  if(duree)
    {
      fprintf (stderr, "On va dumper pendant %ds\n",duree);
      // alarme pour la fin
      if (signal (SIGALRM, SignalHandler) == SIG_IGN)
	signal (SIGALRM, SIG_IGN);
      alarm (duree);
    }


  /* Read packets */


  while (!Interrupted)
    {
      lengthPacket=recv(socketIn,temp_buf,MTU,0);
      num++;
      // mis ici pour pouvoir le tuer s'il n'y a pas de flux (aytomatiser ça à l'avenir)
      if(signaux==0)
	{
	  if (signal (SIGHUP, SignalHandler) == SIG_IGN)
	    signal (SIGHUP, SIG_IGN);
	  if (signal (SIGINT, SignalHandler) == SIG_IGN)
	    signal (SIGINT, SIG_IGN);
	  if (signal (SIGTERM, SignalHandler) == SIG_IGN)
	    signal (SIGTERM, SIG_IGN);
	  signaux=1;
	}
      if (lengthPacket != MTU)
	{
	  fprintf (stderr, "No bytes left to read - aborting\n");
	  break;
	}
	  
      write(STDOUT_FILENO, temp_buf, MTU);
      if(num%500==0)
	{
	  num=0;
	  num80++;
	  if(num80%80==0)
	    {
	      fprintf (stderr,".\n");
	      num80=0;
	    }
	  else
	      fprintf (stderr,".");
	  fflush(stdout);
	}
	  
      if (Interrupted)
	{
	  fprintf (stderr, "Caught signal %d - closing cleanly.\n", Interrupted);
	}
    }

  close (socketIn);
   

  return (0);
}
