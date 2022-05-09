/*
 * MuMuDVB - Stream a DVB transport stream.
 * Based on dvbstream by (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
 *
 * (C) 2004-2014 Brice DUBOST
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
 *
 */

#define _CRT_SECURE_NO_WARNINGS

#include "mumudvb.h"
#include "log.h"
#include "errors.h"
#include "rtp.h"
#include "unicast_http.h"

#ifndef _WIN32
#include <sys/poll.h>
#include <sys/time.h>
#endif
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef ENABLE_SCAM_SUPPORT
#include "scam_common.h"
#endif
#include "hls.h"

static char *log_module="Common: ";

#ifdef _WIN32
static int poll(struct pollfd *pfd, int n, int milliseconds)
{
	struct timeval tv;
	fd_set set;
	int i, result;
	SOCKET maxfd = 0;

	tv.tv_sec = milliseconds / 1000;
	tv.tv_usec = (milliseconds % 1000) * 1000;
	FD_ZERO(&set);

	for (i = 0; i < n; i++) {
		FD_SET((SOCKET)pfd[i].fd, &set);
		pfd[i].revents = 0;

		if (pfd[i].fd > maxfd) {
			maxfd = pfd[i].fd;
		}
	}

	if ((result = select(maxfd + 1, &set, NULL, NULL, &tv)) > 0) {
		for (i = 0; i < n; i++) {
			if (FD_ISSET(pfd[i].fd, &set)) {
				pfd[i].revents = POLLIN;
			}
		}
	}

	return result;
}
#endif

/** @brief : poll the file descriptors fds with a limit in the number of errors and timeout
 */
int mumudvb_poll(struct pollfd *pfds, int pfdsnum,int timeout)
{
	int poll_try,poll_ret;
	int poll_eintr=0;
	int last_poll_error;
	int Interrupted;

	poll_ret=0;
	poll_try=0;
	poll_eintr=0;
	last_poll_error=0;
	do
	{
		poll_ret=poll (pfds, pfdsnum, timeout);
		if(poll_ret<0)
		{
#ifdef _WIN32
			errno = WSAGetLastError();
			printf("poll returns: %08x\n", errno);
#undef EINTR
#define EINTR WSAEINTR
#endif
			if(errno != EINTR) //EINTR means Interrupted System Call, it normally shouldn't matter so much so we don't count it for our Poll tries
			{
				poll_try++;
				last_poll_error=errno;
			}
			else
			{
				poll_eintr++;
				if(poll_eintr==10)
				{
					log_message( log_module, MSG_DEBUG, "Poll : 10 successive EINTR\n");
					poll_eintr=0;
				}
			}
		}
		/**@todo : put a maximum number of interrupted system calls per unit time*/
	}while(( poll_ret<0 )&&(poll_try<MAX_POLL_TRIES));

	if(poll_try==MAX_POLL_TRIES)
	{
		log_message( log_module, MSG_ERROR, "Poll : We reach the maximum number of polling tries\n\tLast error when polling: %s\n", strerror (errno));
		Interrupted=errno<<8; //the <<8 is to make difference beetween signals and errors;
		return -Interrupted;
	}
	else if(poll_try)
	{
		log_message( log_module, MSG_WARN, "Poll : Warning : error when polling: %s\n", strerror (last_poll_error));
	}
	return poll_ret;
}

/** @brief replace a string by another
 * @param source
 * @param length the length of the source buffer (including '\0')
 * @param can_realloc Is the source string allocated by a malloc or fixed. The realloc is done only when the dest is bigger
 * @param toreplace the pattern to replace
 * @param replacement the replacement string for the pattern
 */
