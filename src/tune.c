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
#include "errors.h"


extern int Interrupted;

/** @brief Read a line of the configuration file to check if there is a tuning parameter
 *
 * @param tuneparams the tuning parameters
 * @param substring The currrent line
 */
int read_tuning_configuration(tuning_parameters_t *tuneparams, char *substring)
{

  char delimiteurs[] = CONFIG_FILE_SEPARATOR;
  if (!strcmp (substring, "sat_number"))
  {
    substring = strtok (NULL, delimiteurs);
    tuneparams->sat_number = atoi (substring);
    if (tuneparams->sat_number > 4)
    {
      log_message( MSG_ERROR,
                   "Config issue : sat_number. The satellite number must be between 0 and 4. Please report if you have an equipment wich support more\n");
      return -1;
    }
  }
#ifdef ATSC
  else if (!strcmp (substring, "atsc_modulation"))
  {
    log_message( MSG_WARN, "Warning : The option atsc_modulation is deprecated, use the option modulation instead\n"); 
    substring = strtok (NULL, delimiteurs);
    if (!strcmp (substring, "vsb8"))
      tuneparams->modulation = VSB_8;
    else if (!strcmp (substring, "vsb16"))
      tuneparams->modulation = VSB_16;
    else if (!strcmp (substring, "qam256"))
      tuneparams->modulation = QAM_256;
    else if (!strcmp (substring, "qam64"))
      tuneparams->modulation = QAM_64;
    else if (!strcmp (substring, "qamauto"))
      tuneparams->modulation = QAM_AUTO;
    else 
    {
      log_message( MSG_WARN,
                   "Bad value for atsc_modulation, will try VSB_8 (usual modulation for terrestrial)\n"); //Note : see the initialisation of tuneparams for the default value
    }
  }
#endif
  else if (!strcmp (substring, "dont_tune"))
  {
    substring = strtok (NULL, delimiteurs);
    tuneparams->dont_tune = atoi (substring);
  }
  else if (!strcmp (substring, "freq"))
  {
    substring = strtok (NULL, delimiteurs);
    tuneparams->freq = atol (substring);
    tuneparams->freq *= 1000UL;
  }
  else if (!strcmp (substring, "pol"))
  {
    substring = strtok (NULL, delimiteurs);
    if (tolower (substring[0]) == 'v')
    {
      tuneparams->pol = 'V';
    }
    else if (tolower (substring[0]) == 'h')
    {
      tuneparams->pol = 'H';
    }
    else if (tolower (substring[0]) == 'l')
    {
      tuneparams->pol = 'L';
    }
    else if (tolower (substring[0]) == 'r')
    {
      tuneparams->pol = 'R';
    }
    else
    {
      log_message( MSG_ERROR,
                   "Config issue : polarisation\n");
      return -1;
    }
  }
  else if (!strcmp (substring, "lnb_voltage_off"))
  {
    substring = strtok (NULL, delimiteurs);
    tuneparams->lnb_voltage_off = atoi(substring);
  }
  else if (!strcmp (substring, "lnb_type"))
  {
    substring = strtok (NULL, delimiteurs);
    if(!strcmp (substring, "universal"))
      tuneparams->lnb_type=LNB_UNIVERSAL;
    else if(!strcmp (substring, "standard"))
      tuneparams->lnb_type=LNB_STANDARD;
    else
    {
      log_message( MSG_ERROR,
                   "Config issue : lnb_type\n");
      return -1;
    }
  }
  else if (!strcmp (substring, "lnb_lof_standard"))
  {
    substring = strtok (NULL, delimiteurs);
    tuneparams->lnb_lof_standard = atoi(substring)*1000UL;
  }
  else if (!strcmp (substring, "lnb_slof"))
  {
    substring = strtok (NULL, delimiteurs);
    tuneparams->lnb_slof = atoi(substring)*1000UL;
  }
  else if (!strcmp (substring, "lnb_lof_high"))
  {
    substring = strtok (NULL, delimiteurs);
    tuneparams->lnb_lof_high = atoi(substring)*1000UL;
  }
  else if (!strcmp (substring, "lnb_lof_low"))
  {
    substring = strtok (NULL, delimiteurs);
    tuneparams->lnb_lof_low = atoi(substring)*1000UL;
  }
  else if (!strcmp (substring, "srate"))
  {
    substring = strtok (NULL, delimiteurs);
    tuneparams->srate = atol (substring);
    tuneparams->srate *= 1000UL;
  }
  else if (!strcmp (substring, "card"))
  {
    substring = strtok (NULL, delimiteurs);
    tuneparams->card = atoi (substring);
  }
  else if (!strcmp (substring, "qam"))
  {
  // DVB-T
    log_message( MSG_WARN, "Warning : The option qam is deprecated, use the option modulation instead\n");
    substring = strtok (NULL, delimiteurs);
    sscanf (substring, "%s\n", substring);
    if (!strcmp (substring, "qpsk"))
      tuneparams->modulation=QPSK;
    else if (!strcmp (substring, "16"))
      tuneparams->modulation=QAM_16;
    else if (!strcmp (substring, "32"))
      tuneparams->modulation=QAM_32;
    else if (!strcmp (substring, "64"))
      tuneparams->modulation=QAM_64;
    else if (!strcmp (substring, "128"))
      tuneparams->modulation=QAM_128;
    else if (!strcmp (substring, "256"))
      tuneparams->modulation=QAM_256;
    else if (!strcmp (substring, "auto"))
      tuneparams->modulation=QAM_AUTO;
    else
    {
      log_message( MSG_ERROR,
                   "Config issue : QAM\n");
      return -1;
    }
  }
  else if (!strcmp (substring, "trans_mode"))
  {
    // DVB-T
    substring = strtok (NULL, delimiteurs);
    sscanf (substring, "%s\n", substring);
    if (!strcmp (substring, "2k"))
      tuneparams->TransmissionMode=TRANSMISSION_MODE_2K;
    else if (!strcmp (substring, "8k"))
      tuneparams->TransmissionMode=TRANSMISSION_MODE_8K;
    else if (!strcmp (substring, "auto"))
      tuneparams->TransmissionMode=TRANSMISSION_MODE_AUTO;
    else
    {
      log_message( MSG_ERROR,
                   "Config issue : trans_mode\n");
      return -1;
    }
  }
  else if (!strcmp (substring, "bandwidth"))
  {
	  // DVB-T
    substring = strtok (NULL, delimiteurs);
    sscanf (substring, "%s\n", substring);
    if (!strcmp (substring, "8MHz"))
      tuneparams->bandwidth=BANDWIDTH_8_MHZ;
    else if (!strcmp (substring, "7MHz"))
      tuneparams->bandwidth=BANDWIDTH_7_MHZ;
    else if (!strcmp (substring, "6MHz"))
      tuneparams->bandwidth=BANDWIDTH_6_MHZ;
    else if (!strcmp (substring, "auto"))
      tuneparams->bandwidth=BANDWIDTH_AUTO;
    else
    {
      log_message( MSG_ERROR,
                   "Config issue : bandwidth\n");
      return -1;
    }
  }
  else if (!strcmp (substring, "guardinterval"))
  {
	  // DVB-T
    substring = strtok (NULL, delimiteurs);
    sscanf (substring, "%s\n", substring);
    if (!strcmp (substring, "1/32"))
      tuneparams->guardInterval=GUARD_INTERVAL_1_32;
    else if (!strcmp (substring, "1/16"))
      tuneparams->guardInterval=GUARD_INTERVAL_1_16;
    else if (!strcmp (substring, "1/8"))
      tuneparams->guardInterval=GUARD_INTERVAL_1_8;
    else if (!strcmp (substring, "1/4"))
      tuneparams->guardInterval=GUARD_INTERVAL_1_4;
    else if (!strcmp (substring, "auto"))
      tuneparams->guardInterval=GUARD_INTERVAL_AUTO;
    else
    {
      log_message( MSG_ERROR,
                   "Config issue : guardinterval\n");
      return -1;
    }
  }
  else if (!strcmp (substring, "coderate"))
  {
	  // DVB-T
    substring = strtok (NULL, delimiteurs);
    sscanf (substring, "%s\n", substring);
    if (!strcmp (substring, "none"))
      tuneparams->HP_CodeRate=FEC_NONE;
    else if (!strcmp (substring, "1/2"))
      tuneparams->HP_CodeRate=FEC_1_2;
    else if (!strcmp (substring, "2/3"))
      tuneparams->HP_CodeRate=FEC_2_3;
    else if (!strcmp (substring, "3/4"))
      tuneparams->HP_CodeRate=FEC_3_4;
    else if (!strcmp (substring, "4/5"))
      tuneparams->HP_CodeRate=FEC_4_5;
    else if (!strcmp (substring, "5/6"))
      tuneparams->HP_CodeRate=FEC_5_6;
    else if (!strcmp (substring, "6/7"))
      tuneparams->HP_CodeRate=FEC_6_7;
    else if (!strcmp (substring, "7/8"))
      tuneparams->HP_CodeRate=FEC_7_8;
    else if (!strcmp (substring, "8/9"))
      tuneparams->HP_CodeRate=FEC_8_9;
    else if (!strcmp (substring, "auto"))
      tuneparams->HP_CodeRate=FEC_AUTO;
    else
    {
      log_message( MSG_ERROR,
                   "Config issue : coderate\n");
      return -1;
    }
	  tuneparams->LP_CodeRate=tuneparams->HP_CodeRate; // I found the following : 
	  //In order to achieve hierarchy, two different code rates may be applied to two different levels of the modulation. Since hierarchy is not implemented ...
  }
  else if (!strcmp (substring, "delivery_system"))
  {
#if DVB_API_VERSION >= 5
    substring = strtok (NULL, delimiteurs);
    sscanf (substring, "%s\n", substring);
    if (!strcmp (substring, "DVBC_ANNEX_AC"))
      tuneparams->delivery_system=SYS_DVBC_ANNEX_AC;
    else if (!strcmp (substring, "DVBC_ANNEX_B"))
      tuneparams->delivery_system=SYS_DVBC_ANNEX_B;
    else if (!strcmp (substring, "DVBT"))
      tuneparams->delivery_system=SYS_DVBT;
    else if (!strcmp (substring, "DSS"))
      tuneparams->delivery_system=SYS_DSS;
    else if (!strcmp (substring, "DVBS"))
      tuneparams->delivery_system=SYS_DVBS;
    else if (!strcmp (substring, "DVBS2"))
      tuneparams->delivery_system=SYS_DVBS2;
    else if (!strcmp (substring, "DVBH"))
      tuneparams->delivery_system=SYS_DVBH;
    else if (!strcmp (substring, "ISDBT"))
      tuneparams->delivery_system=SYS_ISDBT;
    else if (!strcmp (substring, "ISDBS"))
      tuneparams->delivery_system=SYS_ISDBS;
    else if (!strcmp (substring, "ISDBS"))
      tuneparams->delivery_system=SYS_ISDBS;
    else if (!strcmp (substring, "ISDBC"))
      tuneparams->delivery_system=SYS_ISDBC;
    else if (!strcmp (substring, "ATSC"))
      tuneparams->delivery_system=SYS_ATSC;
    else if (!strcmp (substring, "ATSCMH"))
      tuneparams->delivery_system=SYS_ATSCMH;
    else if (!strcmp (substring, "DMBTH"))
      tuneparams->delivery_system=SYS_DMBTH;
    else if (!strcmp (substring, "CMMB"))
      tuneparams->delivery_system=SYS_CMMB;
    else if (!strcmp (substring, "DAB"))
      tuneparams->delivery_system=SYS_DAB;
    else
    {
      log_message( MSG_ERROR,
                   "Config issue : delivery_system. Unknown delivery_system : %s\n",substring);
      return -1;
    }
    log_message( MSG_INFO,
                 "You will use DVB API version 5 for tuning your card.\n");
#else
    log_message( MSG_ERROR,
                 "Config issue : delivery_system. You are trying to set the delivery system but your MuMuDVB have not been built with DVB-S2/DVB API 5 support.\n");
    return -1;
#endif
  }
  else if (!strcmp (substring, "rolloff"))
  {
#if DVB_API_VERSION >= 5
    substring = strtok (NULL, delimiteurs);
    sscanf (substring, "%s\n", substring);
    if (!strcmp (substring, "35"))
      tuneparams->rolloff=ROLLOFF_35;
    else if (!strcmp (substring, "20"))
      tuneparams->rolloff=ROLLOFF_20;
    else if (!strcmp (substring, "25"))
      tuneparams->rolloff=ROLLOFF_25;
    else if (!strcmp (substring, "auto"))
      tuneparams->rolloff=ROLLOFF_AUTO;
    else
    {
      log_message( MSG_ERROR,
                   "Config issue : delivery_system. Unknown delivery_system : %s\n",substring);
      return -1;
    }
    log_message( MSG_INFO,
                 "You will use DVB API version 5 for tuning your card.\n");
#else
    log_message( MSG_ERROR,
                 "Config issue : delivery_system. You are trying to set the delivery system but your MuMuDVB have not been built with DVB-S2/DVB API 5 support.\n");
    return -1;
#endif
  }
  else if (!strcmp (substring, "modulation"))
  {
    substring = strtok (NULL, delimiteurs);
    if (!strcmp (substring, "QPSK"))
      tuneparams->modulation = QPSK;
    else if (!strcmp (substring, "QAM_16"))
      tuneparams->modulation = QAM_16;
    else if (!strcmp (substring, "QAM_32"))
      tuneparams->modulation = QAM_32;
    else if (!strcmp (substring, "QAM_64"))
      tuneparams->modulation = QAM_64;
    else if (!strcmp (substring, "QAM_128"))
      tuneparams->modulation = QAM_128;
    else if (!strcmp (substring, "QAM_256"))
      tuneparams->modulation = QAM_256;
    else if (!strcmp (substring, "QAM_AUTO"))
      tuneparams->modulation = QAM_AUTO;
#ifdef ATSC
    else if (!strcmp (substring, "VSB_8"))
      tuneparams->modulation = VSB_8;
    else if (!strcmp (substring, "VSB_16"))
      tuneparams->modulation = VSB_16;
#endif
#if DVB_API_VERSION >= 5
    else if (!strcmp (substring, "PSK_8"))
      tuneparams->modulation = PSK_8;
    else if (!strcmp (substring, "APSK_16"))
      tuneparams->modulation = APSK_16;
    else if (!strcmp (substring, "APSK_32"))
      tuneparams->modulation = APSK_32;
    else if (!strcmp (substring, "DQPSK"))
      tuneparams->modulation = DQPSK;
#endif
    else 
    {
      log_message( MSG_ERROR,
                   "Config issue : Bad value for modulation\n");
      return -1;
    }
  }

  else
    return 0; //Nothing concerning tuning, we return 0 to explore the other possibilities

  return 1;//We found something for tuning, we tell main to go for the next line

}





