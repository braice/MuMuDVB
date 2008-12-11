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

#ifndef __EN50221_APP_UTILS_H__
#define __EN50221_APP_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <sys/uio.h>

/**
 * A decomposed public resource structure.
 *
 * we will ignore private resource (resource_id_type==3),
 * because they are not used by any modules at all and
 * would need special code for any private resource anyway.
 */
struct en50221_app_public_resource_id {
	uint16_t resource_class;
	uint16_t resource_type;
	uint8_t resource_version;
};

typedef int (*en50221_send_data) (void *arg,
				  uint16_t session_number,
				  uint8_t * data,
				  uint16_t data_length);
typedef int (*en50221_send_datav) (void *arg,
				   uint16_t session_number,
				   struct iovec * vector,
				   int iov_count);

/**
 * An abstraction away from hardcoded send functions so different layers may be
 * slotted in under the application layer.
 */
struct en50221_app_send_functions {
	/**
	 * Argument to pass to these functions.
	 */
	void *arg;

	/**
	 * Send data.
	 */
	en50221_send_data send_data;

	/**
	 * Send vector data.
	 */
	en50221_send_datav send_datav;
};

/**
 * Make a host-endian uint32_t formatted resource id.
 *
 * @param CLASS Class of resource.
 * @param TYPE Type of resource.
 * @param VERSION Version of resource.
 * @return Formatted resource id.
 */
#define MKRID(CLASS, TYPE, VERSION) ((((CLASS)&0xffff)<<16) | (((TYPE)&0x3ff)<<6) | ((VERSION)&0x3f))

/**
 * Decode a host-endian public resource_id into an en50221_app_public_resource_id structure.
 *
 * @param idf Structure to write decoded resource_id into.
 * @param resource_id ID to decode.
 * @return Pointer to idf on success, or NULL if this is not a public resource.
 */
struct en50221_app_public_resource_id *
	en50221_app_decode_public_resource_id(struct en50221_app_public_resource_id *idf,
					      uint32_t resource_id);

/**
 * Encode an en50221_app_public_resource_id structure into a host-endian uint32_t.
 *
 * @param idf Structure to encode.
 * @return The encoded value
 */
static inline uint32_t en50221_app_encode_public_resource_id(struct en50221_app_public_resource_id *idf) {
	return MKRID(idf->resource_class, idf->resource_type, idf->resource_version);
}

#ifdef __cplusplus
}
#endif
#endif
