/* dvb.c
 * MuMuDVB - Stream a DVB transport stream.
 *
 * (C) 2004-2013 Brice DUBOST
 * (C) Dave Chapman <dave@dchapman.com> 2001, 2002.
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

/** @file
 * @brief dvb part (except tune) of mumudvb
 * Ie : setting the filters, openning the file descriptors etc...
 */

#define _GNU_SOURCE
#define _CRT_SECURE_NO_WARNINGS

#include "dvb.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#ifndef _WIN32
#include <dirent.h>
#include <unistd.h>
#else
#include <io.h>
#endif
#include <sys/types.h>
#include "log.h"
#include <sys/stat.h>

static char *log_module="DVB: ";

#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif

/**
 * @brief Check the file exists and is readable
 * Return 1 in case it exists, 0 otherwise
 */
static int file_exists(char *filename)
{
	struct stat   buffer;
	return (stat(filename, &buffer) == 0) ? 1 : 0;
}

/**
 * @brief Open the frontend associated with card
 * Return 1 in case of succes, -1 otherwise
 *
 * @param fd_frontend the file descriptor for the frontend
 * @param card the card number
 */
int
open_fe (int *fd_frontend, char *base_path, int tuner, int rw, int full_path)
{

	char *frontend_name=NULL;
	int asprintf_ret;
	int rw_flag;
	if(full_path) //used for pipe input
		asprintf_ret=asprintf(&frontend_name,"%s",base_path);
	else
		asprintf_ret=asprintf(&frontend_name,"%s/%s%d",base_path,FRONTEND_DEV_NAME,tuner);
	if(asprintf_ret==-1)
		return -1;
	if(rw)
		rw_flag=O_RDWR;
	else
		rw_flag=O_RDONLY;
	if ((*fd_frontend = open (frontend_name, rw_flag | O_NONBLOCK)) < 0)
	{
		log_message( log_module,  MSG_ERROR, "FRONTEND DEVICE: \"%s\" : %s\n", frontend_name, strerror(errno));
		free(frontend_name);
		return -1;
	}
	free(frontend_name);
	return 1;
}


/**
 * @brief Set a filter of the pid asked. The file descriptor has to be
 * opened before. Ie it will ask the card for this PID.
 * @param fd the file descriptor
 * @param pid the pid for the filter
 */
