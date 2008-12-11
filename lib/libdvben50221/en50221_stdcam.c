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
#include "en50221_stdcam.h"

struct en50221_stdcam *en50221_stdcam_create(int adapter, int slotnum,
					     struct en50221_transport_layer *tl,
					     struct en50221_session_layer *sl)
{
	struct en50221_stdcam *result = NULL;

	int cafd = dvbca_open(adapter, 0);
	if (cafd == -1)
		return NULL;

	int ca_type = dvbca_get_interface_type(cafd, slotnum);
	switch(ca_type) {
	case DVBCA_INTERFACE_LINK:
		result = en50221_stdcam_llci_create(cafd, slotnum, tl, sl);
		break;

	case DVBCA_INTERFACE_HLCI:
		result = en50221_stdcam_hlci_create(cafd, slotnum);
		break;
	}

	if (result == NULL)
		close(cafd);
	return result;
}
