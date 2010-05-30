/* 
 * MuMuDVB - Stream a DVB transport stream.
 * 
 * (C) 2004-2010 Brice DUBOST
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

/**@file
 * @brief File for demuxing TS stream
 */

#ifndef _TS_H
#define _TS_H


#include <sys/types.h>
#include <stdint.h>

#include "config.h"
#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#endif

//0x1ffb=8187 It's the pid for the information tables in ATSC
#define PSIP_PID 8187

//Part of this code comes from libsi
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
 */
/**
 * @brief Program Association Table (PAT):
 *
 *       - for each service in the multiplex, the PAT indicates the location
 *         (the Packet Identifier (PID) values of the Transport Stream (TS)
 *         packets) of the corresponding Program Map Table (PMT).
 *         It also gives the location of the Network Information Table (NIT).
 *
 */

#define PAT_LEN 8

/** @brief Program Association Table (PAT):*/
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

/** @brief Program Association Table (PAT): program*/
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


//Used to generate the CA_PMT message and for autoconfiguration
/** @brief Mpeg2-TS header*/
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

/** length of the PMT header */
#define PMT_LEN 12

/**@brief  Program Map Table (PMT):
 *
 *       - the PMT identifies and indicates the locations of the streams that
 *         make up each service, and the location of the Program Clock
 *         Reference fields for a service.
 *
 */

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

/** length of the PMT program information section header */
#define PMT_INFO_LEN 5
/** @brief  Program Map Table (PMT) : program information section*/
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


#define DESCR_CA_LEN 6
/** @brief 0x09 ca_descriptor */
typedef struct {
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




/** length of the SDT header */
#define SDT_LEN 11

/** @brief Service Description Table (SDT):
 *
 *       - the SDT contains data describing the services in the system e.g.
 *         names of services, the service provider, etc.
 *
 */
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
/**@brief  Service Description Table (SDT), descriptor */
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

/*
 *
 *    3) Event Information Table (EIT):
 * 
 *       - the EIT contains data concerning events or programmes such as event
 *         name, start time, duration, etc.; - the use of different descriptors
 *         allows the transmission of different kinds of event information e.g.
 *         for different service types.
 *
 */

#define EIT_LEN 14

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
  u_char service_id_hi                          :8;
  u_char service_id_lo                          :8;
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
  u_char transport_stream_id_hi                 :8;
  u_char transport_stream_id_lo                 :8;
  u_char original_network_id_hi                 :8;
  u_char original_network_id_lo                 :8;
  u_char segment_last_section_number            :8;
  u_char segment_last_table_id                  :8;
} eit_t;

/*
 *
 *    5) Network Information Table (NIT):
 *
 *       - the NIT is intended to provide information about the physical
 *         network. The syntax and semantics of the NIT are defined in
 *         ETSI EN 300 468.
 *
 */

#define NIT_LEN 10

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
   u_char network_id_hi                          :8;
   u_char network_id_lo                          :8;
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
   u_char                                        :4;
   u_char network_descriptor_length_hi           :4;
#else
   u_char network_descriptor_length_hi           :4;
   u_char                                        :4;
#endif
   u_char network_descriptor_length_lo           :8;
  /* descriptors */
}nit_t;

#define SIZE_NIT_MID 2

typedef struct {                                 // after descriptors
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :4;
   u_char transport_stream_loop_length_hi        :4;
#else
   u_char transport_stream_loop_length_hi        :4;
   u_char                                        :4;
#endif
   u_char transport_stream_loop_length_lo        :8;
}nit_mid_t;

#define NIT_TS_LEN 6

typedef struct {
   u_char transport_stream_id_hi                 :8;
   u_char transport_stream_id_lo                 :8;
   u_char original_network_id_hi                 :8;
   u_char original_network_id_lo                 :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :4;
   u_char transport_descriptors_length_hi        :4;
#else
   u_char transport_descriptors_length_hi        :4;
   u_char                                        :4;
#endif
   u_char transport_descriptors_length_lo        :8;
   /* descriptors  */
}nit_ts_t;

#define NIT_LCN_LEN 4

typedef struct {
   u_char service_id_hi                          :8;
   u_char service_id_lo                          :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char visible_service_flag                   :1;
   u_char reserved                               :5;
   u_char logical_channel_number_hi              :2;
#else
   u_char logical_channel_number_hi              :2;
   u_char reserved                               :5;
   u_char visible_service_flag                   :1;
#endif
   u_char logical_channel_number_lo              :8;
}nit_lcn_t;