void
set_ts_filt (int fd, uint16_t pid)
{
#ifndef DISABLE_DVB_API
	struct dmx_pes_filter_params pesFilterParams;

	log_message( log_module,  MSG_DEBUG, "Setting filter for PID %d\n", pid);

	memset(&pesFilterParams, 0, sizeof(pesFilterParams));
	pesFilterParams.pid = pid;
	pesFilterParams.input = DMX_IN_FRONTEND;
	pesFilterParams.output = DMX_OUT_TS_TAP;
	pesFilterParams.pes_type = DMX_PES_OTHER;
	pesFilterParams.flags = DMX_IMMEDIATE_START;

	if (ioctl (fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0)
	{
		log_message( log_module,  MSG_ERROR, "FILTER %i: ", pid);
		log_message( log_module,  MSG_ERROR, "DMX SET PES FILTER : %s\n", strerror(errno));
	}
#else
	(void)fd;
	(void)pid;
#endif
}

/**
 * @brief Show the reception power.
 * This information is not alway reliable
 * @param fds the file descriptors of the card
 */
void *show_power_func(void* arg)
{
#ifndef DISABLE_DVB_API
	strength_parameters_t  *strengthparams;
	strengthparams= (strength_parameters_t  *) arg;
	fe_status_t festatus_old;
	int lock_lost;
	int meas_ber_ok=1;
	int meas_strength_ok=1;
	int meas_snr_ok=1;
	int meas_ub_ok=1;
	int wait_time=20;//in units of 100ms
	int i;
	strengthparams->strength = 0;
	strengthparams->ber = 0;
	strengthparams->snr = 0;
	strengthparams->ub = 0;
	strengthparams->ts_discontinuities = 0; //could be initialized somewhere else but sounds fine here
	memset(&festatus_old,0,sizeof(fe_status_t));
	lock_lost=0;
	while(!strengthparams->tune_p->strengththreadshutdown)
	{
		if(strengthparams->tune_p->card_tuned)
		{
			if(strengthparams->tune_p->display_strenght )
				mumu_timing();

			if (ioctl (strengthparams->fds->fd_frontend, FE_READ_BER, &strengthparams->ber) < 0)
			{
				if(meas_ber_ok)
				{
					meas_ber_ok=0;
					log_message( log_module,  MSG_WARN, "An issue happened during the IOCTLS to take BER measurements error: %s",strerror(errno));
				}
			}
			else
				meas_ber_ok=1;

			if (ioctl (strengthparams->fds->fd_frontend, FE_READ_SIGNAL_STRENGTH, &strengthparams->strength) < 0)
			{
				if(meas_strength_ok)
				{
					meas_strength_ok=0;
					log_message( log_module,  MSG_WARN, "An issue happened during the IOCTLS to take strength measurements error: %s",strerror(errno));
				}
			}
			else
				meas_strength_ok=1;
			if (ioctl (strengthparams->fds->fd_frontend, FE_READ_SNR, &strengthparams->snr) < 0)
			{
				if(meas_snr_ok)
				{
					meas_snr_ok=0;
					log_message( log_module,  MSG_WARN, "An issue happened during the IOCTLS to take SNR measurements error: %s",strerror(errno));
				}
			}
			else
				meas_snr_ok=1;
			if (ioctl (strengthparams->fds->fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &strengthparams->ub) < 0 )
			{
				if(meas_ub_ok)
				{
					meas_ub_ok=0;
					log_message( log_module,  MSG_WARN, "An issue happened during the IOCTLS to take uncorrected blocks measurements error: %s",strerror(errno));
				}
			}
			else
				meas_ub_ok=1;

		}
		if(strengthparams->tune_p->display_strenght && strengthparams->tune_p->card_tuned)
		{
			log_message( log_module,  MSG_INFO, "Bit error rate: %10d Signal strength: %10d SNR: %10d Uncorrected blocks: %10d\n", strengthparams->ber,strengthparams->strength,strengthparams->snr,strengthparams->ub);
			log_message( log_module,  MSG_INFO, "ts_discontinuities %10d",strengthparams->ts_discontinuities);

			log_message( log_module,  MSG_FLOOD, "Timing: ioctls took %ld micro seconds\n",mumu_timing());
		}
		if((strengthparams->tune_p->check_status ||strengthparams->tune_p->display_strenght) && strengthparams->tune_p->card_tuned)
		{
			if (ioctl (strengthparams->fds->fd_frontend, FE_READ_STATUS, &strengthparams->festatus) != -1)
			{
				if((!(strengthparams->festatus & FE_HAS_LOCK) ) && (festatus_old != strengthparams->festatus))
				{
					if(!lock_lost)
						log_message( log_module,  MSG_WARN, "The card has lost the lock (antenna unplugged ?). Detailed status");
					else
						log_message( log_module,  MSG_INFO, "Card is still not locked but status changed. Detailed status");
					print_status(strengthparams->festatus);
					festatus_old = strengthparams->festatus;
					lock_lost=1;
				}
				if((strengthparams->festatus & FE_HAS_LOCK)  && lock_lost)
				{
					log_message( log_module,  MSG_INFO, "Card is locked again.");
					lock_lost=0;
				}
			}
		}
		for(i=0;i<wait_time && !strengthparams->tune_p->strengththreadshutdown;i++)
			usleep(100000);
	}
#else
	(void)arg;
#endif
	return 0;
}


/**
 * @brief Open file descriptors for the card. open dvr and one demuxer fd per asked pid. This function can be called
 * more than one time if new pids are added (typical case autoconf)
 * return -1 in case of error
 * @param card the card number
 * @param asked_pid the array of asked pids
 * @param fds the structure with the file descriptors
 */
int
create_card_fd(char *base_path, int tuner, uint8_t *asked_pid, fds_t *fds)
{

	int curr_pid = 0;
	char *demuxdev_name=NULL;
	char *dvrdev_name=NULL;
	int asprintf_ret;

	asprintf_ret=asprintf(&demuxdev_name,"%s/%s%d",base_path,DEMUX_DEV_NAME,tuner);
	if(asprintf_ret==-1)
		return -1;

	// if demux<tuner> not found, attempt to use demux0 (for cards with multiple frontends like CXD2837ER)
	if (!file_exists(demuxdev_name) && tuner > 0) {
		asprintf_ret=asprintf(&demuxdev_name,"%s/%s%d",base_path,DEMUX_DEV_NAME,0);
		if(asprintf_ret==-1)
			return -1;
	}

	for(curr_pid=0;curr_pid<8193;curr_pid++)
		//file descriptors for the demuxer (used to set the filters)
		//we check if we need to open the file descriptor (some cards are limited)
		if ((asked_pid[curr_pid] != 0) && (fds->fd_demuxer[curr_pid] == 0)) {
#ifndef DISABLE_DVB_API
			if ((fds->fd_demuxer[curr_pid] = open(demuxdev_name, O_RDWR)) < 0) {
				log_message(log_module, MSG_ERROR, "FD PID %i: ", curr_pid);
				log_message(log_module, MSG_ERROR, "DEMUX DEVICE: %s : %s\n", demuxdev_name, strerror(errno));
				free(demuxdev_name);
				return -1;
			}
#else
			fds->fd_demuxer[curr_pid] = -1;
#endif
		}

	asprintf_ret=asprintf(&dvrdev_name,"%s/%s%d",base_path,DVR_DEV_NAME,tuner);
	if(asprintf_ret==-1)
		return -1;

	// if dvr<tuner> not found, attempt to use dvr0 (for cards with multiple frontends like CXD2837ER)
	if (!file_exists(dvrdev_name) && tuner > 0) {
		asprintf_ret=asprintf(&dvrdev_name,"%s/%s%d",base_path,DVR_DEV_NAME,0);
		if(asprintf_ret==-1)
			return -1;
	}

	if (fds->fd_dvr==0)  //this function can be called more than one time, we check if we opened it before
		if ((fds->fd_dvr = open (dvrdev_name, O_RDONLY | O_NONBLOCK)) < 0)
		{
			log_message( log_module,  MSG_ERROR, "DVR DEVICE: %s : %s\n", dvrdev_name, strerror(errno));
			free(dvrdev_name);
			return -1;
		}


	free(dvrdev_name);
	free(demuxdev_name);
	return 0;

}


/**
 * @brief Open filters for the pids in asked_pid. This function update the asked_pid array and
 * can be called more than one time if new pids are added (typical case autoconf)
 * Ie it asks the card for the pid list by calling set_ts_filt
 * @param asked_pid the array of asked pids
 * @param fds the structure with the file descriptors
 */
void set_filters(uint8_t *asked_pid, fds_t *fds)
{

	for(int curr_pid=0;curr_pid<8193;curr_pid++)
		if (asked_pid[curr_pid] == PID_ASKED )
		{
			set_ts_filt (fds->fd_demuxer[curr_pid], curr_pid);
			asked_pid[curr_pid] = PID_FILTERED;
		}

}


/**
 * @brief Close the file descriptors associated with the card
 * @param fds the structure with the file descriptors
 */
void
close_card_fd(fds_t *fds)
{
	int curr_pid = 0;

	for(curr_pid=0;curr_pid<8193;curr_pid++)
	{
		if(fds->fd_demuxer[curr_pid])
		{
			close(fds->fd_demuxer[curr_pid]);
			fds->fd_demuxer[curr_pid]=0;
		}
	}

	if(fds->fd_dvr)
		close (fds->fd_dvr);
	fds->fd_dvr=0;
	if(fds->fd_frontend)
		close (fds->fd_frontend);
	fds->fd_frontend=0;

}


/**
 * @brief Function for the tread reading data from the card
 * @param arg the structure with the thread parameters
 */
void *read_card_thread_func(void* arg)
{
	card_thread_parameters_t  *threadparams;
	threadparams= (card_thread_parameters_t  *) arg;

	int poll_ret;
	pthread_mutex_lock(&threadparams->carddatamutex);
	threadparams->card_buffer->bytes_in_write_buffer=0;
	pthread_mutex_unlock(&threadparams->carddatamutex);
	int throwing_packets=0;
	log_message( log_module,  MSG_DEBUG, "Reading thread start\n");

	usleep(100000); //some waiting to be sure the main program is waiting //it is probably useless
	while(!threadparams->threadshutdown&& !get_interrupted())
	{
		//Poll the DVB descriptors
		poll_ret=mumudvb_poll(threadparams->fds->pfds,threadparams->fds->pfdsnum,DVB_POLL_TIMEOUT);
		if(poll_ret<0)
		{
			log_message( log_module,  MSG_ERROR, "Thread polling issue\n");
			set_interrupted(-poll_ret);
			return NULL;
		}
		if((!(threadparams->fds->pfds[0].revents&POLLIN)) && (!(threadparams->fds->pfds[0].revents&POLLPRI))) //Timeout, we give the ball back to the main for unicast polling
		{
			if(threadparams->main_waiting)
			{
				pthread_cond_signal(&threadparams->threadcond);
			}
			//no DVB packet, we continue
			continue;
		}
		if((threadparams->card_buffer->bytes_in_write_buffer+TS_PACKET_SIZE*threadparams->card_buffer->dvr_buffer_size)>threadparams->card_buffer->write_buffer_size)
		{
			/**@todo : use a dynamic buffer ?*/
			if(!throwing_packets)
			{
				throwing_packets=1; /** @todo count them*/
				log_message( log_module,  MSG_INFO, "Thread trowing dvb packets\n");
			}
			if(threadparams->main_waiting)
			{
				pthread_cond_signal(&threadparams->threadcond);
			}
			continue;
		}
		throwing_packets=0;
		pthread_mutex_lock(&threadparams->carddatamutex);
		threadparams->card_buffer->bytes_in_write_buffer+=card_read(threadparams->fds->fd_dvr,
				threadparams->card_buffer->writing_buffer+threadparams->card_buffer->bytes_in_write_buffer,
				threadparams->card_buffer);

		if(threadparams->main_waiting)
		{
			pthread_cond_signal(&threadparams->threadcond);
		}
		pthread_mutex_unlock(&threadparams->carddatamutex);
	}
	return NULL;
}

/** @brief : Read data from the card
 * This function have to be called after a poll to ensure there is data to read
 *
 */
int card_read(int fd_dvr, unsigned char *dest_buffer, card_buffer_t *card_buffer)
{
	/* Attempt to read 188 bytes * dvr_buffer_size from /dev/____/dvr */
	int bytes_read;
	if ((bytes_read = read (fd_dvr, dest_buffer, TS_PACKET_SIZE*card_buffer->dvr_buffer_size)) > 0)
	{
		if((bytes_read>0 )&& (bytes_read % TS_PACKET_SIZE))
		{
			log_message( log_module,  MSG_WARN, "Warning : partial packet received len %d\n", bytes_read);
			card_buffer->partial_packet_number++;
			bytes_read-=bytes_read % TS_PACKET_SIZE;
			if(bytes_read<=0)
				return 0;
		}
	}
	if(bytes_read<0)
	{
		if(errno==EOVERFLOW)
		{
			log_message( log_module,  MSG_WARN,"Error : DVR buffer overrun \n");
			card_buffer->overflow_number++;
		} else if(errno!=EAGAIN)
			log_message( log_module,  MSG_WARN,"Error : DVR Read error : %s \n",strerror(errno));
		return 0;
	}
	return bytes_read;
}


typedef struct frontend_cap_t
{
	long int flag;
	char descr[128];
}frontend_cap_t;

/** @brief : List the capabilities of one card
 *
 *
 */
void show_card_capabilities( int card, int tuner )
{
#ifndef DISABLE_DVB_API
	int frontend_fd;
	int i_ret;
	int display_sr;
	int frequency_factor;
	/** The path of the card */
	char card_dev_path[256];
	strncpy(card_dev_path,DVB_DEV_PATH,256);
	char number[10];
	sprintf(number,"%d",card);
	int l=sizeof(card_dev_path);
	mumu_string_replace(card_dev_path,&l,0,"%card",number);
	//Open the frontend
	if(!open_fe (&frontend_fd, card_dev_path, tuner,0,0)) // we open the card readonly so we can get the capabilities event when used
		return;

	//get frontend info
	struct dvb_frontend_info fe_info;
	i_ret = ioctl(frontend_fd,FE_GET_INFO, &fe_info);
	if (i_ret < 0){
		log_message( log_module,  MSG_ERROR, "FE_GET_INFO: %s \n", strerror(errno));
		close (frontend_fd);
		return;
	}
	log_message( log_module,  MSG_INFO, "=========== Card %d - Tuner %d ===========\n", card, tuner);
	log_message( log_module,  MSG_INFO, " Frontend : %s\n", fe_info.name);
	display_sr=0;
	switch(fe_info.type)
	{
	case FE_OFDM:
		log_message( log_module,  MSG_INFO, " Terrestrial (DVB-T) card\n");
		break;
	case FE_QPSK:
		log_message( log_module,  MSG_INFO, " Satellite (DVB-S) card\n");
		display_sr=1;
		break;
	case FE_QAM:
		log_message( log_module,  MSG_INFO, " Cable (DVB-C) card\n");
		display_sr=1;
		break;
	case FE_ATSC:
		log_message( log_module,  MSG_INFO, " ATSC card\n");
		break;
	default:
		log_message( log_module,  MSG_INFO, " UNKNOWN card (0x%x)\n", fe_info.type);
		break;
	}
	if(fe_info.type==FE_QPSK)
		frequency_factor=1000;
	else
		frequency_factor=1;

	log_message( log_module,  MSG_INFO, " Frequency: %d kHz to %d kHz\n",(int) fe_info.frequency_min/1000*frequency_factor,(int) fe_info.frequency_max/1000*frequency_factor);
	if(display_sr)
		log_message( log_module,  MSG_INFO, " Symbol rate: %d k symbols/s to %d k symbols/s \n", (int)fe_info.symbol_rate_min/1000, (int)fe_info.symbol_rate_max/1000);

	log_message( log_module,  MSG_DETAIL, " == Card capabilities ==");
	log_message( log_module,  MSG_DEBUG, "caps 0x%x\n",fe_info.caps);
	frontend_cap_t caps[]={
			{0x1,"FE_CAN_INVERSION_AUTO"},
			{0x2,"FE_CAN_FEC_1_2"},
			{0x4,"FE_CAN_FEC_2_3"},
			{0x8,"FE_CAN_FEC_3_4"},
			{0x10,"FE_CAN_FEC_4_5"},
			{0x20,"FE_CAN_FEC_5_6"},
			{0x40,"FE_CAN_FEC_6_7"},
			{0x80,"FE_CAN_FEC_7_8"},
			{0x100,"FE_CAN_FEC_8_9"},
			{0x200,"FE_CAN_FEC_AUTO"},
			{0x400,"FE_CAN_QPSK"},
			{0x800,"FE_CAN_QAM_16"},
			{0x1000,"FE_CAN_QAM_32"},
			{0x2000,"FE_CAN_QAM_64"},
			{0x4000,"FE_CAN_QAM_128"},
			{0x8000,"FE_CAN_QAM_256"},
			{0x10000,"FE_CAN_QAM_AUTO"},
			{0x20000,"FE_CAN_TRANSMISSION_MODE_AUTO"},
			{0x40000,"FE_CAN_BANDWIDTH_AUTO"},
			{0x80000,"FE_CAN_GUARD_INTERVAL_AUTO"},
			{0x100000,"FE_CAN_HIERARCHY_AUTO"},
			{0x200000,"FE_CAN_8VSB"},
			{0x400000,"FE_CAN_16VSB"},
			{0x800000,"FE_HAS_EXTENDED_CAPS /* We need more bitspace for newer APIs, indicate this. */"},
			{0x10000000,"FE_CAN_2G_MODULATION /* frontend supports '2nd generation modulation' (DVB-S2) */"},
			{0x20000000,"FE_NEEDS_BENDING /* not supported anymore */"},
			{0x40000000,"FE_CAN_RECOVER /* frontend can recover from a cable unplug automatically */"},
			{0x80000000,"FE_CAN_MUTE_TS /* frontend can stop spurious TS data output */"},
	};
	int numcaps=28;
	int i;
	for(i=0;i<numcaps;i++)
		if(fe_info.caps & caps[i].flag)
			log_message( log_module,  MSG_DETAIL, "%s\n", caps[i].descr);
	close (frontend_fd);

	log_message( log_module,  MSG_INFO, "\n");
#else
	(void)card;
	(void)tuner;
#endif
}

/** @brief : List the DVB cards of the system and their capabilities
 *
 */
void list_dvb_cards(void)
{
#ifndef DISABLE_DVB_API
	DIR *dvb_dir;
	log_message( log_module, MSG_INFO,"==================================");
	log_message( log_module, MSG_INFO,"        DVB CARDS LISTING\n");
	log_message( log_module, MSG_INFO,"==================================");

	dvb_dir = opendir ("/dev/dvb/");
	if (dvb_dir == NULL)
	{
		log_message( log_module,  MSG_ERROR, "Cannot open /dev/dvb : %s\n", strerror (errno));
		return;
	}

	int card_number;
	int num_cards;
	int cards[256];
	num_cards=0;
	struct dirent *d_adapter;
	while ((d_adapter=readdir(dvb_dir))!=NULL)
	{
		if(strlen(d_adapter->d_name)<8)
			continue;
		if(strncmp(d_adapter->d_name,"adapter",7))
			continue;
		card_number= atoi(d_adapter->d_name+7);
		log_message( log_module,  MSG_DEBUG, "found adapter %d\n", card_number);
		cards[num_cards]=card_number;
		num_cards++;
		if(num_cards==256)
		{
			log_message( log_module, MSG_ERROR, "Wow ! You have a system with more than 256 DVB cards, Please Contact me :D");
			closedir(dvb_dir);
			return;
		}
	}
	closedir(dvb_dir);

	//Basic card sorting (O(N^2))
	int i,j,old_card;
	old_card=-1;
	DIR *adapter_dir;
	/** The path of the card */
	char card_dev_path[256];
	int tuner_number;
	struct dirent *d_tuner;
	for(i=0;i<num_cards;i++)
	{
		card_number=-1;
		for(j=0;j<num_cards;j++)
			if((card_number<=old_card)||((cards[j]>old_card) && (cards[j]<card_number)))
				card_number=cards[j];
		old_card=card_number;

		strncpy(card_dev_path,DVB_DEV_PATH,256);
		char number[10];
		sprintf(number,"%d",card_number);
		int l=sizeof(card_dev_path);
		mumu_string_replace(card_dev_path,&l,0,"%card",number);
		adapter_dir = opendir (card_dev_path);
		if (adapter_dir == NULL)
		{
			log_message( log_module,  MSG_ERROR, "Cannot open %s : %s\n", card_dev_path, strerror (errno));
			return;
		}
		while ((d_tuner=readdir(adapter_dir))!=NULL)
		{
			if(strlen(d_tuner->d_name)<(strlen(FRONTEND_DEV_NAME)+1))
				continue;
			if(strncmp(d_tuner->d_name,FRONTEND_DEV_NAME,strlen(FRONTEND_DEV_NAME)))
				continue;
			tuner_number= atoi(d_tuner->d_name+strlen(FRONTEND_DEV_NAME));
			log_message( log_module,  MSG_DEBUG, "\tCard %d found Frontend %d\n", card_number, tuner_number);
			/** show the current tuner */
			show_card_capabilities( card_number , tuner_number);
		}
		closedir(adapter_dir);
	}
#endif
}
