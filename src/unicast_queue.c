/*
 * MuMuDVB - UDP-ize a DVB transport stream.
 *
 * (C) 2009 Brice DUBOST
 *
 * The latest version can be found at http://mumudvb.net
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
 * @brief File for HTTP unicast Queue and data sending
 * @author Brice DUBOST
 * @date 2009-2010
 */

#define _CRT_SECURE_NO_WARNINGS

#include <errno.h>
#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#else
#include <process.h> /* for getpid() */
#endif
#include <string.h>
#include <stdlib.h>
#include "unicast_http.h"
#include "unicast_queue.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"

#ifdef _WIN32
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL (0)
#endif
#endif

static char *log_module="Unicast : ";

int unicast_queue_remove_data(unicast_queue_header_t *header);
int unicast_queue_add_data(unicast_queue_header_t *header, unsigned char *data, int data_len);
int unicast_queue_requeue(unicast_queue_header_t *header, unsigned char *data, int data_len);
unsigned char *unicast_queue_get_data(unicast_queue_header_t* , int* );
void unicast_close_connection(unicast_parameters_t *unicast_vars, int Socket);

/** @brief Send the buffer for the channel
 *
 * This function is called when a buffer for a channel is full and have to be sent to the clients
 *
 */
void unicast_data_send(mumudvb_channel_t *actual_channel, unicast_parameters_t *unicast_vars)
{
	char addr_buf[IPV6_CHAR_LEN] = { 0, };

	if(actual_channel->clients)
	{
		unicast_client_t *actual_client;
		unicast_client_t *temp_client;
		int written_len;
		unsigned char *buffer;
		int buffer_len;
		int data_from_queue;
		int packets_left;
		struct timeval tv;

		actual_client=actual_channel->clients;
		while(actual_client!=NULL)
		{
			buffer=actual_channel->buf;
			buffer_len=actual_channel->nb_bytes;
			data_from_queue=0;
			if(actual_client->queue.packets_in_queue!=0)
			{
				//already some packets in the queue we enqueue the new one and try to send the queued ones
				data_from_queue=1;
				packets_left=UNICAST_MULTIPLE_QUEUE_SEND;
				if((actual_client->queue.data_bytes_in_queue+buffer_len)< unicast_vars->queue_max_size)
					unicast_queue_add_data(&actual_client->queue, buffer, buffer_len );
				else
				{
					if(!actual_client->queue.full)
					{
						actual_client->queue.full=1;
						socket_to_string(actual_client->Socket, addr_buf, sizeof(addr_buf));

						log_message(log_module, MSG_DETAIL, "The queue is full, we now throw away new packets for client %s\n", addr_buf);
					}
				}
				buffer=unicast_queue_get_data(&actual_client->queue, &buffer_len);
			}
			else
				packets_left=1;

			while(packets_left>0)
			{
				//we send the data
				written_len=send(actual_client->Socket,buffer, buffer_len,MSG_NOSIGNAL);
				//We check if all the data was successfully written
				if(written_len<buffer_len)
				{
					//No !
					packets_left=0; //we don't send more packets to this client
					if(written_len==-1)
					{
						if(errno != actual_client->last_write_error)
						{
							socket_to_string(actual_client->Socket, addr_buf, sizeof(addr_buf));

							log_message(log_module, MSG_DEBUG, "New error when writing to client %s : %s\n",
									addr_buf,
									strerror(errno));
							actual_client->last_write_error=errno;
							written_len=0;
						}
					}
					else
					{
						socket_to_string(actual_client->Socket, addr_buf, sizeof(addr_buf));

						log_message( log_module, MSG_DEBUG,"Not all the data was written to %s. Asked len : %d, written len %d\n",
								addr_buf,
								actual_channel->nb_bytes,
								written_len);
					}
					if(!(unicast_vars->flush_on_eagain &&(errno==EAGAIN)))//Debug feature : we can drop data if eagain error
					{
						//No drop on eagain or no eagain
						if(!data_from_queue)
						{
							//We store the non sent data in the queue
							if((actual_client->queue.data_bytes_in_queue+buffer_len-written_len)< unicast_vars->queue_max_size)
							{
								unicast_queue_add_data(&actual_client->queue, buffer+written_len, buffer_len-written_len);
								log_message( log_module, MSG_DEBUG,"We start queuing packets ... \n");
							}
						}
						else if(written_len > 0)
						{
							unicast_queue_remove_data(&actual_client->queue);
							unicast_queue_requeue(&actual_client->queue, buffer+written_len, buffer_len-written_len);
							log_message( log_module, MSG_DEBUG,"We requeue the non sent data ... \n");
						}
					}else{
						//this is an EAGAIN error and we want to drop the data
						if(!data_from_queue)
						{
							//Not from the queue we dont do anything
							log_message( log_module, MSG_DEBUG,"We drop not from queue ... \n");
						}
						else
						{
							unicast_queue_clear(&actual_client->queue);
							log_message( log_module, MSG_DEBUG,"Eagain error we flush the queue ... \n");
						}
					}

					if(!actual_client->consecutive_errors)
					{
						socket_to_string(actual_client->Socket, addr_buf, sizeof(addr_buf));

						log_message( log_module, MSG_DETAIL,"Error when writing to client %s : %s\n",
								addr_buf,
								strerror(errno));
						gettimeofday (&tv, (struct timezone *) NULL);
						actual_client->first_error_time = tv.tv_sec;
						actual_client->consecutive_errors=1;
					}
					else
					{
						//We have errors, we check if we reached the timeout
						gettimeofday (&tv, (struct timezone *) NULL);
						if((unicast_vars->consecutive_errors_timeout > 0) && (tv.tv_sec - actual_client->first_error_time) > unicast_vars->consecutive_errors_timeout)
						{
							socket_to_string(actual_client->Socket, addr_buf, sizeof(addr_buf));

							log_message(log_module, MSG_INFO, "Consecutive errors when writing to client %s during too much time, we disconnect\n", addr_buf);
							temp_client=actual_client->chan_next;
							unicast_close_connection(unicast_vars,actual_client->Socket);
							actual_client=temp_client;
						}
					}
				}
				else
				{
					//data successfully written
					if (actual_client->consecutive_errors)
					{
						socket_to_string(actual_client->Socket, addr_buf, sizeof(addr_buf));

						log_message(log_module, MSG_DETAIL, "We can write again to client %s\n", addr_buf);
						actual_client->consecutive_errors=0;
						actual_client->last_write_error=0;
						if(data_from_queue)
							log_message( log_module, MSG_DEBUG,"We start dequeuing packets Packets in queue: %d. Bytes in queue: %d\n",
									actual_client->queue.packets_in_queue,
									actual_client->queue.data_bytes_in_queue);
					}
					packets_left--;
					if(data_from_queue)
					{
						//The data was successfully sent, we can dequeue it
						unicast_queue_remove_data(&actual_client->queue);
						if(actual_client->queue.packets_in_queue!=0)
						{
							//log_message( log_module, MSG_DEBUG,"Still packets in the queue,next one\n");
							//still packets in the queue, we continue sending
							if(packets_left)
								buffer=unicast_queue_get_data(&actual_client->queue, &buffer_len);
						}
						else //queue now empty
						{
							packets_left=0;
							socket_to_string(actual_client->Socket, addr_buf, sizeof(addr_buf));

							log_message(log_module, MSG_DEBUG, "The queue is now empty :) client %s\n", addr_buf);
						}
					}
				}
			}

			if(actual_client) //Can be null if the client was destroyed
				actual_client=actual_client->chan_next;
		}
	}

}




