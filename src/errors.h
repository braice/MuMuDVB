/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) Brice DUBOST
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

#ifndef _ERRORS_MUMUDVB_H
#define _ERRORS_MUMUDVB_H

enum
  {
    ERROR_ARGS=1,
    ERROR_CONF_FILE,
    ERROR_CONF,
    ERROR_TOO_CHANNELS,
    ERROR_CREATE_FILE,
    ERROR_DEL_FILE,
    ERROR_TUNE,
    ERROR_NO_DIFF,
    ERROR_CAM,
  };

#endif
