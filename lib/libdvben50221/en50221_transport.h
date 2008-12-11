/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 session layer

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


#ifndef __EN50221_TRANSPORT_H__
#define __EN50221_TRANSPORT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <sys/uio.h>

/**
 * Callback reasons.
 */
#define T_CALLBACK_REASON_CONNECTIONOPEN       0x00	// A connection we opened _to_ the cam has been ACKed
#define T_CALLBACK_REASON_CAMCONNECTIONOPEN    0x01	// The cam has opened a connection to _us_.
#define T_CALLBACK_REASON_DATA                 0x02	// Data received
#define T_CALLBACK_REASON_CONNECTIONCLOSE      0x03	// The cam has told us to close a connection.
#define T_CALLBACK_REASON_SLOTCLOSE            0x04	// The cam in the supplied slot id has been removed.

// these are the states a TC can be in
#define T_STATE_IDLE            0x01	// this transport connection is not in use
#define T_STATE_ACTIVE          0x02	// this transport connection is in use
#define T_STATE_ACTIVE_DELETEQUEUED 0x04	// this transport connection is about to be deleted
#define T_STATE_IN_CREATION     0x08	// this transport waits for a T_C_T_C_REPLY to become active
#define T_STATE_IN_DELETION     0x10	// this transport waits for T_D_T_C_REPLY to become idle again

/**
 * Opaque type representing a transport layer.
 */
struct en50221_transport_layer;

/**
 * Type definition for callback function - used when events are received from a module.
 *
 * **IMPORTANT** For all callback reasons except T_CALLBACK_REASON_DATA, an internal lock is held in the
 * transport layer. Therefore, to avoid deadlock, you *must not* call back into the transport layer for
 * these reasons.
 *
 * However, for T_CALLBACK_REASON_DATA, the internal lock is not held, so calling back into the transport
 * layer is fine in this case.
 *
 * @param arg Private data.
 * @param reason One of the T_CALLBACK_REASON_* values.
 * @param data The data.
 * @param data_length Length of the data.
 * @param slot_id Slot_id the data was received from.
 * @param connection_id Connection_id the data was received from.
 */
typedef void (*en50221_tl_callback) (void *arg, int reason,
				     uint8_t * data,
				     uint32_t data_length,
				     uint8_t slot_id,
				     uint8_t connection_id);


/**
 * Construct a new instance of the transport layer.
 *
 * @param max_slots Maximum number of slots to support.
 * @param max_connections_per_slot Maximum connections per slot.
 * @return The en50221_transport_layer instance, or NULL on error.
 */
extern struct en50221_transport_layer *en50221_tl_create(uint8_t max_slots,
							 uint8_t max_connections_per_slot);

/**
 * Destroy an instance of the transport layer.
 *
 * @param tl The en50221_transport_layer instance.
 */
extern void en50221_tl_destroy(struct en50221_transport_layer *tl);

/**
 * Register a new slot with the library.
 *
 * @param tl The en50221_transport_layer instance.
 * @param ca_hndl FD for talking to the slot.
 * @param slot CAM slot where the requested CAM of the CA is in.
 * @param response_timeout Maximum timeout in ms to a response we send before signalling a timeout.
 * @param poll_delay Interval between polls in ms.
 * @return slot_id on sucess, or -1 on error.
 */
extern int en50221_tl_register_slot(struct en50221_transport_layer *tl,
				    int ca_hndl, uint8_t slot,
				    uint32_t response_timeout,
				    uint32_t poll_delay);

/**
 * Destroy a registered slot - e.g. if a CAM is removed, or an error occurs. Does
 * not attempt to reset the CAM.
 *
 * @param tl The en50221_transport_layer instance.
 * @param slot_id Slot to destroy.
 */
extern void en50221_tl_destroy_slot(struct en50221_transport_layer *tl, uint8_t slot_id);

/**
 * Performs one iteration of the transport layer poll -
 * checking for incoming data furthermore it will handle
 * the timeouts of certain commands like T_DELETE_T_C it
 * should be called by the application regularly, generally
 * faster than the poll delay.
 *
 * @param tl The en50221_transport_layer instance.
 * @return 0 on succes, or -1 if there was an error of some sort.
 */
extern int en50221_tl_poll(struct en50221_transport_layer *tl);

/**
 * Register the callback for data reception.
 *
 * @param tl The en50221_transport_layer instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_tl_register_callback(struct en50221_transport_layer *tl,
					 en50221_tl_callback callback, void *arg);

/**
 * Gets the ID of the slot an error occurred on.
 *
 * @param tl The en50221_transport_layer instance.
 * @return The offending slot id.
 */
extern int en50221_tl_get_error_slot(struct en50221_transport_layer *tl);

/**
 * Gets the last error.
 *
 * @param tl The en50221_transport_layer instance.
 * @return One of the EN50221ERR_* values.
 */
extern int en50221_tl_get_error(struct en50221_transport_layer *tl);

/**
 * This function is used to take a data-block, pack into
 * into a TPDU (DATA_LAST) and send it to the device
 *
 * @param tl The en50221_transport_layer instance.
 * @param slot_id ID of the slot.
 * @param connection_id Connection id.
 * @param data Data to send.
 * @param data_length Number of bytes to send.
 * @return 0 on success, or -1 on error.
 */
extern int en50221_tl_send_data(struct en50221_transport_layer *tl,
				uint8_t slot_id,
				uint8_t connection_id,
				uint8_t * data,
				uint32_t data_length);

/**
 * This function is used to take a data-block, pack into
 * into a TPDU (DATA_LAST) and send it to the device
 *
 * @param tl The en50221_transport_layer instance.
 * @param slot_id ID of the slot.
 * @param connection_id Connection id.
 * @param vector iov to send.
 * @param io_count Number of elements in vector.
 * @return 0 on success, or -1 on error.
 */
extern int en50221_tl_send_datav(struct en50221_transport_layer *tl,
				 uint8_t slot_id, uint8_t connection_id,
				 struct iovec *vector, int iov_count);

/**
 * Create a new transport connection to the cam.
 *
 * **IMPORTANT** When this function returns, it means the request to create a connection
 * has been submitted. You will need to poll using en50221_tl_get_connection_state() to find out
 * if/when the connection is established. A callback with T_CALLBACK_REASON_CONNECTIONOPEN reason
 * will also be sent when it is acked by the CAM.
 *
 * @param tl The en50221_transport_layer instance.
 * @param slot_id ID of the slot.
 * @return The allocated connection id on success, or -1 on error.
 */
extern int en50221_tl_new_tc(struct en50221_transport_layer *tl, uint8_t slot_id);

/**
 * Deallocates a transport connection.
 *
 * **IMPORTANT** When this function returns, it means the request to destroy a connection
 * has been submitted. You will need to poll using en50221_tl_get_connection_state() to find out
 * if/when the connection is destroyed.
 *
 * @param tl The en50221_transport_layer instance.
 * @param slot_id ID of the slot.
 * @param connection_id Connection id to send the request _on_.
 * @return 0 on success, or -1 on error.
 */
extern int en50221_tl_del_tc(struct en50221_transport_layer *tl, uint8_t slot_id, uint8_t connection_id);

/**
 * Checks the state of a connection.
 *
 * @param tl The en50221_transport_layer instance.
 * @param slot_id ID of the slot.
 * @param connection_id Connection id to send the request _on_.
 * @return One of the T_STATE_* values.
 */
extern int en50221_tl_get_connection_state(struct en50221_transport_layer *tl,
					   uint8_t slot_id, uint8_t connection_id);

#ifdef __cplusplus
}
#endif
#endif
