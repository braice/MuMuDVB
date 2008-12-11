/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 transport layer

    Copyright (C) 2004, 2005 Manu Abraham <abraham.manu@gmail.com>
    Copyright (C) 2005 Julian Scheel (julian at jusst dot de)
    Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation; either version 2.1 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#ifndef __EN50221_APPLICATION_ca_H__
#define __EN50221_APPLICATION_ca_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>
#include <libucsi/mpeg/pmt_section.h>
#include <libucsi/dvb/descriptor.h>

#define CA_LIST_MANAGEMENT_MORE     0x00
#define CA_LIST_MANAGEMENT_FIRST    0x01
#define CA_LIST_MANAGEMENT_LAST     0x02
#define CA_LIST_MANAGEMENT_ONLY     0x03
#define CA_LIST_MANAGEMENT_ADD      0x04
#define CA_LIST_MANAGEMENT_UPDATE   0x05

#define CA_PMT_CMD_ID_OK_DESCRAMBLING   0x01
#define CA_PMT_CMD_ID_OK_MMI            0x02
#define CA_PMT_CMD_ID_QUERY             0x03
#define CA_PMT_CMD_ID_NOT_SELECTED      0x04

#define CA_ENABLE_DESCRAMBLING_POSSIBLE                     0x01
#define CA_ENABLE_DESCRAMBLING_POSSIBLE_PURCHASE            0x02
#define CA_ENABLE_DESCRAMBLING_POSSIBLE_TECHNICAL           0x03
#define CA_ENABLE_DESCRAMBLING_NOT_POSSIBLE_NO_ENTITLEMENT  0x71
#define CA_ENABLE_DESCRAMBLING_NOT_POSSIBLE_TECHNICAL       0x73


#define EN50221_APP_CA_RESOURCEID MKRID(3,1,1)

/**
 * PMT reply structure.
 */
struct en50221_app_pmt_reply {
	uint16_t program_number;
	EBIT3(uint8_t reserved_1		: 2;,
	      uint8_t version_number		: 5;,
 	      uint8_t current_next_indicator	: 1;);
	EBIT2(uint8_t CA_enable_flag		: 1;,
	      uint8_t CA_enable			: 7;);
	/* struct en50221_app_pmt_stream streams[] */
} __attribute__ ((packed));

/**
 * A stream within a pmt reply structure.
 */
struct en50221_app_pmt_stream {
	EBIT2(uint16_t reserved_1		: 3;,
	      uint16_t es_pid			:13;);
	EBIT2(uint8_t CA_enable_flag		: 1;,
	      uint8_t CA_enable			: 7;);
} __attribute__ ((packed));

/**
 * Convenience iterator for the streams field of the en50221_app_pmt_reply structure.
 *
 * @param pmt Pointer to the en50221_app_pmt_reply structure.
 * @param pos Variable holding a pointer to the current en50221_app_pmt_stream.
 * @param size Total size of the PMT reply.
 */
#define en50221_app_pmt_reply_streams_for_each(pmt, pos, size) \
    for ((pos) = en50221_app_pmt_reply_streams_first(pmt, size); \
         (pos); \
         (pos) = en50221_app_pmt_reply_streams_next(pmt, pos, size))


/**
 * Type definition for command - called when we receive a ca info response.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param ca_id_count Number of ca_system_ids.
 * @param ca_ids Pointer to list of ca_system_ids.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_ca_info_callback) (void *arg,
					     uint8_t slot_id,
					     uint16_t session_number,
					     uint32_t ca_id_count,
					     uint16_t * ca_ids);

/**
 * Type definition for pmt_reply - called when we receive a pmt_reply.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param reply Pointer to a struct en50221_app_pmt_reply.
 * @param reply_size Total size of the struct en50221_app_pmt_reply in bytes.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_ca_pmt_reply_callback) (void *arg,
						  uint8_t slot_id,
						  uint16_t session_number,
						  struct en50221_app_pmt_reply *reply,
						  uint32_t reply_size);

/**
 * Opaque type representing a ca resource.
 */
struct en50221_app_ca;

/**
 * Create an instance of the ca resource.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_ca *en50221_app_ca_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of the ca resource.
 *
 * @param ca Instance to destroy.
 */
extern void en50221_app_ca_destroy(struct en50221_app_ca *ca);

/**
 * Register the callback for when we receive a ca info.
 *
 * @param ca ca resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_ca_register_info_callback(struct en50221_app_ca *ca,
						  en50221_app_ca_info_callback callback,
						  void *arg);

/**
 * Register the callback for when we receive a pmt_reply.
 *
 * @param ca ca resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_ca_register_pmt_reply_callback(struct en50221_app_ca *ca,
						       en50221_app_ca_pmt_reply_callback callback,
						       void *arg);

/**
 * Send a ca_info_req to the CAM.
 *
 * @param ca ca resource instance.
 * @param session_number Session number to send it on.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_ca_info_enq(struct en50221_app_ca *ca,
				   uint16_t session_number);

/**
 * Send a ca_pmt structure to the CAM.
 *
 * @param ca ca resource instance.
 * @param session_number Session number to send it on.
 * @param ca_pmt A ca_pmt structure formatted with the en50221_ca_format_pmt() function.
 * @param ca_pmt_length Length of ca_pmt structure in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_ca_pmt(struct en50221_app_ca *ca,
			      uint16_t session_number,
			      uint8_t * ca_pmt,
			      uint32_t ca_pmt_length);

/**
 * Transform a libucsi PMT into a binary structure for sending to a CAM.
 *
 * @param pmt The source PMT structure.
 * @param data Pointer to data buffer to write it to.
 * @param data_length Number of bytes available in data buffer.
 * @param move_ca_descriptors If non-zero, will attempt to move CA descriptors
 * in order to reduce the size of the formatted CAPMT.
 * @param ca_pmt_list_management One of the CA_LIST_MANAGEMENT_*.
 * @param ca_pmt_cmd_id One of the CA_PMT_CMD_ID_*.
 * @return Number of bytes used, or -1 on error.
 */
extern int en50221_ca_format_pmt(struct mpeg_pmt_section *pmt,
				 uint8_t * data,
				 uint32_t data_length,
				 int move_ca_descriptors,
				 uint8_t ca_pmt_list_management,
				 uint8_t ca_pmt_cmd_id);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param ca CA instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_ca_message(struct en50221_app_ca *ca,
				  uint8_t slot_id,
				  uint16_t session_number,
				  uint32_t resource_id,
				  uint8_t *data,
				  uint32_t data_length);




static inline struct en50221_app_pmt_stream *
	en50221_app_pmt_reply_streams_first(struct en50221_app_pmt_reply *reply,
					    uint32_t reply_size)
{
	uint32_t pos = sizeof(struct en50221_app_pmt_reply);

	if (pos >= reply_size)
		return NULL;

	return (struct en50221_app_pmt_stream *) ((uint8_t *) reply + pos);
}

static inline struct en50221_app_pmt_stream *
	en50221_app_pmt_reply_streams_next(struct en50221_app_pmt_reply *reply,
					   struct en50221_app_pmt_stream *pos,
					   uint32_t reply_size)
{
	uint8_t *end = (uint8_t *) reply + reply_size;
	uint8_t *next =
		(uint8_t *) pos +
		sizeof(struct en50221_app_pmt_stream);

	if (next >= end)
		return NULL;

	return (struct en50221_app_pmt_stream *) next;
}

#ifdef __cplusplus
}
#endif

#endif
