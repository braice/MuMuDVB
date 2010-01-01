/* 
 * MuMuDVB - UDP-ize a DVB transport stream.
 * Code for transcoding
 * 
 * Code written by Utelisys Communications B.V.
 * Copyright (C) 2009 Utelisys Communications B.V.
 *
 * The latest version can be found at http://mumudvb.braice.net
 * 
 * Copyright notice:
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef _TRANSCODE_AVLIB_H
#define _TRANSCODE_AVLIB_H

#include "transcode_common.h"

void* initialize_transcode(transcode_thread_data_t *transcode_thread_data);
void free_transcode(void *transcode_avlib_handle, transcode_thread_data_t *transcode_thread_data);
void transcode(void *transcode_avlib_handle, transcode_thread_data_t *transcode_thread_data);

void transcode_simple(transcode_thread_data_t *transcode_thread_data);

#endif 
