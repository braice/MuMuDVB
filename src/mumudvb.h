#ifndef _MUMUDVB_H
#define _MUMUDVB_H

//the number of pids by channel
#define MAX_PIDS_PAR_CHAINE     18

//the maximum channel number
#define MAX_CHAINES		128

//Size of an MPEG2-TS packet
#define PACKET_SIZE 188

// How often (in seconds) to update the "now" variable
#define ALARM_TIME 5
#define ALARM_TIME_TIMEOUT 60
#define ALARM_TIME_TIMEOUT_NO_DIFF 600

// seven dvb paquets in one UDP
#define MAX_UDP_SIZE (PACKET_SIZE*7)

//the max mandatory pid number
#define MAX_MANDATORY           32
//confid line length
#define CONF_LINELEN 	        512
#define ALARM_COUNT_LIMIT	1024
#define MAX_LEN_NOM		256



#endif
