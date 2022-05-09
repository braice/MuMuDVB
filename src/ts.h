/*
 * MuMuDVB - Stream a DVB transport stream.
 *
 * (C) 2004-2011 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.net/
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
#include <pthread.h>
#ifndef _WIN32
#include <endian.h>
#endif

#include "config.h"

//The maximum size for a TS packet
#define MAX_TS_SIZE 4096

//0x1ffb=8187 It's the pid for the information tables in ATSC
#define PSIP_PID 8187

//Part of this code comes from libsi
//   (C) 2001-03 Rolf Hakenes <hakenes@hippomi.de>, under the
//               GNU GPL with contribution of Oleg Assovski,
//               www.satmania.com

#define TS_HEADER_LEN 5
#define HILO(x) (x##_hi << 8 | x##_lo)

#define BCDHI(x) (((x)>> 4) & 0x0f)
#define BCDLO(x) ((x) & 0x0f)

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
   uint8_t table_id                               :8;
#ifdef __BIG_ENDIAN__
   uint8_t section_syntax_indicator               :1;
   uint8_t dummy                                  :1;        // has to be 0
   uint8_t                                        :2;
   uint8_t section_length_hi                      :4;
#else
   uint8_t section_length_hi                      :4;
   uint8_t                                        :2;
   uint8_t dummy                                  :1;        // has to be 0
   uint8_t section_syntax_indicator               :1;
#endif
   uint8_t section_length_lo                      :8;
   uint8_t transport_stream_id_hi                 :8;
   uint8_t transport_stream_id_lo                 :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :2;
   uint8_t version_number                         :5;
   uint8_t current_next_indicator                 :1;
#else
   uint8_t current_next_indicator                 :1;
   uint8_t version_number                         :5;
   uint8_t                                        :2;
#endif
   uint8_t section_number                         :8;
   uint8_t last_section_number                    :8;
} pat_t;

#define PAT_PROG_LEN 4

/** @brief Program Association Table (PAT): program*/
typedef struct {
   uint8_t program_number_hi                      :8;
   uint8_t program_number_lo                      :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :3;
   uint8_t network_pid_hi                         :5;
#else
   uint8_t network_pid_hi                         :5;
   uint8_t                                        :3;
#endif
   uint8_t network_pid_lo                         :8;
   /* or program_map_pid (if prog_num=0)*/
} pat_prog_t;


/**
 * @brief Conditional Access Table (CAT):
 *
 *       - contains the CA system ID to EMM PID mapping
 *
 */

#define CAT_LEN 8

/** @brief Conditional Access Table (CAT):*/
typedef struct {
    uint8_t table_id                               :8;
#ifdef __BIG_ENDIAN__
    uint8_t section_syntax_indicator               :1;
   uint8_t dummy                                  :1;        // has to be 0
   uint8_t                                        :2;
   uint8_t section_length_hi                      :4;
#else
    uint8_t section_length_hi                      :4;
    uint8_t                                        :2;
    uint8_t dummy                                  :1;        // has to be 0
    uint8_t section_syntax_indicator               :1;
#endif
    uint8_t section_length_lo                      :8;
    uint8_t transport_stream_id_hi                 :8;
    uint8_t transport_stream_id_lo                 :8;
#ifdef __BIG_ENDIAN__
    uint8_t                                        :2;
    uint8_t version_number                         :5;
    uint8_t current_next_indicator                 :1;
#else
    uint8_t current_next_indicator                 :1;
    uint8_t version_number                         :5;
    uint8_t                                        :2;
#endif
    uint8_t section_number                         :8;
    uint8_t last_section_number                    :8;
} cat_t;

//Used to generate the CA_PMT message and for autoconfiguration
/** @brief Mpeg2-TS header*/
typedef struct {
  uint8_t sync_byte                              :8;
#ifdef __BIG_ENDIAN__
  uint8_t transport_error_indicator              :1;
  uint8_t payload_unit_start_indicator           :1;
  uint8_t transport_priority                     :1;
  uint8_t pid_hi                                 :5;
#else
  uint8_t pid_hi                                 :5;
  uint8_t transport_priority                     :1;
  uint8_t payload_unit_start_indicator           :1;
  uint8_t transport_error_indicator              :1;
#endif
  uint8_t pid_lo                                 :8;
#ifdef __BIG_ENDIAN__
  uint8_t transport_scrambling_control           :2;
  uint8_t adaptation_field_control               :2;
  uint8_t continuity_counter                     :4;
#else
  uint8_t continuity_counter                     :4;
  uint8_t adaptation_field_control               :2;
  uint8_t transport_scrambling_control           :2;
#endif
} ts_header_t;

