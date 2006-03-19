#ifndef _MUMUDVB_H
#define _MUMUDVB_H

//the number of pids by channel
#define MAX_PIDS_PAR_CHAINE     18

//the maximum channel number
#define MAX_CHAINES		128

//Size of an MPEG2-TS packet
#define TS_PACKET_SIZE 188

// How often (in seconds) to update the "now" variable
#define ALARM_TIME 5
#define ALARM_TIME_TIMEOUT 60
#define ALARM_TIME_TIMEOUT_NO_DIFF 600

// seven dvb paquets in one UDP
#define MAX_UDP_SIZE (TS_PACKET_SIZE*7)

//the max mandatory pid number
#define MAX_MANDATORY           32
//config line length
#define CONF_LINELEN 	        512
#define ALARM_COUNT_LIMIT	1024
#define MAX_LEN_NOM		256

//For pat rewriting

//From libsi
//   (C) 2001-03 Rolf Hakenes <hakenes@hippomi.de>, under the
//               GNU GPL with contribution of Oleg Assovski,
//               www.satmania.com

#define TS_HEADER_LEN 5
#define HILO(x) (x##_hi << 8 | x##_lo)

 /*
 *
 *    ETSI ISO/IEC 13818-1 specifies SI which is referred to as PSI. The PSI
 *    data provides information to enable automatic configuration of the
 *    receiver to demultiplex and decode the various streams of programs
 *    within the multiplex. The PSI data is structured as four types of table.
 *    The tables are transmitted in sections.
 *
 *    1) Program Association Table (PAT):
 *
 *       - for each service in the multiplex, the PAT indicates the location
 *         (the Packet Identifier (PID) values of the Transport Stream (TS)
 *         packets) of the corresponding Program Map Table (PMT).
 *         It also gives the location of the Network Information Table (NIT).
 *
 */

#define PAT_LEN 8

typedef struct {
   u_char table_id                               :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char dummy                                  :1;        // has to be 0
   u_char                                        :2;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :2;
   u_char dummy                                  :1;        // has to be 0
   u_char section_syntax_indicator               :1;
#endif
   u_char section_length_lo                      :8;
   u_char transport_stream_id_hi                 :8;
   u_char transport_stream_id_lo                 :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :2;
   u_char version_number                         :5;
   u_char current_next_indicator                 :1;
#else
   u_char current_next_indicator                 :1;
   u_char version_number                         :5;
   u_char                                        :2;
#endif
   u_char section_number                         :8;
   u_char last_section_number                    :8;
} pat_t;

#define PAT_PROG_LEN 4

typedef struct {
   u_char program_number_hi                      :8;
   u_char program_number_lo                      :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :3;
   u_char network_pid_hi                         :5;
#else
   u_char network_pid_hi                         :5;
   u_char                                        :3;
#endif
   u_char network_pid_lo                         :8; 
   /* or program_map_pid (if prog_num=0)*/
} pat_prog_t;



#endif
