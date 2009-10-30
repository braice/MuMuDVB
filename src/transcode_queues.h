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
#ifndef _TRANSCODE_QUEUES_H
#define _TRANSCODE_QUEUES_H

typedef struct data_queue_item_t
{
    void *data;
    int data_size;
    struct data_queue_item_t *next;
} data_queue_item_t;

typedef struct data_queue_t
{
    data_queue_item_t *first;
    data_queue_item_t *last;
    int data_size;
    int max_data_size;
} data_queue_t;

void data_queue_init(data_queue_t *queue, int max_data_size);
int data_queue_enqueue(data_queue_t *queue, void *data, int data_size);
int data_queue_dequeue(data_queue_t *queue, void *buf, int buf_size);
void data_queue_free(data_queue_t *queue);

typedef struct ref_queue_item_t
{
    void *ref;
    struct ref_queue_item_t *next;
} ref_queue_item_t;

typedef struct ref_queue_t
{
    ref_queue_item_t *first;
    ref_queue_item_t *last;
    int count;
    int max_count;
} ref_queue_t;

void ref_queue_init(ref_queue_t *queue, int max_count);
int ref_queue_enqueue(ref_queue_t *queue, void *ref);
void* ref_queue_dequeue(ref_queue_t *queue);
//void ref_queue_free(ref_queue_t *queue);

#endif 
