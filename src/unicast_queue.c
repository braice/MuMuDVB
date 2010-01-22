/* 
 * MuMuDVB - UDP-ize a DVB transport stream.
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

/** @file
 * @brief File for HTTP unicast Queue and data sending
 * @author Brice DUBOST
 * @date 2009
 */

#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "unicast_http.h"
#include "unicast_queue.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"
#include "unicast.h"
#include "unicast_common.h"


int unicast_queue_remove_data(unicast_queue_header_t *header);
int unicast_queue_add_data(unicast_queue_header_t *header, unsigned char *data, int data_len);
unsigned char *unicast_queue_get_data(unicast_queue_header_t* , int* );
void unicast_close_connection(unicast_parameters_t *unicast_vars, fds_t *fds, int Socket, mumudvb_channel_t *channels);

/** @brief Send the buffer for the channel
 *
 * This function is called when a buffer for a channel is full and have to be sent to the clients
 *
 */
void unicast_data_send(mumudvb_channel_t *actual_channel, mumudvb_channel_t *channels, fds_t *fds, unicast_parameters_t *unicast_vars)
{
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
    int clientsocket;
    struct sockaddr_in clientsocketaddr;

    actual_client=actual_channel->clients;
    while(actual_client!=NULL)
    {
      if(actual_client->client_type==CLIENT_HTTP)
      {
        buffer=actual_channel->buf;
        buffer_len=actual_channel->nb_bytes;
        clientsocket=actual_client->Socket;
        clientsocketaddr=actual_client->SocketAddr;
      }
      else if(actual_client->client_type==CLIENT_RTSP)
      {
        buffer=actual_channel->buf_with_rtp_header;
        buffer_len=actual_channel->nb_bytes+RTP_HEADER_LEN;
        clientsocket=actual_client->rtsp_Socket;
        clientsocketaddr=actual_client->rtsp_SocketAddr;
      }
      else
      {
        log_message(MSG_ERROR,"bug in the program please contact line %d file %s \n",__LINE__, __FILE__);
        return;
      }
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
	    log_message(MSG_DETAIL,"Unicast: The queue is full, we now throw away new packets for client %s:%d\n",
		        inet_ntoa(clientsocketaddr.sin_addr),
		        ntohs(clientsocketaddr.sin_port));
	  }
	}
	buffer=unicast_queue_get_data(&actual_client->queue, &buffer_len);
      }
      else
	packets_left=1;

      while(packets_left>0)
      {
	//we send the data
	written_len=write(clientsocket,buffer, buffer_len);
	//We check if all the data was successfully written
	if(written_len<buffer_len)
	{
	  //No !
	  packets_left=0; //we don't send more packets to this client
	  if(written_len==-1)
	  {
	    if(errno != actual_client->last_write_error)
	    {
	      log_message(MSG_DEBUG,"Unicast: New error when writing to client %s:%d : %s\n",
			  inet_ntoa(clientsocketaddr.sin_addr),
			  ntohs(clientsocketaddr.sin_port),
			  strerror(errno));
	      actual_client->last_write_error=errno;
	      written_len=0;
	    }
	  }
	  else
	  {
	    log_message(MSG_DEBUG,"Unicast: Not all the data was written to %s:%d. Asked len : %d, written len %d\n",
			inet_ntoa(clientsocketaddr.sin_addr),
			ntohs(clientsocketaddr.sin_port),
			actual_channel->nb_bytes,
			written_len);
	  }
	    if(!data_from_queue)
	    {
	      //We store the non sent data in the queue
	      if((actual_client->queue.data_bytes_in_queue+buffer_len-written_len)< unicast_vars->queue_max_size)
	      {
		unicast_queue_add_data(&actual_client->queue, buffer+written_len, buffer_len-written_len);
		log_message(MSG_DEBUG,"Unicast: We start queuing packets ... \n");
	      }
	    }

	  if(!actual_client->consecutive_errors)
	  {
	    log_message(MSG_DETAIL,"Unicast: Error when writing to client %s:%d : %s\n",
			inet_ntoa(clientsocketaddr.sin_addr),
			ntohs(clientsocketaddr.sin_port),
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
	      log_message(MSG_INFO,"Unicast: Consecutive errors when writing to client %s:%d during too much time, we disconnect\n",
			  inet_ntoa(clientsocketaddr.sin_addr),
			  ntohs(clientsocketaddr.sin_port));
			  temp_client=actual_client->chan_next;
			  unicast_close_connection(unicast_vars,fds,clientsocket,channels);
			  actual_client=temp_client;
	    }
	  }
	}
	else
	{
	  //data successfully written
	  if (actual_client->consecutive_errors)
	  {
	    log_message(MSG_DETAIL,"Unicast: We can write again to client %s:%d\n",
			inet_ntoa(clientsocketaddr.sin_addr),
			ntohs(clientsocketaddr.sin_port));
	    actual_client->consecutive_errors=0;
	    actual_client->last_write_error=0;
	    if(data_from_queue)
	      log_message(MSG_DEBUG,"Unicast: We start dequeuing packets Packets in queue: %d. Bytes in queue: %d\n",
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
	      //log_message(MSG_DEBUG,"Unicast: Still packets in the queue,next one\n");
	      //still packets in the queue, we continue sending
	      if(packets_left)
		buffer=unicast_queue_get_data(&actual_client->queue, &buffer_len);
	    }
	    else //queue now empty
	    {
	      packets_left=0;
	      log_message(MSG_DEBUG,"Unicast: The queue is now empty :) client %s:%d \n",
			  inet_ntoa(clientsocketaddr.sin_addr),
		          ntohs(clientsocketaddr.sin_port));
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
      log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
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
      log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
      return -1;
    }
    dest=header->last->next;
    header->last=dest;
  }
  dest->next=NULL;
  dest->data=malloc(sizeof(unsigned char)*data_len);
  if(dest->data==NULL)
  {
    log_message(MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
    return -1;
  }
  memcpy(dest->data,data,data_len);
  dest->data_length=data_len;
  header->packets_in_queue++;
  header->data_bytes_in_queue+=data_len;
  //log_message(MSG_DEBUG,"Unicast : queuing new packet. Packets in queue: %d. Bytes in queue: %d\n",header->packets_in_queue,header->data_bytes_in_queue);
  return 0;
}

/** @brief Get data from a queue
 *
 */
unsigned char *unicast_queue_get_data(unicast_queue_header_t *header, int *data_len)
{
  if(header->packets_in_queue == 0)
  {
    log_message(MSG_ERROR,"BUG : Cannot dequeue an empty queue\n");
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
    log_message(MSG_ERROR,"BUG : Cannot remove from an empty queue\n");
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

