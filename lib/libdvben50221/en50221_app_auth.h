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

#ifndef __EN50221_APPLICATION_auth_H__
#define __EN50221_APPLICATION_auth_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>

#define EN50221_APP_AUTH_RESOURCEID MKRID(16,1,1)

/**
 * Type definition for request - called when we receive a auth request from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param auth_protocol_id Auth protocol id.
 * @param auth_data Data for the request.
 * @param auth_data_lenghth Number of bytes.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_auth_request_callback) (void *arg,
						  uint8_t slot_id,
						  uint16_t session_number,
						  uint16_t auth_protcol_id,
						  uint8_t *auth_data,
						  uint32_t auth_data_length);

/**
 * Opaque type representing a auth resource.
 */
struct en50221_app_auth;

/**
 * Create an instance of the auth resource.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_auth *en50221_app_auth_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of the auth resource.
 *
 * @param auth Instance to destroy.
 */
extern void en50221_app_auth_destroy(struct en50221_app_auth *auth);

/**
 * Register the callback for when we receive a request.
 *
 * @param auth auth resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_auth_register_request_callback(struct en50221_app_auth *auth,
						       en50221_app_auth_request_callback callback,
						       void *arg);

/**
 * Send an auth response to the CAM.
 *
 * @param auth auth resource instance.
 * @param session_number Session number to send it on.
 * @param auth_protocol_id Auth protocol id.
 * @param auth_data Auth data.
 * @param auth_data_length Number of bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_auth_send(struct en50221_app_auth *auth,
				 uint16_t session_number,
				 uint16_t auth_protocol_id,
				 uint8_t *auth_data,
				 uint32_t auth_data_length);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param auth Authentication instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_auth_message(struct en50221_app_auth *auth,
				    uint8_t slot_id,
				    uint16_t session_number,
				    uint32_t resource_id,
				    uint8_t *data,
				    uint32_t data_length);

#ifdef __cplusplus
}
#endif
#endif
