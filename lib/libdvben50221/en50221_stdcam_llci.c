/*
	en50221 encoder An implementation for libdvb
	an implementation for the en50221 transport layer

	Copyright (C) 2006 Andrew de Quincey (adq_dvb@lidskialf.net)

	This program is free software; you can redistribute it and/or modify
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
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <libdvbapi/dvbca.h>
#include <libdvbmisc/dvbmisc.h>
#include "en50221_app_rm.h"
#include "en50221_app_datetime.h"
#include "en50221_app_utils.h"
#include "en50221_app_tags.h"
#include "en50221_stdcam.h"

#define LLCI_RESPONSE_TIMEOUT_MS 1000
#define LLCI_POLL_DELAY_MS 100

/* resource IDs we support */
static uint32_t resource_ids[] =
{ 	EN50221_APP_RM_RESOURCEID,
	EN50221_APP_CA_RESOURCEID,
	EN50221_APP_AI_RESOURCEID,
	EN50221_APP_MMI_RESOURCEID,
	EN50221_APP_DATETIME_RESOURCEID,
};
#define RESOURCE_IDS_COUNT sizeof(resource_ids)/4

struct llci_resource {
	struct en50221_app_public_resource_id resid;
	uint32_t binary_resource_id;
	en50221_sl_resource_callback callback;
	void *arg;
};

struct en50221_stdcam_llci {
	struct en50221_stdcam stdcam;

	int cafd;
	int slotnum;
	int state;

	struct llci_resource resources[RESOURCE_IDS_COUNT];

	struct en50221_transport_layer *tl;
	struct en50221_session_layer *sl;
	struct en50221_app_send_functions sendfuncs;
	int tl_slot_id;

	struct en50221_app_rm *rm_resource;

	struct en50221_app_datetime *datetime_resource;
	int datetime_session_number;
	uint8_t datetime_response_interval;
	time_t datetime_next_send;
	time_t datetime_dvbtime;
};

static enum en50221_stdcam_status en50221_stdcam_llci_poll(struct en50221_stdcam *stdcam);
static void en50221_stdcam_llci_dvbtime(struct en50221_stdcam *stdcam, time_t dvbtime);
static void en50221_stdcam_llci_destroy(struct en50221_stdcam *stdcam, int closefd);
static void llci_cam_added(struct en50221_stdcam_llci *llci);
static void llci_cam_in_reset(struct en50221_stdcam_llci *llci);
static void llci_cam_removed(struct en50221_stdcam_llci *llci);


static int llci_lookup_callback(void *arg, uint8_t _slot_id, uint32_t requested_resource_id,
				en50221_sl_resource_callback *callback_out, void **arg_out,
				uint32_t *connected_resource_id);
static int llci_session_callback(void *arg, int reason, uint8_t _slot_id, uint16_t session_number, uint32_t resource_id);
static int llci_rm_enq_callback(void *arg, uint8_t _slot_id, uint16_t session_number);
static int llci_rm_reply_callback(void *arg, uint8_t _slot_id, uint16_t session_number, uint32_t resource_id_count, uint32_t *_resource_ids);
static int llci_rm_changed_callback(void *arg, uint8_t _slot_id, uint16_t session_number);

static int llci_datetime_enquiry_callback(void *arg, uint8_t _slot_id, uint16_t session_number, uint8_t response_interval);


struct en50221_stdcam *en50221_stdcam_llci_create(int cafd, int slotnum,
						  struct en50221_transport_layer *tl,
						  struct en50221_session_layer *sl)
{
	// try and allocate space for the LLCI stdcam
	struct en50221_stdcam_llci *llci =
		malloc(sizeof(struct en50221_stdcam_llci));
	if (llci == NULL) {
		return NULL;
	}
	memset(llci, 0, sizeof(struct en50221_stdcam_llci));

	// create the sendfuncs
	llci->sendfuncs.arg  = sl;
	llci->sendfuncs.send_data  = (en50221_send_data) en50221_sl_send_data;
	llci->sendfuncs.send_datav = (en50221_send_datav) en50221_sl_send_datav;

