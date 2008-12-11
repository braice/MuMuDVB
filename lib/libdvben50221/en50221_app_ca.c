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
#include <libucsi/mpeg/descriptor.h>
#include "en50221_app_ca.h"
#include "asn_1.h"

// tags supported by this resource
#define TAG_CA_INFO_ENQUIRY     0x9f8030
#define TAG_CA_INFO             0x9f8031
#define TAG_CA_PMT              0x9f8032
#define TAG_CA_PMT_REPLY        0x9f8033

struct en50221_app_ca {
	struct en50221_app_send_functions *funcs;

	en50221_app_ca_info_callback ca_info_callback;
	void *ca_info_callback_arg;

	en50221_app_ca_pmt_reply_callback ca_pmt_reply_callback;
	void *ca_pmt_reply_callback_arg;

	pthread_mutex_t lock;
};

struct ca_pmt_descriptor {
	uint8_t *descriptor;
	uint16_t length;

	struct ca_pmt_descriptor *next;
};

struct ca_pmt_stream {
	uint8_t stream_type;
	uint16_t pid;
	struct ca_pmt_descriptor *descriptors;
	uint32_t descriptors_length;
	uint32_t descriptors_count;

	struct ca_pmt_stream *next;
};

static int en50221_ca_extract_pmt_descriptors(struct mpeg_pmt_section *pmt,
					      struct ca_pmt_descriptor **outdescriptors);
static int en50221_ca_extract_streams(struct mpeg_pmt_section *pmt,
				      struct ca_pmt_stream **outstreams);
static void en50221_ca_try_move_pmt_descriptors(struct ca_pmt_descriptor **pmt_descriptors,
						struct ca_pmt_stream **pmt_streams);
static uint32_t en50221_ca_calculate_length(struct ca_pmt_descriptor *pmt_descriptors,
					    uint32_t *pmt_descriptors_length,
					    struct ca_pmt_stream *pmt_streams);
static int en50221_app_ca_parse_info(struct en50221_app_ca *ca,
				     uint8_t slot_id,
				     uint16_t session_number,
				     uint8_t * data, uint32_t data_length);
static int en50221_app_ca_parse_reply(struct en50221_app_ca *ca,
				      uint8_t slot_id,
				      uint16_t session_number,
				      uint8_t * data,
				      uint32_t data_length);



struct en50221_app_ca *en50221_app_ca_create(struct en50221_app_send_functions *funcs)
{
	struct en50221_app_ca *ca = NULL;

	// create structure and set it up
	ca = malloc(sizeof(struct en50221_app_ca));
	if (ca == NULL) {
		return NULL;
	}
	ca->funcs = funcs;
	ca->ca_info_callback = NULL;
	ca->ca_pmt_reply_callback = NULL;

	pthread_mutex_init(&ca->lock, NULL);

	// done
	return ca;
}

void en50221_app_ca_destroy(struct en50221_app_ca *ca)
{
	pthread_mutex_destroy(&ca->lock);
	free(ca);
}

void en50221_app_ca_register_info_callback(struct en50221_app_ca *ca,
					   en50221_app_ca_info_callback
					   callback, void *arg)
{
	pthread_mutex_lock(&ca->lock);
	ca->ca_info_callback = callback;
	ca->ca_info_callback_arg = arg;
	pthread_mutex_unlock(&ca->lock);
}

void en50221_app_ca_register_pmt_reply_callback(struct en50221_app_ca *ca,
						en50221_app_ca_pmt_reply_callback
						callback, void *arg)
{
	pthread_mutex_lock(&ca->lock);
	ca->ca_pmt_reply_callback = callback;
	ca->ca_pmt_reply_callback_arg = arg;
	pthread_mutex_unlock(&ca->lock);
}

int en50221_app_ca_info_enq(struct en50221_app_ca *ca,
			    uint16_t session_number)
{
	uint8_t data[4];

	data[0] = (TAG_CA_INFO_ENQUIRY >> 16) & 0xFF;
	data[1] = (TAG_CA_INFO_ENQUIRY >> 8) & 0xFF;
	data[2] = TAG_CA_INFO_ENQUIRY & 0xFF;
	data[3] = 0;
	return ca->funcs->send_data(ca->funcs->arg, session_number, data, 4);
}

int en50221_app_ca_pmt(struct en50221_app_ca *ca,
		       uint16_t session_number,
		       uint8_t * ca_pmt, uint32_t ca_pmt_length)
{
	uint8_t buf[10];

	// set up the tag
	buf[0] = (TAG_CA_PMT >> 16) & 0xFF;
	buf[1] = (TAG_CA_PMT >> 8) & 0xFF;
	buf[2] = TAG_CA_PMT & 0xFF;

	// encode the length field
	int length_field_len;
	if ((length_field_len = asn_1_encode(ca_pmt_length, buf + 3, 3)) < 0) {
		return -1;
	}
	// build the iovecs
	struct iovec iov[2];
	iov[0].iov_base = buf;
	iov[0].iov_len = 3 + length_field_len;
	iov[1].iov_base = ca_pmt;
	iov[1].iov_len = ca_pmt_length;

	// create the data and send it
	return ca->funcs->send_datav(ca->funcs->arg, session_number, iov, 2);
}