typedef struct {
  u_char adaptation_field_length		:8;
#ifdef __BIG_ENDIAN__
  u_char adaptation_field_extension_flag	:1;
  u_char transport_private_data_flag		:1;
  u_char splicing_point_flag			:1;
  u_char OPCR_flag				:1;
  u_char PCR_flag				:1;
  u_char elementary_stream_priority_indicator	:1;
  u_char random_access_indicator		:1;
  u_char discontinuity_indicator		:1;
#else
  u_char discontinuity_indicator		:1;
  u_char random_access_indicator		:1;
  u_char elementary_stream_priority_indicator	:1;
  u_char PCR_flag				:1;
  u_char OPCR_flag				:1;
  u_char splicing_point_flag			:1;
  u_char transport_private_data_flag		:1;
  u_char adaptation_field_extension_flag	:1;
#endif
  char PCR[6];
  char OPCR[6];
  char splice_countdown				:8;
  u_char transport_private_data_length		:8;
} af_header_t;

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
   uint8_t table_id                               :8;
#ifdef __BIG_ENDIAN__
   uint8_t section_syntax_indicator               :1;
   uint8_t dummy                                  :1; // has to be 0
   uint8_t                                        :2;
   uint8_t section_length_hi                      :4;
#else
   uint8_t section_length_hi                      :4;
   uint8_t                                        :2;
   uint8_t dummy                                  :1; // has to be 0
   uint8_t section_syntax_indicator               :1;
#endif
   uint8_t section_length_lo                      :8;
   uint8_t program_number_hi                      :8;
   uint8_t program_number_lo                      :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :2;
   uint8_t version_number                         :5;
   uint8_t current_next_indicator                 :1;
#else
   uint8_t current_next_indicator                 :1;
   uint8_t version_number                         :5;
   uint8_t                                        :2;
#endif
   uint8_t section_number                         :8;
   uint8_t last_section_number                    :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :3;
   uint8_t PCR_PID_hi                             :5;
#else
   uint8_t PCR_PID_hi                             :5;
   uint8_t                                        :3;
#endif
   uint8_t PCR_PID_lo                             :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :4;
   uint8_t program_info_length_hi                 :4;
#else
   uint8_t program_info_length_hi                 :4;
   uint8_t                                        :4;
#endif
   uint8_t program_info_length_lo                 :8;
   //descriptors
} pmt_t;

/** length of the PMT program information section header */
#define PMT_INFO_LEN 5
/** @brief  Program Map Table (PMT) : program information section*/
typedef struct {
   uint8_t stream_type                            :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :3;
   uint8_t elementary_PID_hi                      :5;
#else
   uint8_t elementary_PID_hi                      :5;
   uint8_t                                        :3;
#endif
   uint8_t elementary_PID_lo                      :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :4;
   uint8_t ES_info_length_hi                      :4;
#else
   uint8_t ES_info_length_hi                      :4;
   uint8_t                                        :4;
#endif
   uint8_t ES_info_length_lo                      :8;
     // descriptors
} pmt_info_t;


#define DESCR_CA_LEN 6
/** @brief 0x09 ca_descriptor */
typedef struct {
  uint8_t descriptor_tag                         :8;
  uint8_t descriptor_length                      :8;
  uint8_t CA_type_hi                             :8;
  uint8_t CA_type_lo                             :8;
#ifdef __BIG_ENDIAN__
  uint8_t reserved                               :3;
  uint8_t CA_PID_hi                              :5;
#else
  uint8_t CA_PID_hi                              :5;
  uint8_t reserved                               :3;
#endif
  uint8_t CA_PID_lo                              :8;
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
   uint8_t table_id                               :8;
#ifdef __BIG_ENDIAN__
   uint8_t section_syntax_indicator               :1;
   uint8_t                                        :3;
   uint8_t section_length_hi                      :4;
#else
   uint8_t section_length_hi                      :4;
   uint8_t                                        :3;
   uint8_t section_syntax_indicator               :1;
#endif
   uint8_t section_length_lo                      :8;
   uint8_t transport_stream_id_hi                 :8;
   uint8_t transport_stream_id_lo                 :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :2;
   uint8_t version_number                         :5;
   uint8_t current_next_indicator                 :1;
#else
   uint8_t current_next_indicator                 :1;
   uint8_t version_number                         :5;
   uint8_t                                        :2;
#endif
   uint8_t section_number                         :8;
   uint8_t last_section_number                    :8;
   uint8_t original_network_id_hi                 :8;
   uint8_t original_network_id_lo                 :8;
   uint8_t                                        :8;
} sdt_t;

