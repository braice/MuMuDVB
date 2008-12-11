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

#ifndef __EN50221_APPLICATION_DVB_H__
#define __EN50221_APPLICATION_DVB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>

#define EN50221_APP_DVB_RESOURCEID MKRID(32,1,1)


/**
 * Type definition for tune - called when we receive a tune request from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param network_id Network id requested by CAM.
 * @param original_network_id Original Network id requested by CAM.
 * @param transport_stream_id Transport stream id requested by CAM.
 * @param service_id Service id requested by CAM.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_dvb_tune_callback) (void *arg,
					      uint8_t slot_id,
					      uint16_t session_number,
					      uint16_t network_id,
					      uint32_t original_network_id,
					      uint16_t transport_stream_id,
					      uint16_t service_id);

/**
 * Type definition for replace - called when we receive a replace request from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param replacement_ref Replacement ref.
 * @param replaced_pid PID to replace.
 * @param replacement_pid PID to replace it with.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_dvb_replace_callback) (void *arg,
						 uint8_t slot_id,
						 uint16_t session_number,
						 uint8_t replacement_ref,
						 uint16_t replaced_pid,
						 uint16_t replacement_pid);


/**
 * Type definition for clear_replace - called when we receive a clear_replace request from a CAM.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param replacement_ref Replacement ref.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_dvb_clear_replace_callback) (void *arg,
						       uint8_t slot_id,
						       uint16_t session_number,
						       uint8_t replacement_ref);


/**
 * Opaque type representing a dvb resource.
 */
struct en50221_app_dvb;

/**
 * Create an instance of the dvb resource.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_dvb *en50221_app_dvb_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of the dvb resource.
 *
 * @param dvb Instance to destroy.
 */
extern void en50221_app_dvb_destroy(struct en50221_app_dvb *dvb);

/**
 * Register the callback for when we receive a tune request.
 *
 * @param dvb DVB resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_dvb_register_tune_callback(struct en50221_app_dvb *dvb,
						   en50221_app_dvb_tune_callback callback,
						   void *arg);

/**
 * Register the callback for when we receive a replace request.
 *
 * @param dvb DVB resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_dvb_register_replace_callback(struct en50221_app_dvb *dvb,
						      en50221_app_dvb_replace_callback callback,
						      void *arg);

/**
 * Register the callback for when we receive a clear replace request.
 *
 * @param dvb DVB resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_dvb_register_clear_replace_callback(struct en50221_app_dvb *dvb,
							    en50221_app_dvb_clear_replace_callback callback,
							    void *arg);

/**
 * Send an ask release request to the CAM.
 *
 * @param dvb DVB resource instance.
 * @param session_number Session number to send it on.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_dvb_ask_release(struct en50221_app_dvb *dvb,
				       uint16_t session_number);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param dvb dvb instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_dvb_message(struct en50221_app_dvb *dvb,
				   uint8_t slot_id,
				   uint16_t session_number,
				   uint32_t resource_id,
				   uint8_t *data,
				   uint32_t data_length);

#ifdef __cplusplus
}
#endif
#endif