	// create the resource manager resource
	int resource_idx = 0;
	llci->rm_resource = en50221_app_rm_create(&llci->sendfuncs);
	en50221_app_decode_public_resource_id(&llci->resources[resource_idx].resid, EN50221_APP_RM_RESOURCEID);
	llci->resources[resource_idx].binary_resource_id = EN50221_APP_RM_RESOURCEID;
	llci->resources[resource_idx].callback = (en50221_sl_resource_callback) en50221_app_rm_message;
	llci->resources[resource_idx].arg = llci->rm_resource;
	en50221_app_rm_register_enq_callback(llci->rm_resource, llci_rm_enq_callback, llci);
	en50221_app_rm_register_reply_callback(llci->rm_resource, llci_rm_reply_callback, llci);
	en50221_app_rm_register_changed_callback(llci->rm_resource, llci_rm_changed_callback, llci);
	resource_idx++;

	// create the datetime resource
	llci->datetime_resource = en50221_app_datetime_create(&llci->sendfuncs);
	en50221_app_decode_public_resource_id(&llci->resources[resource_idx].resid, EN50221_APP_DATETIME_RESOURCEID);
	llci->resources[resource_idx].binary_resource_id = EN50221_APP_DATETIME_RESOURCEID;
	llci->resources[resource_idx].callback = (en50221_sl_resource_callback) en50221_app_datetime_message;
	llci->resources[resource_idx].arg = llci->datetime_resource;
	en50221_app_datetime_register_enquiry_callback(llci->datetime_resource, llci_datetime_enquiry_callback, llci);
	resource_idx++;
	llci->datetime_session_number = -1;
	llci->datetime_response_interval = 0;
	llci->datetime_next_send = 0;
	llci->datetime_dvbtime = 0;

	// create the application information resource
	llci->stdcam.ai_resource = en50221_app_ai_create(&llci->sendfuncs);
	en50221_app_decode_public_resource_id(&llci->resources[resource_idx].resid, EN50221_APP_AI_RESOURCEID);
	llci->resources[resource_idx].binary_resource_id = EN50221_APP_AI_RESOURCEID;
	llci->resources[resource_idx].callback = (en50221_sl_resource_callback) en50221_app_ai_message;
	llci->resources[resource_idx].arg = llci->stdcam.ai_resource;
	llci->stdcam.ai_session_number = -1;
	resource_idx++;

	// create the CA resource
	llci->stdcam.ca_resource = en50221_app_ca_create(&llci->sendfuncs);
	en50221_app_decode_public_resource_id(&llci->resources[resource_idx].resid, EN50221_APP_CA_RESOURCEID);
	llci->resources[resource_idx].binary_resource_id = EN50221_APP_CA_RESOURCEID;
	llci->resources[resource_idx].callback = (en50221_sl_resource_callback) en50221_app_ca_message;
	llci->resources[resource_idx].arg = llci->stdcam.ca_resource;
	llci->stdcam.ca_session_number = -1;
	resource_idx++;

	// create the MMI resource
	llci->stdcam.mmi_resource = en50221_app_mmi_create(&llci->sendfuncs);
	en50221_app_decode_public_resource_id(&llci->resources[resource_idx].resid, EN50221_APP_MMI_RESOURCEID);
	llci->resources[resource_idx].binary_resource_id = EN50221_APP_MMI_RESOURCEID;
	llci->resources[resource_idx].callback = (en50221_sl_resource_callback) en50221_app_mmi_message;
	llci->resources[resource_idx].arg = llci->stdcam.mmi_resource;
	llci->stdcam.mmi_session_number = -1;
	resource_idx++;

	// register session layer callbacks
	en50221_sl_register_lookup_callback(sl, llci_lookup_callback, llci);
	en50221_sl_register_session_callback(sl, llci_session_callback, llci);

	// done
	llci->stdcam.destroy = en50221_stdcam_llci_destroy;
	llci->stdcam.poll = en50221_stdcam_llci_poll;
	llci->stdcam.dvbtime = en50221_stdcam_llci_dvbtime;
	llci->cafd = cafd;
	llci->slotnum = slotnum;
	llci->tl = tl;
	llci->sl = sl;
	llci->tl_slot_id = -1;
	llci->state = EN50221_STDCAM_CAM_NONE;
	return &llci->stdcam;
}

static void en50221_stdcam_llci_dvbtime(struct en50221_stdcam *stdcam, time_t dvbtime)
{
	struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) stdcam;

	llci->datetime_dvbtime = dvbtime;
}

