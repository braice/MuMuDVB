/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
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

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

extern int no_daemon;
extern int verbosity;
extern int log_initialised;

void log_message( int type,
                    const char *psz_format, ... )
{
  va_list args;

  va_start( args, psz_format );

  if(type<verbosity)
    {
      if (no_daemon || !log_initialised)
	vfprintf(stderr, psz_format, args );
      else
	//TODO use verbosity to define log level
	vsyslog (LOG_USER|LOG_INFO, psz_format, args );
    }

  va_end( args );
}
