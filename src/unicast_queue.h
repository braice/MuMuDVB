/* 
 * mumudvb - UDP-ize a DVB transport stream.
 * 
 * (C) 2009 Brice DUBOST
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

/**@file
 * @brief HTML unicast headers for queue
 */
#ifndef _UNICAST_QUEUE_H
#define _UNICAST_QUEUE_H

#define UNICAST_DEFAULT_QUEUE_MAX 1024*512
/**How many packets we try to send from the queue per new packet. This value MUST be at least 2*/
#define UNICAST_MULTIPLE_QUEUE_SEND 3

/** @brief A data packet in queue.
 *
 */
typedef struct unicast_queue_data_t{
  int data_length;
  unsigned char *data;
  struct unicast_queue_data_t *next;
}unicast_queue_data_t;

/** @brief The header of a data queue.
 *
 */
typedef struct unicast_queue_header_t{
  int packets_in_queue;
  int data_bytes_in_queue;
  int full;
  unicast_queue_data_t *first;
  unicast_queue_data_t *last;
}unicast_queue_header_t;



void unicast_queue_clear(unicast_queue_header_t *header);














#endif