/** @brief Print the status 
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


/** The structure for a diseqc command*/
struct diseqc_cmd {
   struct dvb_diseqc_master_cmd cmd;
   uint32_t wait;
};

/** @brief Send a diseqc message
 *
 * As defined in the DiseqC norm, we stop the 22kHz tone, we set the voltage. Wait. send the command. Wait. put back the 22kHz tone
 *
 */
static int diseqc_send_msg(int fd, fe_sec_voltage_t v, struct diseqc_cmd *cmd,
		     fe_sec_tone_mode_t t)
{
   if(ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) < 0)
   	return -1;
   if(ioctl(fd, FE_SET_VOLTAGE, v) < 0)
   	return -1;
   usleep(15 * 1000);
   //1.x compatible equipment
   if(ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd->cmd) < 0)
     return -1;
   usleep(15 * 1000);
   if(ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd->cmd) < 0)
     return -1;
   usleep(cmd->wait * 1000);
   usleep(15 * 1000);


   if(ioctl(fd, FE_SET_TONE, t) < 0)
   	return -1;

   return 0;
}

/** @brief generate and sent the digital satellite equipment control "message",
 * specification is available from http://www.eutelsat.com/ 
 *
 * This function will set the LNB voltage and the 22kHz tone. If a satellite switching is asked
 * it will send a diseqc message
 *
 * @param fd : the file descriptor of the frontend
 * @param sat_no : the satellite number (0 for non diseqc compliant hardware, 1 to 4 for diseqc compliant)
 * @param pol_v_r : 1 : vertical or circular right, 0 : horizontal or circular left
 * @param hi_lo : the band for a dual band lnb
 * @param lnb_voltage_off : if one, force the 13/18V voltage to be 0 independantly of polarization
 */
