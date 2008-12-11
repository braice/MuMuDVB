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
#include <libucsi/endianops.h>
#include "en50221_app_rm.h"
#include "en50221_app_tags.h"
#include "asn_1.h"

struct en50221_app_rm {
	struct en50221_app_send_functions *funcs;

	en50221_app_rm_enq_callback enqcallback;
	void *enqcallback_arg;

	en50221_app_rm_reply_callback replycallback;
	void *replycallback_arg;

	en50221_app_rm_changed_callback changedcallback;
	void *changedcallback_arg;

	pthread_mutex_t lock;
};

static int en50221_app_rm_parse_profile_enq(struct en50221_app_rm *rm,
					    uint8_t slot_id,
					    uint16_t session_number,
					    uint8_t * data,
					    uint32_t data_length);
static int en50221_app_rm_parse_profile_reply(struct en50221_app_rm *rm,
					      uint8_t slot_id,
					      uint16_t session_number,
					      uint8_t * data,
					      uint32_t data_length);
static int en50221_app_rm_parse_profile_change(struct en50221_app_rm *rm,
					       uint8_t slot_id,
					       uint16_t session_number,
					       uint8_t * data,
					       uint32_t data_length);


struct en50221_app_rm *en50221_app_rm_create(struct
					     en50221_app_send_functions
					     *funcs)
{
	struct en50221_app_rm *rm = NULL;

	// create structure and set it up
	rm = malloc(sizeof(struct en50221_app_rm));
	if (rm == NULL) {
		return NULL;
	}
	rm->funcs = funcs;
	rm->enqcallback = NULL;
	rm->replycallback = NULL;
	rm->changedcallback = NULL;

	pthread_mutex_init(&rm->lock, NULL);

	// done
	return rm;
}

void en50221_app_rm_destroy(struct en50221_app_rm *rm)
{
	pthread_mutex_destroy(&rm->lock);
	free(rm);
}

void en50221_app_rm_register_enq_callback(struct en50221_app_rm *rm,
					  en50221_app_rm_enq_callback
					  callback, void *arg)
{
	pthread_mutex_lock(&rm->lock);
	rm->enqcallback = callback;
	rm->enqcallback_arg = arg;
	pthread_mutex_unlock(&rm->lock);
}

void en50221_app_rm_register_reply_callback(struct en50221_app_rm *rm,
					    en50221_app_rm_reply_callback
					    callback, void *arg)
{
	pthread_mutex_lock(&rm->lock);
	rm->replycallback = callback;
	rm->replycallback_arg = arg;
	pthread_mutex_unlock(&rm->lock);
}

void en50221_app_rm_register_changed_callback(struct en50221_app_rm *rm,
					      en50221_app_rm_changed_callback
					      callback, void *arg)
{
	pthread_mutex_lock(&rm->lock);
	rm->changedcallback = callback;
	rm->changedcallback_arg = arg;
	pthread_mutex_unlock(&rm->lock);
}

int en50221_app_rm_enq(struct en50221_app_rm *rm, uint16_t session_number)
{
	uint8_t buf[4];

	// set up the tag
	buf[0] = (TAG_PROFILE_ENQUIRY >> 16) & 0xFF;
	buf[1] = (TAG_PROFILE_ENQUIRY >> 8) & 0xFF;
	buf[2] = TAG_PROFILE_ENQUIRY & 0xFF;
	buf[3] = 0;

	// create the data and send it
	return rm->funcs->send_data(rm->funcs->arg, session_number, buf, 4);
}

