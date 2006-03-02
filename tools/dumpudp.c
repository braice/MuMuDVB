/* 
Dumpudp : dump a raw udp multicast stream
By Brice DUBOST
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

#define _GNU_SOURCE             // pour utiliser le program_invocation_short_name (extension gnu)

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
#include <getopt.h>
#include <errno.h>              // pour utiliser le program_invocation_short_name (extension gnu)


#define MAX_IP_LENGTH 18

#define PACKET_SIZE 188

//the cache size in UDP packet numbers
#define CACHE_SIZE 10

#define MAX_UDP_SIZE 188*7

  //same MTU as mumudvb (no sync errors)
#define MTU MAX_UDP_SIZE 

#define ERROR_ARGS 8
#define ERROR_FILE 9

int Interrupted=0;
int fini=0;


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
usage (char *name)
{
  fprintf (stderr, "%s \n"
	   "Program used to dump an UDP multicast stream to a file\n"
           "\n"
	   "Usage: %s [options] \n"
           "-i, --ip       : Multicast Ip\n"
           "-p, --port     : Port\n"
           "-o, --out      : Output file\n"
           "-d, --duration : Dump duration\n"
           "-h, --help     : Help\n"
           "\n"
           "Released under the GPL.\n"
           "Latest version available from http://mumudvb.braice.net/\n"
           "by Brice DUBOST (mumudvb@braice.net)\n", name, name);
}


static void
SignalHandler (int signum)
{
  if (signum == SIGALRM)
    {
      if(!fini)
	{
	  fprintf (stderr, "\nFin\n");
	  fini=1;
	}
      else
	{
	  exit(1);
	}
      alarm (1);
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
  int portIn=0;
  struct sockaddr_in sIn;
  int socketIn;
  int lengthPacket;
  int num=0;
  int num80=0;
  int signaux=0;
  int duree=0;
  int cache_pos=0;
  char *out_filename=NULL;
  FILE *out_file;

  unsigned char cache[MTU*CACHE_SIZE];

  const char short_options[] = "i:p:o:d:h";
  const struct option long_options[] = {
    {"ip", required_argument, NULL, 'i'},
    {"port", required_argument, NULL, 'p'},
    {"out", required_argument, NULL, 'o'},
    {"duration", no_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0}
  };

  int c, option_index = 0;

  while (1)
    {
      c = getopt_long (argc, argv, short_options,
                       long_options, &option_index);

      if (c == -1)
        {
          break;
        }
      switch (c)
        {
        case 'i':
	  if(strlen (optarg)>MAX_IP_LENGTH){
	      fprintf(stderr, "Ip too long\n"); 
	  }
	  else{
	    strncpy (ipIn, optarg,MAX_IP_LENGTH);
	  }

          break;
        case 'p':
	  portIn=strtol(optarg,NULL,10);
          break;
        case 'o':
          out_filename = (char *) malloc (strlen (optarg) + 1);
          if (!out_filename)
            {
	      fprintf(stderr, "malloc() failed: %s\n", strerror(errno));
              exit(errno);
            }
          strncpy (out_filename, optarg, strlen (optarg) + 1);
          break;
        case 'd':
	  duree=atoi(optarg);
          break;
        case 'h':
          usage (program_invocation_short_name);
          exit(ERROR_ARGS);
          break;
        }
    }
  if (optind < argc)
    {
      usage (program_invocation_short_name);
      exit(ERROR_ARGS);
    }



  // output file
  out_file = fopen (out_filename, "w");
  if (out_file == NULL)
    {
      fprintf (stderr,
	       "%s: %s\n",
	       out_filename, strerror (errno));
      exit(ERROR_FILE);
    }


  ttl = 2;
  fprintf (stderr, "Dump start\n");
  fprintf (stderr, "Ip \"%s\", Port \"%d\", Output file : \"%s\", ",ipIn,portIn,out_filename);
  if(duree)
    fprintf (stderr, "Duration \"%d\" \n",duree);
  else
    fprintf (stderr, "\n");
  fprintf (stderr, "A dot is printed every 500 packets\n");
  
  /* Init udp */
  socketIn = makeclientsocket (ipIn, portIn, ttl, &sIn); //le makeclientsocket est pour joindre automatiquement le flux


  //In order to program records
  if(duree)
    {
      // alarme pour la fin
      if (signal (SIGALRM, SignalHandler) == SIG_IGN)
	signal (SIGALRM, SIG_IGN);
      alarm (duree);
    }


  /* Read packets */


  while (!Interrupted)
    {
      lengthPacket=recv(socketIn,cache+cache_pos,MTU,0);
      cache_pos+=MTU;
      num++;
      // mis ici pour pouvoir le tuer s'il n'y a pas de flux (automatiser ça à l'avenir)
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
      if(cache_pos==MTU*CACHE_SIZE){
	fwrite(cache, sizeof(unsigned char), cache_pos,out_file);
	cache_pos=0;
      }
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
	  fflush(stderr);
	}
	  
      if (Interrupted)
	{
	  //if some data need to be written
	  if(cache_pos)
	    fwrite(cache, sizeof(unsigned char), cache_pos,out_file);
	  fprintf (stderr, "\nCaught signal %d - closing cleanly.\n", Interrupted);
	}
    }

  close (socketIn);
  fclose (out_file);
   

  return (0);
}
