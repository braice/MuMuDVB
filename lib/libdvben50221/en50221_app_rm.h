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

#ifndef __EN50221_APPLICATION_RM_H__
#define __EN50221_APPLICATION_RM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>

#define EN50221_APP_RM_RESOURCEID MKRID(1,1,1)

/**
 * Type definition for profile_enq callback function - called when we receive
 * a profile_enq from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_rm_enq_callback) (void *arg,
					    uint8_t slot_id,
					    uint16_t session_number);

/**
 * Type definition for profile_reply callback function - called when we receive
 * a profile_reply from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param resource_id_count Number of resource_ids.
 * @param resource_ids The resource ids themselves.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_rm_reply_callback) (void *arg,
					      uint8_t slot_id,
					      uint16_t session_number,
					      uint32_t resource_id_count,
					      uint32_t *resource_ids);
/**
 * Type definition for profile_changed callback function - called when we receive
 * a profile_changed from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_rm_changed_callback) (void *arg,
						uint8_t slot_id,
						uint16_t session_number);



/**
 * Opaque type representing a resource manager.
 */
struct en50221_app_rm;

/**
 * Create an instance of the resource manager.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_rm *en50221_app_rm_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of the resource manager.
 *
 * @param rm Instance to destroy.
 */
extern void en50221_app_rm_destroy(struct en50221_app_rm *rm);

/**
 * Register the callback for when we receive a profile_enq from a CAM.
 *
 * @param rm Resource manager instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_rm_register_enq_callback(struct en50221_app_rm *rm,
						 en50221_app_rm_enq_callback callback,
						 void *arg);

/**
 * Register the callback for when we receive a profile_reply from a CAM.
 *
 * @param rm Resource manager instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_rm_register_reply_callback(struct en50221_app_rm *rm,
						   en50221_app_rm_reply_callback callback,
						   void *arg);

/**
 * Register the callback for when we receive a profile_changed from a CAM.
 *
 * @param rm Resource manager instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_rm_register_changed_callback(struct en50221_app_rm *rm,
						     en50221_app_rm_changed_callback callback,
						     void *arg);

/**
 * Send a profile_enq to a CAM.
 *
 * @param rm Resource manager resource instance.
 * @param session_number Session number to send it on.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_rm_enq(struct en50221_app_rm *rm, uint16_t session_number);

/**
 * Send a profile_reply to a CAM.
 *
 * @param rm Resource manager resource instance.
 * @param session_number Session number to send it on.
 * @param resource_id_count Number of resource ids.
 * @param resource_ids The resource IDs themselves
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_rm_reply(struct en50221_app_rm *rm,
				uint16_t session_number,
				uint32_t resource_id_count,
				uint32_t * resource_ids);

/**
 * Send a profile_changed to a CAM.
 *
 * @param rm Resource manager resource instance.
 * @param session_number Session number to send it on.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_rm_changed(struct en50221_app_rm *rm, uint16_t session_number);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param rm rm instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_rm_message(struct en50221_app_rm *rm,
				  uint8_t slot_id,
				  uint16_t session_number,
				  uint32_t resource_id,
				  uint8_t *data,
				  uint32_t data_length);

#ifdef __cplusplus
}
#endif
#endif
