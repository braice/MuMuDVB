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

#ifndef __EN50221_APPLICATION_epg_H__
#define __EN50221_APPLICATION_epg_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>

#define EPG_COMMAND_ID_MMI                          0x02
#define EPG_COMMAND_ID_QUERY                        0x03

#define EPG_EVENTSTATUS_ENTITLEMENT_UNKNOWN         0x00
#define EPG_EVENTSTATUS_ENTITLEMENT_AVAILABLE       0x01
#define EPG_EVENTSTATUS_ENTITLEMENT_NOT_AVAILABLE   0x02
#define EPG_EVENTSTATUS_MMI_DIALOGUE_REQUIRED       0x03
#define EPG_EVENTSTATUS_MMI_COMPLETE_UNKNOWN        0x04
#define EPG_EVENTSTATUS_MMI_COMPLETE_AVAILABLE      0x05
#define EPG_EVENTSTATUS_MMI_COMPLETE_NOT_AVAILABLE  0x06

#define EN50221_APP_EPG_RESOURCEID(INSTANCE_NUM) MKRID(120,(INSTANCE_NUM),1)



/**
 * Type definition for reply - called when we receive an EPG reply from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param event_status One of the EPG_EVENTSTATUS_* values.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_epg_reply_callback) (void *arg,
					       uint8_t slot_id,
					       uint16_t session_number,
					       uint8_t event_status);

/**
 * Opaque type representing a epg resource.
 */
struct en50221_app_epg;

/**
 * Create an instance of the epg resource.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_epg *en50221_app_epg_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of the epg resource.
 *
 * @param epg Instance to destroy.
 */
extern void en50221_app_epg_destroy(struct en50221_app_epg *epg);

/**
 * Register the callback for when we receive a enquiry response.
 *
 * @param epg epg resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_epg_register_reply_callback(struct en50221_app_epg *epg,
						    en50221_app_epg_reply_callback callback,
						    void *arg);

/**
 * Enquire about the entitlement status for an EPG entry.
 *
 * @param epg epg resource instance.
 * @param session_number Session number to send it on.
 * @param command_id One of the EPG_COMMAND_ID_* fields.
 * @param network_id Network ID concerned.
 * @param original_network_id Original network ID concerned.
 * @param transport_stream_id Transport stream ID concerned.
 * @param service_id Service ID concerned.
 * @param event_id Event ID concerned.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_epg_enquire(struct en50221_app_epg *epg,
				   uint16_t session_number,
				   uint8_t command_id,
				   uint16_t network_id,
				   uint16_t original_network_id,
				   uint16_t transport_stream_id,
				   uint16_t service_id,
				   uint16_t event_id);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param epg epg instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_epg_message(struct en50221_app_epg *epg,
				   uint8_t slot_id,
				   uint16_t session_number,
				   uint32_t resource_id,
				   uint8_t *data,
				   uint32_t data_length);

#ifdef __cplusplus
}
#endif
#endif
