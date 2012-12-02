/* 
 * MuMuDVB - Stream a DVB transport stream.
 * File for software descrambling connection with oscam
 * 
 * (C) 2004-2010 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.braice.net
 *
 * Code inspired by vdr plugin dvbapi
 * Copyright (C) 2011,2012 Mariusz Białończyk <manio@skyboo.net>
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
#include <stdio.h>
#include <time.h>


#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>


#include "errors.h"
#include "ts.h"
#include "mumudvb.h"
#include "log.h"

/**@file
 * @brief scam support
 * 
 * Code for asking oscam to begin geting cw's for channel
 */

static char *log_module="SCAM_CAPMT: ";


/** @brief sending to oscam data to begin geting cw's for channel */
int scam_send_capmt(mumudvb_channel_t *channel, int adapter)
{
  char *caPMT = (char *) malloc(1024);
  int length_field;
  int toWrite;
  caPMT[0] = 0x9F;
  caPMT[1] = 0x80;
  caPMT[2] = 0x32;
  caPMT[3] = 0x82; //2 following bytes for size

  caPMT[6] = 0x03; //list management = 3
  caPMT[7] = channel->service_id >> 8; //program_number
  caPMT[8] = channel->service_id & 0xff; //program_number
  caPMT[9] = 0; //version_number, current_next_indicator

  caPMT[12] = 0x01; //ca_pmt_cmd_id = CAPMT_CMD_OK_DESCRAMBLING
  //adding own descriptor with demux and adapter_id
  caPMT[13] = 0x82; //CAPMT_DESC_DEMUX
  caPMT[14] = 0x02; //length
  caPMT[15] = 0x00; //demux id
  caPMT[16] = (char) adapter; //adapter id
  caPMT[10] = (*(channel->pmt_packet)).data_full[10]; //reserved+program_info_length
  caPMT[11] = (*(channel->pmt_packet)).data_full[11] + 1 + 4; //reserved+program_info_length (+1 for ca_pmt_cmd_id, +4 for above CAPMT_DESC_DEMUX)
 
  memcpy(caPMT + 17, (*(channel->pmt_packet)).data_full + 12, (*(channel->pmt_packet)).len_full -12 - 4);
  length_field = 17 + ((*(channel->pmt_packet)).len_full - 11 - 4) - 6;
  caPMT[4] = length_field >> 8;
  caPMT[5] = length_field & 0xff;	
  toWrite = length_field + 6;

  if (channel->camd_socket == 0)
  {
    channel->camd_socket = socket(AF_LOCAL, SOCK_STREAM, 0);
    struct sockaddr_un serv_addr_un;
    memset(&serv_addr_un, 0, sizeof(serv_addr_un));
    serv_addr_un.sun_family = AF_LOCAL;
    snprintf(serv_addr_un.sun_path, sizeof(serv_addr_un.sun_path), "/tmp/camd.socket");
    if (connect(channel->camd_socket, (const struct sockaddr *) &serv_addr_un, sizeof(serv_addr_un)) != 0)
    {
	  log_message(log_module, MSG_ERROR,"Canot connect to /tmp/camd.socket for channel %s, Do you have OSCam running?\n", channel->name);
      channel->camd_socket = 0;
	  return 1;
    }
    else
	  log_message(log_module,  MSG_DEBUG, "created socket for channel %s\n", channel->name);
	  
  }
  if (channel->camd_socket != 0)
  {
    int wrote = write(channel->camd_socket, caPMT, toWrite);
	log_message( log_module,  MSG_DEBUG, "sent CAPMT message to socket for channel %s, toWrite=%d wrote=%d\n", channel->name, toWrite, wrote);
    if (wrote != toWrite)
    {
	  log_message(log_module, MSG_ERROR,"channel %s:wrote != toWrite\n", channel->name);
      close(channel->camd_socket);
      channel->camd_socket = 0;
    }
  }
  free(caPMT);

  return 0;
}

