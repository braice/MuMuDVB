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
#include "en50221_app_teletext.h"
#include "en50221_app_tags.h"
#include "asn_1.h"

struct en50221_app_teletext {
	struct en50221_app_send_functions *funcs;

	en50221_app_teletext_callback callback;
	void *callback_arg;

	pthread_mutex_t lock;
};

static int en50221_app_teletext_parse_ebu(struct en50221_app_teletext *teletext,
					  uint8_t slot_id,
					  uint16_t session_number,
					  uint8_t * data,
					  uint32_t data_length);



struct en50221_app_teletext *
	en50221_app_teletext_create(struct en50221_app_send_functions *funcs)
{
	struct en50221_app_teletext *teletext = NULL;

	// create structure and set it up
	teletext = malloc(sizeof(struct en50221_app_teletext));
	if (teletext == NULL) {
		return NULL;
	}
	teletext->funcs = funcs;
	teletext->callback = NULL;

	pthread_mutex_init(&teletext->lock, NULL);

	// done
	return teletext;
}

void en50221_app_teletext_destroy(struct en50221_app_teletext *teletext)
{
	pthread_mutex_destroy(&teletext->lock);
	free(teletext);
}

void en50221_app_teletext_register_callback(struct en50221_app_teletext *teletext,
					    en50221_app_teletext_callback callback, void *arg)
{
	pthread_mutex_lock(&teletext->lock);
	teletext->callback = callback;
	teletext->callback_arg = arg;
	pthread_mutex_unlock(&teletext->lock);
}

int en50221_app_teletext_message(struct en50221_app_teletext *teletext,
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
	case TAG_TELETEXT_EBU:
		return en50221_app_teletext_parse_ebu(teletext, slot_id,
						      session_number,
						      data + 3,
						      data_length - 3);
	}

	print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
	return -1;
}


static int en50221_app_teletext_parse_ebu(struct en50221_app_teletext *teletext,
					  uint8_t slot_id,
					  uint16_t session_number,
					  uint8_t *data,
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
	uint8_t *teletext_data = data + length_field_len;

	// tell the app
	pthread_mutex_lock(&teletext->lock);
	en50221_app_teletext_callback cb = teletext->callback;
	void *cb_arg = teletext->callback_arg;
	pthread_mutex_unlock(&teletext->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number, teletext_data,
			  asn_data_length);
	}
	return 0;
}
