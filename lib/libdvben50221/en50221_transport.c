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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <time.h>
#include <libdvbmisc/dvbmisc.h>
#include <libdvbapi/dvbca.h>
#include "en50221_errno.h"
#include "en50221_transport.h"
#include "asn_1.h"

// these are the Transport Tags, like
// described in EN50221, Annex A.4.1.13 (pg70)
#define T_SB                0x80	// sb                           primitive   h<--m
#define T_RCV               0x81	// receive                      primitive   h-->m
#define T_CREATE_T_C        0x82	// create transport connection  primitive   h-->m
#define T_C_T_C_REPLY       0x83	// ctc reply                    primitive   h<--m
#define T_DELETE_T_C        0x84	// delete tc                    primitive   h<->m
#define T_D_T_C_REPLY       0x85	// dtc reply                    primitive   h<->m
#define T_REQUEST_T_C       0x86	// request transport connection primitive   h<--m
#define T_NEW_T_C           0x87	// new tc / reply to t_request  primitive   h-->m
#define T_T_C_ERROR         0x77	// error creating tc            primitive   h-->m
#define T_DATA_LAST         0xA0	// convey data from higher      constructed h<->m
				 // layers
#define T_DATA_MORE         0xA1	// convey data from higher      constructed h<->m
				 // layers

struct en50221_message {
	struct en50221_message *next;
	uint32_t length;
	uint8_t data[0];
};

struct en50221_connection {
	uint32_t state;		// the current state: idle/in_delete/in_create/active
	struct timeval tx_time;	// time last request was sent from host->module, or 0 if ok
	struct timeval last_poll_time;	// time of last poll transmission
	uint8_t *chain_buffer;	// used to save parts of chained packets
	uint32_t buffer_length;

	struct en50221_message *send_queue;
	struct en50221_message *send_queue_tail;
};

struct en50221_slot {
	int ca_hndl;
	uint8_t slot;		// CAM slot
	struct en50221_connection *connections;

	pthread_mutex_t slot_lock;

	uint32_t response_timeout;
	uint32_t poll_delay;
};

struct en50221_transport_layer {
	uint8_t max_slots;
	uint8_t max_connections_per_slot;
	struct en50221_slot *slots;
	struct pollfd *slot_pollfds;
	int slots_changed;

	pthread_mutex_t global_lock;
	pthread_mutex_t setcallback_lock;

	int error;
	int error_slot;

	en50221_tl_callback callback;
	void *callback_arg;
};

static int en50221_tl_process_data(struct en50221_transport_layer *tl,
				   uint8_t slot_id, uint8_t * data,
				   uint32_t data_length);
static int en50221_tl_poll_tc(struct en50221_transport_layer *tl,
			      uint8_t slot_id, uint8_t connection_id);
static int en50221_tl_alloc_new_tc(struct en50221_transport_layer *tl,
				   uint8_t slot_id);
static void queue_message(struct en50221_transport_layer *tl,
			  uint8_t slot_id, uint8_t connection_id,
			  struct en50221_message *msg);
static int en50221_tl_handle_create_tc_reply(struct en50221_transport_layer
					     *tl, uint8_t slot_id,
					     uint8_t connection_id);
static int en50221_tl_handle_delete_tc(struct en50221_transport_layer *tl,
				       uint8_t slot_id,
				       uint8_t connection_id);
static int en50221_tl_handle_delete_tc_reply(struct en50221_transport_layer
					     *tl, uint8_t slot_id,
					     uint8_t connection_id);
static int en50221_tl_handle_request_tc(struct en50221_transport_layer *tl,
					uint8_t slot_id,
					uint8_t connection_id);
static int en50221_tl_handle_data_more(struct en50221_transport_layer *tl,
				       uint8_t slot_id,
				       uint8_t connection_id,
				       uint8_t * data,
				       uint32_t data_length);
static int en50221_tl_handle_data_last(struct en50221_transport_layer *tl,
				       uint8_t slot_id,
				       uint8_t connection_id,
				       uint8_t * data,
				       uint32_t data_length);
static int en50221_tl_handle_sb(struct en50221_transport_layer *tl,
				uint8_t slot_id, uint8_t connection_id,
				uint8_t * data, uint32_t data_length);


struct en50221_transport_layer *en50221_tl_create(uint8_t max_slots,
						  uint8_t
						  max_connections_per_slot)
{
	struct en50221_transport_layer *tl = NULL;
	int i;
	int j;

	// setup structure
	tl = (struct en50221_transport_layer *)
		malloc(sizeof(struct en50221_transport_layer));
	if (tl == NULL)
		goto error_exit;
	tl->max_slots = max_slots;
	tl->max_connections_per_slot = max_connections_per_slot;
	tl->slots = NULL;
	tl->slot_pollfds = NULL;
	tl->slots_changed = 1;
	tl->callback = NULL;
	tl->callback_arg = NULL;
	tl->error_slot = 0;
	tl->error = 0;
	pthread_mutex_init(&tl->global_lock, NULL);
	pthread_mutex_init(&tl->setcallback_lock, NULL);

	// create the slots
	tl->slots = malloc(sizeof(struct en50221_slot) * max_slots);
	if (tl->slots == NULL)
		goto error_exit;

	// set them up
	for (i = 0; i < max_slots; i++) {
		tl->slots[i].ca_hndl = -1;

		// create the connections for this slot
		tl->slots[i].connections =
		    malloc(sizeof(struct en50221_connection) * max_connections_per_slot);
		if (tl->slots[i].connections == NULL)
			goto error_exit;

		// create a mutex for the slot
		pthread_mutex_init(&tl->slots[i].slot_lock, NULL);

		// set them up
		for (j = 0; j < max_connections_per_slot; j++) {
			tl->slots[i].connections[j].state = T_STATE_IDLE;
			tl->slots[i].connections[j].tx_time.tv_sec = 0;
			tl->slots[i].connections[j].last_poll_time.tv_sec = 0;
			tl->slots[i].connections[j].last_poll_time.tv_usec = 0;
			tl->slots[i].connections[j].chain_buffer = NULL;
			tl->slots[i].connections[j].buffer_length = 0;
			tl->slots[i].connections[j].send_queue = NULL;
			tl->slots[i].connections[j].send_queue_tail = NULL;
		}
	}

	// create the pollfds
	tl->slot_pollfds = malloc(sizeof(struct pollfd) * max_slots);
	if (tl->slot_pollfds == NULL) {
		goto error_exit;
	}
	memset(tl->slot_pollfds, 0, sizeof(struct pollfd) * max_slots);

	return tl;

      error_exit:
	en50221_tl_destroy(tl);
	return NULL;
}

