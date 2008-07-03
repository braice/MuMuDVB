/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) Brice DUBOST
 * 
 * The latest version can be found at http://mumudvb.braice.net
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
 */


#ifndef _TS_H
#define _TS_H


#include <sys/types.h>

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


//Used to generate the CA_PMT message

typedef struct {
  u_char sync_byte                              :8;
#if BYTE_ORDER == BIG_ENDIAN
  u_char transport_error_indicator              :1;
  u_char payload_unit_start_indicator           :1;
  u_char transport_priority                     :1;
  u_char pid_hi                                 :5;
#else
  u_char pid_hi                                 :5;
  u_char transport_priority                     :1;
  u_char payload_unit_start_indicator           :1;
  u_char transport_error_indicator              :1;
#endif
  u_char pid_lo                                 :8;
#if BYTE_ORDER == BIG_ENDIAN
  u_char transport_scrambling_control           :2;
  u_char adaptation_field_control               :2;
  u_char continuity_counter                     :4;
#else
  u_char continuity_counter                     :4;
  u_char adaptation_field_control               :2;
  u_char transport_scrambling_control           :2;
#endif
} ts_header_t;


//For cam support and autoconfigure

/*
 *
 *    3) Program Map Table (PMT):
 *
 *       - the PMT identifies and indicates the locations of the streams that
 *         make up each service, and the location of the Program Clock
 *         Reference fields for a service.
 *
 */

#define PMT_LEN 12

typedef struct {
   u_char table_id                               :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char dummy                                  :1; // has to be 0
   u_char                                        :2;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :2;
   u_char dummy                                  :1; // has to be 0
   u_char section_syntax_indicator               :1;
#endif
   u_char section_length_lo                      :8;
   u_char program_number_hi                      :8;
   u_char program_number_lo                      :8;
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
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :3;
   u_char PCR_PID_hi                             :5;
#else
   u_char PCR_PID_hi                             :5;
   u_char                                        :3;
#endif
   u_char PCR_PID_lo                             :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :4;
   u_char program_info_length_hi                 :4;
#else
   u_char program_info_length_hi                 :4;
   u_char                                        :4;
#endif
   u_char program_info_length_lo                 :8;
   //descriptors
} pmt_t;

#define PMT_INFO_LEN 5

typedef struct {
   u_char stream_type                            :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :3;
   u_char elementary_PID_hi                      :5;
#else
   u_char elementary_PID_hi                      :5;
   u_char                                        :3;
#endif
   u_char elementary_PID_lo                      :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :4;
   u_char ES_info_length_hi                      :4;
#else
   u_char ES_info_length_hi                      :4;
   u_char                                        :4;
#endif
   u_char ES_info_length_lo                      :8;
     // descriptors
} pmt_info_t;


/* 0x09 ca_descriptor */

#define DESCR_CA_LEN 6
typedef struct descr_ca_struct {
  u_char descriptor_tag                         :8;
  u_char descriptor_length                      :8;
  u_char CA_type_hi                             :8;
  u_char CA_type_lo                             :8;
#if BYTE_ORDER == BIG_ENDIAN
  u_char reserved                               :3;
  u_char CA_PID_hi                              :5;
#else
  u_char CA_PID_hi                              :5;
  u_char reserved                               :3;
#endif
  u_char CA_PID_lo                              :8;
} descr_ca_t;



/*
 *
 *    2) Service Description Table (SDT):
 *
 *       - the SDT contains data describing the services in the system e.g.
 *         names of services, the service provider, etc.
 *
 */

#define SDT_LEN 11

typedef struct {
   u_char table_id                               :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char                                        :3;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :3;
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
   u_char original_network_id_hi                 :8;
   u_char original_network_id_lo                 :8;
   u_char                                        :8;
} sdt_t;

#define GetSDTTransportStreamId(x) (HILO(((sdt_t *) x)->transport_stream_id))
#define GetSDTOriginalNetworkId(x) (HILO(((sdt_t *) x)->original_network_id))

#define SDT_DESCR_LEN 5

typedef struct {
   u_char service_id_hi                          :8;
   u_char service_id_lo                          :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :6;
   u_char eit_schedule_flag                      :1;
   u_char eit_present_following_flag             :1;
   u_char running_status                         :3;
   u_char free_ca_mode                           :1;
   u_char descriptors_loop_length_hi             :4;
#else
   u_char eit_present_following_flag             :1;
   u_char eit_schedule_flag                      :1;
   u_char                                        :6;
   u_char descriptors_loop_length_hi             :4;
   u_char free_ca_mode                           :1;
   u_char running_status                         :3;
#endif
   u_char descriptors_loop_length_lo             :8;
} sdt_descr_t;


//structur for the build of the pmt packet
typedef struct {
  int empty; //say if the pmt is empty
  int pid;   //The PID of the packet
  int continuity_counter; //the countinuity counter, incremented in each packet
  int len;
  int i_program_number; 
  int need_descr;
  unsigned char packet[4096]; //the buffer
  unsigned char converted_packet[4096]; //the buffer for the cam (for cam_support)
}mumudvb_ts_packet_t;


int ts_check_CRC( mumudvb_ts_packet_t *pmt);
int get_ts_packet(unsigned char *buf, mumudvb_ts_packet_t *pmt);
int AddPacketStart (unsigned char *packet, unsigned char *buf, unsigned int len);
int AddPacketContinue  (unsigned char *packet, unsigned char *buf, unsigned int len, unsigned int act_len);


#endif
