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

#include <string.h>
#include <libdvbmisc/dvbmisc.h>
#include <pthread.h>
#include "en50221_app_auth.h"
#include "en50221_app_tags.h"
#include "asn_1.h"

struct en50221_app_auth {
	struct en50221_app_send_functions *funcs;

	en50221_app_auth_request_callback callback;
	void *callback_arg;

	pthread_mutex_t lock;
};

static int en50221_app_auth_parse_request(struct en50221_app_auth *private,
					  uint8_t slot_id,
					  uint16_t session_number,
					  uint8_t * data,
					  uint32_t data_length);


struct en50221_app_auth *en50221_app_auth_create(struct en50221_app_send_functions *funcs)
{
	struct en50221_app_auth *auth = NULL;

	// create structure and set it up
	auth = malloc(sizeof(struct en50221_app_auth));
	if (auth == NULL) {
		return NULL;
	}
	auth->funcs = funcs;
	auth->callback = NULL;

	pthread_mutex_init(&auth->lock, NULL);

	// done
	return auth;
}

void en50221_app_auth_destroy(struct en50221_app_auth *auth)
{
	pthread_mutex_destroy(&auth->lock);
	free(auth);
}

void en50221_app_auth_register_request_callback(struct en50221_app_auth *auth,
						en50221_app_auth_request_callback callback, void *arg)
{
	pthread_mutex_lock(&auth->lock);
	auth->callback = callback;
	auth->callback_arg = arg;
	pthread_mutex_unlock(&auth->lock);
}

int en50221_app_auth_send(struct en50221_app_auth *auth,
			  uint16_t session_number,
			  uint16_t auth_protocol_id, uint8_t * auth_data,
			  uint32_t auth_data_length)
{
	uint8_t buf[10];

	// the header
	buf[0] = (TAG_AUTH_RESP >> 16) & 0xFF;
	buf[1] = (TAG_AUTH_RESP >> 8) & 0xFF;
	buf[2] = TAG_AUTH_RESP & 0xFF;

	// encode the length field
	int length_field_len;
	if ((length_field_len = asn_1_encode(auth_data_length + 2, buf + 3, 3)) < 0) {
		return -1;
	}
	// the phase_id
	buf[3 + length_field_len] = auth_protocol_id >> 8;
	buf[3 + length_field_len + 1] = auth_protocol_id;

	// build the iovecs
	struct iovec iov[2];
	iov[0].iov_base = buf;
	iov[0].iov_len = 3 + length_field_len + 2;
	iov[1].iov_base = auth_data;
	iov[1].iov_len = auth_data_length;

	// sendit
	return auth->funcs->send_datav(auth->funcs->arg, session_number,
				       iov, 2);
}

int en50221_app_auth_message(struct en50221_app_auth *auth,
			     uint8_t slot_id,
			     uint16_t session_number,
			     uint32_t resource_id,
			     uint8_t * data, uint32_t data_length)
{
	(void) resource_id;

	// get the tag
	if (data_length < 3) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	uint32_t tag = (data[0] << 16) | (data[1] << 8) | data[2];

	switch (tag) {
	case TAG_AUTH_REQ:
		return en50221_app_auth_parse_request(auth, slot_id,
						      session_number,
						      data + 3,
						      data_length - 3);
	}

	print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
	return -1;
}



static int en50221_app_auth_parse_request(struct en50221_app_auth *auth,
					  uint8_t slot_id,
					  uint16_t session_number,
					  uint8_t * data,
					  uint32_t data_length)
{
	// first of all, decode the length field
	uint16_t asn_data_length;
	int length_field_len;
	if ((length_field_len = asn_1_decode(&asn_data_length, data, data_length)) < 0) {
		print(LOG_LEVEL, ERROR, 1, "ASN.1 decode error\n");
		return -1;
	}
	// check it
	if (asn_data_length < 2) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	if (asn_data_length > (data_length - length_field_len)) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	uint8_t *auth_data = data + length_field_len;

	// process it
	uint16_t auth_protocol_id = (auth_data[0] << 8) | auth_data[1];

	// tell the app
	pthread_mutex_lock(&auth->lock);
	en50221_app_auth_request_callback cb = auth->callback;
	void *cb_arg = auth->callback_arg;
	pthread_mutex_unlock(&auth->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number,
			  auth_protocol_id, auth_data + 2,
			  asn_data_length - 2);
	}
	return 0;
}