// Destroy an instance of the transport layer
void en50221_tl_destroy(struct en50221_transport_layer *tl)
{
	int i, j;

	if (tl) {
		if (tl->slots) {
			for (i = 0; i < tl->max_slots; i++) {
				if (tl->slots[i].connections) {
					for (j = 0; j < tl->max_connections_per_slot; j++) {
						if (tl->slots[i].connections[j].chain_buffer) {
							free(tl->slots[i].connections[j].chain_buffer);
						}

						struct en50221_message *cur_msg =
							tl->slots[i].connections[j].send_queue;
						while (cur_msg) {
							struct en50221_message *next_msg = cur_msg->next;
							free(cur_msg);
							cur_msg = next_msg;
						}
						tl->slots[i].connections[j].send_queue = NULL;
						tl->slots[i].connections[j].send_queue_tail = NULL;
					}
					free(tl->slots[i].connections);
					pthread_mutex_destroy(&tl->slots[i].slot_lock);
				}
			}
			free(tl->slots);
		}
		if (tl->slot_pollfds) {
			free(tl->slot_pollfds);
		}
		pthread_mutex_destroy(&tl->setcallback_lock);
		pthread_mutex_destroy(&tl->global_lock);
		free(tl);
	}
}

// this can be called from the user-space app to
// register new slots that we should work with
int en50221_tl_register_slot(struct en50221_transport_layer *tl,
			     int ca_hndl, uint8_t slot,
			     uint32_t response_timeout,
			     uint32_t poll_delay)
{
	// lock
	pthread_mutex_lock(&tl->global_lock);

	// we browse through the array of slots
	// to look for the first unused one
	int i;
	int16_t slot_id = -1;
	for (i = 0; i < tl->max_slots; i++) {
		if (tl->slots[i].ca_hndl == -1) {
			slot_id = i;
			break;
		}
	}
	if (slot_id == -1) {
		tl->error = EN50221ERR_OUTOFSLOTS;
		pthread_mutex_unlock(&tl->global_lock);
		return -1;
	}
	// set up the slot struct
	pthread_mutex_lock(&tl->slots[slot_id].slot_lock);
	tl->slots[slot_id].ca_hndl = ca_hndl;
	tl->slots[slot_id].slot = slot;
	tl->slots[slot_id].response_timeout = response_timeout;
	tl->slots[slot_id].poll_delay = poll_delay;
	pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);

	tl->slots_changed = 1;
	pthread_mutex_unlock(&tl->global_lock);
	return slot_id;
}

void en50221_tl_destroy_slot(struct en50221_transport_layer *tl,
			     uint8_t slot_id)
{
	int i;

	if (slot_id >= tl->max_slots)
		return;

	// lock
	pthread_mutex_lock(&tl->global_lock);

	// clear the slot
	pthread_mutex_lock(&tl->slots[slot_id].slot_lock);
	tl->slots[slot_id].ca_hndl = -1;
	for (i = 0; i < tl->max_connections_per_slot; i++) {
		tl->slots[slot_id].connections[i].state = T_STATE_IDLE;
		tl->slots[slot_id].connections[i].tx_time.tv_sec = 0;
		tl->slots[slot_id].connections[i].last_poll_time.tv_sec = 0;
		tl->slots[slot_id].connections[i].last_poll_time.tv_usec = 0;
		if (tl->slots[slot_id].connections[i].chain_buffer) {
			free(tl->slots[slot_id].connections[i].
			     chain_buffer);
		}
		tl->slots[slot_id].connections[i].chain_buffer = NULL;
		tl->slots[slot_id].connections[i].buffer_length = 0;

		struct en50221_message *cur_msg =
		    tl->slots[slot_id].connections[i].send_queue;
		while (cur_msg) {
			struct en50221_message *next_msg = cur_msg->next;
			free(cur_msg);
			cur_msg = next_msg;
		}
		tl->slots[slot_id].connections[i].send_queue = NULL;
		tl->slots[slot_id].connections[i].send_queue_tail = NULL;
	}
	pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);

	// tell upper layers
	pthread_mutex_lock(&tl->setcallback_lock);
	en50221_tl_callback cb = tl->callback;
	void *cb_arg = tl->callback_arg;
	pthread_mutex_unlock(&tl->setcallback_lock);
	if (cb)
		cb(cb_arg, T_CALLBACK_REASON_SLOTCLOSE, NULL, 0, slot_id, 0);

	tl->slots_changed = 1;
	pthread_mutex_unlock(&tl->global_lock);
}

