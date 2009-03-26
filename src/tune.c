/* dvbtune - tune.c

   part of mumudvb

   last version availaible from http://mumudvb.braice.net/

   Copyright (C) 2004-2009 Brice DUBOST
   Copyright (C) Dave Chapman 2001,2002
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/

/** @file
 * @brief Tuning of the dvb card
 *
 * This file contains functions for tuning the card, or displaying signal strength...
 */
   
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <string.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>

#include "tune.h"
#include "mumudvb.h"


/*Do we support ATSC ?*/
#undef ATSC
#if defined(DVB_API_VERSION_MINOR)
#if DVB_API_VERSION == 3 && DVB_API_VERSION_MINOR >= 1
#define ATSC 1
#endif
#endif


/** @ brief Print the status 
 * Print the status contained in festatus, this status says if the card is lock, sync etc.
 *
 * @param festatus the status to display
*/
void print_status(fe_status_t festatus) {
  log_message( MSG_INFO, "FE_STATUS:\n");
  if (festatus & FE_HAS_SIGNAL) log_message( MSG_INFO, "     FE_HAS_SIGNAL : found something above the noise level\n");
  if (festatus & FE_HAS_CARRIER) log_message( MSG_INFO, "     FE_HAS_CARRIER : found a DVB signal\n");
  if (festatus & FE_HAS_VITERBI) log_message( MSG_INFO, "     FE_HAS_VITERBI : FEC is stable\n");
  if (festatus & FE_HAS_SYNC) log_message( MSG_INFO, "     FE_HAS_SYNC : found sync bytes\n");
  if (festatus & FE_HAS_LOCK) log_message( MSG_INFO, "     FE_HAS_LOCK : everything's working... \n");
  if (festatus & FE_TIMEDOUT) log_message( MSG_INFO, "     FE_TIMEDOUT : no lock within the last ... seconds\n");
  if (festatus & FE_REINIT) log_message( MSG_INFO, "     FE_REINIT : frontend was reinitialized\n");
}


/** @todo document*/
struct diseqc_cmd {
   struct dvb_diseqc_master_cmd cmd;
   uint32_t wait;
};

/** @todo document*/
static int diseqc_send_msg(int fd, fe_sec_voltage_t v, struct diseqc_cmd *cmd,
		     fe_sec_tone_mode_t t, unsigned char sat_no)
{
   if(ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) < 0)
   	return -1;
   if(ioctl(fd, FE_SET_VOLTAGE, v) < 0)
   	return -1;
   usleep(15 * 1000);
   if(sat_no >= 1 && sat_no <= 4)	//1.x compatible equipment
   {
     if(ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd->cmd) < 0)
       return -1;
     usleep(15 * 1000);
    if(ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd->cmd) < 0)
   	return -1;
    usleep(cmd->wait * 1000);
    usleep(15 * 1000);
   }
   else	//A or B simple diseqc
   {
    log_message( MSG_INFO, "SETTING SIMPLE %c BURST\n", sat_no);
    if(ioctl(fd, FE_DISEQC_SEND_BURST, (sat_no == 'B' ? SEC_MINI_B : SEC_MINI_A)) < 0)
   	return -1;
    usleep(15 * 1000);
   }
   if(ioctl(fd, FE_SET_TONE, t) < 0)
   	return -1;

   return 0;
}

/** digital satellite equipment control,
 * specification is available from http://www.eutelsat.com/ 
 * @todo document more
 */
static int do_diseqc(int fd, unsigned char sat_no, int polv, int hi_lo)
{
    struct diseqc_cmd cmd =  { {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4}, 0 };

    if(sat_no != 0)
    {
	unsigned char d = sat_no;

	/* param: high nibble: reset bits, low nibble set bits,
	* bits are: option, position, polarizaion, band
	*/
	sat_no--;
	cmd.cmd.msg[3] =
    	    0xf0 | (((sat_no * 4) & 0x0f) | (polv ? 0 : 2) | (hi_lo ? 1 : 0));

	return diseqc_send_msg(fd, 
		    polv ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18,
		    &cmd, 
		    hi_lo ? SEC_TONE_ON : SEC_TONE_OFF, 
		    d);
    }
    else 	//only tone and voltage
    {
	log_message( MSG_INFO, "Setting only tone %s and voltage %dV\n", (hi_lo ? "ON" : "OFF"), (polv ? 13 : 18));
	
	if(ioctl(fd, FE_SET_VOLTAGE, (polv ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18)) < 0)
   	    return -1;
	    
	if(ioctl(fd, FE_SET_TONE, (hi_lo ? SEC_TONE_ON : SEC_TONE_OFF)) < 0)
   	    return -1;
	
	usleep(15 * 1000);
	
	return 0;
    }
}