static int do_diseqc(int fd, unsigned char sat_no, int pol_v_r, int hi_lo, int lnb_voltage_off)
{

  fe_sec_voltage_t lnb_voltage;
  struct diseqc_cmd cmd =  { {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4}, 0 };
  //Framing byte : Command from master, no reply required, first transmission : 0xe0
  //Address byte : Any LNB, switcher or SMATV
  //Command byte : Write to port group 0 (Committed switches)


  //Compute the lnb voltage : 0 if we asked, of 13V for vertical and circular right, 18 for horizontal and circular left
  if (lnb_voltage_off)
    {
      lnb_voltage=SEC_VOLTAGE_OFF;
      log_message( MSG_INFO, "LNB voltage 0V\n");
    }
  else if(pol_v_r)
    {
      lnb_voltage=SEC_VOLTAGE_13;
      log_message( MSG_INFO, "LNB voltage 13V\n");
    }
  else
    {
      lnb_voltage=SEC_VOLTAGE_18;
      log_message( MSG_INFO, "LNB voltage 18V\n");
    }

  //Diseqc compliant hardware
  if(sat_no != 0)
    {
      /* param: high nibble: reset bits, low nibble set bits,
       * bits are: option, position, polarization, band
       */
      cmd.cmd.msg[3] =
	0xf0 | ((((sat_no-1) * 4) & 0x0f) | (pol_v_r ? 0 : 2) | (hi_lo ? 1 : 0)); 
      
      return diseqc_send_msg(fd, 
			     lnb_voltage,
			     &cmd, 
			     hi_lo ? SEC_TONE_ON : SEC_TONE_OFF);
    }
  else 	//only tone and voltage
    {
      
      if(ioctl(fd, FE_SET_VOLTAGE, lnb_voltage) < 0)
	{
	  log_message( MSG_WARN, "Warning : problem to set the LNB voltage");
	  return -1;
	}
      
      if(ioctl(fd, FE_SET_TONE, (hi_lo ? SEC_TONE_ON : SEC_TONE_OFF)) < 0)
	{
	  log_message( MSG_WARN, "Warning : problem to set the 22kHz tone");
	  return -1;
	}
      
      usleep(15 * 1000);
      
      return 0;
    }
}