int en50221_tl_poll(struct en50221_transport_layer *tl)
{
	uint8_t data[4096];
	int slot_id;
	int j;

	// make up pollfds if the slots have changed
	pthread_mutex_lock(&tl->global_lock);
	if (tl->slots_changed) {
		for (slot_id = 0; slot_id < tl->max_slots; slot_id++) {
			if (tl->slots[slot_id].ca_hndl != -1) {
				tl->slot_pollfds[slot_id].fd = tl->slots[slot_id].ca_hndl;
				tl->slot_pollfds[slot_id].events = POLLIN | POLLPRI | POLLERR;
				tl->slot_pollfds[slot_id].revents = 0;
			} else {
				tl->slot_pollfds[slot_id].fd = 0;
				tl->slot_pollfds[slot_id].events = 0;
				tl->slot_pollfds[slot_id].revents = 0;
			}
		}
		tl->slots_changed = 0;
	}
	pthread_mutex_unlock(&tl->global_lock);

	// anything happened?
	if (poll(tl->slot_pollfds, tl->max_slots, 10) < 0) {
		tl->error_slot = -1;
		tl->error = EN50221ERR_CAREAD;
		return -1;
	}
	// go through all slots (even though poll may not have reported any events
	for (slot_id = 0; slot_id < tl->max_slots; slot_id++) {

		// check if this slot is still used and get its handle
		pthread_mutex_lock(&tl->slots[slot_id].slot_lock);
		if (tl->slots[slot_id].ca_hndl == -1) {
			pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
			continue;
		}
		int ca_hndl = tl->slots[slot_id].ca_hndl;

		if (tl->slot_pollfds[slot_id].revents & (POLLPRI | POLLIN)) {
			// read data
			uint8_t r_slot_id;
			uint8_t connection_id;
			int readcnt = dvbca_link_read(ca_hndl, &r_slot_id,
						      &connection_id,
						      data, sizeof(data));
			if (readcnt < 0) {
				tl->error_slot = slot_id;
				tl->error = EN50221ERR_CAREAD;
				pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
				return -1;
			}
			// process it if we got some
			if (readcnt > 0) {
				if (tl->slots[slot_id].slot != r_slot_id) {
					// this message is for an other CAM of the same CA
					int new_slot_id;
					for (new_slot_id = 0; new_slot_id < tl->max_slots; new_slot_id++) {
						if ((tl->slots[new_slot_id].ca_hndl == ca_hndl) &&
						    (tl->slots[new_slot_id].slot == r_slot_id))
							break;
					}
					if (new_slot_id != tl->max_slots) {
						// we found the requested CAM
						pthread_mutex_lock(&tl->slots[new_slot_id].slot_lock);
						if (en50221_tl_process_data(tl, new_slot_id, data, readcnt)) {
							pthread_mutex_unlock(&tl->slots[new_slot_id].slot_lock);
							pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
							return -1;
						}
						pthread_mutex_unlock(&tl->slots[new_slot_id].slot_lock);
					} else {
						tl->error = EN50221ERR_BADSLOTID;
						pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
						return -1;
					}
				} else
				    if (en50221_tl_process_data(tl, slot_id, data, readcnt)) {
					pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
					return -1;
				}
			}
		} else if (tl->slot_pollfds[slot_id].revents & POLLERR) {
			// an error was reported
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_CAREAD;
			pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
			return -1;
		}
		// poll the connections on this slot + check for timeouts
		for (j = 0; j < tl->max_connections_per_slot; j++) {
			// ignore connection if idle
			if (tl->slots[slot_id].connections[j].state == T_STATE_IDLE) {
				continue;
			}
			// send queued data
			if (tl->slots[slot_id].connections[j].state &
				(T_STATE_IN_CREATION | T_STATE_ACTIVE | T_STATE_ACTIVE_DELETEQUEUED)) {
				// send data if there is some to go and we're not waiting for a response already
				if (tl->slots[slot_id].connections[j].send_queue &&
				    (tl->slots[slot_id].connections[j].tx_time.tv_sec == 0)) {

					// get the message
					struct en50221_message *msg =
						tl->slots[slot_id].connections[j].send_queue;
					if (msg->next != NULL) {
						tl->slots[slot_id].connections[j].send_queue = msg->next;
					} else {
						tl->slots[slot_id].connections[j].send_queue = NULL;
						tl->slots[slot_id].connections[j].send_queue_tail = NULL;
					}

					// send the message
					if (dvbca_link_write(tl->slots[slot_id].ca_hndl,
					    		     tl->slots[slot_id].slot,
							     j,
							     msg->data, msg->length) < 0) {
						free(msg);
						pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
						tl->error_slot = slot_id;
						tl->error = EN50221ERR_CAWRITE;
						print(LOG_LEVEL, ERROR, 1, "CAWrite failed");
						return -1;
					}
					gettimeofday(&tl->slots[slot_id].connections[j].tx_time, 0);

					// fixup connection state for T_DELETE_T_C
					if (msg->length && (msg->data[0] == T_DELETE_T_C)) {
						tl->slots[slot_id].connections[j].state = T_STATE_IN_DELETION;
						if (tl->slots[slot_id].connections[j].chain_buffer) {
							free(tl->slots[slot_id].connections[j].chain_buffer);
						}
						tl->slots[slot_id].connections[j].chain_buffer = NULL;
						tl->slots[slot_id].connections[j].buffer_length = 0;
					}

					free(msg);
				}
			}
			// poll it if we're not expecting a reponse and the poll time has elapsed
			if (tl->slots[slot_id].connections[j].state & T_STATE_ACTIVE) {
				if ((tl->slots[slot_id].connections[j].tx_time.tv_sec == 0) &&
				    (time_after(tl->slots[slot_id].connections[j].last_poll_time,
				     		tl->slots[slot_id].poll_delay))) {

					gettimeofday(&tl->slots[slot_id].connections[j].last_poll_time, 0);
					if (en50221_tl_poll_tc(tl, slot_id, j)) {
						pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
						return -1;
					}
				}
			}

			// check for timeouts - in any state
			if (tl->slots[slot_id].connections[j].tx_time.tv_sec &&
			    (time_after(tl->slots[slot_id].connections[j].tx_time,
			     		tl->slots[slot_id].response_timeout))) {

				if (tl->slots[slot_id].connections[j].state &
				    (T_STATE_IN_CREATION |T_STATE_IN_DELETION)) {
					tl->slots[slot_id].connections[j].state = T_STATE_IDLE;
				} else if (tl->slots[slot_id].connections[j].state &
					   (T_STATE_ACTIVE | T_STATE_ACTIVE_DELETEQUEUED)) {
					tl->error_slot = slot_id;
					tl->error = EN50221ERR_TIMEOUT;
					pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
					return -1;
				}
			}
		}
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
	}

	return 0;
}

