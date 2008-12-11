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

#ifndef __EN50221_APPLICATION_DATETIME_H__
#define __EN50221_APPLICATION_DATETIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>

#define EN50221_APP_DATETIME_RESOURCEID MKRID(36,1,1)

/**
 * Type definition for enquiry - called when we receive a date/time enquiry from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param response_interval Response interval requested by CAM.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_datetime_enquiry_callback) (void *arg,
						      uint8_t slot_id,
						      uint16_t session_number,
						      uint8_t response_interval);

/**
 * Opaque type representing a datetime resource.
 */
struct en50221_app_datetime;

/**
 * Create an instance of the datetime resource.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_datetime
	*en50221_app_datetime_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of the datetime resource.
 *
 * @param datetime Instance to destroy.
 */
extern void en50221_app_datetime_destroy(struct en50221_app_datetime *datetime);

/**
 * Register the callback for when we receive a enquiry request.
 *
 * @param datetime datetime resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_datetime_register_enquiry_callback(struct en50221_app_datetime *datetime,
							   en50221_app_datetime_enquiry_callback callback,
							   void *arg);

/**
 * Send the time to the CAM.
 *
 * @param datetime datetime resource instance.
 * @param session_number Session number to send it on.
 * @param utc_time UTC time in unix time format.
 * @param time_offset If -1, the field will not be transmitted, otherwise it is the offset between
 * UTC and local time in minutes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_datetime_send(struct en50221_app_datetime *datetime,
				     uint16_t session_number,
				     time_t utc_time,
				     int time_offset);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param datetime datetime instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_datetime_message(struct en50221_app_datetime *datetime,
					uint8_t slot_id,
					uint16_t session_number,
					uint32_t resource_id,
					uint8_t *data,
					uint32_t data_length);

#ifdef __cplusplus
}
#endif
#endif
