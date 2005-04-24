#ifndef _MUMUDVB_H
#define _MUMUDVB_H

#define MAX_PIDS_PAR_CHAINE     18
#define MAX_CHAINES		32

#define PACKET_SIZE 188

// How often (in seconds) to update the "now" variable
#define ALARM_TIME 5
#define ALARM_TIME_TIMEOUT 60
#define ALARM_TIME_TIMEOUT_NO_DIFF 600

#define MAX_UDP_SIZE 188*7
#define MTU MAX_UDP_SIZE

  //Crans defines
#define MAX_FLUX 	        128
#define CONF_LINELEN 	        512
#define ALARM_COUNT_LIMIT	1024
#define MAX_LEN_NOM		256



#endif
