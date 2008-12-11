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

#ifndef __EN50221_APPLICATION_LOWSPEED_H__
#define __EN50221_APPLICATION_LOWSPEED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <libdvben50221/en50221_app_utils.h>
#include <libucsi/dvb/descriptor.h>

#define COMMS_COMMAND_ID_CONNECT_ON_CHANNEL     0x01
#define COMMS_COMMAND_ID_DISCONNECT_ON_CHANNEL  0x02
#define COMMS_COMMAND_ID_SET_PARAMS             0x03
#define COMMS_COMMAND_ID_ENQUIRE_STATUS         0x04
#define COMMS_COMMAND_ID_GET_NEXT_BUFFER        0x05

#define CONNECTION_DESCRIPTOR_TYPE_TELEPHONE    0x01
#define CONNECTION_DESCRIPTOR_TYPE_CABLE        0x02

#define COMMS_REPLY_ID_CONNECT_ACK              0x01
#define COMMS_REPLY_ID_DISCONNECT_ACK           0x02
#define COMMS_REPLY_ID_SET_PARAMS_ACK           0x03
#define COMMS_REPLY_ID_STATUS_REPLY             0x04
#define COMMS_REPLY_ID_GET_NEXT_BUFFER_ACK      0x05
#define COMMS_REPLY_ID_SEND_ACK                 0x06

#define EN50221_APP_LOWSPEED_RESOURCEID(DEVICE_TYPE, DEVICE_NUMBER) MKRID(96,((DEVICE_TYPE)<<2)|((DEVICE_NUMBER) & 0x03),1)


/**
 * Structure holding information on a received comms command.
 */
struct en50221_app_lowspeed_command {
	union {
		struct {
			uint8_t descriptor_type;	// CONNECTION_DESCRIPTOR_TYPE_*
			uint8_t retry_count;
			uint8_t timeout;
			union {
				struct dvb_telephone_descriptor *telephone;
				uint8_t cable_channel_id;
			} descriptor;
		} connect_on_channel;

		struct {
			uint8_t buffer_size;
			uint8_t timeout;
		} set_params;

		struct {
			uint8_t phase_id;
		} get_next_buffer;
	} u;
};

/**
 * Type definition for command - called when we receive a comms command.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param command_id One of the COMMS_COMMAND_ID_* values
 * @param command Pointer to a lowspeed command structure containing the command data.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_lowspeed_command_callback) (void *arg,
						      uint8_t slot_id,
						      uint16_t session_number,
						      uint8_t command_id,
						      struct en50221_app_lowspeed_command *command);

/**
 * Type definition for send - called when we receive data to send. The block can be segmented into
 * multiple pieces - last_more indicates the details of this.
 *
 * @param arg Private argument.
 * @param slot_id Slot id concerned.
 * @param session_number Session number concerned.
 * @param phase_id Comms phase id.
 * @param data The data.
 * @param length Number of bytes.
 * @return 0 on success, -1 on failure.
 */
typedef int (*en50221_app_lowspeed_send_callback) (void *arg,
						   uint8_t slot_id,
						   uint16_t session_number,
						   uint8_t phase_id,
						   uint8_t *data,
						   uint32_t length);

/**
 * Opaque type representing a lowspeed resource.
 */
struct en50221_app_lowspeed;

/**
 * Create an instance of the lowspeed resource.
 *
 * @param funcs Send functions to use.
 * @return Instance, or NULL on failure.
 */
extern struct en50221_app_lowspeed *
	en50221_app_lowspeed_create(struct en50221_app_send_functions *funcs);

/**
 * Destroy an instance of the lowspeed resource.
 *
 * @param lowspeed Instance to destroy.
 */
extern void en50221_app_lowspeed_destroy(struct en50221_app_lowspeed *lowspeed);

/**
 * Informs the lowspeed object that a session to it has been closed - cleans up internal state.
 *
 * @param lowspeed lowspeed resource instance.
 * @param session_number The session concerned.
 */
extern void en50221_app_lowspeed_clear_session(struct en50221_app_lowspeed *lowspeed,
					       uint16_t session_number);

/**
 * Register the callback for when we receive a comms command.
 *
 * @param lowspeed lowspeed resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_lowspeed_register_command_callback(struct en50221_app_lowspeed *lowspeed,
							   en50221_app_lowspeed_command_callback callback,
							   void *arg);

/**
 * Register the callback for when we receive data to send.
 *
 * @param lowspeed lowspeed resource instance.
 * @param callback The callback. Set to NULL to remove the callback completely.
 * @param arg Private data passed as arg0 of the callback.
 */
extern void en50221_app_lowspeed_register_send_callback(struct en50221_app_lowspeed *lowspeed,
							en50221_app_lowspeed_send_callback callback,
							void *arg);

/**
 * Send a comms reply to the CAM.
 *
 * @param lowspeed lowspeed resource instance.
 * @param session_number Session number to send it on.
 * @param comms_reply_id One of the COMMS_REPLY_ID_* values.
 * @param return_value Comms reply specific value.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_lowspeed_send_comms_reply(struct en50221_app_lowspeed *lowspeed,
						 uint16_t session_number,
						 uint8_t comms_reply_id,
						 uint8_t return_value);

/**
 * Send received data to the CAM.
 *
 * @param lowspeed lowspeed resource instance.
 * @param session_number Session number to send it on.
 * @param phase_id Comms phase id.
 * @param tx_data_length Length of data in bytes (max 254 bytes as per spec).
 * @param tx_data Data.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_lowspeed_send_comms_data(struct en50221_app_lowspeed *lowspeed,
						uint16_t session_number,
						uint8_t phase_id,
						uint32_t tx_data_length,
						uint8_t * tx_data);

/**
 * Pass data received for this resource into it for parsing.
 *
 * @param lowspeed lowspeed instance.
 * @param slot_id Slot ID concerned.
 * @param session_number Session number concerned.
 * @param resource_id Resource ID concerned.
 * @param data The data.
 * @param data_length Length of data in bytes.
 * @return 0 on success, -1 on failure.
 */
extern int en50221_app_lowspeed_message(struct en50221_app_lowspeed *lowspeed,
					uint8_t slot_id,
					uint16_t session_number,
					uint32_t resource_id,
					uint8_t * data,
					uint32_t data_length);

#ifdef __cplusplus
}
#endif
#endif
