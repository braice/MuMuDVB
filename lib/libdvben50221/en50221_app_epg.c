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
#include <libucsi/dvb/types.h>
#include "en50221_app_epg.h"
#include "en50221_app_tags.h"
#include "asn_1.h"

struct en50221_app_epg {
	struct en50221_app_send_functions *funcs;

	en50221_app_epg_reply_callback callback;
	void *callback_arg;

	pthread_mutex_t lock;
};

static int en50221_app_epg_parse_reply(struct en50221_app_epg *private,
				       uint8_t slot_id,
				       uint16_t session_number,
				       uint8_t * data,
				       uint32_t data_length);



struct en50221_app_epg *en50221_app_epg_create(struct en50221_app_send_functions *funcs)
{
	struct en50221_app_epg *epg = NULL;

	// create structure and set it up
	epg = malloc(sizeof(struct en50221_app_epg));
	if (epg == NULL) {
		return NULL;
	}
	epg->funcs = funcs;
	epg->callback = NULL;

	pthread_mutex_init(&epg->lock, NULL);

	// done
	return epg;
}

void en50221_app_epg_destroy(struct en50221_app_epg *epg)
{
	pthread_mutex_destroy(&epg->lock);
	free(epg);
}

void en50221_app_epg_register_enquiry_callback(struct en50221_app_epg *epg,
					       en50221_app_epg_reply_callback callback,
					       void *arg)
{
	pthread_mutex_lock(&epg->lock);
	epg->callback = callback;
	epg->callback_arg = arg;
	pthread_mutex_unlock(&epg->lock);
}

int en50221_app_epg_enquire(struct en50221_app_epg *epg,
			    uint16_t session_number,
			    uint8_t command_id,
			    uint16_t network_id,
			    uint16_t original_network_id,
			    uint16_t transport_stream_id,
			    uint16_t service_id, uint16_t event_id)
{
	uint8_t data[15];

	data[0] = (TAG_EPG_ENQUIRY >> 16) & 0xFF;
	data[1] = (TAG_EPG_ENQUIRY >> 8) & 0xFF;
	data[2] = TAG_EPG_ENQUIRY & 0xFF;
	data[3] = 11;
	data[4] = command_id;
	data[5] = network_id >> 8;
	data[6] = network_id;
	data[7] = original_network_id >> 8;
	data[8] = original_network_id;
	data[9] = transport_stream_id >> 8;
	data[10] = transport_stream_id;
	data[11] = service_id >> 8;
	data[12] = service_id;
	data[13] = event_id >> 8;
	data[14] = event_id;
	return epg->funcs->send_data(epg->funcs->arg, session_number, data, 15);
}

int en50221_app_epg_message(struct en50221_app_epg *epg,
			    uint8_t slot_id,
			    uint16_t session_number,
			    uint32_t resource_id,
			    uint8_t * data, uint32_t data_length)
{
	struct en50221_app_epg *private = (struct en50221_app_epg *) epg;
	(void) resource_id;

	// get the tag
	if (data_length < 3) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	uint32_t tag = (data[0] << 16) | (data[1] << 8) | data[2];

	switch (tag) {
	case TAG_EPG_REPLY:
		return en50221_app_epg_parse_reply(private, slot_id,
						   session_number,
						   data + 3,
						   data_length - 3);
	}

	print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
	return -1;
}



static int en50221_app_epg_parse_reply(struct en50221_app_epg *epg,
				       uint8_t slot_id,
				       uint16_t session_number,
				       uint8_t * data,
				       uint32_t data_length)
{
	// validate data
	if (data_length != 2) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	if (data[0] != 1) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	uint8_t event_status = data[1];

	// tell the app
	pthread_mutex_lock(&epg->lock);
	en50221_app_epg_reply_callback cb = epg->callback;
	void *cb_arg = epg->callback_arg;
	pthread_mutex_unlock(&epg->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number, event_status);
	}
	return 0;
}