/** @todo document*/
int check_status(int fd_frontend,int type, struct dvb_frontend_parameters* feparams,int hi_lo, int display_strength) {
  int32_t strength;
  fe_status_t festatus;
  struct dvb_frontend_event event;
  struct pollfd pfd[1];
  int status;

  while(1)  {
	if (ioctl(fd_frontend, FE_GET_EVENT, &event) < 0)	//EMPTY THE EVENT QUEUE
	break;
  }
  
  if (ioctl(fd_frontend,FE_SET_FRONTEND,feparams) < 0) {
    log_message( MSG_ERROR, "ERROR tuning channel : %s \n", strerror(errno));
    return -1;
  }

  pfd[0].fd = fd_frontend;
  pfd[0].events = POLLPRI;

  event.status=0;
  while (((event.status & FE_TIMEDOUT)==0) && ((event.status & FE_HAS_LOCK)==0)) {
    log_message( MSG_DETAIL, "polling....\n");
    if (poll(pfd,1,10000) > 0){
      if (pfd[0].revents & POLLPRI){
        log_message( MSG_DETAIL, "Getting frontend event\n");
        if ((status = ioctl(fd_frontend, FE_GET_EVENT, &event)) < 0){
	  if (errno != EOVERFLOW) {
	    log_message( MSG_ERROR, "FE_GET_EVENT %s. status = %s\n", strerror(errno), status);
	    return -1;
	  }
	  else log_message( MSG_WARN, "Overflow error, trying again (status = %d, errno = %d)", status, errno);
        }
      }
      if(display_strength)
	{
	  strength=0;
	  if(ioctl(fd_frontend,FE_READ_SIGNAL_STRENGTH,&strength) >= 0)
	    log_message( MSG_INFO, "Strength: %10d ",strength);
	  strength=0;
	  if(ioctl(fd_frontend,FE_READ_SNR,&strength) >= 0)
	    log_message( MSG_INFO, "SNR: %10d\n",strength);
	}

      print_status(event.status);
    }
  }

  if (event.status & FE_HAS_LOCK) {
      switch(type) {
         case FE_OFDM:
           log_message( MSG_INFO, "Event:  Frequency: %d\n",event.parameters.frequency);
	   /**\todo : display the other parameters*/
           break;
         case FE_QPSK:
           log_message( MSG_INFO, "Event:  Frequency: %d\n",(unsigned int)((event.parameters.frequency)+(hi_lo ? LOF2 : LOF1)));
           log_message( MSG_INFO, "        SymbolRate: %d\n",event.parameters.u.qpsk.symbol_rate);
           log_message( MSG_INFO, "        FEC_inner:  %d\n",event.parameters.u.qpsk.fec_inner);
           log_message( MSG_INFO, "\n");
           break;
         case FE_QAM:
           log_message( MSG_INFO, "Event:  Frequency: %d\n",event.parameters.frequency);
           log_message( MSG_INFO, "        SymbolRate: %d\n",event.parameters.u.qpsk.symbol_rate);
           log_message( MSG_INFO, "        FEC_inner:  %d\n",event.parameters.u.qpsk.fec_inner);
           break;
         default:
           break;
      }

      strength=0;
      if(ioctl(fd_frontend,FE_READ_BER,&strength) >= 0)
      log_message( MSG_INFO, "Bit error rate: %d\n",strength);

      strength=0;
      if(ioctl(fd_frontend,FE_READ_SIGNAL_STRENGTH,&strength) >= 0)
      log_message( MSG_INFO, "Signal strength: %d\n",strength);

      strength=0;
      if(ioctl(fd_frontend,FE_READ_SNR,&strength) >= 0)
      log_message( MSG_INFO, "SNR: %d\n",strength);

      festatus=0;
      if(ioctl(fd_frontend,FE_READ_STATUS,&festatus) >= 0)
      print_status(festatus);
    } else {
    log_message( MSG_ERROR, "Not able to lock to the signal on the given frequency\n");
    return -1;
  }
  return 0;
}

