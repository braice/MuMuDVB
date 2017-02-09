/*
 * MuMuDVB - Stream a DVB transport stream.
 * PMT rewrite by Danijel Tudek, Dec 2016.
 *
 * (C) 2008-2013 Brice DUBOST <mumudvb@braice.net>
 *
 * Parts of this code come from libdvb, modified for mumudvb
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

/** @file
 *  @brief This file contains code for PMT rewrite
 *  PMT must be rewritten if we don't stream all PIDs, otherwise some players
 *  may get confused.
 *  (See "Problem with streaming individual PIDs", Nov 2016 on mailing list.)
 *
 */

#include <stdlib.h>
#include <string.h>

#include "mumudvb.h"
#include "ts.h"
#include "rewrite.h"
#include "log.h"
#include <stdint.h>

extern uint32_t crc32_table[256];
static char *log_module = "PMT rewrite: ";

/**
 * @brief Check the streamed PMT version and compare to the stored PMT.
 * @param ts_packet
 * @param channel
 * @return return 1 if update is needed
 */
int pmt_need_update(unsigned char *ts_packet, mumudvb_channel_t *channel) {
	pmt_t *pmt = (pmt_t *) (ts_packet + TS_HEADER_LEN);
	if (pmt->version_number != channel->generated_pmt_version) {
		log_message(log_module, MSG_DEBUG, "PMT changed, old version: %d, new version: %d, rewriting...",
					channel->generated_pmt_version, pmt->version_number);
		return 1;
	} else {
		return 0;
	}
}

/** @brief Main function for PMT rewrite.
 * The goal of this function is to make a new PMT with only the announcement for the streamed PIDs for each channel.
 * By default it contains all PIDs which confuses players if we don't actually stream all of them.
 * The PMT is read and the list of PIDs is compared to user-specified PID list for the channel.
 * If there is a match, PID is copied to generated PMT.
 * @param ts_packet
 * @param channel
 * @return 0
 */