#define GetSDTTransportStreamId(x) (HILO(((sdt_t *) x)->transport_stream_id))
#define GetSDTOriginalNetworkId(x) (HILO(((sdt_t *) x)->original_network_id))

#define SDT_DESCR_LEN 5
/**@brief  Service Description Table (SDT), descriptor */
typedef struct {
   uint8_t service_id_hi                          :8;
   uint8_t service_id_lo                          :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :6;
   uint8_t eit_schedule_flag                      :1;
   uint8_t eit_present_following_flag             :1;
   uint8_t running_status                         :3;
   uint8_t free_ca_mode                           :1;
   uint8_t descriptors_loop_length_hi             :4;
#else
   uint8_t eit_present_following_flag             :1;
   uint8_t eit_schedule_flag                      :1;
   uint8_t                                        :6;
   uint8_t descriptors_loop_length_hi             :4;
   uint8_t free_ca_mode                           :1;
   uint8_t running_status                         :3;
#endif
   uint8_t descriptors_loop_length_lo             :8;
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
  uint8_t table_id                               :8;
#ifdef __BIG_ENDIAN__
  uint8_t section_syntax_indicator               :1;
  uint8_t                                        :3;
  uint8_t section_length_hi                      :4;
#else
  uint8_t section_length_hi                      :4;
  uint8_t                                        :3;
  uint8_t section_syntax_indicator               :1;
#endif
  uint8_t section_length_lo                      :8;
  uint8_t service_id_hi                          :8;
  uint8_t service_id_lo                          :8;
#ifdef __BIG_ENDIAN__
  uint8_t                                        :2;
  uint8_t version_number                         :5;
  uint8_t current_next_indicator                 :1;
#else
  uint8_t current_next_indicator                 :1;
  uint8_t version_number                         :5;
  uint8_t                                        :2;
#endif
  uint8_t section_number                         :8;
  uint8_t last_section_number                    :8;
  uint8_t transport_stream_id_hi                 :8;
  uint8_t transport_stream_id_lo                 :8;
  uint8_t original_network_id_hi                 :8;
  uint8_t original_network_id_lo                 :8;
  uint8_t segment_last_section_number            :8;
  uint8_t segment_last_table_id                  :8;
} eit_t;

#define EIT_EVENT_LEN 12
/**@brief  Event Information Table (EIT), descriptor header*/
typedef struct {
   uint8_t event_id_hi                            :8;
   uint8_t event_id_lo                            :8;
   uint8_t start_time_0                           :8;
   uint8_t start_time_1                           :8;
   uint8_t start_time_2                           :8;
   uint8_t start_time_3                           :8;
   uint8_t start_time_4                           :8;
   uint8_t duration_0                             :8;
   uint8_t duration_1                             :8;
   uint8_t duration_2                             :8;
#ifdef __BIG_ENDIAN__
   uint8_t running_status                         :3;
   uint8_t free_ca_mode                           :1;
   uint8_t descriptors_loop_length_hi             :4;
#else
   uint8_t descriptors_loop_length_hi             :4;
   uint8_t free_ca_mode                           :1;
   uint8_t running_status                         :3;
#endif
   uint8_t descriptors_loop_length_lo             :8;
} eit_event_t;



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
   uint8_t table_id                               :8;
#ifdef __BIG_ENDIAN__
   uint8_t section_syntax_indicator               :1;
   uint8_t                                        :3;
   uint8_t section_length_hi                      :4;
#else
   uint8_t section_length_hi                      :4;
   uint8_t                                        :3;
   uint8_t section_syntax_indicator               :1;
#endif
   uint8_t section_length_lo                      :8;
   uint8_t network_id_hi                          :8;
   uint8_t network_id_lo                          :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :2;
   uint8_t version_number                         :5;
   uint8_t current_next_indicator                 :1;
#else
   uint8_t current_next_indicator                 :1;
   uint8_t version_number                         :5;
   uint8_t                                        :2;