int en50221_app_rm_reply(struct en50221_app_rm *rm,
			 uint16_t session_number,
			 uint32_t resource_id_count,
			 uint32_t * resource_ids)
{
	uint8_t buf[10];

	// set up the tag
	buf[0] = (TAG_PROFILE >> 16) & 0xFF;
	buf[1] = (TAG_PROFILE >> 8) & 0xFF;
	buf[2] = TAG_PROFILE & 0xFF;

	// encode the length field
	int length_field_len;
	if ((length_field_len = asn_1_encode(resource_id_count * 4, buf + 3, 3)) < 0) {
		return -1;
	}
	// copy the data and byteswap it
	uint32_t *copy_resource_ids = alloca(4 * resource_id_count);
	if (copy_resource_ids == NULL) {
		return -1;
	}
	uint8_t *data = (uint8_t *) copy_resource_ids;
	memcpy(data, resource_ids, resource_id_count * 4);
	uint32_t i;
	for (i = 0; i < resource_id_count; i++) {
		bswap32(data);
		data += 4;
	}

	// build the iovecs
	struct iovec iov[2];
	iov[0].iov_base = buf;
	iov[0].iov_len = 3 + length_field_len;
	iov[1].iov_base = (uint8_t *) copy_resource_ids;
	iov[1].iov_len = resource_id_count * 4;

	// create the data and send it
	return rm->funcs->send_datav(rm->funcs->arg, session_number, iov, 2);
}

int en50221_app_rm_changed(struct en50221_app_rm *rm,
			   uint16_t session_number)
{
	uint8_t buf[4];

	// set up the tag
	buf[0] = (TAG_PROFILE_CHANGE >> 16) & 0xFF;
	buf[1] = (TAG_PROFILE_CHANGE >> 8) & 0xFF;
	buf[2] = TAG_PROFILE_CHANGE & 0xFF;
	buf[3] = 0;

	// create the data and send it
	return rm->funcs->send_data(rm->funcs->arg, session_number, buf, 4);
}

int en50221_app_rm_message(struct en50221_app_rm *rm,
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

	// dispatch it
	switch (tag) {
	case TAG_PROFILE_ENQUIRY:
		return en50221_app_rm_parse_profile_enq(rm, slot_id,
							session_number,
							data + 3,
							data_length - 3);
	case TAG_PROFILE:
		return en50221_app_rm_parse_profile_reply(rm, slot_id,
							  session_number,
							  data + 3,
							  data_length - 3);
	case TAG_PROFILE_CHANGE:
		return en50221_app_rm_parse_profile_change(rm, slot_id,
							   session_number,
							   data + 3,
							   data_length - 3);
	}

	print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
	return -1;
}


static int en50221_app_rm_parse_profile_enq(struct en50221_app_rm *rm,
					    uint8_t slot_id,
					    uint16_t session_number,
					    uint8_t * data,
					    uint32_t data_length)
{
	(void) data;
	(void) data_length;

	pthread_mutex_lock(&rm->lock);
	en50221_app_rm_enq_callback cb = rm->enqcallback;
	void *cb_arg = rm->enqcallback_arg;
	pthread_mutex_unlock(&rm->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number);
	}
	return 0;
}

static int en50221_app_rm_parse_profile_reply(struct en50221_app_rm *rm,
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
	if (asn_data_length > (data_length - length_field_len)) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	uint32_t resources_count = asn_data_length / 4;
	uint32_t *resource_ids = (uint32_t *) (data + length_field_len);
	data += length_field_len;

	// byteswap it
	uint32_t i;
	for (i = 0; i < resources_count; i++) {
		bswap32(data);
		data += 4;
	}

	// inform observer
	pthread_mutex_lock(&rm->lock);
	en50221_app_rm_reply_callback cb = rm->replycallback;
	void *cb_arg = rm->replycallback_arg;
	pthread_mutex_unlock(&rm->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number, resources_count, resource_ids);
	}
	return 0;
}

static int en50221_app_rm_parse_profile_change(struct en50221_app_rm *rm,
					       uint8_t slot_id,
					       uint16_t session_number,
					       uint8_t * data,
					       uint32_t data_length)
{
	(void) data;
	(void) data_length;

	pthread_mutex_lock(&rm->lock);
	en50221_app_rm_changed_callback cb = rm->changedcallback;
	void *cb_arg = rm->changedcallback_arg;
	pthread_mutex_unlock(&rm->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number);
	}
	return 0;
}
