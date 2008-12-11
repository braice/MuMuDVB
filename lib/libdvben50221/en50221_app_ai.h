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

#ifndef __EN50221_APPLICATION_AI_H__
#define __EN50221_APPLICATION_AI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>

#define EN50221_APP_AI_RESOURCEID MKRID(2,1,1)

#define APPLICATION_TYPE_CA 0x01
#define APPLICATION_TYPE_EPG 0x02

/**
 * Type definition for application callback function - called when we receive
 * an application info object.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Resource id concerned.
 * @param application_type Type of application.
 * @param application_manufacturer Manufacturer of application.
 * @param manufacturer_code Manufacturer specific code.
 * @param menu_string_length Length of menu string.
 * @param menu_string The menu string itself.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_ai_callback) (void *arg,
					uint8_t slot_id,
					uint16_t session_number,
					uint8_t application_type,
					uint16_t application_manufacturer,
					uint16_t manufacturer_code,
					uint8_t menu_string_length,
					uint8_t * menu_string);

/**
 * Opaque type representing an application information resource.
 */
struct en50221_app_ai;

/**
 * Create an instance of an application information resource.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_ai *en50221_app_ai_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of an application information resource.
 *
 * @param ai Instance to destroy.
 */
extern void en50221_app_ai_destroy(struct en50221_app_ai *ai);

/**
 * Register a callback for reception of application_info objects.
 *
 * @param ai Application information instance.
 * @param callback Callback function.
 * @param arg Private argument passed during calls to the callback.
 */
extern void en50221_app_ai_register_callback(struct en50221_app_ai *ai,
					     en50221_app_ai_callback,
					     void *arg);

/**
 * send a enquiry for the app_info provided by a module
 *
 * @param ai Application information instance.
 * @param session_number Session to send on.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_ai_enquiry(struct en50221_app_ai *ai,
				  uint16_t session_number);

/**
 * send a enter_menu tag, this will make the application
 * open a new MMI session to provide a Menu, or so.
 *
 * @param ai Application information instance.
 * @param session_number Session to send on.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_ai_entermenu(struct en50221_app_ai *ai,
				    uint16_t session_number);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param ai Application information instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_ai_message(struct en50221_app_ai *ai,
				  uint8_t slot_id,
				  uint16_t session_number,
				  uint32_t resource_id,
				  uint8_t *data,
				  uint32_t data_length);

#ifdef __cplusplus
}
#endif
#endif