/* ================= QUEUE ======================*/

/** @brief Add data to a queue
 *
 */
int unicast_queue_add_data(unicast_queue_header_t *header, unsigned char *data, int data_len)
{
	unicast_queue_data_t *dest;
	if(header->packets_in_queue == 0)
	{
		//first packet in the queue
		header->first=malloc(sizeof(unicast_queue_data_t));
		if(header->first==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			return -1;
		}
		dest=header->first;
		header->last=header->first;
	}
	else
	{
		//already packets in the queue
		header->last->next=malloc(sizeof(unicast_queue_data_t));
		if(header->last->next==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			return -1;
		}
		dest=header->last->next;
		header->last=dest;
	}
	dest->next=NULL;
	dest->data=malloc(sizeof(unsigned char)*data_len);
	if(dest->data==NULL)
	{
		log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		return -1;
	}
	memcpy(dest->data,data,data_len);
	dest->data_length=data_len;
	header->packets_in_queue++;
	header->data_bytes_in_queue+=data_len;
	//log_message( log_module, MSG_DEBUG,"queuing new packet. Packets in queue: %d. Bytes in queue: %d\n",header->packets_in_queue,header->data_bytes_in_queue);
	return 0;
}

/** @brief Get data from a queue
 *
 */
unsigned char *unicast_queue_get_data(unicast_queue_header_t *header, int *data_len)
{
	if(header->packets_in_queue == 0)
	{
		log_message( log_module, MSG_ERROR,"BUG : Cannot dequeue an empty queue\n");
		return NULL;
	}

	*data_len=header->first->data_length;
	return header->first->data;
}


/** @brief Remove the first packet of the queue
 *
 */
int unicast_queue_remove_data(unicast_queue_header_t *header)
{
	unicast_queue_data_t *tobedeleted;
	if(header->packets_in_queue == 0)
	{
		log_message( log_module, MSG_ERROR,"BUG : Cannot remove from an empty queue\n");
		return -1;
	}
	tobedeleted=header->first;
	header->first=header->first->next;
	header->packets_in_queue--;
	header->full=0;
	header->data_bytes_in_queue-=tobedeleted->data_length;
	free(tobedeleted->data);
	free(tobedeleted);
	return 0;
}

/** @brief Clear the queue
 *
 */
void unicast_queue_clear(unicast_queue_header_t *header)
{
	while(header->packets_in_queue > 0)
		unicast_queue_remove_data(header);
}


int unicast_queue_requeue(unicast_queue_header_t *header, unsigned char *data, int data_len)
{
	int last_pkt_size;
	unicast_queue_data_t *dest;
	unsigned char *tempbuf;

	if(header->packets_in_queue == 0)
	{
		log_message(log_module, MSG_ERROR, "BUG: unicast_queue_requeue() called with no packets in the queue\n");
		return -1;
	}

	tempbuf = malloc(sizeof(unsigned char)*data_len);
	if(tempbuf == NULL)
	{
		log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		return -1;
	}
	memcpy(tempbuf, data, data_len);

	dest=header->first;
	last_pkt_size = dest->data_length;

	free(dest->data);
	dest->data = tempbuf;

	dest->data_length = data_len;
	header->data_bytes_in_queue -= last_pkt_size;
	header->data_bytes_in_queue += data_len;
	return 0;
}