#endif
   uint8_t section_number                         :8;
   uint8_t last_section_number                    :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :4;
   uint8_t network_descriptor_length_hi           :4;
#else
   uint8_t network_descriptor_length_hi           :4;
   uint8_t                                        :4;
#endif
   uint8_t network_descriptor_length_lo           :8;
  /* descriptors */
}nit_t;

#define SIZE_NIT_MID 2

typedef struct {                                 // after descriptors
#ifdef __BIG_ENDIAN__
    uint8_t                                        :4;
    uint8_t transport_stream_loop_length_hi        :4;
#else
    uint8_t transport_stream_loop_length_hi        :4;
    uint8_t                                        :4;
#endif
   uint8_t transport_stream_loop_length_lo        :8;
}nit_mid_t;

#define NIT_TS_LEN 6

typedef struct {
   uint8_t transport_stream_id_hi                 :8;
   uint8_t transport_stream_id_lo                 :8;
   uint8_t original_network_id_hi                 :8;
   uint8_t original_network_id_lo                 :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :4;
   uint8_t transport_descriptors_length_hi        :4;
#else
   uint8_t transport_descriptors_length_hi        :4;
   uint8_t                                        :4;
#endif
   uint8_t transport_descriptors_length_lo        :8;
   /* descriptors  */
}nit_ts_t;

#define NIT_LCN_LEN 4

typedef struct {
   uint8_t service_id_hi                          :8;
   uint8_t service_id_lo                          :8;
#ifdef __BIG_ENDIAN__
   uint8_t visible_service_flag                   :1;
   uint8_t reserved                               :5;
   uint8_t logical_channel_number_hi              :2;
#else
   uint8_t logical_channel_number_hi              :2;
   uint8_t reserved                               :5;
   uint8_t visible_service_flag                   :1;
#endif
   uint8_t logical_channel_number_lo              :8;
}nit_lcn_t;


/** length of the common tables header */
#define TABLE_LEN 8
#define BYTES_BFR_SEC_LEN 3 //the number of bytes before the section_length (so must be added to section_length to get full len)

/** @brief Common Table headers (PAT, EIT, SDT, PMT, NIT):
 * This header is the first 8 bytes common to all tables
 * it's mainly used to get the section length
 */
typedef struct {
   uint8_t table_id                               :8;
#ifdef __BIG_ENDIAN__
   uint8_t section_syntax_indicator               :1;
   uint8_t                                        :3;
   uint8_t section_length_hi                      :4;
#else
   uint8_t section_length_hi                      :4;
   uint8_t                                        :3;
   uint8_t section_syntax_indicator               :1;
#endif
   uint8_t section_length_lo                      :8;
   uint8_t transport_stream_id_hi                 :8;
   uint8_t transport_stream_id_lo                 :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :2;
   uint8_t version_number                         :5;
   uint8_t current_next_indicator                 :1;
#else
   uint8_t current_next_indicator                 :1;
   uint8_t version_number                         :5;
   uint8_t                                        :2;
#endif
   uint8_t section_number                         :8;
   uint8_t last_section_number                    :8;
} tbl_h_t;



/** @brief 0x5a terrestrial_delivery_system_descriptor */
typedef struct {
  uint8_t descriptor_tag                         :8;
  uint8_t descriptor_length                      :8;
  uint8_t frequency_4                            :8;
  uint8_t frequency_3                            :8;
  uint8_t frequency_2                            :8;
  uint8_t frequency_1                            :8;
#ifdef __BIG_ENDIAN__
  uint8_t bandwidth                              :3;
  uint8_t priority                               :1;
  uint8_t Time_Slicing_indicator                 :1;
  uint8_t MPE_FEC_indicator                      :1;
  uint8_t                                        :2;
#else
  uint8_t                                        :2;
  uint8_t MPE_FEC_indicator                      :1;
  uint8_t Time_Slicing_indicator                 :1;
  uint8_t priority                               :1;
  uint8_t bandwidth                              :3;
#endif
#ifdef __BIG_ENDIAN__
  uint8_t constellation                          :2;
  uint8_t hierarchy_information                  :3;
  uint8_t code_rate_HP_stream                    :3;
#else
  uint8_t code_rate_HP_stream                    :3;
  uint8_t hierarchy_information                  :3;
  uint8_t constellation                          :2;
#endif
#ifdef __BIG_ENDIAN__
  uint8_t code_rate_LP_stream                    :3;
  uint8_t guard_interval                         :2;
  uint8_t transmission_mode                      :2;
  uint8_t other_frequency_flag                   :1;
#else
  uint8_t other_frequency_flag                   :1;
  uint8_t transmission_mode                      :2;
  uint8_t guard_interval                         :2;
  uint8_t code_rate_LP_stream                    :3;
#endif
} descr_terr_delivery_t;