int en50221_app_ca_message(struct en50221_app_ca *ca,
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
	case TAG_CA_INFO:
		return en50221_app_ca_parse_info(ca, slot_id,
						 session_number, data + 3,
						 data_length - 3);
	case TAG_CA_PMT_REPLY:
		return en50221_app_ca_parse_reply(ca, slot_id,
						  session_number, data + 3,
						  data_length - 3);
	}

	print(LOG_LEVEL, ERROR, 1, "Received unexpected tag %x\n", tag);
	return -1;
}

int en50221_ca_format_pmt(struct mpeg_pmt_section *pmt, uint8_t * data,
			  uint32_t data_length, int move_ca_descriptors,
			  uint8_t ca_pmt_list_management,
			  uint8_t ca_pmt_cmd_id)
{
	struct ca_pmt_descriptor *pmt_descriptors = NULL;
	uint32_t pmt_descriptors_length = 0;
	struct ca_pmt_stream *pmt_streams = NULL;
	uint32_t total_required_length = 0;
	struct ca_pmt_descriptor *cur_d;
	struct ca_pmt_stream *cur_s;
	int result = -1;

	// extract the descriptors and streams
	if (en50221_ca_extract_pmt_descriptors(pmt, &pmt_descriptors))
		goto cleanup;
	if (en50221_ca_extract_streams(pmt, &pmt_streams))
		goto cleanup;

	// try and merge them if we have no PMT descriptors
	if ((pmt_descriptors == NULL) && move_ca_descriptors) {
		en50221_ca_try_move_pmt_descriptors(&pmt_descriptors,
						    &pmt_streams);
	}
	// calculate the length of all descriptors/streams and the total length required
	total_required_length =
	    en50221_ca_calculate_length(pmt_descriptors,
					&pmt_descriptors_length,
					pmt_streams);

	// ensure we were supplied with enough data
	if (total_required_length > data_length) {
		goto cleanup;
	}
	// format the start of the PMT
	uint32_t data_pos = 0;
	data[data_pos++] = ca_pmt_list_management;
	data[data_pos++] = mpeg_pmt_section_program_number(pmt) >> 8;
	data[data_pos++] = mpeg_pmt_section_program_number(pmt);
	data[data_pos++] =
	    (pmt->head.version_number << 1) | pmt->head.
	    current_next_indicator;
	data[data_pos++] = (pmt_descriptors_length >> 8) & 0x0f;
	data[data_pos++] = pmt_descriptors_length;

	// append the PMT descriptors
	if (pmt_descriptors_length) {
		data[data_pos++] = ca_pmt_cmd_id;
		cur_d = pmt_descriptors;
		while (cur_d) {
			memcpy(data + data_pos, cur_d->descriptor,
			       cur_d->length);
			data_pos += cur_d->length;
			cur_d = cur_d->next;
		}
	}
	// now, append the streams
	cur_s = pmt_streams;
	while (cur_s) {
		data[data_pos++] = cur_s->stream_type;
		data[data_pos++] = (cur_s->pid >> 8) & 0x1f;
		data[data_pos++] = cur_s->pid;
		data[data_pos++] = (cur_s->descriptors_length >> 8) & 0x0f;
		data[data_pos++] = cur_s->descriptors_length;

		// append the stream descriptors
		if (cur_s->descriptors_length) {
			data[data_pos++] = ca_pmt_cmd_id;
			cur_d = cur_s->descriptors;
			while (cur_d) {
				memcpy(data + data_pos, cur_d->descriptor,
				       cur_d->length);
				data_pos += cur_d->length;
				cur_d = cur_d->next;
			}
		}
		cur_s = cur_s->next;
	}
	result = data_pos;


      cleanup:
	// free the PMT descriptors
	cur_d = pmt_descriptors;
	while (cur_d) {
		struct ca_pmt_descriptor *next = cur_d->next;
		free(cur_d);
		cur_d = next;
	}

	// free the streams
	cur_s = pmt_streams;
	while (cur_s) {
		struct ca_pmt_stream *next_s = cur_s->next;

		// free the stream descriptors
		cur_d = cur_s->descriptors;
		while (cur_d) {
			struct ca_pmt_descriptor *next_d = cur_d->next;
			free(cur_d);
			cur_d = next_d;
		}

		free(cur_s);
		cur_s = next_s;
	}
	return result;
}







