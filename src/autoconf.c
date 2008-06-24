/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * File for Autoconfiguration
 * 
 * (C) Brice DUBOST <mumudvb@braice.net>
 *  
 * Parts of this code come from libdvbpsi, modified for mumudvb
 * by Brice DUBOST 
 * Libdvb part : Copyright (C) 2000 Klaus Schmidinger
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
 */

#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/poll.h>

#include "errors.h"
#include "ts.h"
#include "mumudvb.h"



/****************************************************************************/
//Code from libdvb, strongly modified, with commentaries added
//read the pmt for autoconfiguration
/****************************************************************************/

int autoconf_read_pmt(mumudvb_pmt_t *pmt, mumudvb_channel_t *channel)
{
	int slen, dslen, i;
	int pid;

	log_message( MSG_DEBUG,"Autoconf : ===PMT read for autoconfiguration of channel \"%s\"\n", channel->name);

	slen=pmt->len;
	dslen=((pmt->packet[10]&0x0f)<<8)|pmt->packet[11]; //program_info_length
	for (i=dslen+12; i<slen-9; i+=dslen+5) {      //we parse the part after the descriptors
	  dslen=((pmt->packet[i+3]&0x0f)<<8)|pmt->packet[i+4];        //ES_info_length
	  pid=((pmt->packet[i+1] & 0x1f)<<8) | pmt->packet[i+2];
	  if ((pmt->packet[i]==0)||(pmt->packet[i]>4))                //stream_type
	    {
	      log_message( MSG_DEBUG, "Autoconf : \t!!!!Stream dropped, type : %d, PID : %d \n",pmt->packet[i],pid);
	      continue;
	    }
	  switch(pmt->packet[i]){
	  case 1:
	  case 2:
	    log_message( MSG_DEBUG,"Autoconf :   Video ");
	    break;
	  case 3:
	  case 4:
	    log_message( MSG_DEBUG,"Autoconf :   Audio ");
	    break;
	  default:
	    log_message( MSG_DEBUG,"Autoconf : \t==Stream type : %d ",pmt->packet[i]);
	  }

	  log_message( MSG_DETAIL,"PID %d added\n", pid);
	  channel->pids[channel->num_pids]=pid;
	  channel->num_pids++;
	}
	log_message( MSG_DETAIL,"Autoconf : == Number of pids after autoconf %d\n", channel->num_pids);
	return 0;
	  
}
