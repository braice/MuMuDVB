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
#include "en50221_app_dvb.h"
#include "en50221_app_tags.h"
#include "asn_1.h"

struct en50221_app_dvb {
	struct en50221_app_send_functions *funcs;

	en50221_app_dvb_tune_callback tune_callback;
	void *tune_callback_arg;

	en50221_app_dvb_replace_callback replace_callback;
	void *replace_callback_arg;

	en50221_app_dvb_clear_replace_callback clear_replace_callback;
	void *clear_replace_callback_arg;

	pthread_mutex_t lock;
};

static int en50221_app_dvb_parse_tune(struct en50221_app_dvb *dvb,
				      uint8_t slot_id,
				      uint16_t session_number,
				      uint8_t * data,
				      uint32_t data_length);

static int en50221_app_dvb_parse_replace(struct en50221_app_dvb *dvb,
					 uint8_t slot_id,
					 uint16_t session_number,
					 uint8_t * data,
					 uint32_t data_length);

static int en50221_app_dvb_parse_clear_replace(struct en50221_app_dvb *dvb,
					       uint8_t slot_id,
					       uint16_t session_number,
					       uint8_t * data,
					       uint32_t data_length);



struct en50221_app_dvb *en50221_app_dvb_create(struct en50221_app_send_functions *funcs)
{
	struct en50221_app_dvb *dvb = NULL;

	// create structure and set it up
	dvb = malloc(sizeof(struct en50221_app_dvb));
	if (dvb == NULL) {
		return NULL;
	}
	dvb->funcs = funcs;
	dvb->tune_callback = NULL;
	dvb->replace_callback = NULL;
	dvb->clear_replace_callback = NULL;

	pthread_mutex_init(&dvb->lock, NULL);

	// done
	return dvb;
}

void en50221_app_dvb_destroy(struct en50221_app_dvb *dvb)
{
	pthread_mutex_destroy(&dvb->lock);
	free(dvb);
}

void en50221_app_dvb_register_tune_callback(struct en50221_app_dvb *dvb,
					    en50221_app_dvb_tune_callback callback,
					    void *arg)
{
	pthread_mutex_lock(&dvb->lock);
	dvb->tune_callback = callback;
	dvb->tune_callback_arg = arg;
	pthread_mutex_unlock(&dvb->lock);
}

void en50221_app_dvb_register_replace_callback(struct en50221_app_dvb *dvb,
					       en50221_app_dvb_replace_callback callback,
					       void *arg)
{
	pthread_mutex_lock(&dvb->lock);
	dvb->replace_callback = callback;
	dvb->replace_callback_arg = arg;
	pthread_mutex_unlock(&dvb->lock);
}

void en50221_app_dvb_register_clear_replace_callback(struct en50221_app_dvb *dvb,
						     en50221_app_dvb_clear_replace_callback callback,
						     void *arg)
{
	pthread_mutex_lock(&dvb->lock);
	dvb->clear_replace_callback = callback;
	dvb->clear_replace_callback_arg = arg;
	pthread_mutex_unlock(&dvb->lock);
}

int en50221_app_dvb_ask_release(struct en50221_app_dvb *dvb,
				uint16_t session_number)
{
	uint8_t data[4];

	data[0] = (TAG_ASK_RELEASE >> 16) & 0xFF;
	data[1] = (TAG_ASK_RELEASE >> 8) & 0xFF;
	data[2] = TAG_ASK_RELEASE & 0xFF;
	data[3] = 0;

	return dvb->funcs->send_data(dvb->funcs->arg, session_number, data, 4);
}

int en50221_app_dvb_message(struct en50221_app_dvb *dvb,
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
	case TAG_TUNE:
		return en50221_app_dvb_parse_tune(dvb, slot_id,
						  session_number, data + 3,
						  data_length - 3);
	case TAG_REPLACE:
		return en50221_app_dvb_parse_replace(dvb, slot_id,
						     session_number,
						     data + 3,
						     data_length - 3);
	case TAG_CLEAR_REPLACE:
		return en50221_app_dvb_parse_clear_replace(dvb, slot_id,
							   session_number,
							   data + 3,
							   data_length - 3);
	}

	print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
	return -1;
}










static int en50221_app_dvb_parse_tune(struct en50221_app_dvb *dvb,
				      uint8_t slot_id,
				      uint16_t session_number,
				      uint8_t * data, uint32_t data_length)
{
	// validate data
	if (data_length < 9) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	if (data[0] != 8) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	uint8_t *tune_data = data + 1;

	// parse it
	uint16_t network_id = (tune_data[0] << 8) | tune_data[1];
	uint16_t original_network_id = (tune_data[2] << 8) | tune_data[3];
	uint16_t transport_stream_id = (tune_data[4] << 8) | tune_data[5];
	uint16_t service_id = (tune_data[6] << 8) | tune_data[7];

	// tell the app
	pthread_mutex_lock(&dvb->lock);
	en50221_app_dvb_tune_callback cb = dvb->tune_callback;
	void *cb_arg = dvb->tune_callback_arg;
	pthread_mutex_unlock(&dvb->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number, network_id,
			  original_network_id, transport_stream_id,
			  service_id);
	}
	return 0;
}

static int en50221_app_dvb_parse_replace(struct en50221_app_dvb *dvb,
					 uint8_t slot_id,
					 uint16_t session_number,
					 uint8_t * data,
					 uint32_t data_length)
{
	// validate data
	if (data_length < 6) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	if (data[0] != 5) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	uint8_t *replace_data = data + 1;

	// parse it
	uint8_t replacement_ref = replace_data[0];
	uint16_t replace_pid =
	    ((replace_data[1] & 0x1f) << 8) | replace_data[2];
	uint16_t replacement_pid =
	    ((replace_data[3] & 0x1f) << 8) | replace_data[4];

	// tell the app
	pthread_mutex_lock(&dvb->lock);
	en50221_app_dvb_replace_callback cb = dvb->replace_callback;
	void *cb_arg = dvb->replace_callback_arg;
	pthread_mutex_unlock(&dvb->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number, replacement_ref,
			  replace_pid, replacement_pid);
	}
	return 0;
}

static int en50221_app_dvb_parse_clear_replace(struct en50221_app_dvb *dvb,
					       uint8_t slot_id,
					       uint16_t session_number,
					       uint8_t * data,
					       uint32_t data_length)
{
	// validate data
	if (data_length < 2) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	if (data[0] != 1) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	uint8_t *replace_data = data + 1;

	// parse it
	uint8_t replacement_ref = replace_data[0];

	// tell the app
	pthread_mutex_lock(&dvb->lock);
	en50221_app_dvb_clear_replace_callback cb =
	    dvb->clear_replace_callback;
	void *cb_arg = dvb->clear_replace_callback_arg;
	pthread_mutex_unlock(&dvb->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number,
			  replacement_ref);
	}
	return 0;
}