static int en50221_ca_extract_pmt_descriptors(struct mpeg_pmt_section *pmt,
					      struct ca_pmt_descriptor **outdescriptors)
{
	struct ca_pmt_descriptor *descriptors = NULL;
	struct ca_pmt_descriptor *descriptors_tail = NULL;
	struct ca_pmt_descriptor *cur_d;

	struct descriptor *cur_descriptor;
	mpeg_pmt_section_descriptors_for_each(pmt, cur_descriptor) {
		if (cur_descriptor->tag == dtag_mpeg_ca) {
			// create a new structure for this one
			struct ca_pmt_descriptor *new_d =
			    malloc(sizeof(struct ca_pmt_descriptor));
			if (new_d == NULL) {
				goto error_exit;
			}
			new_d->descriptor = (uint8_t *) cur_descriptor;
			new_d->length = cur_descriptor->len + 2;
			new_d->next = NULL;

			// append it to the list
			if (descriptors == NULL) {
				descriptors = new_d;
			} else {
				descriptors_tail->next = new_d;
			}
			descriptors_tail = new_d;
		}
	}
	*outdescriptors = descriptors;
	return 0;

error_exit:
	cur_d = descriptors;
	while (cur_d) {
		struct ca_pmt_descriptor *next = cur_d->next;
		free(cur_d);
		cur_d = next;
	}
	return -1;
}

static int en50221_ca_extract_streams(struct mpeg_pmt_section *pmt,
				      struct ca_pmt_stream **outstreams)
{
	struct ca_pmt_stream *streams = NULL;
	struct ca_pmt_stream *streams_tail = NULL;
	struct mpeg_pmt_stream *cur_stream;
	struct descriptor *cur_descriptor;
	struct ca_pmt_stream *cur_s;

	mpeg_pmt_section_streams_for_each(pmt, cur_stream) {
		struct ca_pmt_descriptor *descriptors_tail = NULL;

		// create a new structure
		struct ca_pmt_stream *new_s =
		    malloc(sizeof(struct ca_pmt_stream));
		if (new_s == NULL) {
			goto exit_cleanup;
		}
		new_s->stream_type = cur_stream->stream_type;
		new_s->pid = cur_stream->pid;
		new_s->descriptors = NULL;
		new_s->next = NULL;
		new_s->descriptors_count = 0;

		// append it to the list
		if (streams == NULL) {
			streams = new_s;
		} else {
			streams_tail->next = new_s;
		}
		streams_tail = new_s;

		// now process the descriptors
		mpeg_pmt_stream_descriptors_for_each(cur_stream,
						     cur_descriptor) {
			if (cur_descriptor->tag == dtag_mpeg_ca) {
				// create a new structure
				struct ca_pmt_descriptor *new_d =
				    malloc(sizeof(struct ca_pmt_descriptor));
				if (new_d == NULL) {
					goto exit_cleanup;
				}
				new_d->descriptor =
				    (uint8_t *) cur_descriptor;
				new_d->length = cur_descriptor->len + 2;
				new_d->next = NULL;

				// append it to the list
				if (new_s->descriptors == NULL) {
					new_s->descriptors = new_d;
				} else {
					descriptors_tail->next = new_d;
				}
				descriptors_tail = new_d;
				new_s->descriptors_count++;
			}
		}
	}
	*outstreams = streams;
	return 0;

exit_cleanup:
	// free the streams
	cur_s = streams;
	while (cur_s) {
		struct ca_pmt_stream *next_s = cur_s->next;

		// free the stream descriptors
		struct ca_pmt_descriptor *cur_d = cur_s->descriptors;
		while (cur_d) {
			struct ca_pmt_descriptor *next_d = cur_d->next;
			free(cur_d);
			cur_d = next_d;
		}

		free(cur_s);
		cur_s = next_s;
	}
	return -1;
}

static void en50221_ca_try_move_pmt_descriptors(struct ca_pmt_descriptor **pmt_descriptors,
						struct ca_pmt_stream **pmt_streams)
{
	// get the first stream
	struct ca_pmt_stream *first_stream = *pmt_streams;
	if (first_stream == NULL)
		return;

	// Check that all the other streams with CA descriptors have exactly the same CA descriptors
	struct ca_pmt_stream *cur_stream = first_stream->next;
	while (cur_stream) {
		// if there are differing numbers of descriptors, exit right now
		if (cur_stream->descriptors_count != first_stream->descriptors_count)
			return;

		// now verify the descriptors match
		struct ca_pmt_descriptor *cur_descriptor = cur_stream->descriptors;
		struct ca_pmt_descriptor *first_cur_descriptor = first_stream->descriptors;
		while (cur_descriptor) {
			// check the descriptors are the same length
			if (cur_descriptor->length != first_cur_descriptor->length)
				return;

			// check their contents match
			if (memcmp(cur_descriptor->descriptor,
			    	   first_cur_descriptor->descriptor,
				   cur_descriptor->length)) {
				return;
			}
			// move to next
			cur_descriptor = cur_descriptor->next;
			first_cur_descriptor = first_cur_descriptor->next;
		}

		// move to next
		cur_stream = cur_stream->next;
	}