static void en50221_stdcam_llci_destroy(struct en50221_stdcam *stdcam, int closefd)
{
	struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) stdcam;

	// "remove" the cam
	llci_cam_removed(llci);

	// destroy resources
	if (llci->rm_resource)
		en50221_app_rm_destroy(llci->rm_resource);
	if (llci->datetime_resource)
		en50221_app_datetime_destroy(llci->datetime_resource);
	if (llci->stdcam.ai_resource)
		en50221_app_ai_destroy(llci->stdcam.ai_resource);
	if (llci->stdcam.ca_resource)
		en50221_app_ca_destroy(llci->stdcam.ca_resource);
	if (llci->stdcam.mmi_resource)
		en50221_app_mmi_destroy(llci->stdcam.mmi_resource);

	if (closefd)
		close(llci->cafd);

	free(llci);
}




static enum en50221_stdcam_status en50221_stdcam_llci_poll(struct en50221_stdcam *stdcam)
{
	struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) stdcam;

	switch(dvbca_get_cam_state(llci->cafd, llci->slotnum)) {
	case DVBCA_CAMSTATE_MISSING:
		if (llci->state != EN50221_STDCAM_CAM_NONE)
			llci_cam_removed(llci);
		break;

	case DVBCA_CAMSTATE_READY:
		if (llci->state == EN50221_STDCAM_CAM_NONE)
			llci_cam_added(llci);
		else if (llci->state == EN50221_STDCAM_CAM_INRESET)
			llci_cam_in_reset(llci);
		break;
	}

	// poll the stack
	int error;
	if ((error = en50221_tl_poll(llci->tl)) != 0) {
		print(LOG_LEVEL, ERROR, 1, "Error reported by stack:%i\n", en50221_tl_get_error(llci->tl));
	}

	// send date/time response
	if (llci->datetime_session_number != -1) {
		time_t cur_time = time(NULL);
		if (llci->datetime_response_interval && (cur_time > llci->datetime_next_send)) {
			en50221_app_datetime_send(llci->datetime_resource,
						llci->datetime_session_number,
						llci->datetime_dvbtime, 0);
			llci->datetime_next_send = cur_time + llci->datetime_response_interval;
		}
	}

	return llci->state;
}

static void llci_cam_added(struct en50221_stdcam_llci *llci)
{
	// clear down any old structures
	if (llci->tl_slot_id != -1) {
		llci_cam_removed(llci);
	}

	// reset the CAM
	dvbca_reset(llci->cafd, llci->slotnum);
	llci->state = EN50221_STDCAM_CAM_INRESET;
}

static void llci_cam_in_reset(struct en50221_stdcam_llci *llci)
{
	if (dvbca_get_cam_state(llci->cafd, llci->slotnum) != DVBCA_CAMSTATE_READY) {
		return;
	}

	// register the slot
	if ((llci->tl_slot_id = en50221_tl_register_slot(llci->tl, llci->cafd, llci->slotnum,
	      						 LLCI_RESPONSE_TIMEOUT_MS, LLCI_POLL_DELAY_MS)) < 0) {
		llci->state = EN50221_STDCAM_CAM_BAD;
		return;
	}

	// create a new connection on the slot
	if (en50221_tl_new_tc(llci->tl, llci->tl_slot_id) < 0) {
		llci->state = EN50221_STDCAM_CAM_BAD;
		llci->tl_slot_id = -1;
		en50221_tl_destroy_slot(llci->tl, llci->tl_slot_id);
		return;
	}

	llci->state = EN50221_STDCAM_CAM_OK;
}

static void llci_cam_removed(struct en50221_stdcam_llci *llci)
{
	if (llci->tl_slot_id != -1) {
		en50221_tl_destroy_slot(llci->tl, llci->tl_slot_id);
		llci->tl_slot_id = -1;
		llci->datetime_session_number = -1;
		llci->stdcam.ai_session_number = -1;
		llci->stdcam.ca_session_number = -1;
		llci->stdcam.mmi_session_number = -1;
	}
	llci->state = EN50221_STDCAM_CAM_NONE;
}



