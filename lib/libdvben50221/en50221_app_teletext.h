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

#ifndef __EN50221_APPLICATION_teletext_H__
#define __EN50221_APPLICATION_teletext_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>

#define EN50221_APP_TELETEXT_RESOURCEID MKRID(128, 1, 1)


/**
 * Type definition for request - called when we receive teletext from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param teletext_data Data for the request.
 * @param teletext_data_lenghth Number of bytes.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_teletext_callback) (void *arg,
					      uint8_t slot_id,
					      uint16_t session_number,
					      uint8_t *teletext_data,
					      uint32_t teletext_data_length);

/**
 * Opaque type representing a teletext resource.
 */
struct en50221_app_teletext;

/**
 * Create an instance of the teletext resource.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_teletext *
	en50221_app_teletext_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of the teletext resource.
 *
 * @param teletext Instance to destroy.
 */
extern void en50221_app_teletext_destroy(struct en50221_app_teletext *teletext);

/**
 * Register the callback for when we receive a request.
 *
 * @param teletext teletext resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_teletext_register_callback(struct en50221_app_teletext *teletext,
						   en50221_app_teletext_callback callback,
						   void *arg);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param teletext teletext instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_teletext_message(struct en50221_app_teletext *teletext,
					uint8_t slot_id,
					uint16_t session_number,
					uint32_t resource_id,
					uint8_t * data,
					uint32_t data_length);

#ifdef __cplusplus
}
#endif
#endif
