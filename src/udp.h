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
char ipOut[MAX_CHAINES][20];
int portOut[MAX_CHAINES];
int ttl;
struct sockaddr_in sOut[MAX_FLUX];
int socketOut[MAX_FLUX];

int makeclientsocket (char *szAddr, unsigned short port, int TTL, struct sockaddr_in *sSockAddr, int no_daemon);
int sendudp (int fd, struct sockaddr_in *sSockAddr, char *data, int len);
int makesocket (char *szAddr, unsigned short port, int TTL, struct sockaddr_in *sSockAddr, int no_daemon);


#endif


