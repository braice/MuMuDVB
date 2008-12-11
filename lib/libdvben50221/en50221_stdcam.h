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

#ifndef EN50221_STDCAM_H
#define EN50221_STDCAM_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <libdvben50221/en50221_app_ai.h>
#include <libdvben50221/en50221_app_ca.h>
#include <libdvben50221/en50221_app_mmi.h>
#include <libdvben50221/en50221_session.h>
#include <libdvben50221/en50221_transport.h>

enum en50221_stdcam_status {
	EN50221_STDCAM_CAM_NONE,
	EN50221_STDCAM_CAM_INRESET,
	EN50221_STDCAM_CAM_OK,
	EN50221_STDCAM_CAM_BAD,
};

struct en50221_stdcam {
	/* one of more of the following may be NULL if a CAM does not support it */
	struct en50221_app_ai *ai_resource;
	struct en50221_app_ca *ca_resource;
	struct en50221_app_mmi *mmi_resource;

	/* if any of these are -1, no connection is in place to this resource yet */
	int ai_session_number;
	int ca_session_number;
	int mmi_session_number;

	/* poll the stdcam instance */
	enum en50221_stdcam_status (*poll)(struct en50221_stdcam *stdcam);

	/* inform the stdcam of the current DVB time */
	void (*dvbtime)(struct en50221_stdcam *stdcam, time_t dvbtime);

	/* destroy the stdcam instance */
	void (*destroy)(struct en50221_stdcam *stdcam, int closefd);
};

/**
 * Create an instance of the STDCAM for an LLCI interface.
 *
 * @param cafd FD of the CA device.
 * @param slotnum Slotnum on that CA device.
 * @param tl Transport layer instance to use.
 * @param sl Session layer instance to use.
 * @return en50221_stdcam instance, or NULL on error.
 */
extern struct en50221_stdcam *en50221_stdcam_llci_create(int cafd, int slotnum,
						  struct en50221_transport_layer *tl,
						  struct en50221_session_layer *sl);

/**
 * Create an instance of the STDCAM for an HLCI interface.
 *
 * @param cafd FD of the CA device.
 * @param slotnum Slotnum on that CA device.
 * @return en50221_stdcam instance, or NULL on error.
 */
extern struct en50221_stdcam *en50221_stdcam_hlci_create(int cafd, int slotnum);

/**
 * Convenience method to create a STDCAM interface for a ca device on a particular adapter.
 *
 * @param adapter The DVB adapter concerned.
 * @param slotnum The ca slot number on that adapter.
 * @param tl Transport layer instance to use (unused for HLCI cams).
 * @param sl Session layer instance to use (unused for HLCI cams).
 * @return en50221_stdcam instance, or NULL on error.
 */
extern struct en50221_stdcam *en50221_stdcam_create(int adapter, int slotnum,
						    struct en50221_transport_layer *tl,
						    struct en50221_session_layer *sl);

#ifdef __cplusplus
}
#endif

#endif
