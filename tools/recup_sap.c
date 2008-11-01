/* 

Recup_sap - get sap announces
By Brice DUBOST

This program is part of mumudvb
 
The latest version can be found at http://mumudvb.braice.net

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
#include <errno.h>

#define MAX_IP_LENGTH 18

//#define PACKET_SIZE 32

#define MAX_UDP_SIZE 188*7
#define MTU MAX_UDP_SIZE 

#define MAX_PACKET_SIZE MTU

#define PORT 9875
#define IP "224.2.127.254\0"

#define DUREE_PRISE_SAP 60
#define SAP_FILE "/tmp/chaines_recup_sap.txt"

#define MAX_CHAINES 256

int Interrupted=0;

int sendudp(int fd, struct sockaddr_in *sSockAddr, char *data, int len) {
  return sendto(fd,data,len,0,(struct sockaddr *)sSockAddr,sizeof(*sSockAddr));
}


static void SignalHandler (int signum);
void on_ferme();

//en global pke je le vaux bien
int socketIn;
char chaines[MAX_CHAINES][512];
int num_chaines=0;


/* create a sender socket. */
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
/* create a receiver socket, i.e. join the multicast group. */
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
  fprintf (stderr, "Usage: just launch recup sap without arguments.\n");
}



int
main (int argc, char **argv)
{

  /* sockets */
  int ttl;
  char ipIn[]=IP;
  int portIn=PORT;
  struct sockaddr_in sIn;
  int lengthPacket;
  int signaux=0;
   char caract;
   int i;
   int j=0;
   int print_line=0;
   char chaine_ip[256];
   char chaine_nom[256];
   char chaine_total[512];
   char *chaine_temp;
   char delimiteur_egal[]="=";
   char delimiteurs[]=" =/";
   int trouve;
   
  unsigned char temp_buf[MAX_PACKET_SIZE];

  fprintf (stderr, "recup_sap\nProgram used to get a SAP stream\n");
  fprintf (stderr, "This program only get well formated sap announces, it does NO checking\n");
  fprintf (stderr, "It's a very simple program, if you experience issues please contact me\n\n");
  fprintf (stderr, "Released under the GPL.\n");
  fprintf (stderr,
	   "Latest version available from http://mumudvb.braice.net/\n");
  fprintf (stderr,
	   "By Brice DUBOST (mumudvb@braice.net)\n");

  if (argc != 1)
    {
      Usage ();
      exit (1);
    }


  ttl = 2;
  fprintf (stderr, "We will get sap announces during %d seconds\nThey will be put in %s\n",DUREE_PRISE_SAP, SAP_FILE);
  
  /* Init udp */
  socketIn = makeclientsocket (ipIn, portIn, ttl, &sIn); //le makeclientsocket est pour joindre automatiquement le flux

   if (signal (SIGALRM, SignalHandler) == SIG_IGN)
     signal (SIGALRM, SIG_IGN);
  alarm (DUREE_PRISE_SAP);

   
  /* Read packets */

  while (!Interrupted)
    {
      lengthPacket=recv(socketIn,temp_buf,MAX_PACKET_SIZE,0);
       if(lengthPacket>0)
	 {
	    if(signaux==0)//mis ici pour pouvoir le tuer s'il n'y a pas de flux (aytomatiser ça à l'avenir)
	      {
		 if (signal (SIGHUP, SignalHandler) == SIG_IGN)
		   signal (SIGHUP, SIG_IGN);
		 if (signal (SIGINT, SignalHandler) == SIG_IGN)
		   signal (SIGINT, SIG_IGN);
		 if (signal (SIGTERM, SignalHandler) == SIG_IGN)
		   signal (SIGTERM, SIG_IGN);
		 signaux=1;
	      }
	    for(i=0;i<lengthPacket;i++)
	      {
		 caract=temp_buf[i];
		 //si c'est un nom  ou in ip ...
		 if((i>0)&&(i<lengthPacket)&&(temp_buf[i-1]=='\n')&&((temp_buf[i]=='c')||(temp_buf[i]=='s'))&&(temp_buf[i+1]=='='))
		   {
		      print_line=caract;
		      for(j=0;j<256;j++)
			{
			   if(print_line=='c')
			     chaine_ip[j]=0;
			   if(print_line=='s')
			     chaine_nom[j]=0;
			}
		      j=0;
		   }
		 
		 if((print_line)&&((caract>=32)||(caract=='\n')))
		   {
		      if(caract=='\n')
			{
			   print_line=0;
			}
		      if(print_line=='c')
			chaine_ip[j]=caract;
		      if(print_line=='s')
			chaine_nom[j]=caract;
		      j++;
		      if(j>255)
			{
			   fprintf(stderr,"String too long.\nBad sap announces\n");
			   print_line=0;
			}
		      
		   }
	      }
	    //traitement de la chaine nom
	    chaine_temp=strtok(chaine_nom,delimiteur_egal);
	    chaine_temp=strtok(NULL,delimiteur_egal);
	    strncpy(chaine_nom,chaine_temp,256);
	    chaine_temp=strtok(chaine_ip,delimiteurs);
	    chaine_temp=strtok(NULL,delimiteurs);
	    chaine_temp=strtok(NULL,delimiteurs);
	    chaine_temp=strtok(NULL,delimiteurs);
	    strncpy(chaine_ip,chaine_temp,256);
	    trouve=0;
	    sprintf(chaine_total,"%s:%s",chaine_nom,chaine_ip);
	    for(i=0;i<num_chaines&&trouve==0;i++)
	      if(!strcmp(chaine_total,chaines[i]))
		trouve=1;
	    if(!trouve)
	      {
//		 printf("On ajoute une chaine\n");
		 strncpy(chaines[num_chaines],chaine_total,256);
		 num_chaines++;
	      }
	    
	    //	   write(STDOUT_FILENO, temp_buf, MTU);
	    

	    if (Interrupted)
	      {
		 fprintf (stderr, "Caught signal %d - closing cleanly.\n", Interrupted);
	      }
	 }
    }
   
   on_ferme();

  return (0);
}


void on_ferme()
{
   int i;
   FILE *fich_chaines;
   fich_chaines=fopen(SAP_FILE,"w");
   if(fich_chaines==NULL)
     {
       printf("Issues with the file %s\n", SAP_FILE);
	exit(1);
     }
   
   for(i=0;i<num_chaines;i++)
     {
	fprintf(fich_chaines,"%s\n",chaines[i]);
     }
   fclose(fich_chaines);
   close (socketIn);
}


static void
SignalHandler (int signum)
{
  if (signum != SIGPIPE)
    {
      Interrupted = signum;
    }
  signal (signum, SignalHandler);

  if (signum == SIGALRM)
    {
       on_ferme();
       exit(0);
    }
  signal (signum, SignalHandler);
}
