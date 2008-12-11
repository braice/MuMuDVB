/*
	ASN.1 routines, implementation for libdvben50221
	an implementation for the High Level Common Interface

	Copyright (C) 2004, 2005 Manu Abraham <abraham.manu@gmail.com>
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
#include "asn_1.h"

int asn_1_decode(uint16_t * length, uint8_t * asn_1_array,
		 uint32_t asn_1_array_len)
{
	uint8_t length_field;

	if (asn_1_array_len < 1)
		return -1;
	length_field = asn_1_array[0];

	if (length_field < 0x80) {
		// there is only one word
		*length = length_field & 0x7f;
		return 1;
	} else if (length_field == 0x81) {
		if (asn_1_array_len < 2)
			return -1;

		*length = asn_1_array[1];
		return 2;
	} else if (length_field == 0x82) {
		if (asn_1_array_len < 3)
			return -1;

		*length = (asn_1_array[1] << 8) | asn_1_array[2];
		return 3;
	}

	return -1;
}

int asn_1_encode(uint16_t length, uint8_t * asn_1_array,
		 uint32_t asn_1_array_len)
{
	if (length < 0x80) {
		if (asn_1_array_len < 1)
			return -1;

		asn_1_array[0] = length & 0x7f;
		return 1;
	} else if (length < 0x100) {
		if (asn_1_array_len < 2)
			return -1;

		asn_1_array[0] = 0x81;
		asn_1_array[1] = length;
		return 2;
	} else {
		if (asn_1_array_len < 3)
			return -1;

		asn_1_array[0] = 0x82;
		asn_1_array[1] = length >> 8;
		asn_1_array[2] = length;
		return 3;
	}

	// never reached
}
