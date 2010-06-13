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

#include "transcode_queues.h"
#include "mumudvb.h"  
#include "log.h"

#include <stdlib.h>
#include <string.h>

static char *log_module="Transcode : ";

void data_queue_init(data_queue_t *queue, int max_data_size)
{
    queue->first = NULL;
    queue->last = NULL;
    queue->data_size = 0;
    queue->max_data_size = max_data_size;
}

int data_queue_enqueue(data_queue_t *queue, void *data, int data_size)
{
    if (queue->max_data_size < data_size) {
        log_message( log_module, MSG_ERROR, "Failed to enqueue data - too big data.\n");
        return -1; /* Failed to enqueue */
    }

    int result = 1;

    /* Free data in queue if needed */

    while (queue->data_size + data_size > queue->max_data_size) {
        /* Remove first item  from queue */
        data_queue_item_t *item = queue->first;
        
        queue->first = item->next;
        queue->data_size -= item->data_size;
        
        free(item->data);
        free(item);

        result = 0; /* Queue was full */
    }

    if (NULL == queue->first) {
        queue->last = NULL;
    }

    /* Queue is not full - create data queue item */

    data_queue_item_t *item = malloc(sizeof(data_queue_item_t));

    if (NULL == item) {
        log_message( log_module, MSG_ERROR, "Failed to enqueue data.\n");
        return -1; /* Failed to enqueue */
    }

    item->next = NULL;
    item->data_size = data_size;
    item->data = malloc(data_size);

    if (NULL == item->data) {
        log_message( log_module, MSG_ERROR, "Failed to enqueue data.\n");
        free(item);
        return -1; /* Failed to enqueue */
    }

    /* Copy data to the data queue item */
    memcpy(item->data, data, data_size);

    /* Add item to queue */

    if (NULL == queue->first) {
        /* Queue is empty */
        queue->first = item;
        queue->last = item;
        queue->data_size = data_size;
    }
    else {
        /* Queue is not empty - appending */
        queue->last->next = item;
        queue->last = item;
        queue->data_size += data_size;
    }

    return result; /* Data enqueued */
}

int data_queue_dequeue(data_queue_t *queue, void *buf, int buf_size)
{
    int buf_pos = 0;

    while (NULL != queue->first) {
        data_queue_item_t *item = queue->first;

        if (buf_pos + item->data_size > buf_size) {
            break; /* Output buffer is full */
        }

        /* Copy data from queue item into buffer */
        
        memcpy(buf + buf_pos, item->data, item->data_size);
        buf_pos += item->data_size;

        /* Remove from queue and free data item */
        
        queue->first = item->next;
        queue->data_size -= item->data_size;
        
        free(item->data);
        free(item);
    }

    if (NULL == queue->first) {
        /* Queue is empty now */
        queue->last = NULL;
    }
    else if (0 == buf_pos) {
        /* We have item in queue, but it is too big to fit output buffer */

        data_queue_item_t *item = queue->first;
        
        /* Copy part of the item */
        memcpy(buf, item->data, buf_size);
        buf_pos = buf_size;

        /* Realloc item */
        void *old_buf = item->data;
        item->data_size -= buf_pos;
        item->data = malloc(item->data_size);
        memcpy(item->data, old_buf + buf_pos, item->data_size);
        free(old_buf);

        queue->data_size -= buf_size;
    }

    return buf_pos;
}

void data_queue_free(data_queue_t *queue)
{
    while (NULL != queue->first) {
        data_queue_item_t *next_item = queue->first->next;

        free(queue->first->data);
        free(queue->first);

        queue->first = next_item;
    }

    queue->last = NULL;
    queue->data_size = 0;
}

void ref_queue_init(ref_queue_t *queue, int max_count)
{
    queue->first = NULL;
    queue->last = NULL;
    queue->count = 0;
    queue->max_count = max_count;
}

int ref_queue_enqueue(ref_queue_t *queue, void *ref)
{
    if (queue->max_count != 0 && queue->count >= queue->max_count) {
        return 0; /* Queue is full */
    }

    if (NULL == ref) {
        log_message( log_module, MSG_ERROR, "Can not enqueue NULL ref.\n");
        return -1; /* Do not enqueue NULLs */
    }

    /* Queue is not full - create ref queue item */

    ref_queue_item_t *item = malloc(sizeof(ref_queue_item_t));

    if (NULL == item) {
        log_message( log_module, MSG_ERROR, "Can not enqueue.\n");
        return -1; /* Failed to enqueue */
    }

    item->next = NULL;
    item->ref = ref;

    /* Add item to queue */

    if (NULL == queue->first) {
        /* Queue is empty */
        queue->first = item;
        queue->last = item;
        queue->count = 1;
    }
    else {
        /* Queue is not empty - appending */
        queue->last->next = item;
        queue->last = item;
        queue->count++;
    }

    return 1; /* Data enqueued */
}

void* ref_queue_dequeue(ref_queue_t *queue)
{
    if (NULL == queue->first) {
        return NULL;
    }

    void *ref = queue->first->ref;

    ref_queue_item_t *next_item = queue->first->next;
    
    free(queue->first);

    queue->first = next_item;
    
    if (NULL == next_item) {
        queue->last = NULL;
        queue->count = 0;
    }
    else {
        queue->count--;
    }

    return ref;
}

/*void ref_queue_free(ref_queue_t *queue)
{
    while (queue->first) {
        ref_queue_item_t *next_item = queue->first->next;
        free(queue->first);
        queue->first = next_item;
    }

    queue->last = NULL;
    queue->count = 0;
}*/
