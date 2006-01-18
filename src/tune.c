/* dvbtune - tune.c

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
   
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>

#include "tune.h"


void print_status(FILE* fd,fe_status_t festatus) {
  fprintf(fd,"FE_STATUS:");
  if (festatus & FE_HAS_SIGNAL) fprintf(fd," FE_HAS_SIGNAL");
  if (festatus & FE_TIMEDOUT) fprintf(fd," FE_TIMEDOUT");
  if (festatus & FE_HAS_LOCK) fprintf(fd," FE_HAS_LOCK");
  if (festatus & FE_HAS_CARRIER) fprintf(fd," FE_HAS_CARRIER");
  if (festatus & FE_HAS_VITERBI) fprintf(fd," FE_HAS_VITERBI");
  if (festatus & FE_HAS_SYNC) fprintf(fd," FE_HAS_SYNC");
  fprintf(fd,"\n");
}


struct diseqc_cmd {
   struct dvb_diseqc_master_cmd cmd;
   uint32_t wait;
};

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
    usleep(cmd->wait * 1000);
    usleep(15 * 1000);
   }
   else	//A or B simple diseqc
   {
    fprintf(stderr, "SETTING SIMPLE %c BURST\n", sat_no);
    if(ioctl(fd, FE_DISEQC_SEND_BURST, (sat_no == 'B' ? SEC_MINI_B : SEC_MINI_A)) < 0)
   	return -1;
    usleep(15 * 1000);
   }
   if(ioctl(fd, FE_SET_TONE, t) < 0)
   	return -1;

   return 0;
}

/* digital satellite equipment control,
 * specification is available from http://www.eutelsat.com/ 
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
	fprintf(stderr, "Setting only tone %s and voltage %dV\n", (hi_lo ? "ON" : "OFF"), (polv ? 13 : 18));
	
	if(ioctl(fd, FE_SET_VOLTAGE, (polv ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18)) < 0)
   	    return -1;
	    
	if(ioctl(fd, FE_SET_TONE, (hi_lo ? SEC_TONE_ON : SEC_TONE_OFF)) < 0)
   	    return -1;
	
	usleep(15 * 1000);
	
	return 0;
    }
}

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
    perror("ERROR tuning channel\n");
    return -1;
  }

  pfd[0].fd = fd_frontend;
  pfd[0].events = POLLPRI;

  event.status=0;
  while (((event.status & FE_TIMEDOUT)==0) && ((event.status & FE_HAS_LOCK)==0)) {
    fprintf(stderr,"polling....\n");
    if (poll(pfd,1,10000) > 0){
      if (pfd[0].revents & POLLPRI){
        fprintf(stderr,"Getting frontend event\n");
        if ((status = ioctl(fd_frontend, FE_GET_EVENT, &event)) < 0){
	  if (errno != EOVERFLOW) {
	    perror("FE_GET_EVENT");
	    fprintf(stderr,"status = %d\n", status);
	    fprintf(stderr,"errno = %d\n", errno);
	    return -1;
	  }
	  else fprintf(stderr,"Overflow error, trying again (status = %d, errno = %d)", status, errno);
        }
      }
      if(display_strength)
	{
	  //Braice
	  strength=0;
	  if(ioctl(fd_frontend,FE_READ_SIGNAL_STRENGTH,&strength) >= 0)
	    fprintf(stderr,"Strength: %10d ",strength);
	  strength=0;
	  if(ioctl(fd_frontend,FE_READ_SNR,&strength) >= 0)
	    fprintf(stderr,"SNR: %10d\n",strength);
	}

      print_status(stderr,event.status);
    }
  }

  if (event.status & FE_HAS_LOCK) {
      switch(type) {
         case FE_OFDM:
           fprintf(stderr,"Event:  Frequency: %d\n",event.parameters.frequency);
           break;
         case FE_QPSK:
           fprintf(stderr,"Event:  Frequency: %d\n",(unsigned int)((event.parameters.frequency)+(hi_lo ? LOF2 : LOF1)));
           fprintf(stderr,"        SymbolRate: %d\n",event.parameters.u.qpsk.symbol_rate);
           fprintf(stderr,"        FEC_inner:  %d\n",event.parameters.u.qpsk.fec_inner);
           fprintf(stderr,"\n");
           break;
         case FE_QAM:
           fprintf(stderr,"Event:  Frequency: %d\n",event.parameters.frequency);
           fprintf(stderr,"        SymbolRate: %d\n",event.parameters.u.qpsk.symbol_rate);
           fprintf(stderr,"        FEC_inner:  %d\n",event.parameters.u.qpsk.fec_inner);
           break;
         default:
           break;
      }

      strength=0;
      if(ioctl(fd_frontend,FE_READ_BER,&strength) >= 0)
      fprintf(stderr,"Bit error rate: %d\n",strength);

      strength=0;
      if(ioctl(fd_frontend,FE_READ_SIGNAL_STRENGTH,&strength) >= 0)
      fprintf(stderr,"Signal strength: %d\n",strength);

      strength=0;
      if(ioctl(fd_frontend,FE_READ_SNR,&strength) >= 0)
      fprintf(stderr,"SNR: %d\n",strength);

      festatus=0;
      if(ioctl(fd_frontend,FE_READ_STATUS,&festatus) >= 0)
      print_status(stderr,festatus);
    } else {
    fprintf(stderr,"Not able to lock to the signal on the given frequency\n");
    return -1;
  }
  return 0;
}

int tune_it(int fd_frontend, unsigned int freq, unsigned int srate, char pol, int tone, fe_spectral_inversion_t specInv, unsigned char diseqc,fe_modulation_t modulation,fe_code_rate_t HP_CodeRate,fe_transmit_mode_t TransmissionMode,fe_guard_interval_t guardInterval, fe_bandwidth_t bandwidth, fe_code_rate_t LP_CodeRate, fe_hierarchy_t hier, int display_strength) {
  int res, hi_lo, dfd;
  struct dvb_frontend_parameters feparams;
  struct dvb_frontend_info fe_info;

  //no warning
  hi_lo = 0;

  if ( (res = ioctl(fd_frontend,FE_GET_INFO, &fe_info) < 0)){
     perror("FE_GET_INFO: ");
     return -1;
  }
  
  fprintf(stderr,"Using DVB card \"%s\"\n",fe_info.name);

  switch(fe_info.type) {
    case FE_OFDM:
      if (freq < 1000000) freq*=1000UL;
      feparams.frequency=freq;
      feparams.inversion=INVERSION_AUTO;
      feparams.u.ofdm.bandwidth=bandwidth;
      feparams.u.ofdm.code_rate_HP=HP_CodeRate;
      feparams.u.ofdm.code_rate_LP=LP_CodeRate;
      feparams.u.ofdm.constellation=modulation;
      feparams.u.ofdm.transmission_mode=TransmissionMode;
      feparams.u.ofdm.guard_interval=guardInterval;
      feparams.u.ofdm.hierarchy_information=hier;
      fprintf(stderr,"tuning DVB-T (%s) to %d Hz, Bandwidth: %d\n",DVB_T_LOCATION,freq, 
	bandwidth==BANDWIDTH_8_MHZ ? 8 : (bandwidth==BANDWIDTH_7_MHZ ? 7 : 6));
      break;
    case FE_QPSK:
    	pol = toupper(pol);
        if (freq < SLOF) {
          feparams.frequency=(freq-LOF1);
	  hi_lo = 0;
        } else {
          feparams.frequency=(freq-LOF2);
	  hi_lo = 1;
      }

	fprintf(stderr,"tuning DVB-S to Freq: %u, Pol:%c Srate=%d, 22kHz tone=%s, LNB: %d\n",feparams.frequency,pol,srate,tone == SEC_TONE_ON ? "on" : "off", diseqc);
      feparams.inversion=specInv;
      feparams.u.qpsk.symbol_rate=srate;
      feparams.u.qpsk.fec_inner=FEC_AUTO;
      dfd = fd_frontend;

   if(do_diseqc(dfd, diseqc, (pol == 'V' ? 1 : 0), hi_lo) == 0)
	fprintf(stderr, "DISEQC SETTING SUCCEDED\n");
   else  {
	fprintf(stderr, "DISEQC SETTING FAILED\n");
          return -1;
        }
      break;
    case FE_QAM:
      fprintf(stderr,"tuning DVB-C to %d, srate=%d\n",freq,srate);
      feparams.frequency=freq;
      feparams.inversion=INVERSION_OFF;
      feparams.u.qam.symbol_rate = srate;
      feparams.u.qam.fec_inner = FEC_AUTO;
      feparams.u.qam.modulation = modulation;
      break;
    default:
      fprintf(stderr,"Unknown FE type. Aborting\n");
      exit(-1);
  }
  usleep(100000);

  return(check_status(fd_frontend,fe_info.type,&feparams,hi_lo,display_strength));
}
