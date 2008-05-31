#ifndef _CAM_H
#define _CAM_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/dvb/ca.h>

#define CA_DEV "/dev/dvb/adapter%d/ca%d"

struct ca_info {
  int sys_num;
  uint16_t sys_id[256];
  char app_name[256];
};

//structur for the build of the pmt packet
typedef struct {
  int empty; //say if the pmt is empty
  int pid;   //The PID of the packet
  int continuity_counter; //the countinuity counter, incremented in each packet
  int len;
  int i_program_number; //VLC COMPAT, a virer plus tard
  unsigned char packet[4096]; //the buffer
}mumudvb_pmt_t;

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


#endif