static int llci_lookup_callback(void *arg, uint8_t _slot_id, uint32_t requested_resource_id,
				en50221_sl_resource_callback *callback_out, void **arg_out,
				uint32_t *connected_resource_id)
{
	struct en50221_app_public_resource_id resid;
	struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) arg;
	(void) _slot_id;

	// decode the resource id
	if (!en50221_app_decode_public_resource_id(&resid, requested_resource_id)) {
		return -1;
	}

	// try and find an instance of the resource
	uint32_t i;
	for(i=0; i<RESOURCE_IDS_COUNT; i++) {
		if ((resid.resource_class == llci->resources[i].resid.resource_class) &&
		    (resid.resource_type == llci->resources[i].resid.resource_type)) {

			// limit sessions to certain resources
			switch(requested_resource_id) {
			case EN50221_APP_DATETIME_RESOURCEID:
				if (llci->datetime_session_number != -1)
					return -3;
				break;
			case EN50221_APP_AI_RESOURCEID:
				if (llci->stdcam.ai_session_number != -1)
					return -3;
				break;
			case EN50221_APP_CA_RESOURCEID:
				if (llci->stdcam.ca_session_number != -1)
					return -3;
				break;
			case EN50221_APP_MMI_RESOURCEID:
				if (llci->stdcam.mmi_session_number != -1)
					return -3;
				break;
			}

			// resource is ok.
			*callback_out = llci->resources[i].callback;
			*arg_out = llci->resources[i].arg;
			*connected_resource_id = llci->resources[i].binary_resource_id;
			return 0;
		}
	}

	return -1;
}

static int llci_session_callback(void *arg, int reason, uint8_t _slot_id, uint16_t session_number, uint32_t resource_id)
{
	struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) arg;
	(void) _slot_id;

	switch(reason) {
	case S_SCALLBACK_REASON_CAMCONNECTED:
		if (resource_id == EN50221_APP_RM_RESOURCEID) {
			en50221_app_rm_enq(llci->rm_resource, session_number);
		} else if (resource_id == EN50221_APP_DATETIME_RESOURCEID) {
			llci->datetime_session_number = session_number;
		} else if (resource_id == EN50221_APP_AI_RESOURCEID) {
			en50221_app_ai_enquiry(llci->stdcam.ai_resource, session_number);
			llci->stdcam.ai_session_number = session_number;
		} else if (resource_id == EN50221_APP_CA_RESOURCEID) {
			en50221_app_ca_info_enq(llci->stdcam.ca_resource, session_number);
			llci->stdcam.ca_session_number = session_number;
		} else if (resource_id == EN50221_APP_MMI_RESOURCEID) {
			llci->stdcam.mmi_session_number = session_number;
		}

		break;
    case S_SCALLBACK_REASON_CLOSE:
        if (resource_id == EN50221_APP_MMI_RESOURCEID) {
            llci->stdcam.mmi_session_number = -1;
        }

        break;
	}
	return 0;
}

static int llci_rm_enq_callback(void *arg, uint8_t _slot_id, uint16_t session_number)
{
	struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) arg;
	(void) _slot_id;

	if (en50221_app_rm_reply(llci->rm_resource, session_number, RESOURCE_IDS_COUNT, resource_ids)) {
		print(LOG_LEVEL, ERROR, 1, "Failed to send RM ENQ on slot %02x\n", _slot_id);
	}
	return 0;
}

static int llci_rm_reply_callback(void *arg, uint8_t _slot_id, uint16_t session_number, uint32_t resource_id_count, uint32_t *_resource_ids)
{
	struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) arg;
	(void) _slot_id;
	(void) resource_id_count;
	(void) _resource_ids;

	if (en50221_app_rm_changed(llci->rm_resource, session_number)) {
		print(LOG_LEVEL, ERROR, 1, "Failed to send RM REPLY on slot %02x\n", _slot_id);
	}
	return 0;
}

static int llci_rm_changed_callback(void *arg, uint8_t _slot_id, uint16_t session_number)
{
	struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) arg;
	(void) _slot_id;

	if (en50221_app_rm_enq(llci->rm_resource, session_number)) {
		print(LOG_LEVEL, ERROR, 1, "Failed to send RM CHANGED on slot %02x\n", _slot_id);
	}
	return 0;
}

static int llci_datetime_enquiry_callback(void *arg, uint8_t _slot_id, uint16_t session_number, uint8_t response_interval)
{
	struct en50221_stdcam_llci *llci = (struct en50221_stdcam_llci *) arg;
	(void) _slot_id;

	llci->datetime_response_interval = response_interval;
	llci->datetime_next_send = 0;
	if (response_interval) {
		llci->datetime_next_send = time(NULL) + response_interval;
	}
	en50221_app_datetime_send(llci->datetime_resource, session_number, llci->datetime_dvbtime, 0);

	return 0;
}