/** @brief 0x43 satellite_delivery_system_descriptor */
typedef struct {
  uint8_t descriptor_tag                         :8;
  uint8_t descriptor_length                      :8;
  uint8_t frequency_4                            :8;
  uint8_t frequency_3                            :8;
  uint8_t frequency_2                            :8;
  uint8_t frequency_1                            :8;
  uint8_t orbital_position_hi                    :8;
  uint8_t orbital_position_lo                    :8;
#ifdef __BIG_ENDIAN__
  uint8_t west_east_flag                         :1;
  uint8_t polarization	                        :2;
  uint8_t roll_off		               			:2;
  uint8_t modulation_system                      :1;
  uint8_t modulation_type	               		:2;
#else
  uint8_t modulation_type	               		:2;
  uint8_t modulation_system                      :1;
  uint8_t roll_off		               			:2;
  uint8_t polarization	                        :2;
  uint8_t west_east_flag                         :1;
#endif
  uint8_t symbol_rate_12		               		:8;
  uint8_t symbol_rate_34		               		:8;
  uint8_t symbol_rate_56		               		:8;
#ifdef __BIG_ENDIAN__
  uint8_t symbol_rate_7		               		:4;
  uint8_t FEC_inner								:4;
#else
  uint8_t FEC_inner								:4;
  uint8_t symbol_rate_7		               		:4;
#endif
} descr_sat_delivery_t;


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
   uint8_t table_id                               :8;
#ifdef __BIG_ENDIAN__
   uint8_t section_syntax_indicator               :1;
   uint8_t private_indicator                      :1;
   uint8_t                                        :2;
   uint8_t section_length_hi                      :4;
#else
   uint8_t section_length_hi                      :4;
   uint8_t                                        :2;
   uint8_t private_indicator                      :1;
   uint8_t section_syntax_indicator               :1;
#endif
   uint8_t section_length_lo                      :8;
   uint8_t transport_stream_id_hi                 :8;
   uint8_t transport_stream_id_lo                 :8;
#ifdef __BIG_ENDIAN__
   uint8_t                                        :2;
   uint8_t version_number                         :5;
   uint8_t current_next_indicator                 :1;
#else
   uint8_t current_next_indicator                 :1;
   uint8_t version_number                         :5;
   uint8_t                                        :2;
#endif
   uint8_t section_number                         :8;
   uint8_t last_section_number                    :8;
   uint8_t protocol_version                       :8;
// uint8_t num_channels_in_section                :8; //For information, in case of TVCT or CVCT
} psip_t;


#define PSIP_VCT_LEN 32

/**@brief PSIP (TC)VCT (Terrestrial/Cable Virtual Channel Table) channels descriptors*/
typedef struct {
   uint8_t short_name[14];//The channel short name in UTF-16

#ifdef __BIG_ENDIAN__
   uint8_t                                        :4; //reserved
   uint8_t major_channel_number_hi4               :4;
#else
   uint8_t major_channel_number_hi4               :4;
   uint8_t                                        :4; //reserved
#endif

#ifdef __BIG_ENDIAN__
   uint8_t major_channel_number_lo6               :6;
   uint8_t minor_channel_number_hi                :2;
#else
   uint8_t minor_channel_number_hi                :2;
   uint8_t major_channel_number_lo6               :6;
#endif

   uint8_t minor_channel_number_lo                :8;

   uint8_t modulation_mode                        :8;

   uint8_t carrier_frequency[4];

   uint8_t channel_tsid_hi                        :8;

   uint8_t channel_tsid_lo                        :8;

   uint8_t program_number_hi                      :8;

   uint8_t program_number_lo                      :8;

#ifdef __BIG_ENDIAN__
   uint8_t ETM_location                           :2;
   uint8_t access_controlled                      :1;
   uint8_t hidden                                 :1;
   uint8_t path_select                            :1; //Only cable
   uint8_t out_of_band                            :1; //Only cable
   uint8_t hide_guide                             :1;
   uint8_t                                        :1; //reserved
#else
   uint8_t                                        :1; //reserved
   uint8_t hide_guide                             :1;
   uint8_t out_of_band                            :1; //Only cable
   uint8_t path_select                            :1; //Only cable
   uint8_t hidden                                 :1;
   uint8_t access_controlled                      :1;
   uint8_t ETM_location                           :2;
#endif

#ifdef __BIG_ENDIAN__
   uint8_t                                        :2; //reserved
   uint8_t service_type                           :6;
#else
   uint8_t service_type                           :6;
   uint8_t                                        :2; //reserved
#endif

   uint8_t source_id_hi                           :8;

   uint8_t source_id_lo                           :8;

#ifdef __BIG_ENDIAN__
   uint8_t                                        :6;
   uint8_t descriptor_length_hi                   :2;
#else
   uint8_t descriptor_length_hi                   :2;
   uint8_t                                        :6;
#endif
   uint8_t descriptor_length_lo                   :8;
} psip_vct_channel_t;