/** @todo document*/
int check_status(int fd_frontend,int type, struct dvb_frontend_parameters* feparams,uint32_t lo_frequency, int display_strength) {
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
           break;
         case FE_QPSK:
           log_message( MSG_INFO, "Event:  Frequency: %d\n",(unsigned int)((event.parameters.frequency)+lo_frequency));
           log_message( MSG_INFO, "        SymbolRate: %d\n",event.parameters.u.qpsk.symbol_rate);
           log_message( MSG_INFO, "        FEC_inner:  %d\n",event.parameters.u.qpsk.fec_inner);
           log_message( MSG_INFO, "\n");
           break;
         case FE_QAM:
           log_message( MSG_INFO, "Event:  Frequency: %d\n",event.parameters.frequency);
           log_message( MSG_INFO, "        SymbolRate: %d\n",event.parameters.u.qpsk.symbol_rate);
           log_message( MSG_INFO, "        FEC_inner:  %d\n",event.parameters.u.qpsk.fec_inner);
           break;
#ifdef ATSC
         case FE_ATSC:
           log_message( MSG_INFO, "Event:  Frequency: %d\n",event.parameters.frequency);
           break;
#endif
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

/** @brief Tune the card
 *
 */
int tune_it(int fd_frontend, tuning_parameters_t *tuneparams)
{
  int res, hi_lo, dfd;
  struct dvb_frontend_parameters feparams;
  struct dvb_frontend_info fe_info;
  uint32_t lo_frequency=0;

  //no warning
  hi_lo = 0;

  if ( (res = ioctl(fd_frontend,FE_GET_INFO, &fe_info) < 0)){
    log_message( MSG_ERROR, "FE_GET_INFO: %s \n", strerror(errno));
    return -1;
  }

  /** @todo here check the capabilities of the card*/

  log_message( MSG_INFO, "Using DVB card \"%s\"\n",fe_info.name);

  tuneparams->fe_type=fe_info.type;

  switch(fe_info.type) {
  case FE_OFDM: //DVB-T
    if (tuneparams->freq < 1000000) tuneparams->freq*=1000UL;
    feparams.frequency=tuneparams->freq;
    feparams.inversion=INVERSION_AUTO;
    feparams.u.ofdm.bandwidth=tuneparams->bandwidth;
    feparams.u.ofdm.code_rate_HP=tuneparams->HP_CodeRate;
    feparams.u.ofdm.code_rate_LP=tuneparams->LP_CodeRate;
    if(tuneparams->modulation==-1)
      tuneparams->modulation=MODULATION_DEFAULT;
    feparams.u.ofdm.constellation=tuneparams->modulation;
    feparams.u.ofdm.transmission_mode=tuneparams->TransmissionMode;
    feparams.u.ofdm.guard_interval=tuneparams->guardInterval;
    feparams.u.ofdm.hierarchy_information=tuneparams->hier;
    log_message( MSG_INFO, "tuning DVB-T to %d Hz, Bandwidth: %d\n", tuneparams->freq, 
		 tuneparams->bandwidth==BANDWIDTH_8_MHZ ? 8 : (tuneparams->bandwidth==BANDWIDTH_7_MHZ ? 7 : 6));
    break;
  case FE_QPSK: //DVB-S
    //Universal lnb : two bands, hi and low one and two local oscilators
    if(tuneparams->lnb_type==LNB_UNIVERSAL)
      {
	if (tuneparams->freq < tuneparams->lnb_slof) {
	  lo_frequency=tuneparams->lnb_lof_low;
	  hi_lo = 0;
	} else {
	  lo_frequency=tuneparams->lnb_lof_high;
	  hi_lo = 1;
	}
      }
    //LNB_STANDARD one band and one local oscillator
    else if (tuneparams->lnb_type==LNB_STANDARD)
      {
	hi_lo=0;
	lo_frequency=tuneparams->lnb_lof_standard;
      }

    feparams.frequency=tuneparams->freq-lo_frequency;

    
    log_message( MSG_INFO, "Tuning DVB-S to Freq: %u Hz, LO frequency %u kHz Pol:%c Srate=%d, LNB number: %d\n",
		 feparams.frequency,
		 lo_frequency,
		 tuneparams->pol,
		 tuneparams->srate,
		 tuneparams->sat_number);
    feparams.inversion=tuneparams->specInv;
    feparams.u.qpsk.symbol_rate=tuneparams->srate;
    feparams.u.qpsk.fec_inner=tuneparams->HP_CodeRate;
    dfd = fd_frontend;
    
    //For diseqc vertical==circular right and horizontal == circular left
    if(do_diseqc( dfd, 
		  tuneparams->sat_number,
		  (tuneparams->pol == 'V' ? 1 : 0) + (tuneparams->pol == 'R' ? 1 : 0),
		  hi_lo,
		  tuneparams->lnb_voltage_off) == 0)
      log_message( MSG_INFO, "DISEQC SETTING SUCCEDED\n");
    else  {
      log_message( MSG_WARN, "DISEQC SETTING FAILED\n");
      return -1;
    }
    break;
  case FE_QAM: //DVB-C
    log_message( MSG_INFO, "tuning DVB-C to %d Hz, srate=%d\n",tuneparams->freq,tuneparams->srate);
    feparams.frequency=tuneparams->freq;
    feparams.inversion=INVERSION_OFF;
    feparams.u.qam.symbol_rate = tuneparams->srate;
    feparams.u.qam.fec_inner = tuneparams->HP_CodeRate;
    if(tuneparams->modulation==-1)
      tuneparams->modulation=MODULATION_DEFAULT;
    feparams.u.qam.modulation = tuneparams->modulation;
    break;
#ifdef ATSC
  case FE_ATSC: //ATSC
    log_message( MSG_INFO, "tuning ATSC to %d Hz, modulation=%d\n",tuneparams->freq,tuneparams->modulation);
    feparams.frequency=tuneparams->freq;
    if(tuneparams->modulation==-1)
      tuneparams->modulation=ATSC_MODULATION_DEFAULT;
    feparams.u.vsb.modulation = tuneparams->modulation;
    break;
#endif
  default:
    log_message( MSG_ERROR, "Unknown FE type : %x. Aborting\n", fe_info.type);
    Interrupted=ERROR_TUNE<<8;
  }
  usleep(100000);

  return(check_status(fd_frontend,fe_info.type,&feparams,lo_frequency,tuneparams->display_strenght));
}
