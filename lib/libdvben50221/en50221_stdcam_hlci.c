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
#include "en50221_app_utils.h"
#include "en50221_app_tags.h"
#include "en50221_stdcam.h"


struct en50221_stdcam_hlci {
	struct en50221_stdcam stdcam;

	int cafd;
	int slotnum;
	int initialised;
	struct en50221_app_send_functions sendfuncs;
};

static void en50221_stdcam_hlci_destroy(struct en50221_stdcam *stdcam, int closefd);
static enum en50221_stdcam_status en50221_stdcam_hlci_poll(struct en50221_stdcam *stdcam);
static int hlci_cam_added(struct en50221_stdcam_hlci *hlci);
static int hlci_send_data(void *arg, uint16_t session_number,
			  uint8_t * data, uint16_t data_length);
static int hlci_send_datav(void *arg, uint16_t session_number,
			   struct iovec *vector, int iov_count);




struct en50221_stdcam *en50221_stdcam_hlci_create(int cafd, int slotnum)
{
	// try and allocate space for the HLCI stdcam
	struct en50221_stdcam_hlci *hlci =
		malloc(sizeof(struct en50221_stdcam_hlci));
	if (hlci == NULL) {
		return NULL;
	}
	memset(hlci, 0, sizeof(struct en50221_stdcam_hlci));

	// create the sendfuncs
	hlci->sendfuncs.arg = hlci;
	hlci->sendfuncs.send_data = hlci_send_data;
	hlci->sendfuncs.send_datav = hlci_send_datav;

	// create the resources (NOTE: we just use fake session numbers here)
	hlci->stdcam.ai_resource = en50221_app_ai_create(&hlci->sendfuncs);
	hlci->stdcam.ai_session_number = 0;
	hlci->stdcam.ca_resource = en50221_app_ca_create(&hlci->sendfuncs);
	hlci->stdcam.ca_session_number = 1;
//      hlci->stdcam.mmi_resource = en50221_app_mmi_create(&hlci->sendfuncs);
	hlci->stdcam.mmi_session_number = -1;

	// done
	hlci->stdcam.destroy = en50221_stdcam_hlci_destroy;
	hlci->stdcam.poll = en50221_stdcam_hlci_poll;
	hlci->slotnum = slotnum;
	hlci->cafd = cafd;
	return &hlci->stdcam;
}

static void en50221_stdcam_hlci_destroy(struct en50221_stdcam *stdcam, int closefd)
{
	struct en50221_stdcam_hlci *hlci = (struct en50221_stdcam_hlci *) stdcam;

	if (hlci->stdcam.ai_resource)
		en50221_app_ai_destroy(hlci->stdcam.ai_resource);
	if (hlci->stdcam.ca_resource)
		en50221_app_ca_destroy(hlci->stdcam.ca_resource);
	if (hlci->stdcam.mmi_resource)
		en50221_app_mmi_destroy(hlci->stdcam.mmi_resource);

	if (closefd)
		close(hlci->cafd);

	free(hlci);
}

static enum en50221_stdcam_status en50221_stdcam_hlci_poll(struct en50221_stdcam *stdcam)
{
	struct en50221_stdcam_hlci *hlci = (struct en50221_stdcam_hlci *) stdcam;

	switch(dvbca_get_cam_state(hlci->cafd, hlci->slotnum)) {
	case DVBCA_CAMSTATE_MISSING:
		hlci->initialised = 0;
		break;

	case DVBCA_CAMSTATE_READY:
	case DVBCA_CAMSTATE_INITIALISING:
		if (!hlci->initialised)
			hlci_cam_added(hlci);
		break;
	}

	// delay to prevent busy loop
	usleep(10);

	if (!hlci->initialised) {
		return EN50221_STDCAM_CAM_NONE;
	}
	return EN50221_STDCAM_CAM_OK;
}



static int hlci_cam_added(struct en50221_stdcam_hlci *hlci)
{
	uint8_t buf[256];
	int size;

	// get application information
	if (en50221_app_ai_enquiry(hlci->stdcam.ai_resource, 0)) {
		return -EIO;
	}
	if ((size = dvbca_hlci_read(hlci->cafd, TAG_APP_INFO, buf, sizeof(buf))) < 0) {
		return size;
	}
	if (en50221_app_ai_message(hlci->stdcam.ai_resource, 0, 0, EN50221_APP_AI_RESOURCEID, buf, size)) {
		return -EIO;
	}

	// we forge a fake CA_INFO here so the main app works - since it will expect a CA_INFO
	// this will be replaced with a proper call (below) when the driver support is there
	buf[0] = TAG_CA_INFO >> 16;
	buf[1] = (uint8_t) (TAG_CA_INFO >> 8);
	buf[2] = (uint8_t) TAG_CA_INFO;
	buf[3] = 0;
	if (en50221_app_ca_message(hlci->stdcam.ca_resource, 0, 0, EN50221_APP_CA_RESOURCEID, buf, 4)) {
		return -EIO;
	}

	/*
	// get CA information
	   if (en50221_app_ca_info_enq(ca_resource, 0)) {
	   fprintf(stderr, "Failed to send CA INFO enquiry\n");
	   cafd = -1;
	   return -1;
	   }
	   if ((size = dvbca_hlci_read(cafd, TAG_CA_INFO, buf, sizeof(buf))) < 0) {
	   fprintf(stderr, "Failed to read CA INFO\n");
	   cafd = -1;
	   return -1;
	   }
	   if (en50221_app_ca_message(ca_resource, 0, 0, EN50221_APP_CA_RESOURCEID, buf, size)) {
	   fprintf(stderr, "Failed to parse CA INFO\n");
	   cafd = -1;
	   return -1;
	   }
	 */

	// done
	hlci->initialised = 1;
	return 0;
}

static int hlci_send_data(void *arg, uint16_t session_number,
			  uint8_t * data, uint16_t data_length)
{
	(void) session_number;
	struct en50221_stdcam_hlci *hlci = arg;

	return dvbca_hlci_write(hlci->cafd, data, data_length);
}

static int hlci_send_datav(void *arg, uint16_t session_number,
			   struct iovec *vector, int iov_count)
{
	(void) session_number;
	struct en50221_stdcam_hlci *hlci = arg;

	// calculate the total length of the data to send
	uint32_t data_size = 0;
	int i;
	for (i = 0; i < iov_count; i++) {
		data_size += vector[i].iov_len;
	}

	// allocate memory for it
	uint8_t *buf = malloc(data_size);
	if (buf == NULL) {
		return -1;
	}
	// merge the iovecs
	uint32_t pos = 0;
	for (i = 0; i < iov_count; i++) {
		memcpy(buf + pos, vector[i].iov_base, vector[i].iov_len);
		pos += vector[i].iov_len;
	}

	// sendit and cleanup
	int status = dvbca_hlci_write(hlci->cafd, buf, data_size);
	free(buf);
	return status;
}
