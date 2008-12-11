/*
    en50221 encoder An implementation for libdvb
    an implementation for the en50221 session layer

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

#ifndef EN50221_ERRNO
#define EN50221_ERRNO 1

#ifdef __cplusplus
extern "C" {
#endif

#define EN50221ERR_CAREAD -1	/* error during read from CA device. */
#define EN50221ERR_CAWRITE -2	/* error during write to CA device. */
#define EN50221ERR_TIMEOUT -3	/* timeout occured waiting for a response from a device. */
#define EN50221ERR_BADSLOTID -4	/* bad slot ID supplied by user - the offending slot_id will not be set. */
#define EN50221ERR_BADCONNECTIONID -5	/* bad connection ID supplied by user. */
#define EN50221ERR_BADSTATE -6	/* slot/connection in the wrong state. */
#define EN50221ERR_BADCAMDATA -7	/* CAM supplied an invalid request. */
#define EN50221ERR_OUTOFMEMORY -8	/* memory allocation failed. */
#define EN50221ERR_ASNENCODE -9	/* ASN.1 encode failure - indicates library bug. */
#define EN50221ERR_OUTOFCONNECTIONS -10	/* no more connections available. */
#define EN50221ERR_OUTOFSLOTS -11	/* no more slots available - the offending slot_id will not be set. */
#define EN50221ERR_IOVLIMIT -12	/* Too many struct iovecs were used. */
#define EN50221ERR_BADSESSIONNUMBER -13	/* Bad session number suppplied by user. */
#define EN50221ERR_OUTOFSESSIONS -14	/* no more sessions available. */

#ifdef __cplusplus
}
#endif
#endif