/***************************************************
 *                ATSC PSIP tables                 *
 * See A/65C                                       *
 *              Atsc standard :                    *
 *   Program and System Information Protocol for   *
 *  Terrestrial Broadcast and Cable (revision C).  *
 ***************************************************/

#define PSIP_HEADER_LEN 9

/**@brief Header of an ATSC PSIP (Program and System Information Protocol) table*/
typedef struct {
   u_char table_id                               :8;
#if BYTE_ORDER == BIG_ENDIAN
   u_char section_syntax_indicator               :1;
   u_char private_indicator                      :1;
   u_char                                        :2;
   u_char section_length_hi                      :4;
#else
   u_char section_length_hi                      :4;
   u_char                                        :2;
   u_char private_indicator                      :1;
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
   u_char protocol_version                       :8;
// u_char num_channels_in_section                :8; //For information, in case of TVCT or CVCT
} psip_t;


#define PSIP_VCT_LEN 32

/**@brief PSIP (TC)VCT (Terrestrial/Cable Virtual Channel Table) channels descriptors*/
typedef struct {
  uint8_t short_name[14];//The channel short name in UTF-16

#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :4; //reserved
   u_char major_channel_number_hi4               :4;
#else
   u_char major_channel_number_hi4               :4;
   u_char                                        :4; //reserved
#endif  

#if BYTE_ORDER == BIG_ENDIAN
   u_char major_channel_number_lo6               :6;
   u_char minor_channel_number_hi                :2;
#else
   u_char minor_channel_number_hi                :2;
   u_char major_channel_number_lo6               :6;
#endif  

   u_char minor_channel_number_lo                :8;

   u_char modulation_mode                        :8;

  u_int8_t carrier_frequency[4]; //deprecated

   u_char channel_tsid_hi                        :8;

   u_char channel_tsid_lo                        :8;

   u_char program_number_hi                      :8;

   u_char program_number_lo                      :8;

#if BYTE_ORDER == BIG_ENDIAN
   u_char ETM_location                           :2;
   u_char access_controlled                      :1;
   u_char hidden                                 :1;
   u_char path_select                            :1; //Only cable
   u_char out_of_band                            :1; //Only cable
   u_char hide_guide                             :1;
   u_char                                        :1; //reserved
#else
   u_char                                        :1; //reserved
   u_char hide_guide                             :1;
   u_char out_of_band                            :1; //Only cable
   u_char path_select                            :1; //Only cable
   u_char hidden                                 :1;
   u_char access_controlled                      :1;
   u_char ETM_location                           :2;
#endif  

#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :2; //reserved
   u_char service_type                           :6;
#else
   u_char service_type                           :6;
   u_char                                        :2; //reserved
#endif  

   u_char source_id_hi                           :8;

   u_char source_id_lo                           :8;

#if BYTE_ORDER == BIG_ENDIAN
   u_char                                        :6;
   u_char descriptor_length_hi                   :2;
#else
   u_char descriptor_length_hi                   :2;
   u_char                                        :6;
#endif  
   u_char descriptor_length_lo                   :8;
} psip_vct_channel_t;

//1 for number strings, 3 for language code,1 for number of segments
#define MULTIPLE_STRING_STRUCTURE_HEADER 5

/*****************************
 *  End of ATSC PSIP tables  *
 *****************************/


/**@brief structure for the build of a ts packet*/
typedef struct {
  /**say if the packet is empty*/
  int empty;
  /**say if the packet is ok*/
  int packet_ok;
  /**The PID of the packet*/
  int pid;
  /**the countinuity counter, incremented in each packet*/
  int continuity_counter;
  /** packet len*/
  int len;
  /**the buffer*/
  unsigned char packet[4096];
#ifdef HAVE_LIBPTHREAD
  /** If we have threads, the lock on the packet */
  pthread_mutex_t packetmutex;
#endif
}mumudvb_ts_packet_t;


int ts_check_CRC( mumudvb_ts_packet_t *pmt);
int get_ts_packet(unsigned char *buf, mumudvb_ts_packet_t *pmt);
int AddPacketStart (unsigned char *packet, unsigned char *buf, unsigned int len);
int AddPacketContinue  (unsigned char *packet, unsigned char *buf, unsigned int len, unsigned int act_len);

struct mumudvb_channel_t;
int check_pmt_service_id(mumudvb_ts_packet_t *pmt, struct mumudvb_channel_t *channel);


#endif