int tune_it(int fd_frontend, tuning_parameters_t tuneparams)
{
  int res, hi_lo, dfd;
  struct dvb_frontend_parameters feparams;
  struct dvb_frontend_info fe_info;

  //no warning
  hi_lo = 0;

  if ( (res = ioctl(fd_frontend,FE_GET_INFO, &fe_info) < 0)){
    log_message( MSG_ERROR, "FE_GET_INFO: %s \n", strerror(errno));
    return -1;
  }

  /**\todo here check the capabilities of the card*/

  log_message( MSG_INFO, "Using DVB card \"%s\"\n",fe_info.name);

  switch(fe_info.type) {
  case FE_OFDM: //DVB-T
    if (tuneparams.freq < 1000000) tuneparams.freq*=1000UL;
    feparams.frequency=tuneparams.freq;
    feparams.inversion=INVERSION_AUTO;
    feparams.u.ofdm.bandwidth=tuneparams.bandwidth;
    feparams.u.ofdm.code_rate_HP=tuneparams.HP_CodeRate;
    feparams.u.ofdm.code_rate_LP=tuneparams.LP_CodeRate;
    feparams.u.ofdm.constellation=tuneparams.modulation;
    feparams.u.ofdm.transmission_mode=tuneparams.TransmissionMode;
    feparams.u.ofdm.guard_interval=tuneparams.guardInterval;
    feparams.u.ofdm.hierarchy_information=tuneparams.hier;
    log_message( MSG_INFO, "tuning DVB-T to %d Hz, Bandwidth: %d\n", tuneparams.freq, 
		 tuneparams.bandwidth==BANDWIDTH_8_MHZ ? 8 : (tuneparams.bandwidth==BANDWIDTH_7_MHZ ? 7 : 6));
    break;
  case FE_QPSK: //DVB-S
    tuneparams.pol = toupper(tuneparams.pol);
    if (tuneparams.freq < SLOF) {
      feparams.frequency=(tuneparams.freq-LOF1);
      hi_lo = 0;
    } else {
      feparams.frequency=(tuneparams.freq-LOF2);
      hi_lo = 1;
    }
    
    log_message( MSG_INFO, "tuning DVB-S to Freq: %u, Pol:%c Srate=%d, LNB: %d\n",feparams.frequency,tuneparams.pol,
		 tuneparams.srate, tuneparams.sat_number);
    feparams.inversion=tuneparams.specInv;
    feparams.u.qpsk.symbol_rate=tuneparams.srate;
    feparams.u.qpsk.fec_inner=FEC_AUTO;
    dfd = fd_frontend;
    
    if(do_diseqc(dfd, tuneparams.sat_number, (tuneparams.pol == 'V' ? 1 : 0), hi_lo) == 0)
      log_message( MSG_INFO, "DISEQC SETTING SUCCEDED\n");
    else  {
      log_message( MSG_INFO, "DISEQC SETTING FAILED\n");
      return -1;
    }
    break;
  case FE_QAM: //DVB-C
    log_message( MSG_INFO, "tuning DVB-C to %d, srate=%d\n",tuneparams.freq,tuneparams.srate);
    feparams.frequency=tuneparams.freq;
    feparams.inversion=INVERSION_OFF;
    feparams.u.qam.symbol_rate = tuneparams.srate;
    feparams.u.qam.fec_inner = FEC_AUTO;
    feparams.u.qam.modulation = tuneparams.modulation;
    break;
#ifdef ATSC
  case FE_ATSC: //ATSC
    log_message( MSG_INFO, "tuning ATSC to %d, modulation=%d\n",tuneparams.freq,tuneparams.atsc_modulation);
    feparams.frequency=tuneparams.freq;
    feparams.u.vsb.modulation = tuneparams.atsc_modulation;
    break;
#endif
  default:
    log_message( MSG_ERROR, "Unknown FE type : %x. Aborting\n", fe_info.type);
    exit(-1);
  }
  usleep(100000);

  return(check_status(fd_frontend,fe_info.type,&feparams,hi_lo,tuneparams.display_strenght));
}