	// if we end up here, all descriptors in all streams matched

	// hook the first stream's descriptors into the PMT's
	*pmt_descriptors = first_stream->descriptors;
	first_stream->descriptors = NULL;
	first_stream->descriptors_count = 0;

	// now free up all the descriptors in the other streams
	cur_stream = first_stream->next;
	while (cur_stream) {
		struct ca_pmt_descriptor *cur_descriptor = cur_stream->descriptors;
		while (cur_descriptor) {
			struct ca_pmt_descriptor *next = cur_descriptor->next;
			free(cur_descriptor);
			cur_descriptor = next;
		}
		cur_stream->descriptors = NULL;
		cur_stream->descriptors_count = 0;
		cur_stream = cur_stream->next;
	}
}

static uint32_t en50221_ca_calculate_length(struct ca_pmt_descriptor *pmt_descriptors,
					    uint32_t *pmt_descriptors_length,
					    struct ca_pmt_stream *pmt_streams)
{
	uint32_t total_required_length = 6;	// header
	struct ca_pmt_stream *cur_s;

	// calcuate the PMT descriptors length
	(*pmt_descriptors_length) = 0;
	struct ca_pmt_descriptor *cur_d = pmt_descriptors;
	while (cur_d) {
		(*pmt_descriptors_length) += cur_d->length;
		cur_d = cur_d->next;
	}

	// add on 1 byte for the ca_pmt_cmd_id if we have some descriptors.
	if (*pmt_descriptors_length)
		(*pmt_descriptors_length)++;

	// update the total required length
	total_required_length += *pmt_descriptors_length;

	// calculate the length of descriptors in the streams
	cur_s = pmt_streams;
	while (cur_s) {
		// calculate the size of descriptors in this stream
		cur_s->descriptors_length = 0;
		cur_d = cur_s->descriptors;
		while (cur_d) {
			cur_s->descriptors_length += cur_d->length;
			cur_d = cur_d->next;
		}

		// add on 1 byte for the ca_pmt_cmd_id if we have some descriptors.
		if (cur_s->descriptors_length)
			cur_s->descriptors_length++;

		// update the total required length;
		total_required_length += 5 + cur_s->descriptors_length;

		cur_s = cur_s->next;
	}

	// done
	return total_required_length;
}

static int en50221_app_ca_parse_info(struct en50221_app_ca *ca,
				     uint8_t slot_id,
				     uint16_t session_number,
				     uint8_t * data, uint32_t data_length)
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
	data += length_field_len;

	// parse
	uint32_t ca_id_count = asn_data_length / 2;

	// byteswap the IDs
	uint16_t *ids = (uint16_t *) data;
	uint32_t i;
	for (i = 0; i < ca_id_count; i++) {
		bswap16(data);
		data += 2;
	}

	// tell the app
	pthread_mutex_lock(&ca->lock);
	en50221_app_ca_info_callback cb = ca->ca_info_callback;
	void *cb_arg = ca->ca_info_callback_arg;
	pthread_mutex_unlock(&ca->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number, ca_id_count,
			  ids);
	}
	return 0;
}

static int en50221_app_ca_parse_reply(struct en50221_app_ca *ca,
				      uint8_t slot_id,
				      uint16_t session_number,
				      uint8_t * data, uint32_t data_length)
{
	// first of all, decode the length field
	uint16_t asn_data_length;
	int length_field_len;
	if ((length_field_len = asn_1_decode(&asn_data_length, data, data_length)) < 0) {
		print(LOG_LEVEL, ERROR, 1, "ASN.1 decode error\n");
		return -1;
	}
	// check it
	if (asn_data_length < 4) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	if (asn_data_length > (data_length - length_field_len)) {
		print(LOG_LEVEL, ERROR, 1, "Received short data\n");
		return -1;
	}
	data += length_field_len;
	data_length -= length_field_len;

	// process the reply table to fix endian issues
	uint32_t pos = 4;
	bswap16(data);
	while (pos < asn_data_length) {
		bswap16(data + pos);
		pos += 3;
	}

	// tell the app
	pthread_mutex_lock(&ca->lock);
	en50221_app_ca_pmt_reply_callback cb = ca->ca_pmt_reply_callback;
	void *cb_arg = ca->ca_pmt_reply_callback_arg;
	pthread_mutex_unlock(&ca->lock);
	if (cb) {
		return cb(cb_arg, slot_id, session_number,
			  (struct en50221_app_pmt_reply *) data,
			  asn_data_length);
	}
	return 0;
}