//1 for number strings, 3 for language code,1 for number of segments
#define MULTIPLE_STRING_STRUCTURE_HEADER 5


/*****************************
 *  End of ATSC PSIP tables  *
 *****************************/

/** Enum to tell if the option is set*/
typedef enum packet_status {
  EMPTY,          //No data in the packet
  PARTIAL_HEADER, //An incomplete header is in the packet
  STARTED,        //Some data are in the packet
  VALID           //All the expected data are in the packet and the CRC32 is valid
} packet_status_t;



//The number of complete section we accept to have in one TS
//A TS is 188bytes long minus 4 bytes for the header 184 bytes left
//A section is at least 8 bytes long + one descriptor 3 bytes + CRC32 4 bytes
//it's a total of 15bytes / section
#define MAX_FULL_PACKETS 15
//A minimum is MAX_TS_SIZE + TS_PACKET_SIZE
//Just to add flexibility on how to write the code I take some margin
#define FULL_BUFFER_SIZE 2*MAX_TS_SIZE


/**@brief structure for the build of a ts packet
  Since a packet can be finished and another one starts in the same
  elementary TS packet, there is two packets in this structure

 */
typedef struct {
  /** the buffer for the packet full (empty or contains a valid full packet)*/
  unsigned char data_full[MAX_TS_SIZE];
  /** the length of the data contained in data_full */
  int len_full;

  //starting from here, these variables MUSN'T be accessed outside ts.c
  /** The number of full packets */
  int full_number;
  /** The lengths of the full packets */
  int full_lengths[MAX_FULL_PACKETS];
  /** The amount of data in the full buffer */
  int full_buffer_len;
  /** The buffer containing the full packets */
  unsigned char buffer_full[FULL_BUFFER_SIZE];
  /** the buffer for the partial packet (never valid, shouldn't be accessed by funtions other than get_ts_packet)*/
  unsigned char data_partial[MAX_TS_SIZE];
  /** the length of the data contained in data_partial */
  int len_partial;
  /** the expected length of the data contained in data_partial */
  int expected_len_partial;
  /** The packet status*/
  packet_status_t status_partial;
  /**The PID of the packet*/
  int pid;
  /**the countinuity counter, incremented in each packet*/
  int cc;

  /** If we have threads, the lock on the packet */
  pthread_mutex_t packetmutex;
}mumudvb_ts_packet_t;


int get_ts_packet(unsigned char *, mumudvb_ts_packet_t *);

unsigned char *get_ts_begin(unsigned char *buf);

struct mumudvb_channel_t;
void ts_display_pat(char* log_module,unsigned char *buf);
void ts_display_country_avaibility_descriptor(char* log_module,unsigned char *buf);

void ts_display_nit_network_descriptors(char *log_module, unsigned char *buf,int descriptors_loop_len);
void ts_display_network_name_descriptor(char* log_module, unsigned char *buf);
void ts_display_multilingual_network_name_descriptor(char* log_module, unsigned char *buf);
void ts_display_service_list_descriptor(char* log_module, unsigned char *buf);
void ts_display_lcn_descriptor(char* log_module, unsigned char *buf);
void ts_display_satellite_delivery_system_descriptor(char* log_module, unsigned char *buf);
void ts_display_terrestrial_delivery_system_descriptor(char* log_module, unsigned char *buf);
void ts_display_frequency_list_descriptor(char* log_module, unsigned char* buf);


#endif