char *mumu_string_replace(char *source, int *length, int can_realloc, char *toreplace, char *replacement)
{
	char *pospattern;
	char *reallocresult;
	char *tempstring=NULL;
	int lengthpattern;
	int lengthreplacment;
	int lengthtempstring;
	int lengthsource;

	pospattern=strstr(source,toreplace);
	if(pospattern==NULL)
		return source;
	lengthpattern=strlen(toreplace);
	lengthreplacment=strlen(replacement);
	lengthsource=strlen(source);
	lengthtempstring=lengthsource+1;
	tempstring=malloc(sizeof(char)*lengthtempstring);
	if(tempstring==NULL)
	{
		log_message(log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		return NULL;
	}
	strcpy(tempstring,source);
	pospattern=strstr(tempstring,toreplace);
	while(pospattern!=NULL)
	{
		if(lengthreplacment>lengthpattern)
		{
			reallocresult=realloc(tempstring,sizeof(char)*(lengthtempstring+lengthreplacment-lengthpattern+1));
			if(reallocresult==NULL)
			{
				log_message(log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
				free(tempstring);
				return NULL;
			}
			tempstring=reallocresult;
			pospattern=strstr(tempstring,toreplace);
		}
		memmove(pospattern+lengthreplacment,pospattern+lengthpattern,lengthtempstring-((int)(pospattern-tempstring))-lengthpattern);
		memcpy(pospattern,replacement,lengthreplacment);
		lengthtempstring+=lengthreplacment-lengthpattern;
		pospattern=strstr(tempstring,toreplace);
	}
	tempstring[lengthtempstring-1]='\0';
	if(can_realloc)
	{
		if(lengthtempstring>*length)
		{
			reallocresult=realloc(source,sizeof(char)*(lengthtempstring));
			if(reallocresult==NULL)
			{
				log_message(log_module, MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
				free(tempstring);
				return NULL;
			}
			source=reallocresult;
			*length=lengthtempstring;
		}
		strcpy(source,tempstring);
	}
	else if(lengthtempstring<=*length)
	{
		strcpy(source,tempstring);
	}
	else
	{
		strncpy(source,tempstring,*length-1);
		source[*length-1]='\0';
	}
	free(tempstring);
	return source;
}

int string_mult(char *string);
/** @brief Evaluate a string containing sum and mult keeping the priority of the mult over the +
 * Ex : string_sum("2+2*3") returns 8
 * @param string the string to evaluate
 */
int string_comput(char *string)
{
	int number1,len;
	char *pluspos=NULL;
	char *tempchar;
	if(string==NULL)
		return 0;
	pluspos=strchr(string,'+');
	if(pluspos==NULL)
	{
		len=strlen(string);
	}
	else
	{
		len=pluspos-string;
	}
	tempchar=malloc(sizeof(char)*(len+1));
	if (tempchar == NULL)
		return 0;
	strncpy(tempchar,string,len);
	tempchar[len]='\0';
	number1=string_mult(tempchar);
	free(tempchar);
	if(pluspos==NULL)
		return number1;
	if(strchr(pluspos+1,'+')!=NULL)
		return number1+string_comput(pluspos+1);
	return number1+string_mult(pluspos+1);
}

/** @brief Evaluate a string containing a multiplication. Doesn't work if there is a sum inside
 * Ex : string_sum("2*6") returns 6
 * @param string the string to evaluate
 */
int string_mult(char *string)
{
	int number1,len;
	char *multpos=NULL;
	char *tempchar;
	multpos=strchr(string,'*');
	if(multpos==NULL)
		return atoi(string);
	len=multpos-string;
	tempchar=malloc(sizeof(char)*(len+1));
	strncpy(tempchar,string,len);
	tempchar[len]='\0';
	number1=atoi(tempchar);
	free(tempchar);
	if(strchr(multpos+1,'*')!=NULL)
		return number1*string_mult(multpos+1);
	return number1*atoi(multpos+1);
}

/** @brief Special sprintf wich append the text to an existing string and allocate the memory for it
 */
int mumu_string_append(mumu_string_t *string, const char *psz_format, ...)
{
	int size;
	va_list args;

	va_start( args, psz_format );
	size=vsnprintf(NULL, 0, psz_format, args);
	va_end( args );
	string->string=realloc(string->string,(string->length+size+1)*sizeof(char));
	if(string->string==NULL)
	{
		log_message(log_module,MSG_ERROR,"Problem with realloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		return ERROR_MEMORY<<8;
	}
	va_start( args, psz_format );
	vsnprintf(string->string+string->length, size+1, psz_format, args);
	string->length=string->length+size;
	va_end( args );
	return 0;
}

/** @brief Free a MuMuDVB string
 */
void mumu_free_string(mumu_string_t *string)
{
	if(string->string)
	{
		free(string->string);
		string->string=NULL;
		string->length=0;
	}
}








/** @brief return the time (in usec) elapsed between the two last calls of this function.
 */
long int mumu_timing(void)
{
	static int started=0;
	static struct timeval oldtime;
	struct timeval tv;
	long delta;
	gettimeofday(&tv,NULL);
	if(started)
	{
		delta=(tv.tv_sec-oldtime.tv_sec)*1000000+(tv.tv_usec-oldtime.tv_usec);
	}
	else
	{
		delta=0;
		started=1;
	}
	oldtime=tv;
	return delta;
}

/** @brief getting current system time (in usec).
 */
uint64_t get_time(void) {
#ifndef _WIN32
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec * 1000000ll + ts.tv_nsec / 1000);
#else
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	uint64_t tt = ft.dwHighDateTime;
	tt <<= 32;
	tt |= ft.dwLowDateTime;
	tt /= 10;
	tt -= 11644473600000000ULL;

	return tt;
#endif
}
/** @brief function for buffering demultiplexed data.
 */
void buffer_func(mumudvb_channel_t *channel, unsigned char *ts_packet, struct unicast_parameters_t *unicast_vars, void *scam_vars_v)
{
    int pid;			/** pid of the current mpeg2 packet */
    int ScramblingControl;
    int curr_pid = 0;
    int send_packet = 0;
    extern int dont_send_scrambled;

#ifndef ENABLE_SCAM_SUPPORT
    (void) scam_vars_v; //to make compiler happy
#else
    scam_parameters_t *scam_vars = (scam_parameters_t *)scam_vars_v;
#endif
    uint64_t now_time;
#ifdef ENABLE_SCAM_SUPPORT
    if (channel->scam_support && channel->scam_support_started && scam_vars->scam_support) {
        pthread_mutex_lock(&channel->ring_buf->lock);
        memcpy(channel->ring_buf->data + TS_PACKET_SIZE * channel->ring_buf->write_idx, ts_packet, TS_PACKET_SIZE);
        now_time = get_time();
        channel->ring_buf->time_send[channel->ring_buf->write_idx] = now_time + channel->send_delay;
        channel->ring_buf->time_decsa[channel->ring_buf->write_idx] = now_time + channel->decsa_delay;
        ++channel->ring_buf->write_idx;
        channel->ring_buf->write_idx &= (channel->ring_buffer_size - 1);

        ++channel->ring_buf->to_descramble;

        pthread_mutex_unlock(&channel->ring_buf->lock);
    } else
#endif
    {

        pid = ((ts_packet[1] & 0x1f) << 8) | (ts_packet[2]);
        ScramblingControl = (ts_packet[3] & 0xc0) >> 6;
        pthread_mutex_lock(&channel->stats_lock);
        for (curr_pid = 0; (curr_pid < channel->pid_i.num_pids); curr_pid++) {
            if ((channel->pid_i.pids[curr_pid] == pid) || (channel->pid_i.pids[curr_pid] == 8192)) //We can stream whole transponder using 8192
            {
                if ((ScramblingControl > 0) && (pid != channel->pid_i.pmt_pid))
                    channel->num_scrambled_packets++;

                //check if the PID is scrambled for determining its state
                if (ScramblingControl > 0) channel->pid_i.pids_num_scrambled_packets[curr_pid]++;

                //we don't count the PMT pid for up channels
                if (pid != channel->pid_i.pmt_pid)
                    channel->num_packet++;
                break;
            }
        }
        pthread_mutex_unlock(&channel->stats_lock);
        //avoid sending of scrambled channels if we asked to
        send_packet = 1;
        if (dont_send_scrambled && (ScramblingControl > 0) && (channel->pid_i.pmt_pid))
            send_packet = 0;

        if (send_packet) {
            // we fill the channel buffer
            memcpy(channel->buf + channel->nb_bytes, ts_packet, TS_PACKET_SIZE);
            channel->nb_bytes += TS_PACKET_SIZE;
        }
        //The buffer is full, we send it
        if ((!channel->rtp && ((channel->nb_bytes + TS_PACKET_SIZE) > MAX_UDP_SIZE))
            || (channel->rtp && ((channel->nb_bytes + RTP_HEADER_LEN + TS_PACKET_SIZE) > MAX_UDP_SIZE))) {
            now_time = get_time();
            send_func(channel, now_time, unicast_vars);
        }
    }
}


/** @brief function for sending demultiplexed data.
 */
void send_func(mumudvb_channel_t *channel, uint64_t now_time, struct unicast_parameters_t *unicast_vars)
{
    //For bandwith measurement (traffic)
    pthread_mutex_lock(&channel->stats_lock);
    channel->sent_data += channel->nb_bytes + 20 + 8; // IP=20 bytes header and UDP=8 bytes header
    if (channel->rtp)
        channel->sent_data += RTP_HEADER_LEN;
    pthread_mutex_unlock(&channel->stats_lock);

    /********** MULTICAST *************/
    //if the multicast TTL is set to 0 we don't send the multicast packets
    if ((channel->socketOut4 > 0) || (channel->socketOut6 > 0)) {
        unsigned char *data;
        int data_len;
        if (channel->rtp) {
            /****** RTP *******/
            rtp_update_sequence_number(channel, now_time);
            data = channel->buf_with_rtp_header;
            data_len = channel->nb_bytes + RTP_HEADER_LEN;
        } else {
            data = channel->buf;
            data_len = channel->nb_bytes;
        }
        if (channel->socketOut4)
            sendudp(channel->socketOut4, &channel->sOut4, data, data_len);
        if (channel->socketOut6)
            sendudp6(channel->socketOut6, &channel->sOut6, data, data_len);
    }
    /*********** UNICAST **************/
    unicast_data_send(channel, unicast_vars);
    if (unicast_vars->hls) hls_data_send(channel, unicast_vars, now_time);
    /********* END of UNICAST **********/
    channel->nb_bytes = 0;
}

static int interrupted = 0;
static pthread_mutex_t interrupted_mutex = PTHREAD_MUTEX_INITIALIZER;

int set_interrupted(int value)
{
	if (value != 0) {
		pthread_mutex_lock(&interrupted_mutex);
		if (interrupted == 0) {
			interrupted = value;
		}
		pthread_mutex_unlock(&interrupted_mutex);
	}
	return value;
}

int get_interrupted(void)
{
	int ret;
	pthread_mutex_lock(&interrupted_mutex);
	ret = interrupted;
	pthread_mutex_unlock(&interrupted_mutex);
	return ret;
}
