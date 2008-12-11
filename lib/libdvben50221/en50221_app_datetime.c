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
#include "en50221_app_datetime.h"
#include "en50221_app_tags.h"
#include "asn_1.h"

struct en50221_app_datetime {
	struct en50221_app_send_functions *funcs;

	en50221_app_datetime_enquiry_callback callback;
	void *callback_arg;

	pthread_mutex_t lock;
};

static int en50221_app_datetime_parse_enquiry(struct en50221_app_datetime *datetime,
					      uint8_t slot_id,
					      uint16_t session_number,
					      uint8_t * data,
					      uint32_t data_length);



struct en50221_app_datetime *en50221_app_datetime_create(struct en50221_app_send_functions *funcs)
{
	struct en50221_app_datetime *datetime = NULL;

	// create structure and set it up
	datetime = malloc(sizeof(struct en50221_app_datetime));
	if (datetime == NULL) {
		return NULL;
	}
	datetime->funcs = funcs;
	datetime->callback = NULL;

	pthread_mutex_init(&datetime->lock, NULL);

	// done
	return datetime;
}

void en50221_app_datetime_destroy(struct en50221_app_datetime *datetime)
{
	pthread_mutex_destroy(&datetime->lock);
	free(datetime);
}

void en50221_app_datetime_register_enquiry_callback(struct en50221_app_datetime *datetime,
						    en50221_app_datetime_enquiry_callback callback,
						    void *arg)
{
	pthread_mutex_lock(&datetime->lock);
	datetime->callback = callback;
	datetime->callback_arg = arg;
	pthread_mutex_unlock(&datetime->lock);
}

int en50221_app_datetime_send(struct en50221_app_datetime *datetime,
			      uint16_t session_number,
			      time_t utc_time, int time_offset)
{
	uint8_t data[11];
	int data_length;

	data[0] = (TAG_DATE_TIME >> 16) & 0xFF;
	data[1] = (TAG_DATE_TIME >> 8) & 0xFF;
	data[2] = TAG_DATE_TIME & 0xFF;
	if (time_offset != -1) {
		data[3] = 7;
		unixtime_to_dvbdate(utc_time, data + 4);
		data[9] = time_offset >> 8;
		data[10] = time_offset;
		data_length = 11;
	} else {
		data[3] = 5;
		unixtime_to_dvbdate(utc_time, data + 4);
		data_length = 9;
	}
	return datetime->funcs->send_data(datetime->funcs->arg,
					  session_number, data,
					  data_length);
}

int en50221_app_datetime_message(struct en50221_app_datetime *datetime,
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
	case TAG_DATE_TIME_ENQUIRY:
		return en50221_app_datetime_parse_enquiry(datetime,
							  slot_id,
							  session_number,
							  data + 3,
							  data_length - 3);
	}

	print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
	return -1;
}










static int en50221_app_datetime_parse_enquiry(struct en50221_app_datetime *datetime,
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
	uint8_t response_interval = data[1];

	// tell the app
	pthread_mutex_lock(&datetime->lock);
	en50221_app_datetime_enquiry_callback cb = datetime->callback;
	void *cb_arg = datetime->callback_arg;
	pthread_mutex_unlock(&datetime->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number,
			  response_interval);
	}
	return 0;
}