void en50221_tl_register_callback(struct en50221_transport_layer *tl,
				  en50221_tl_callback callback, void *arg)
{
	pthread_mutex_lock(&tl->setcallback_lock);
	tl->callback = callback;
	tl->callback_arg = arg;
	pthread_mutex_unlock(&tl->setcallback_lock);
}

int en50221_tl_get_error_slot(struct en50221_transport_layer *tl)
{
	return tl->error_slot;
}

int en50221_tl_get_error(struct en50221_transport_layer *tl)
{
	return tl->error;
}

int en50221_tl_send_data(struct en50221_transport_layer *tl,
			 uint8_t slot_id, uint8_t connection_id,
			 uint8_t * data, uint32_t data_size)
{
#ifdef DEBUG_TXDATA
	printf("[[[[[[[[[[[[[[[[[[[[\n");
	uint32_t ii = 0;
	for (ii = 0; ii < data_size; ii++) {
		printf("%02x: %02x\n", ii, data[ii]);
	}
	printf("]]]]]]]]]]]]]]]]]]]]\n");
#endif

	if (slot_id >= tl->max_slots) {
		tl->error = EN50221ERR_BADSLOTID;
		return -1;
	}

	pthread_mutex_lock(&tl->slots[slot_id].slot_lock);
	if (tl->slots[slot_id].ca_hndl == -1) {
		tl->error = EN50221ERR_BADSLOTID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	if (connection_id >= tl->max_connections_per_slot) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCONNECTIONID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	if (tl->slots[slot_id].connections[connection_id].state != T_STATE_ACTIVE) {
		tl->error = EN50221ERR_BADCONNECTIONID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	// allocate msg structure
	struct en50221_message *msg =
	    malloc(sizeof(struct en50221_message) + data_size + 10);
	if (msg == NULL) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_OUTOFMEMORY;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	// make up data to send
	int length_field_len;
	msg->data[0] = T_DATA_LAST;
	if ((length_field_len = asn_1_encode(data_size + 1, msg->data + 1, 3)) < 0) {
		free(msg);
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_ASNENCODE;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	msg->data[1 + length_field_len] = connection_id;
	memcpy(msg->data + 1 + length_field_len + 1, data, data_size);
	msg->length = 1 + length_field_len + 1 + data_size;

	// queue it for transmission
	queue_message(tl, slot_id, connection_id, msg);

	pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
	return 0;
}

int en50221_tl_send_datav(struct en50221_transport_layer *tl,
			  uint8_t slot_id, uint8_t connection_id,
			  struct iovec *vector, int iov_count)
{
#ifdef DEBUG_TXDATA
	printf("[[[[[[[[[[[[[[[[[[[[\n");
	uint32_t ii = 0;
	uint32_t iipos = 0;
	for (ii = 0; ii < (uint32_t) iov_count; ii++) {
		uint32_t jj;
		for (jj = 0; jj < vector[ii].iov_len; jj++) {
			printf("%02x: %02x\n", jj + iipos,
			       *((uint8_t *) (vector[ii].iov_base) + jj));
		}
		iipos += vector[ii].iov_len;
	}
	printf("]]]]]]]]]]]]]]]]]]]]\n");
#endif

	if (slot_id >= tl->max_slots) {
		tl->error = EN50221ERR_BADSLOTID;
		return -1;
	}

	pthread_mutex_lock(&tl->slots[slot_id].slot_lock);
	if (tl->slots[slot_id].ca_hndl == -1) {
		tl->error = EN50221ERR_BADSLOTID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	if (connection_id >= tl->max_connections_per_slot) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCONNECTIONID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	if (tl->slots[slot_id].connections[connection_id].state != T_STATE_ACTIVE) {
		tl->error = EN50221ERR_BADCONNECTIONID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	// calculate the total length of the data to send
	uint32_t data_size = 0;
	int i;
	for (i = 0; i < iov_count; i++) {
		data_size += vector[i].iov_len;
	}

	// allocate msg structure
	struct en50221_message *msg =
	    malloc(sizeof(struct en50221_message) + data_size + 10);
	if (msg == NULL) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_OUTOFMEMORY;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	// make up data to send
	int length_field_len;
	msg->data[0] = T_DATA_LAST;
	if ((length_field_len = asn_1_encode(data_size + 1, msg->data + 1, 3)) < 0) {
		free(msg);
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_ASNENCODE;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	msg->data[1 + length_field_len] = connection_id;
	msg->length = 1 + length_field_len + 1 + data_size;
	msg->next = NULL;

	// merge the iovecs
	uint32_t pos = 1 + length_field_len + 1;
	for (i = 0; i < iov_count; i++) {
		memcpy(msg->data + pos, vector[i].iov_base,
		       vector[i].iov_len);
		pos += vector[i].iov_len;
	}

	// queue it for transmission
	queue_message(tl, slot_id, connection_id, msg);

	pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
	return 0;
}

int en50221_tl_new_tc(struct en50221_transport_layer *tl, uint8_t slot_id)
{
	// check
	if (slot_id >= tl->max_slots) {
		tl->error = EN50221ERR_BADSLOTID;
		return -1;
	}

	pthread_mutex_lock(&tl->slots[slot_id].slot_lock);
	if (tl->slots[slot_id].ca_hndl == -1) {
		tl->error = EN50221ERR_BADSLOTID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	// allocate a new connection if possible
	int conid = en50221_tl_alloc_new_tc(tl, slot_id);
	if (conid == -1) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_OUTOFCONNECTIONS;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	// allocate msg structure
	struct en50221_message *msg =
	    malloc(sizeof(struct en50221_message) + 3);
	if (msg == NULL) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_OUTOFMEMORY;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	// make up the data to send
	msg->data[0] = T_CREATE_T_C;
	msg->data[1] = 1;
	msg->data[2] = conid;
	msg->length = 3;
	msg->next = NULL;

	// queue it for transmission
	queue_message(tl, slot_id, conid, msg);

	// done
	pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
	return conid;
}

int en50221_tl_del_tc(struct en50221_transport_layer *tl, uint8_t slot_id,
		      uint8_t connection_id)
{
	// check
	if (slot_id >= tl->max_slots) {
		tl->error = EN50221ERR_BADSLOTID;
		return -1;
	}

	pthread_mutex_lock(&tl->slots[slot_id].slot_lock);
	if (tl->slots[slot_id].ca_hndl == -1) {
		tl->error = EN50221ERR_BADSLOTID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	if (connection_id >= tl->max_connections_per_slot) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCONNECTIONID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	if (!(tl->slots[slot_id].connections[connection_id].state &
	      (T_STATE_ACTIVE | T_STATE_IN_DELETION))) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADSTATE;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	// allocate msg structure
	struct en50221_message *msg =
	    malloc(sizeof(struct en50221_message) + 3);
	if (msg == NULL) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_OUTOFMEMORY;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	// make up the data to send
	msg->data[0] = T_DELETE_T_C;
	msg->data[1] = 1;
	msg->data[2] = connection_id;
	msg->length = 3;
	msg->next = NULL;

	// queue it for transmission
	queue_message(tl, slot_id, connection_id, msg);
	tl->slots[slot_id].connections[connection_id].state =
	    T_STATE_ACTIVE_DELETEQUEUED;

	pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
	return 0;
}

int en50221_tl_get_connection_state(struct en50221_transport_layer *tl,
				    uint8_t slot_id, uint8_t connection_id)
{
	if (slot_id >= tl->max_slots) {
		tl->error = EN50221ERR_BADSLOTID;
		return -1;
	}

	pthread_mutex_lock(&tl->slots[slot_id].slot_lock);
	if (tl->slots[slot_id].ca_hndl == -1) {
		tl->error = EN50221ERR_BADSLOTID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	if (connection_id >= tl->max_connections_per_slot) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCONNECTIONID;
		pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);
		return -1;
	}
	int state = tl->slots[slot_id].connections[connection_id].state;
	pthread_mutex_unlock(&tl->slots[slot_id].slot_lock);

	return state;
}




// ask the module for new data
static int en50221_tl_poll_tc(struct en50221_transport_layer *tl,
			      uint8_t slot_id, uint8_t connection_id)
{
	gettimeofday(&tl->slots[slot_id].connections[connection_id].
		     tx_time, 0);

	// send command
	uint8_t hdr[3];
	hdr[0] = T_DATA_LAST;
	hdr[1] = 1;
	hdr[2] = connection_id;
	if (dvbca_link_write(tl->slots[slot_id].ca_hndl,
	    		     tl->slots[slot_id].slot,
			     connection_id, hdr, 3) < 0) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_CAWRITE;
		return -1;
	}
	return 0;
}

// handle incoming data
static int en50221_tl_process_data(struct en50221_transport_layer *tl,
				   uint8_t slot_id, uint8_t * data,
				   uint32_t data_length)
{
	int result;

#ifdef DEBUG_RXDATA
	printf("-------------------\n");
	uint32_t ii = 0;
	for (ii = 0; ii < data_length; ii++) {
		printf("%02x: %02x\n", ii, data[ii]);
	}
	printf("+++++++++++++++++++\n");
#endif

	// process the received data
	while (data_length) {
		// parse the header
		uint8_t tpdu_tag = data[0];
		uint16_t asn_data_length;
		int length_field_len;
		if ((length_field_len = asn_1_decode(&asn_data_length, data + 1, data_length - 1)) < 0) {
			print(LOG_LEVEL, ERROR, 1,
			      "Received data with invalid asn from module on slot %02x\n",
			      slot_id);
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_BADCAMDATA;
			return -1;
		}
		if ((asn_data_length < 1) ||
		    (asn_data_length > (data_length - (1 + length_field_len)))) {
			print(LOG_LEVEL, ERROR, 1,
			      "Received data with invalid length from module on slot %02x\n",
			      slot_id);
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_BADCAMDATA;
			return -1;
		}
		uint8_t connection_id = data[1 + length_field_len];
		data += 1 + length_field_len + 1;
		data_length -= (1 + length_field_len + 1);
		asn_data_length--;

		// check the connection_id
		if (connection_id >= tl->max_connections_per_slot) {
			print(LOG_LEVEL, ERROR, 1,
			      "Received bad connection id %02x from module on slot %02x\n",
			      connection_id, slot_id);
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_BADCONNECTIONID;
			return -1;
		}
		// process the TPDUs
		switch (tpdu_tag) {
		case T_C_T_C_REPLY:
			if ((result = en50221_tl_handle_create_tc_reply(tl, slot_id, connection_id)) < 0) {
				return -1;
			}
			break;
		case T_DELETE_T_C:
			if ((result = en50221_tl_handle_delete_tc(tl, slot_id, connection_id)) < 0) {
				return -1;
			}
			break;
		case T_D_T_C_REPLY:
			if ((result = en50221_tl_handle_delete_tc_reply(tl, slot_id, connection_id)) < 0) {
				return -1;
			}
			break;
		case T_REQUEST_T_C:
			if ((result = en50221_tl_handle_request_tc(tl, slot_id, connection_id)) < 0) {
				return -1;
			}
			break;
		case T_DATA_MORE:
			if ((result = en50221_tl_handle_data_more(tl, slot_id,
			     					  connection_id,
								  data,
								  asn_data_length)) < 0) {
				return -1;
			}
			break;
		case T_DATA_LAST:
			if ((result = en50221_tl_handle_data_last(tl, slot_id,
			     					  connection_id,
								  data,
								  asn_data_length)) < 0) {
				return -1;
			}
			break;
		case T_SB:
			if ((result = en50221_tl_handle_sb(tl, slot_id,
			     				   connection_id,
							   data,
							   asn_data_length)) < 0) {
				return -1;
			}
			break;
		default:
			print(LOG_LEVEL, ERROR, 1,
			      "Recieved unexpected TPDU tag %02x from module on slot %02x\n",
			      tpdu_tag, slot_id);
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_BADCAMDATA;
			return -1;
		}

		// skip over the consumed data
		data += asn_data_length;
		data_length -= asn_data_length;
	}

	return 0;
}

static int en50221_tl_handle_create_tc_reply(struct en50221_transport_layer
					     *tl, uint8_t slot_id,
					     uint8_t connection_id)
{
	// set this connection to state active
	if (tl->slots[slot_id].connections[connection_id].state == T_STATE_IN_CREATION) {
		tl->slots[slot_id].connections[connection_id].state = T_STATE_ACTIVE;
		tl->slots[slot_id].connections[connection_id].tx_time.tv_sec = 0;

		// tell upper layers
		pthread_mutex_lock(&tl->setcallback_lock);
		en50221_tl_callback cb = tl->callback;
		void *cb_arg = tl->callback_arg;
		pthread_mutex_unlock(&tl->setcallback_lock);
		if (cb)
			cb(cb_arg, T_CALLBACK_REASON_CONNECTIONOPEN, NULL, 0, slot_id, connection_id);
	} else {
		print(LOG_LEVEL, ERROR, 1,
		      "Received T_C_T_C_REPLY for connection not in "
		      "T_STATE_IN_CREATION from module on slot %02x\n",
		      slot_id);
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCAMDATA;
		return -1;
	}

	return 0;
}

static int en50221_tl_handle_delete_tc(struct en50221_transport_layer *tl,
				       uint8_t slot_id,
				       uint8_t connection_id)
{
	// immediately delete this connection and send D_T_C_REPLY
	if (tl->slots[slot_id].connections[connection_id].state &
	    (T_STATE_ACTIVE | T_STATE_IN_DELETION)) {
		// clear down the slot
		tl->slots[slot_id].connections[connection_id].state = T_STATE_IDLE;
		if (tl->slots[slot_id].connections[connection_id].chain_buffer) {
			free(tl->slots[slot_id].connections[connection_id].chain_buffer);
		}
		tl->slots[slot_id].connections[connection_id].chain_buffer = NULL;
		tl->slots[slot_id].connections[connection_id].buffer_length = 0;

		// send the reply
		uint8_t hdr[3];
		hdr[0] = T_D_T_C_REPLY;
		hdr[1] = 1;
		hdr[2] = connection_id;
		if (dvbca_link_write(tl->slots[slot_id].ca_hndl,
		    		     tl->slots[slot_id].slot,
				     connection_id, hdr, 3) < 0) {
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_CAWRITE;
			return -1;
		}
		// tell upper layers
		pthread_mutex_lock(&tl->setcallback_lock);
		en50221_tl_callback cb = tl->callback;
		void *cb_arg = tl->callback_arg;
		pthread_mutex_unlock(&tl->setcallback_lock);
		if (cb)
			cb(cb_arg, T_CALLBACK_REASON_CONNECTIONCLOSE, NULL, 0, slot_id, connection_id);
	} else {
		print(LOG_LEVEL, ERROR, 1,
		      "Received T_DELETE_T_C for inactive connection from module on slot %02x\n",
		      slot_id);
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCAMDATA;
		return -1;
	}

	return 0;
}

static int en50221_tl_handle_delete_tc_reply(struct en50221_transport_layer
					     *tl, uint8_t slot_id,
					     uint8_t connection_id)
{
	// delete this connection, should be in T_STATE_IN_DELETION already
	if (tl->slots[slot_id].connections[connection_id].state == T_STATE_IN_DELETION) {
		tl->slots[slot_id].connections[connection_id].state = T_STATE_IDLE;
	} else {
		print(LOG_LEVEL, ERROR, 1,
		      "Received T_D_T_C_REPLY received for connection not in "
		      "T_STATE_IN_DELETION from module on slot %02x\n",
		      slot_id);
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCAMDATA;
		return -1;
	}

	return 0;
}

static int en50221_tl_handle_request_tc(struct en50221_transport_layer *tl,
					uint8_t slot_id,
					uint8_t connection_id)
{
	// allocate a new connection if possible
	int conid = en50221_tl_alloc_new_tc(tl, slot_id);
	int ca_hndl = tl->slots[slot_id].ca_hndl;
	if (conid == -1) {
		print(LOG_LEVEL, ERROR, 1,
		      "Too many connections requested by module on slot %02x\n",
		      slot_id);

		// send the error
		uint8_t hdr[4];
		hdr[0] = T_T_C_ERROR;
		hdr[1] = 2;
		hdr[2] = connection_id;
		hdr[3] = 1;
		if (dvbca_link_write(ca_hndl, tl->slots[slot_id].slot, connection_id, hdr, 4) < 0) {
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_CAWRITE;
			return -1;
		}
		tl->slots[slot_id].connections[connection_id].tx_time.
		    tv_sec = 0;
	} else {
		// send the NEW_T_C on the connection we received it on
		uint8_t hdr[4];
		hdr[0] = T_NEW_T_C;
		hdr[1] = 2;
		hdr[2] = connection_id;
		hdr[3] = conid;
		if (dvbca_link_write(ca_hndl, tl->slots[slot_id].slot, connection_id, hdr, 4) < 0) {
			tl->slots[slot_id].connections[conid].state = T_STATE_IDLE;
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_CAWRITE;
			return -1;
		}
		tl->slots[slot_id].connections[connection_id].tx_time.tv_sec = 0;

		// send the CREATE_T_C on the new connnection
		hdr[0] = T_CREATE_T_C;
		hdr[1] = 1;
		hdr[2] = conid;
		if (dvbca_link_write(ca_hndl, tl->slots[slot_id].slot, conid, hdr, 3) < 0) {
			tl->slots[slot_id].connections[conid].state = T_STATE_IDLE;
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_CAWRITE;
			return -1;
		}
		gettimeofday(&tl->slots[slot_id].connections[conid].tx_time, 0);

		// tell upper layers
		pthread_mutex_lock(&tl->setcallback_lock);
		en50221_tl_callback cb = tl->callback;
		void *cb_arg = tl->callback_arg;
		pthread_mutex_unlock(&tl->setcallback_lock);
		if (cb)
			cb(cb_arg, T_CALLBACK_REASON_CAMCONNECTIONOPEN, NULL, 0, slot_id, conid);
	}

	return 0;
}

static int en50221_tl_handle_data_more(struct en50221_transport_layer *tl,
				       uint8_t slot_id,
				       uint8_t connection_id,
				       uint8_t * data,
				       uint32_t data_length)
{
	// connection in correct state?
	if (tl->slots[slot_id].connections[connection_id].state != T_STATE_ACTIVE) {
		print(LOG_LEVEL, ERROR, 1,
		      "Received T_DATA_MORE for connection not in "
		      "T_STATE_ACTIVE from module on slot %02x\n",
		      slot_id);
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCAMDATA;
		return -1;
	}
	// a chained data packet is coming in, save
	// it to the buffer and wait for more
	tl->slots[slot_id].connections[connection_id].tx_time.tv_sec = 0;
	int new_data_length =
	    tl->slots[slot_id].connections[connection_id].buffer_length + data_length;
	uint8_t *new_data_buffer =
	    realloc(tl->slots[slot_id].connections[connection_id].chain_buffer, new_data_length);
	if (new_data_buffer == NULL) {
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_OUTOFMEMORY;
		return -1;
	}
	tl->slots[slot_id].connections[connection_id].chain_buffer = new_data_buffer;

	memcpy(tl->slots[slot_id].connections[connection_id].chain_buffer +
	       tl->slots[slot_id].connections[connection_id].buffer_length,
	       data, data_length);
	tl->slots[slot_id].connections[connection_id].buffer_length = new_data_length;

	return 0;
}

static int en50221_tl_handle_data_last(struct en50221_transport_layer *tl,
				       uint8_t slot_id,
				       uint8_t connection_id,
				       uint8_t * data,
				       uint32_t data_length)
{
	// connection in correct state?
	if (tl->slots[slot_id].connections[connection_id].state != T_STATE_ACTIVE) {
		print(LOG_LEVEL, ERROR, 1,
		      "Received T_DATA_LAST received for connection not in "
		      "T_STATE_ACTIVE from module on slot %02x\n",
		      slot_id);
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCAMDATA;
		return -1;
	}
	// last package of a chain or single package comes in
	tl->slots[slot_id].connections[connection_id].tx_time.tv_sec = 0;
	if (tl->slots[slot_id].connections[connection_id].chain_buffer == NULL) {
		// single package => dispatch immediately
		pthread_mutex_lock(&tl->setcallback_lock);
		en50221_tl_callback cb = tl->callback;
		void *cb_arg = tl->callback_arg;
		pthread_mutex_unlock(&tl->setcallback_lock);

		if (cb && data_length) {
			pthread_mutex_unlock(&tl->slots[slot_id].
					     slot_lock);
			cb(cb_arg, T_CALLBACK_REASON_DATA, data, data_length, slot_id, connection_id);
			pthread_mutex_lock(&tl->slots[slot_id].slot_lock);
		}
	} else {
		int new_data_length =
		    tl->slots[slot_id].connections[connection_id].buffer_length + data_length;
		uint8_t *new_data_buffer =
		    realloc(tl->slots[slot_id].connections[connection_id].chain_buffer, new_data_length);
		if (new_data_buffer == NULL) {
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_OUTOFMEMORY;
			return -1;
		}

		memcpy(new_data_buffer +
		       tl->slots[slot_id].connections[connection_id].
		       buffer_length, data, data_length);

		// clean the buffer position
		tl->slots[slot_id].connections[connection_id].chain_buffer = NULL;
		tl->slots[slot_id].connections[connection_id].buffer_length = 0;

		// tell the upper layers
		pthread_mutex_lock(&tl->setcallback_lock);
		en50221_tl_callback cb = tl->callback;
		void *cb_arg = tl->callback_arg;
		pthread_mutex_unlock(&tl->setcallback_lock);
		if (cb && data_length) {
			pthread_mutex_unlock(&tl->slots[slot_id].
					     slot_lock);
			cb(cb_arg, T_CALLBACK_REASON_DATA, new_data_buffer,
			   new_data_length, slot_id, connection_id);
			pthread_mutex_lock(&tl->slots[slot_id].slot_lock);
		}

		free(new_data_buffer);
	}

	return 0;
}

static int en50221_tl_handle_sb(struct en50221_transport_layer *tl,
				uint8_t slot_id, uint8_t connection_id,
				uint8_t * data, uint32_t data_length)
{
	// is the connection id ok?
	if (tl->slots[slot_id].connections[connection_id].state != T_STATE_ACTIVE) {
		print(LOG_LEVEL, ERROR, 1,
		      "Received T_SB for connection not in T_STATE_ACTIVE from module on slot %02x\n",
		      slot_id);
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCAMDATA;
		return -1;
	}
	// did we get enough data in the T_SB?
	if (data_length != 1) {
		print(LOG_LEVEL, ERROR, 1,
		      "Recieved T_SB with invalid length from module on slot %02x\n",
		      slot_id);
		tl->error_slot = slot_id;
		tl->error = EN50221ERR_BADCAMDATA;
		return -1;
	}
	// tell it to send the data if it says there is some
	if (data[0] & 0x80) {
		int ca_hndl = tl->slots[slot_id].ca_hndl;

		// send the RCV
		uint8_t hdr[3];
		hdr[0] = T_RCV;
		hdr[1] = 1;
		hdr[2] = connection_id;
		if (dvbca_link_write(ca_hndl, tl->slots[slot_id].slot, connection_id, hdr, 3) < 0) {
			tl->error_slot = slot_id;
			tl->error = EN50221ERR_CAWRITE;
			return -1;
		}
		gettimeofday(&tl->slots[slot_id].connections[connection_id].tx_time, 0);

	} else {
		// no data - indicate not waiting for anything now
		tl->slots[slot_id].connections[connection_id].tx_time.tv_sec = 0;
	}

	return 0;
}

static int en50221_tl_alloc_new_tc(struct en50221_transport_layer *tl,
				   uint8_t slot_id)
{
	// we browse through the array of connection
	// types, to look for the first unused one
	int i, conid = -1;
	for (i = 1; i < tl->max_connections_per_slot; i++) {
		if (tl->slots[slot_id].connections[i].state == T_STATE_IDLE) {
			conid = i;
			break;
		}
	}
	if (conid == -1) {
		print(LOG_LEVEL, ERROR, 1,
		      "CREATE_T_C failed: no more connections available\n");
		return -1;
	}
	// set up the connection struct
	tl->slots[slot_id].connections[conid].state = T_STATE_IN_CREATION;
	tl->slots[slot_id].connections[conid].chain_buffer = NULL;
	tl->slots[slot_id].connections[conid].buffer_length = 0;

	return conid;
}

static void queue_message(struct en50221_transport_layer *tl,
			  uint8_t slot_id, uint8_t connection_id,
			  struct en50221_message *msg)
{
	msg->next = NULL;
	if (tl->slots[slot_id].connections[connection_id].send_queue_tail) {
		tl->slots[slot_id].connections[connection_id].send_queue_tail->next = msg;
		tl->slots[slot_id].connections[connection_id].send_queue_tail = msg;
	} else {
		tl->slots[slot_id].connections[connection_id].send_queue = msg;
		tl->slots[slot_id].connections[connection_id].send_queue_tail = msg;
	}
}