int pmt_channel_rewrite(unsigned char *ts_packet, mumudvb_channel_t *channel) {
	ts_header_t *ts_header = (ts_header_t *) ts_packet;
	pmt_t *pmt = (pmt_t *) (ts_packet + TS_HEADER_LEN);

	if (pmt->table_id != 0x02) {
		log_message(log_module, MSG_DETAIL, "We didn't get the good PMT (wrong table ID 0x%02X), search for a new one",
					pmt->table_id);
		return 0;
	}

	pmt_info_t *pmt_info;
	unsigned long crc32;
	//destination buffer
	unsigned char buf_dest[TS_PACKET_SIZE];
	int buf_dest_pos = 0;

	int section_length = 0, elem_pid = 0;
	int new_section_length = 0;
	int es_info_len = 0;
	int new_es_info_len = 0;

	int i = 0, j = 0;

	log_message(log_module, MSG_DEBUG, "PMT pid = %d; channel name = \"%s\"\n", channel->pmt_packet->pid,
				channel->name);

	section_length = HILO(pmt->section_length) + 3;

	//lets start the copy
	//we copy the ts header and adapt it a bit
	//the continuity counter is updated elsewhere
	if (ts_header->payload_unit_start_indicator) {
		if (ts_packet[TS_HEADER_LEN - 1])
			log_message(log_module, MSG_DEBUG, "pointer field 0x%x \n", ts_packet[TS_HEADER_LEN - 1]);
	}
	ts_header->payload_unit_start_indicator = 1;
	ts_packet[TS_HEADER_LEN - 1] = 0; //we erase the pointer field
	//we copy the modified SDT header
	pmt->current_next_indicator = 1; //applicable immediately
	pmt->section_number = 0;
	pmt->last_section_number = 0;

	memcpy(buf_dest, ts_header, TS_HEADER_LEN + PMT_LEN);
	buf_dest_pos = TS_HEADER_LEN + PMT_LEN;

	/**
	 * Parse PMT
	 */
	int es_len = section_length - PMT_LEN - 4;
	log_message(log_module, MSG_DEBUG, "Number of PIDs requested from PMT: %d\n", channel->pid_i.num_pids-1); // one of them is PMT PID itself
	for (i = 0; i < es_len; i += PMT_INFO_LEN + es_info_len) {
		pmt_info = (pmt_info_t *) ((char *) pmt + PMT_LEN + i);
		elem_pid = HILO(pmt_info->elementary_PID);
		es_info_len = HILO(pmt_info->ES_info_length);
		//Prevent read overflow
		if (i + PMT_LEN + 4 + PMT_INFO_LEN + es_info_len >= 184) {
			log_message(log_module, MSG_ERROR, " Read overflow detected, aborting parsing");
			break;
		}
		for (j = 0; j < channel->pid_i.num_pids; j++) {
			if (elem_pid == channel->pid_i.pids[j]) {
				new_es_info_len += PMT_INFO_LEN + es_info_len;
				log_message(log_module, MSG_DEBUG, " PID %d found, copy to new PMT\n", channel->pid_i.pids[j]);
				//Prevent write overflow
				if(buf_dest_pos + PMT_INFO_LEN + es_info_len >= 184) {
					log_message(log_module, MSG_DEBUG, "  Write overflow detected, aborting copy");
					break;
				}
				memcpy(buf_dest + buf_dest_pos, pmt_info, PMT_INFO_LEN + es_info_len);
				buf_dest_pos += PMT_INFO_LEN + es_info_len;
			}
		}
	}

	new_section_length = buf_dest_pos - TS_HEADER_LEN + 1;

	//We write the new section length
	buf_dest[1 + TS_HEADER_LEN] = (((new_section_length) & 0x0f00) >> 8) | (0xf0 & buf_dest[1 + TS_HEADER_LEN]);
	buf_dest[2 + TS_HEADER_LEN] = new_section_length & 0xff;

	//CRC32 calculation inspired by the xine project
	//Now we must adjust the CRC32
	//we compute the CRC32
	crc32 = 0xffffffff;
	for (i = 0; i < new_section_length - 1; i++) {
		crc32 = (crc32 << 8) ^ crc32_table[((crc32 >> 24) ^ buf_dest[i + TS_HEADER_LEN]) & 0xff];
	}

	//We write the CRC32 to the buffer
	buf_dest[buf_dest_pos] = (crc32 >> 24) & 0xff;
	buf_dest_pos += 1;
	buf_dest[buf_dest_pos] = (crc32 >> 16) & 0xff;
	buf_dest_pos += 1;
	buf_dest[buf_dest_pos] = (crc32 >> 8) & 0xff;
	buf_dest_pos += 1;
	buf_dest[buf_dest_pos] = crc32 & 0xff;
	buf_dest_pos += 1;

	//Padding with 0xFF
	memset(buf_dest + buf_dest_pos, 0xFF, TS_PACKET_SIZE - buf_dest_pos);

	//update generated PMT version (matches the original PMT version)
	log_message(log_module, MSG_DEBUG, "PMT rewritten\n");
	channel->generated_pmt_version = pmt->version_number;
	memcpy(channel->generated_pmt, buf_dest, TS_PACKET_SIZE);

	//Everything is OK
	return 1;
}

int pmt_rewrite_new_channel_packet(unsigned char *ts_packet, unsigned char *pmt_ts_packet, mumudvb_channel_t *channel, int curr_channel) {
	if (channel->channel_ready >= READY && pmt_need_update(ts_packet, channel))
		if (!pmt_channel_rewrite(ts_packet, channel)) {
			log_message(log_module, MSG_DEBUG, "Cannot rewrite (for the moment) the PMT for the channel %d : \"%s\"\n",
						curr_channel, channel->name);
			return 0;
		}
	channel->pmt_continuity_counter++;
	channel->pmt_continuity_counter = channel->pmt_continuity_counter % 32;
	memcpy(pmt_ts_packet, channel->generated_pmt, TS_PACKET_SIZE);
	set_continuity_counter(pmt_ts_packet, channel->pmt_continuity_counter);
	return 1;
}
