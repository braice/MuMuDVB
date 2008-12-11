/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 session layer

    Copyright (C) 2004, 2005 Manu Abraham <abraham.manu@gmail.com>
    Copyright (C) 2005 Julian Scheel (julian@jusst.de)
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


#ifndef __EN50221_SESSION_H__
#define __EN50221_SESSION_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_transport.h>

#define S_SCALLBACK_REASON_CAMCONNECTING  0x00	// CAM originated session connecting to resource (check for availability)
#define S_SCALLBACK_REASON_CAMCONNECTED   0x01	// CAM originated session connection established succesfully
#define S_SCALLBACK_REASON_CAMCONNECTFAIL 0x02	// CAM originated session connection failed
#define S_SCALLBACK_REASON_CONNECTED      0x03	// Host originated session ACKed by CAM.
#define S_SCALLBACK_REASON_CONNECTFAIL    0x04	// Host originated session NACKed by CAM.
#define S_SCALLBACK_REASON_CLOSE          0x05	// Session closed
#define S_SCALLBACK_REASON_TC_CONNECT     0x06	// A host originated transport connection has been established.
#define S_SCALLBACK_REASON_TC_CAMCONNECT  0x07	// A CAM originated transport connection has been established.


/**
 * Opaque type representing a session layer.
 */
struct en50221_session_layer;

/**
 * Type definition for resource callback function - called by session layer when data
 * arrives for a particular resource.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number.
 * @param resource_id Resource id.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, or -1 on failure.
 */
typedef int (*en50221_sl_resource_callback) (void *arg,
					     uint8_t slot_id,
					     uint16_t session_number,
					     uint32_t resource_id,
					     uint8_t * data,
					     uint32_t data_length);

/**
 * Type definition for resource lookup callback function - used by the session layer to
 * look up requested resources.
 *
 * @param arg Private argument.
 * @param slot_id Slot id the request came from.
 * @param requested_resource_id Resource id requested.
 * @param callback_out Output parameter for pointer to resource callback function.
 * @param arg_out Output parameter for arg to pass to resource callback.
 * @param resource_id_out Set this to the resource_id connected to (e.g. may differ from resource_id due to versions).
 * @return 0 on success,
 * -1 if the resource was not found,
 * -2 if it exists, but had a lower version, or
 * -3 if it exists, but was unavailable.
 */
typedef int (*en50221_sl_lookup_callback) (void *arg,
					   uint8_t slot_id,
					   uint32_t requested_resource_id,
					   en50221_sl_resource_callback * callback_out,
					   void **arg_out,
					   uint32_t *resource_id_out);


/**
 * Type definition for session callback function - used to inform top level code when a CAM
 * modifies a session to a resource.
 *
 * @param arg Private argument.
 * @param reason One of the S_CCALLBACK_REASON_* values above.
 * @param slot_id Slot id concerned.
 * @param session_number Session number.
 * @param resource_id Resource id.
 * @return 0 on sucess, or -1 on error.
 */
typedef int (*en50221_sl_session_callback) (void *arg, int reason,
					    uint8_t slot_id,
					    uint16_t session_number,
					    uint32_t resource_id);

/**
 * Construct a new instance of the session layer.
 *
 * @param tl The en50221_transport_layer instance to use.
 * @param max_sessions Maximum number of sessions supported.
 * @return The en50221_session_layer instance, or NULL on error.
 */
extern struct en50221_session_layer *en50221_sl_create(struct en50221_transport_layer *tl,
						       uint32_t max_sessions);

/**
 * Destroy an instance of the session layer.
 *
 * @param tl The en50221_session_layer instance.
 */
extern void en50221_sl_destroy(struct en50221_session_layer *sl);

/**
 * Gets the last error.
 *
 * @param tl The en50221_session_layer instance.
 * @return One of the EN50221ERR_* values.
 */
extern int en50221_sl_get_error(struct en50221_session_layer *tl);

/**
 * Register the callback for resource lookup.
 *
 * @param sl The en50221_session_layer instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_sl_register_lookup_callback(struct en50221_session_layer *sl,
						en50221_sl_lookup_callback callback,
						void *arg);

/**
 * Register the callback for informing about session from a cam.
 *
 * @param sl The en50221_session_layer instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_sl_register_session_callback(struct en50221_session_layer *sl,
						 en50221_sl_session_callback callback,
						 void *arg);

/**
 * Create a new session to a module in a slot.
 *
 * @param sl The en50221_session_layer instance.
 * @param slot The slot to connect to.
 * @param resource_id The resource_id to connect to.
 * @param callback The callback for received data.
 * @param arg Argument to pass to the callback.
 * @return The new session_number, or -1 on error.
 */
extern int en50221_sl_create_session(struct en50221_session_layer *sl, int slot_id,
				     uint8_t connection_id,
				     uint32_t resource_id,
				     en50221_sl_resource_callback callback,
				     void *arg);

/**
 * Destroy a session.
 *
 * @param sl The en50221_session_layer instance.
 * @param session_number The session to destroy.
 * @return 0 on success, or -1 on error.
 */
extern int en50221_sl_destroy_session(struct en50221_session_layer *sl,
				      uint16_t session_number);

/**
 * this function is used to take a data-block, pack into
 * into a SPDU (SESSION_NUMBER) and send it to the transport layer
 *
 * @param sl The en50221_session_layer instance to use.
 * @param session_number Session number concerned.
 * @param data Data to send.
 * @param data_length Length of data in bytes.
 * @return 0 on success, or -1 on error.
 */
extern int en50221_sl_send_data(struct en50221_session_layer *sl,
				uint16_t session_number,
				uint8_t * data,
				uint16_t data_length);

/**
 * this function is used to take a data-block, pack into
 * into a SPDU (SESSION_NUMBER) and send it to the transport layer
 *
 * @param sl The en50221_session_layer instance to use.
 * @param session_number Session number concerned.
 * @param vector IOVEC to send.
 * @param iov_count Number of elements in io vector.
 * @return 0 on success, or -1 on error.
 */
extern int en50221_sl_send_datav(struct en50221_session_layer *sl,
				 uint16_t session_number,
				 struct iovec *vector,
				 int iov_count);

/**
 * this is used to send a message to all sessions, linked
 * to resource res
 *
 * @param tl The en50221_session_layer instance to use.
 * @param slot_id Set to -1 to send to any slot. Other values will send to only that slot.
 * @param resource_id Resource id concerned.
 * @param data Data to send.
 * @param data_length Length of data in bytes.
 * @return 0 on success, or -1 on error.
 */
extern int en50221_sl_broadcast_data(struct en50221_session_layer *sl,
				     int slot_id,
				     uint32_t resource_id,
				     uint8_t * data,
				     uint16_t data_length);

#ifdef __cplusplus
}
#endif
#endif
