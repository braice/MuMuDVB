/* MuMuDVB - Stream a DVB transport stream.
 * File for tuning DVB cards
 *
 * last version availaible from http://mumudvb.net/
 *
 * Copyright (C) 2004-2013 Brice DUBOST
 * Copyright (C) Dave Chapman 2001,2002
 * Part of this code from Romolo Manfredini
 * Part of this code from Klaus Reinhard
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
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
#ifndef _WIN32
#include <sys/ioctl.h>
#include <sys/poll.h>
#endif
#include <unistd.h>
#include "config.h"
#include <errno.h>
#include <string.h>

#include "config.h"

#ifndef DISABLE_DVB_API
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>
#endif

#include "tune.h"
#include "mumudvb.h"
#include "errors.h"
#include "log.h"
#include "dvb.h"
#include "math.h"


static char *log_module="Tune: ";

/** Initialize tune variables*/
void init_tune_v(tune_p_t *tune_p)
{
#ifndef DISABLE_DVB_API
	 * tune_p=(tune_p_t){
				.card = -1,
				.tuner = 0,
				.card_dev_path=DVB_DEV_PATH,
				.card_tuned = 0,
				.tuning_timeout = ALARM_TIME_TIMEOUT,
				.freq = 0,
				.srate = 0,
				.pol = 0,
				.lnb_voltage_off=0,
				.lnb_type=LNB_UNIVERSAL,
				.lnb_lof_standard=DEFAULT_LOF_STANDARD,
				.lnb_slof=DEFAULT_SLOF,
				.lnb_lof_low=DEFAULT_LOF1_UNIVERSAL,
				.lnb_lof_high=DEFAULT_LOF2_UNIVERSAL,
				.uni_freq = 0,
				.sat_number = 0,
				.switch_no = -1,
				.pin_no = -1,
				.switch_type = 'C',
				.diseqc_repeat = 0,
				.diseqc_time = 15,
				.modulation_set = 0,
				.display_strenght = 0,
				.check_status = 1,
				.strengththreadshutdown = 0,
				.HP_CodeRate = HP_CODERATE_DEFAULT,//cf tune.h
				.LP_CodeRate = LP_CODERATE_DEFAULT,
				.TransmissionMode = TRANSMISSION_MODE_DEFAULT,
				.guardInterval = GUARD_INTERVAL_DEFAULT,
				.bandwidth = BANDWIDTH_DEFAULT,
				.hier = HIERARCHY_DEFAULT,
				.fe_type=FE_QPSK, //sat by default
#if ISDBT
				// ISDB-T https://01.org/linuxgraphics/gfx-docs/drm/media/uapi/dvb/fe_property_parameters.html
				.isdbt_sound_broadcasting = -1, //AUTO If need to configure please request
				.isdbt_sb_subchanel_id = -1, //AUTO If need to configure please request
				.isdbt_layer = 0, //Undef
				.inversion = INVERSION_AUTO,
#endif
	#if DVB_API_VERSION >= 5
				.delivery_system=SYS_UNDEFINED,
				.rolloff=ROLLOFF_35,
	#endif
	#if STREAM_ID
				.stream_id = 0,
				.pls_code = 0,
				.pls_type = PLS_ROOT,
	#endif
				.read_file_path = {'\0'}
		};
#else
	*tune_p = (tune_p_t){
		.fe_type = FE_QPSK, //sat by default
		.read_file_path = {'\0'}
	};
#endif
}

/** @brief Read a line of the configuration file to check if there is a tuning parameter
 *
 * @param tuneparams the tuning parameters
 * @param substring The currrent line
 */
int read_tuning_configuration(tune_p_t *tuneparams, char *substring)
{
	char delimiteurs[] = CONFIG_FILE_SEPARATOR;
	if (!strcmp (substring, "sat_number"))
	{
		substring = strtok (NULL, delimiteurs);
		tuneparams->sat_number = atoi (substring);
		if (tuneparams->sat_number > 4)
		{
			log_message( log_module,  MSG_ERROR, "Config issue : sat_number. The satellite number must be between 0 and 4. Please report if you have an equipment wich support more\n");
			return -1;
		}
	}
	else if (!strcmp (substring, "switch_input"))
	{
		substring = strtok (NULL, delimiteurs);
		tuneparams->switch_no = atoi (substring);
		if (tuneparams->switch_no > 31)
		{
			log_message( log_module,  MSG_ERROR, "Configuration issue : switch_input. The diseqc switch input number must be between 0 and 31.\n");
			return -1;
		}
	}
	else if (!strcmp (substring, "pin_number"))
	{
		substring = strtok (NULL, delimiteurs);
		tuneparams->pin_no = atoi (substring);
		if ((tuneparams->pin_no > 255) || (tuneparams->pin_no < 0))
		{
			log_message( log_module,  MSG_ERROR, "Config issue : pin_number. The diseqc pin number must be between 0 and 255.\n");
			tuneparams->pin_no=-1;
		}
	}
	else if (!strcmp (substring, "freq"))
	{
		double temp_freq;
		substring = strtok (NULL, delimiteurs);
		temp_freq = atof (substring);
		//If we did not entered the frequency in Hz (Cable and Terrestrial) we go to kHz or Hz, and for satellite we go in kHz (we expect MHz)
		if(temp_freq < 1000000)
			temp_freq *= 1000;
		tuneparams->freq = (temp_freq);
	}
	else if (!strcmp (substring, "uni_freq"))
	{
		double temp_freq;
		substring = strtok (NULL, delimiteurs);
		temp_freq = atof (substring);
		tuneparams->uni_freq = (int)( temp_freq);
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
			log_message( log_module,  MSG_ERROR,
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
			log_message( log_module,  MSG_ERROR,
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
		if(tuneparams->card!=-1)
		{
					log_message( log_module,  MSG_ERROR, "Card defined on the command line: %d, overrides conf file %d",tuneparams->card,atoi (substring));
		}
		else
		{
			tuneparams->card = atoi (substring);
		}
	}
	else if (!strcmp (substring, "check_status"))
	{
		substring = strtok (NULL, delimiteurs);
		tuneparams->check_status = atoi (substring);
	}
	else if (!strcmp (substring, "tuner"))
	{
		substring = strtok (NULL, delimiteurs);
		tuneparams->tuner = atoi (substring);
	}
	else if (!strcmp (substring, "card_dev_path"))
	{
		substring = strtok (NULL, delimiteurs);
		if(strlen(substring)>(256-1))
		{
			log_message( log_module,  MSG_ERROR,
					"The card dev path is too long\n");
			return -1;
		}
		strcpy (tuneparams->card_dev_path, substring);
	}
	else if (!strcmp (substring, "trans_mode"))
	{
#ifndef DISABLE_DVB_API
		// DVB-T
		substring = strtok (NULL, delimiteurs);
		sscanf (substring, "%s\n", substring);
		if (!strcmp (substring, "2k"))
			tuneparams->TransmissionMode=TRANSMISSION_MODE_2K;
		else if (!strcmp (substring, "8k"))
			tuneparams->TransmissionMode=TRANSMISSION_MODE_8K;
		else if (!strcmp (substring, "auto"))
			tuneparams->TransmissionMode=TRANSMISSION_MODE_AUTO;
#ifdef TRANSMISSION_MODE_4K //DVB-T2 ISBT
		else if (!strcmp (substring, "4k"))
			tuneparams->TransmissionMode=TRANSMISSION_MODE_4K;
#endif
#ifdef TRANSMISSION_MODE_16K //DVB-T2
		else if (!strcmp (substring, "16k"))
			tuneparams->TransmissionMode=TRANSMISSION_MODE_16K;
#endif
#ifdef TRANSMISSION_MODE_32K //DVB-T2
		else if (!strcmp (substring, "32k"))
			tuneparams->TransmissionMode=TRANSMISSION_MODE_32K;
#endif
		else
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue : trans_mode\n");
			return -1;
		}
#endif
	}
	else if (!strcmp (substring, "bandwidth"))
	{
#ifndef DISABLE_DVB_API
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
		// DVB-T2
		// @See https://patchwork.kernel.org/patch/761652/
		else if (!strcmp (substring, "5MHz"))
#ifdef BANDWIDTH_5_MHZ
			tuneparams->bandwidth=BANDWIDTH_5_MHZ;
#else
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue: 5MHz bandwidth is not supported on your system please check your drivers installation");
			return -1;
		}
#endif
		else if (!strcmp (substring, "10MHz"))
#ifdef BANDWIDTH_10_MHZ
			tuneparams->bandwidth=BANDWIDTH_10_MHZ;
#else
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue: 10MHz bandwidth is not supported on your system please check your drivers installation");
			return -1;
		}
#endif
		else if (!strcmp (substring, "1.712MHz"))
#ifdef BANDWIDTH_1_712_MHZ
			tuneparams->bandwidth=BANDWIDTH_1_712_MHZ;
#else
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue: 1.712MHz bandwidth is not supported on your system please check your drivers installation");
			return -1;
		}
#endif
		else
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue: bandwidth");
			return -1;
		}
#endif
	}
	else if (!strcmp (substring, "guardinterval"))
	{
#ifndef DISABLE_DVB_API
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
		// DVB-T2
#ifdef GUARD_INTERVAL_1_128
		else if (!strcmp (substring, "1/128"))
			tuneparams->guardInterval=GUARD_INTERVAL_1_128;
#endif
#ifdef GUARD_INTERVAL_19_128
		else if (!strcmp (substring, "19/128"))
			tuneparams->guardInterval=GUARD_INTERVAL_19_128;
#endif
#ifdef GUARD_INTERVAL_19_256
		else if (!strcmp (substring, "19/256"))
			tuneparams->guardInterval=GUARD_INTERVAL_19_256;
#endif
		else
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue : guardinterval\n");
			return -1;
		}
#endif
	}
	else if (!strcmp (substring, "coderate"))
	{
#ifndef DISABLE_DVB_API
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
#if DVB_API_VERSION >= 5
		else if (!strcmp (substring, "3/5"))
			tuneparams->HP_CodeRate=FEC_3_5;
		else if (!strcmp (substring, "9/10"))
			tuneparams->HP_CodeRate=FEC_9_10;
#endif
		else
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue : coderate\n");
			return -1;
		}
		tuneparams->LP_CodeRate=tuneparams->HP_CodeRate; // I found the following :
		//In order to achieve hierarchy, two different code rates may be applied to two different levels of the modulation. Since hierarchy is not implemented ...
#endif
	}
	else if (!strcmp (substring, "delivery_system"))
	{
#ifndef DISABLE_DVB_API
#if DVB_API_VERSION >= 5
		substring = strtok (NULL, delimiteurs);
		sscanf (substring, "%s\n", substring);
		if (!strcmp (substring, "DVBC_ANNEX_AC"))
			tuneparams->delivery_system=SYS_DVBC_ANNEX_AC;
		else if (!strcmp (substring, "DVB-C"))
			tuneparams->delivery_system=SYS_DVBC_ANNEX_AC;
		else if (!strcmp (substring, "DVBC"))
			tuneparams->delivery_system=SYS_DVBC_ANNEX_AC;
		else if (!strcmp (substring, "DVBC_ANNEX_B"))
			tuneparams->delivery_system=SYS_DVBC_ANNEX_B;
		else if (!strcmp (substring, "DVBT"))
			tuneparams->delivery_system=SYS_DVBT;
		else if (!strcmp (substring, "DVB-T"))
			tuneparams->delivery_system=SYS_DVBT;
#ifdef DVBT2
		else if (!strcmp (substring, "DVBT2"))
			tuneparams->delivery_system=SYS_DVBT2;
		else if (!strcmp (substring, "DVB-T2"))
			tuneparams->delivery_system=SYS_DVBT2;
#endif
		else if (!strcmp (substring, "DSS"))
			tuneparams->delivery_system=SYS_DSS;
		else if (!strcmp (substring, "DVBS"))
			tuneparams->delivery_system=SYS_DVBS;
		else if (!strcmp (substring, "DVB-S"))
			tuneparams->delivery_system=SYS_DVBS;
		else if (!strcmp (substring, "DVBS2"))
			tuneparams->delivery_system=SYS_DVBS2;
		else if (!strcmp (substring, "DVB-S2"))
			tuneparams->delivery_system=SYS_DVBS2;
		else if (!strcmp (substring, "DVBH"))
			tuneparams->delivery_system=SYS_DVBH;
		else if (!strcmp (substring, "DVB-H"))
			tuneparams->delivery_system=SYS_DVBH;
#if ISDBT
		else if (!strcmp (substring, "ISDBT"))
			tuneparams->delivery_system=SYS_ISDBT;
		else if (!strcmp (substring, "ISDBS"))
			tuneparams->delivery_system=SYS_ISDBS;
		else if (!strcmp (substring, "ISDBS"))
			tuneparams->delivery_system=SYS_ISDBS;
		else if (!strcmp (substring, "ISDBC"))
			tuneparams->delivery_system=SYS_ISDBC;
#endif
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
			log_message( log_module,  MSG_ERROR,
					"Config issue : delivery_system. Unknown delivery_system : %s\n",substring);
			return -1;
		}
		log_message( log_module,  MSG_INFO,
				"You will use DVB API version 5 for tuning your card.\n");
#else
		log_message( log_module,  MSG_ERROR,
				"Config issue : delivery_system. You are trying to set the delivery system but your MuMuDVB have not been built with DVB-S2/DVB API 5 support.\n");
		return -1;
#endif
#endif
	}
	else if (!strcmp (substring, "rolloff"))
	{
#ifndef DISABLE_DVB_API
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
			log_message( log_module,  MSG_ERROR,
					"Config issue : rolloff. Unknown rolloff : %s\n",substring);
			return -1;
		}
#else
		log_message( log_module,  MSG_ERROR,
				"Config issue : rolloff. You are trying to set the rolloff but your MuMuDVB have not been built with DVB-S2/DVB API 5 support.\n");
		return -1;
#endif
#endif
	}
	else if (!strcmp (substring, "modulation"))
	{
#ifndef DISABLE_DVB_API
		tuneparams->modulation_set = 1;
		substring = strtok (NULL, delimiteurs);
		if (!strcmp (substring, "QPSK"))
			tuneparams->modulation = QPSK;
		else if (!strcmp (substring, "QAM16"))
			tuneparams->modulation = QAM_16;
		else if (!strcmp (substring, "QAM32"))
			tuneparams->modulation = QAM_32;
		else if (!strcmp (substring, "QAM64"))
			tuneparams->modulation = QAM_64;
		else if (!strcmp (substring, "QAM128"))
			tuneparams->modulation = QAM_128;
		else if (!strcmp (substring, "QAM256"))
			tuneparams->modulation = QAM_256;
		else if (!strcmp (substring, "QAMAUTO"))
			tuneparams->modulation = QAM_AUTO;
#ifdef ATSC
		else if (!strcmp (substring, "VSB8"))
			tuneparams->modulation = VSB_8;
		else if (!strcmp (substring, "VSB16"))
			tuneparams->modulation = VSB_16;
#endif
#if DVB_API_VERSION >= 5
		else if (!strcmp (substring, "8PSK"))
			tuneparams->modulation = PSK_8;
		else if (!strcmp (substring, "16APSK"))
			tuneparams->modulation = APSK_16;
		else if (!strcmp (substring, "32APSK"))
			tuneparams->modulation = APSK_32;
		else if (!strcmp (substring, "DQPSK"))
			tuneparams->modulation = DQPSK;
#endif
		else
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue : Bad value for modulation\n");
			tuneparams->modulation_set = 0;
			return -1;
		}
#endif
	}
	else if ((!strcmp (substring, "timeout_accord"))||(!strcmp (substring, "tuning_timeout")))
	{
		substring = strtok (NULL, delimiteurs);	//we extract the substring
		tuneparams->tuning_timeout = atoi (substring);
	}
	else if (!strcmp (substring, "switch_type"))
	{
		substring = strtok (NULL, delimiteurs);
		if (tolower (substring[0]) == 'u')
		{
			tuneparams->switch_type = 'U';
		}
		else if (tolower (substring[0]) == 'c')
		{
			tuneparams->switch_type = 'C';
		}
		else if (tolower (substring[0]) == 'b')
		{
			tuneparams->switch_type = 'B';
		}
		else if (tolower (substring[0]) == 'n')
		{
			tuneparams->switch_type = 'N';
		}
		else if (tolower (substring[0]) == 'j')
		{
			tuneparams->switch_type = 'J';
		}
		else
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue : switch_type\n");
			return -1;
		}
	}
	else if (!strcmp (substring, "diseqc_repeat"))
	{
		substring = strtok (NULL, delimiteurs);
		tuneparams->diseqc_repeat = atoi (substring);
	}
	else if (!strcmp (substring, "diseqc_timing"))
	{
		substring = strtok (NULL, delimiteurs);
		tuneparams->diseqc_time = atoi (substring);
		if (tuneparams->diseqc_time<0)
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue : diseqc_timing. wrong value : %d , set to default\n",tuneparams->diseqc_time);
			tuneparams->diseqc_time=15;
		}
	}
	else if (!strcmp (substring, "stream_id"))
	{
#ifdef STREAM_ID
		substring = strtok (NULL, delimiteurs);
		tuneparams->stream_id = atoi (substring);
		if (tuneparams->stream_id<0)
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue : stream_id. wrong value : %d\n",tuneparams->stream_id);
			tuneparams->stream_id=0;
		}
		log_message( log_module,  MSG_DEBUG, "MuMuDVB have been compiled with DTV_STREAM_ID = %d.",DTV_STREAM_ID);

#else
		log_message( log_module,  MSG_ERROR,
				"Config issue : stream_id. You are trying to set the stream_id but your MuMuDVB have not been built with DVB-S2/DVB API > 5.8 support.\n");
		return -1;
#endif
	}
	else if (!strcmp (substring, "pls_code"))
	{
#ifdef STREAM_ID
		substring = strtok (NULL, delimiteurs);
		tuneparams->pls_code = atoi (substring);
		if (tuneparams->pls_code<0)
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue : pls_code. wrong value : %d\n",tuneparams->pls_code);
			tuneparams->pls_code=0;
		}
#else
		log_message( log_module,  MSG_ERROR,
				"Config issue : pls_code. You are trying to set the pls_code but your MuMuDVB have not been built with DVB-S2/DVB API > 5.8 support.\n");
#endif
	}
	else if (!strcmp (substring, "pls_type"))
	{
#ifdef STREAM_ID
		substring = strtok (NULL, delimiteurs);
		if (!strcmp(substring, "root"))
		{
			tuneparams->pls_type = PLS_ROOT;
		}
		else if (!strcmp(substring, "gold"))
		{
			tuneparams->pls_type = PLS_GOLD;
		}
		else if (!strcmp(substring, "common"))
		{
			log_message( log_module,  MSG_ERROR, "Config issue : pls_type common not implemented, please contact");
			tuneparams->pls_type = PLS_COMMON;
		}
		else
		{
			log_message( log_module,  MSG_ERROR, "Config issue : pls_type not valid %s \n",substring);
		}
#else
                log_message( log_module,  MSG_ERROR,
                                "Config issue : pls_type. You are trying to set the pls_type but your MuMuDVB have not been built with DVB-S2/DVB API > 5.8 support.\n");
#endif
	}
	else if (!strcmp (substring, "read_file_path"))
	{
		// other substring extraction method in order to keep spaces
		substring = strtok (NULL, "=");
		strncpy(tuneparams->read_file_path,strtok(substring,"\n"),MAX_FILENAME_LEN-1);
		tuneparams->read_file_path[MAX_FILENAME_LEN-1]='\0';
		if (strlen (substring) >= MAX_NAME_LEN - 1)
			log_message( log_module,  MSG_WARN,"File name too long\n");
		log_message( log_module,  MSG_DEBUG,
				"Overriding file from which we read the data %s", tuneparams->read_file_path);

	}
	else if (!strcmp (substring, "isdbt_layer"))
	{
#ifndef DISABLE_DVB_API
#if DVB_API_VERSION >= 5
		substring = strtok (NULL, delimiteurs);
		sscanf (substring, "%s\n", substring);
		if (!strcmp(substring, "A") || !strcmp (substring, "a"))
			tuneparams->isdbt_layer |= ISDBT_LAYER_A;
		else if (!strcmp(substring, "B") || !strcmp (substring, "b"))
			tuneparams->isdbt_layer |= ISDBT_LAYER_B;
		else if (!strcmp(substring, "C") || !strcmp (substring, "c"))
			tuneparams->isdbt_layer |= ISDBT_LAYER_C;
		else if (!strcmp (substring, "ALL") || !strcmp (substring, "all"))
			tuneparams->isdbt_layer = ISDBT_LAYER_ALL;
		else
		{
			log_message( log_module,  MSG_ERROR,
					"Config issue : isdbt_layer. Unknown isdbt_layer : %s\n",substring);
			return -1;
		}
#else
		log_message( log_module,  MSG_ERROR,
				"Config issue : isdbt_layer. You are trying to set the isdbt_layer but your MuMuDVB have not been built with DVB-S2/DVB API 5 support.\n");
		return -1;
#endif
#endif
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
void print_status(fe_status_t festatus)
{
#ifndef DISABLE_DVB_API
	log_message( log_module,  MSG_INFO, "FE_STATUS:\n");
	if (festatus & FE_HAS_SIGNAL) log_message( log_module,  MSG_INFO, "     FE_HAS_SIGNAL : found something above the noise level\n");
	if (festatus & FE_HAS_CARRIER) log_message( log_module,  MSG_INFO, "     FE_HAS_CARRIER : found a DVB signal\n");
	if (festatus & FE_HAS_VITERBI) log_message( log_module,  MSG_INFO, "     FE_HAS_VITERBI : FEC is stable\n");
	if (festatus & FE_HAS_SYNC) log_message( log_module,  MSG_INFO, "     FE_HAS_SYNC : found sync bytes\n");
	if (festatus & FE_HAS_LOCK) log_message( log_module,  MSG_INFO, "     FE_HAS_LOCK : everything's working... \n");
	if (festatus & FE_TIMEDOUT) log_message( log_module,  MSG_INFO, "     FE_TIMEDOUT : no lock within the last about 2 seconds\n");
	if (festatus & FE_REINIT) log_message( log_module,  MSG_INFO, "     FE_REINIT : frontend was reinitialized\n");
#endif
}

#ifndef DISABLE_DVB_API
/** The structure for a diseqc command*/
struct diseqc_cmd {
	struct dvb_diseqc_master_cmd cmd;
	uint32_t wait;
};

/** @brief Wait msec miliseconds
 */
static inline void msleep(uint32_t msec)
{
	struct timespec req = { msec / 1000, 1000000 * (msec % 1000) };
	while (nanosleep(&req, &req));
}

/** @brief Send a diseqc message
 *
 * As defined in the DiseqC norm, we stop the 22kHz tone,
 * we set the voltage. Wait. send the command. Wait.
 * send burst. Wait. put back the 22kHz tone
 *
 */
static int diseqc_send_msg(int fd, fe_sec_voltage_t v, struct diseqc_cmd **cmd, fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
	int err, wait = (*cmd)->wait;

	if((err = ioctl(fd, FE_SET_TONE, SEC_TONE_OFF)))
	{
		log_message( log_module,  MSG_WARN, "problem Setting the Tone OFF\n");
		return -1;
	}
	log_message( log_module,  MSG_INFO, "DISEQC: Setting Tone OFF\n");

	if((err = ioctl(fd, FE_SET_VOLTAGE, v)))
	{
		log_message( log_module,  MSG_WARN, "problem Setting the Voltage\n");
		return -1;
	}
	log_message( log_module,  MSG_INFO, "DISEQC: Setting Voltage and wait %d ms\n",wait);
	msleep(wait);

	while (*cmd) {

		if ((err = ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &(*cmd)->cmd)))
		{ log_message( log_module,  MSG_WARN, "problem sending the DiseqC message\n");
			return -1;
		}
		msleep(wait);
		log_message( log_module,  MSG_INFO, "DISEQC: Send CMD and wait %d ms\n",wait);
		cmd++;
	}

	if ((err = ioctl(fd, FE_DISEQC_SEND_BURST, b)))
		{	log_message( log_module,  MSG_WARN, "problem sending the Tone Burst\n");
			return err;
		}
	log_message( log_module,  MSG_INFO, "DISEQC: Send BURST and wait %d ms\n",wait);
	msleep(wait);

	if(ioctl(fd, FE_SET_TONE, t) < 0)
		{	log_message( log_module,  MSG_WARN, "problem Setting the Tone back\n");
			return -1;
		}
	msleep(wait);
	log_message( log_module,  MSG_INFO, "DISEQC: Set TONE back and wait %d ms\n",wait);

	return 0;
}

/** @brief Send a unicable message
 *
 * As defined in the DiseqC norm, we stop the 22kHz tone,
 * we set the voltage. Wait. send the command. Wait.
 * put back the voltage
 *
 */
static int unicable_send_msg(int fd, struct diseqc_cmd **cmd)
{
	int err;

	if((err = ioctl(fd, FE_SET_TONE, SEC_TONE_OFF)))
	{
		log_message( log_module,  MSG_WARN, "problem Setting the Tone OFF\n");
		return -1;
	}
	log_message( log_module,  MSG_INFO, "UNICABLE: Setting Tone OFF\n");

	if((err = ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_18)))
	{
		log_message( log_module,  MSG_WARN, "problem Setting the Voltage\n");
		return -1;
	}
	msleep((*cmd)->wait);  // AN2056: "more than 4ms" after 13V -> 18V; EN50494: 4..22ms
	log_message( log_module,  MSG_INFO, "UNICABLE: Setting the Voltage 18V and for %d ms\n",(*cmd)->wait);

	while (*cmd) {

		if ((err = ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &(*cmd)->cmd)))
		{ log_message( log_module,  MSG_WARN, "problem sending the DiseqC message\n");
			return -1;
		}
		msleep((*cmd)->wait); // no data in AN2056; i guess any value (2..50)msec should be okay.
		log_message( log_module,  MSG_INFO, "UNICABLE: Send CMD and wait %d ms\n",(*cmd)->wait);
		cmd++;
	}

	if(ioctl(fd, FE_SET_VOLTAGE, SEC_VOLTAGE_13) < 0)
	{	log_message( log_module,  MSG_WARN, "problem Setting the Voltage back\n");
			return -1;
	}
	log_message( log_module,  MSG_INFO, "UNICABLE: Setting the Voltage 13V\n");

	return 0;
}


#define ROUND(x) ( 0.5 + x )


/** @brief generate and sent the digital satellite equipment control "message",
 * specification is available from http://www.eutelsat.com/
 *
 * This function will set the LNB voltage and the 22kHz tone. If a satellite switching is asked
 * it will send a diseqc message
 *
 * @param fd : the file descriptor of the frontend
 * @param sat_no : the satellite number (0 for non diseqc compliant hardware, 1 to 4 for diseqc compliant)
 * @param switch_no : the switch number (0 to 31 for diseqc compliant)
 * @param switch_type the switch type (commited or uncommited or unicable)
 * @param pol_v_r : 1 : vertical or circular right, 0 : horizontal or circular left
 * @param hi_lo : the band for a dual band lnb
 * @param lnb_voltage_off : if one, force the 13/18V voltage to be 0 independantly of polarization
 * @param diseqc_repeat : 1 : repeat message, 0 : no repetition
 * @param diseqc_time : ms-wait after commands
 * @param fefrequency : transponder frequency
 * @param uni_freq : unicable frequency
*/
static int do_diseqc(int fd, unsigned char sat_no,  int switch_no,  char switch_type, int pol_v_r, int hi_lo, int lnb_voltage_off, int diseqc_repeat,int diseqc_time)
{

	fe_sec_voltage_t lnb_voltage;
	struct diseqc_cmd *cmd[2] = { NULL, NULL };
	int ret;


	//Compute the lnb voltage : 0 if we asked, of 13V for vertical and circular right, 18 for horizontal and circular left
	if (lnb_voltage_off)
	{
		lnb_voltage=SEC_VOLTAGE_OFF;
		log_message( log_module,  MSG_INFO, "LNB voltage 0V\n");
	}
	else if(pol_v_r)
	{
		lnb_voltage=SEC_VOLTAGE_13;
		log_message( log_module,  MSG_INFO, "LNB voltage 13V\n");
	}
	else
	{
		lnb_voltage=SEC_VOLTAGE_18;
		log_message( log_module,  MSG_INFO, "LNB voltage 18V\n");
	}

	// if unicable switch is configure without uni_freq --> error
	if ((switch_type=='N') || (switch_type=='J'))
	{
		log_message( log_module,  MSG_WARN, "Incompatible switch type\n");
		return -1;
	}
	// if it is a diseqc switch to be controlled
	if ((switch_type=='B') || (switch_type=='U') || (switch_type=='C'))
	{

		cmd[0]=malloc(sizeof(struct diseqc_cmd));
		if(cmd[0]==NULL)
		{
			log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
			set_interrupted(ERROR_MEMORY<<8);
			return -1;
		}
		// extra wait time 15ms for Diseqc switch, set to 5ms for Unicable
		// to be configured in arguments
		cmd[0]->wait=diseqc_time;

		//Framing byte : Command from master, no reply required, first transmission : 0xe0
		cmd[0]->cmd.msg[0] = 0xe0;

		//Address byte : Any LNB, switcher or SMATV
		cmd[0]->cmd.msg[1] = 0x10;

		//Command byte : Write to port group 1 (Uncommited switches) 0x39
		//Command byte : Write to port group 0 (Committed switches) 0x38
		if(switch_type=='U')
			cmd[0]->cmd.msg[2] = 0x39;
		else
			cmd[0]->cmd.msg[2] = 0x38;

		/* param: high nibble: reset bits, low nibble set bits,
		 * bits are: option, position, polarization, band */
		cmd[0]->cmd.msg[3] =
				0xf0 | ((((sat_no-1) * 4) & 0x0f) | (pol_v_r ? 0 : 2) | (hi_lo ? 1 : 0));

		if(switch_no != -1)
		{
			log_message( log_module,  MSG_INFO ,"Diseqc switch position specified, we force switch input to %d\n",switch_no);
			cmd[0]->cmd.msg[3] = 0xf0 | (switch_no& 0x0f);
		}

		//
		cmd[0]->cmd.msg[4] = 0x00;
		cmd[0]->cmd.msg[5] = 0x00;
		cmd[0]->cmd.msg_len=4;

		log_message( log_module,  MSG_DETAIL ,"Diseqc message %02x %02x %02x %02x %02x %02x len %d\n",
				cmd[0]->cmd.msg[0],cmd[0]->cmd.msg[1],cmd[0]->cmd.msg[2],cmd[0]->cmd.msg[3],cmd[0]->cmd.msg[4],cmd[0]->cmd.msg[5],
				cmd[0]->cmd.msg_len);

		// sending DISEQC CMD
		log_message( log_module,  MSG_INFO, "Sending DISEQC\n");
		ret = diseqc_send_msg(fd,
				lnb_voltage,
				cmd,
				hi_lo ? SEC_TONE_ON : SEC_TONE_OFF,
				((sat_no) % 2) ? SEC_MINI_B : SEC_MINI_A);

		if(ret) log_message( log_module,  MSG_WARN, "problem sending the DiseqC message or setting tone/voltage\n");

		// re-sending DISEQC CMD if configured: diseqc_repeat
		if (diseqc_repeat)
		{
			log_message( log_module,  MSG_INFO, "Re-sending DISEQC\n");
			//Framing byte : Command from master, no reply required, repeated transmission : 0xe1
			cmd[0]->cmd.msg[0] = 0xe1;

			log_message( log_module,  MSG_DETAIL ,"Repeated Diseqc message %02x %02x %02x %02x %02x %02x len %d\n",
			cmd[0]->cmd.msg[0],cmd[0]->cmd.msg[1],cmd[0]->cmd.msg[2],cmd[0]->cmd.msg[3],cmd[0]->cmd.msg[4],cmd[0]->cmd.msg[5],
			cmd[0]->cmd.msg_len);

			ret = diseqc_send_msg(fd,
				lnb_voltage,
				cmd,
				hi_lo ? SEC_TONE_ON : SEC_TONE_OFF,
				((sat_no) % 2) ? SEC_MINI_B : SEC_MINI_A);
		}

		if(ret) log_message( log_module,  MSG_WARN, "problem repeating the DiseqC message or setting tone/voltage\n");

		free(cmd[0]);
		return ret;
	}
	else 	//only tone and voltage
	{
		if(ioctl(fd, FE_SET_VOLTAGE, lnb_voltage) < 0)
		{
			log_message( log_module,  MSG_WARN, "problem to set the LNB voltage\n");
			return -1;
		}

		if(ioctl(fd, FE_SET_TONE, (hi_lo ? SEC_TONE_ON : SEC_TONE_OFF)) < 0)
		{
			log_message( log_module,  MSG_WARN, "problem to set the 22kHz tone\n");
			return -1;
		}
		msleep(diseqc_time);
		return 0;
	}
}

/** @brief generate and sent the digital satellite equipment control "message",
 * specification is available from http://www.eutelsat.com/
 *
 * This function will set the LNB voltage and the 22kHz tone. If a satellite switching is asked
 * it will send a diseqc message
 *
 * @param fd : the file descriptor of the frontend
 * @param sat_no : the satellite number (0 for non diseqc compliant hardware, 1 to 4 for diseqc compliant)
 * @param switch_no : the switch number (0 to 31 for diseqc compliant)
 * @param pin_no : the pin number (0 to 255 for diseqc compliant)
 * @param switch_type the switch type (commited or uncommited or unicable)
 * @param pol_v_r : 1 : vertical or circular right, 0 : horizontal or circular left
 * @param hi_lo : the band for a dual band lnb
 * @param diseqc_repeat : 1 : repeat message, 0 : no repetition
 * @param diseqc_time : ms-wait after commands
 * @param fefrequency : transponder frequency
 * @param uni_freq : unicable frequency
 */
static int do_unicable(int fd, unsigned char sat_no,  int switch_no,  int pin_no, char switch_type, int pol_v_r, int hi_lo, int diseqc_repeat,int diseqc_time, uint32_t *fefrequency, uint32_t uni_freq)
{

	struct diseqc_cmd *cmd[2] = { NULL, NULL };
	int ret;

	cmd[0]=malloc(sizeof(struct diseqc_cmd));
	if(cmd[0]==NULL)
	{
		log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
		set_interrupted(ERROR_MEMORY<<8);
		return -1;
	}

	if(switch_type=='N')
	{ //UnicableI compliant hardware

		log_message( log_module,  MSG_INFO, "SCR/UNICABLE_I message ");
		// extra wait time 10ms for diseqc switch

		// no extra waiting in case of UNICABLE diseqc messages sending
		// equivalent to w_scan implementation having 5ms before/after sending cmd
		cmd[0]->wait=diseqc_time;
		log_message( log_module,  MSG_INFO, "Waiting set to %d ms ", cmd[0]->wait);

		//Framing byte : Command from master, no reply required, first transmission : 0xe0
		cmd[0]->cmd.msg[0] = 0xe0;

		//Address byte : Any LNB, switcher or SMATV
		cmd[0]->cmd.msg[1] = 0x10;

		//Command byte : Write to port group 1 (Uncommited switches)
		//Command byte : Write to port group 0 (Committed switches) 0x38
		//Command byte : Unicable switch : 5A
		cmd[0]->cmd.msg[2] = 0x5A;

		//*fefrequency
		uint8_t channel_byte_1, channel_byte_2;
		channel_byte_1 = (uint8_t) (switch_no<<  5);
		uint16_t t;

		if (sat_no != 1)
			channel_byte_1 |= (1<<  4);

		if (!pol_v_r ) /* horizontal*/
			channel_byte_1 |= (1<<  3);

		if (hi_lo) /* high band*/
			channel_byte_1 |= (1<<  2);

		//https://code.mythtv.org/trac/attachment/ticket/9726/0001-libmythtv-Unicable-SCR-DIN-EN-50494.patch
		//seems that we will have to tune to another frequency too and adjust the voltage
		//Then we will have to tune the card to the SCR frequency as the LNB change it
		//https://patchwork.linuxtv.org/patch/7994/

		t = (uint16_t) (ROUND((((*fefrequency / 1000) + uni_freq + 2) / 4) - 350));
		channel_byte_1 |= (((uint8_t) (t>>  8))&  0x03);
		channel_byte_2 = (uint8_t) (t&  0x00FF);

		if(t>1024)
			log_message( log_module,  MSG_ERROR, "SCR/UNICABLE T out of range (is the SCR frequency valid ?) ");

		log_message( log_module,  MSG_DEBUG, "Unicable tuning information : unicable freq %d lo_freq %d channel bytes: 0x%02x  0x%02x  t: 0x%02x",
					uni_freq, *fefrequency, channel_byte_1, channel_byte_2, t);

		cmd[0]->cmd.msg[3] = channel_byte_1;
		cmd[0]->cmd.msg[4] = channel_byte_2;

		// handling pin code for user band if given: pin_no > 0
		if (pin_no > 0)
		{	cmd[0]->cmd.msg[0] = 0xe0;
			cmd[0]->cmd.msg[2] = 0x5C;
			cmd[0]->cmd.msg[5] = (uint8_t) (pin_no&  0x00FF);
			cmd[0]->cmd.msg_len = 6;
		}
		else
		{	cmd[0]->cmd.msg[0] = 0xe0;
			cmd[0]->cmd.msg[2] = 0x5A;
			cmd[0]->cmd.msg[5] = 0x00;
			cmd[0]->cmd.msg_len = 5;
		}

		//We rewrite the frontend frequency
		//*fefrequency = uni_freq*1000UL;
		*fefrequency = ((t + 350) *4 - (*fefrequency / 1000) )*1000UL;

	}
	else if(switch_type=='J')
	{ //JESS or UNICABLE II, we recompute the proper messages
		log_message( log_module,  MSG_INFO, "SCR/JESS_UNICABLE_II message ");

		// no extra waiting in case of UNICABLE_II diseqc messages sending
		// equivalent to w_scan implementation having 5ms before/after sending cmd
		cmd[0]->wait=diseqc_time;
		log_message( log_module,  MSG_INFO, "Waiting set to %d ms ", cmd[0]->wait);

		//Framing byte : Command from master, no reply required, first transmission : 0x70
		cmd[0]->cmd.msg[0] = 0x70;

		//Address byte : Any LNB, switcher or SMATV
		cmd[0]->cmd.msg[1] = 0x10;

		//Command byte : Unicable_II switch : 38
		cmd[0]->cmd.msg[2] = 0x38;

		/* param: high nibble: reset bits, low nibble set bits,
		 * bits are: option, position, polarization, band */
		cmd[0]->cmd.msg[3] =
				0xf0 | ((((sat_no-1) * 4) & 0x0f) | (pol_v_r ? 0 : 2) | (hi_lo ? 1 : 0));

		cmd[0]->cmd.msg[4] = 0x00;
		uint8_t channel_byte_1, channel_byte_2, channel_byte_3;
		uint16_t t;

		// no extra waiting in case of UNICABLE diseqc messages sending
		cmd[0]->wait=diseqc_time;
		log_message( log_module,  MSG_INFO, "Waiting set to %d ms ", cmd[0]->wait);

		t = (uint16_t) ((*fefrequency / 1000) - 100);

		channel_byte_1 = (switch_no & 0x1f) << 3;
		channel_byte_1 |= (t >> 8) & 0x7;
		channel_byte_2 = t & 0xff;
		channel_byte_3  = ((sat_no-1) & 0x3f) << 2;
		channel_byte_3 |= (!pol_v_r & 1) << 1;
		channel_byte_3 |= hi_lo & 1;


		log_message(log_module,  MSG_DEBUG, "JESS / Unicable II tuning information : JESS / unicable_II freq %d lo_freq %d channel bytes: 0x%02x  0x%02x 0x%02x 0x%02x t: 0x%02x",
					uni_freq, *fefrequency, channel_byte_1, channel_byte_2, channel_byte_3, (uint8_t) (pin_no&  0x00FF), t);

		cmd[0]->cmd.msg[1] = channel_byte_1;
		cmd[0]->cmd.msg[2] = channel_byte_2;
		cmd[0]->cmd.msg[3] = channel_byte_3;

		// handling pin code for user band if given: pin_no > 0
		if (pin_no > 0)
		{	cmd[0]->cmd.msg[0] = 0x71;
			cmd[0]->cmd.msg[4] = (uint8_t) (pin_no&  0x00FF);
			cmd[0]->cmd.msg[5] = 0x00;
			cmd[0]->cmd.msg_len = 5;
		}
		else
		{	cmd[0]->cmd.msg[0] = 0x70;
			cmd[0]->cmd.msg[4] = 0x00;
			cmd[0]->cmd.msg[5] = 0x00;
			cmd[0]->cmd.msg_len = 4;
		}

		*fefrequency = uni_freq*1000UL;
	}

	log_message( log_module,  MSG_DETAIL ,"Diseqc message %02x %02x %02x %02x %02x %02x len %d\n",
			cmd[0]->cmd.msg[0],cmd[0]->cmd.msg[1],cmd[0]->cmd.msg[2],cmd[0]->cmd.msg[3],cmd[0]->cmd.msg[4],cmd[0]->cmd.msg[5],
			cmd[0]->cmd.msg_len);

	// sending UNICABLE CMD
	ret = unicable_send_msg(fd, cmd);
	if(ret) log_message( log_module,  MSG_WARN, "problem sending the Unicable message or setting tone/voltage\n");

	// re-sending UNICABLE CMD if requested using diseqc_repeat
	if (diseqc_repeat)
	{
		msleep(100); // wait 100ms between repeated message
		log_message( log_module,  MSG_INFO, "Wait 100 ms for before re-sending\n");
		//Framing byte : Command from master, no reply required, repeated transmission : 0xe1
		if (switch_type=='N') cmd[0]->cmd.msg[0] = 0xe1;
		log_message( log_module,  MSG_DETAIL ,"Repeated Diseqc message %02x %02x %02x %02x %02x %02x len %d\n",
			cmd[0]->cmd.msg[0],cmd[0]->cmd.msg[1],cmd[0]->cmd.msg[2],cmd[0]->cmd.msg[3],cmd[0]->cmd.msg[4],cmd[0]->cmd.msg[5],
			cmd[0]->cmd.msg_len);
		ret = unicable_send_msg(fd, cmd);
	}
	if(ret) log_message( log_module,  MSG_WARN, "problem repeating the Unicable message or setting tone/voltage\n");

	free(cmd[0]);
	return ret;
}
#endif

/** @brief Check the status of the card

 */
int check_status(int fd_frontend,int type,uint32_t lo_frequency, int display_strength)
{
#ifndef DISABLE_DVB_API
	int32_t strength;
	fe_status_t festatus;
	//We keep the old tuning compatibility just in case, as the new one should work it is done via the configure

	struct dvb_frontend_parameters parameters;

	do
	{
		log_message( log_module, MSG_DETAIL, "polling....\n");
		if (ioctl(fd_frontend, FE_READ_STATUS, &festatus) < 0){
			if (errno == EINTR) {
				continue;
			}
			log_message( log_module,  MSG_ERROR, "FE_READ_STATUS %s\n", strerror(errno));
			return -1;
		}
		print_status(festatus);
		if(display_strength)
		{
			strength=0;
			if(ioctl(fd_frontend,FE_READ_SIGNAL_STRENGTH,&strength) >= 0)
				log_message( log_module,  MSG_INFO, "Strength: %10d\n",strength);
			strength=0;
			if(ioctl(fd_frontend,FE_READ_SNR,&strength) >= 0)
				log_message( log_module,  MSG_INFO, "SNR: %10d\n",strength);
		}
		sleep(1);
	} while ((festatus & (FE_HAS_LOCK))==0);

	if (festatus & FE_HAS_LOCK) {
		int status;
		do {
			status = ioctl(fd_frontend, FE_GET_FRONTEND, &parameters);
		} while (status == -1 && errno == EINTR);

		if (status < 0) {
			log_message( log_module,  MSG_ERROR, "FE_GET_FRONTEND %s\n", strerror(errno));
		}
		else
		{
			switch(type) {
			case FE_OFDM:
				log_message( log_module,  MSG_INFO, "Event:  Frequency: %d\n",parameters.frequency);
				break;
			case FE_QPSK:
				log_message( log_module,  MSG_INFO, "Event:  Frequency: %d (or %d)\n",(unsigned int)((parameters.frequency)+lo_frequency),(unsigned int) abs((parameters.frequency)-lo_frequency));
				log_message( log_module,  MSG_INFO, "        SymbolRate: %d\n",parameters.u.qpsk.symbol_rate);
				log_message( log_module,  MSG_INFO, "        FEC_inner:  %d\n",parameters.u.qpsk.fec_inner);
				break;
			case FE_QAM:
				log_message( log_module,  MSG_INFO, "Event:  Frequency: %d\n",parameters.frequency);
				log_message( log_module,  MSG_INFO, "        SymbolRate: %d\n",parameters.u.qpsk.symbol_rate);
				log_message( log_module,  MSG_INFO, "        FEC_inner:  %d\n",parameters.u.qpsk.fec_inner);
				break;
#ifdef ATSC
			case FE_ATSC:
				log_message( log_module,  MSG_INFO, "Event:  Frequency: %d\n",parameters.frequency);
				break;
#endif
			default:
				break;
			}
		}

		strength=0;
		if(ioctl(fd_frontend,FE_READ_BER,&strength) >= 0)
			log_message( log_module,  MSG_INFO, "Bit error rate: %d\n",strength);

		strength=0;
		if(ioctl(fd_frontend,FE_READ_SIGNAL_STRENGTH,&strength) >= 0)
			log_message( log_module,  MSG_INFO, "Signal strength: %d\n",strength);

		strength=0;
		if(ioctl(fd_frontend,FE_READ_SNR,&strength) >= 0)
			log_message( log_module,  MSG_INFO, "SNR: %d\n",strength);
	} else {
		log_message( log_module,  MSG_ERROR, "Not able to lock to the signal on the given frequency\n");
		return -1;
	}
#endif

	return 0;
}

#ifndef DISABLE_DVB_API
/** @brief change the delivery subsystem
 *
 */
static int change_delivery_system(fe_delivery_system_t delivery_system,int fd_frontend)
{
#if DVB_API_VERSION >= 5
	log_message( log_module,  MSG_WARN, "We ask the card to change the delivery system (multi frontend cards).");

	struct dtv_property pclear[] = {
			{ .cmd = DTV_CLEAR,},
	};
	struct dtv_properties cmdclear = {
			.num = 1,
			.props = pclear
	};

	struct dtv_property dvb_deliv[1];
	struct dtv_properties cmddeliv = {
			.num = 1,
			.props = dvb_deliv
	};

	dvb_deliv[0].cmd = DTV_DELIVERY_SYSTEM;
	dvb_deliv[0].u.data = delivery_system;

	if ((ioctl(fd_frontend, FE_SET_PROPERTY, &cmdclear)) == -1) {
		log_message( log_module,  MSG_ERROR,"FE_SET_PROPERTY clear failed : %s\n", strerror(errno));
		set_interrupted(ERROR_TUNE<<8);
		return -1;
	}

	if ((ioctl(fd_frontend, FE_SET_PROPERTY, &cmddeliv)) == -1) {
		log_message( log_module,  MSG_ERROR,"FE_SET_PROPERTY failed : %s\n", strerror(errno));
		set_interrupted(ERROR_TUNE<<8);
		return -1;
	}
	return 0;
#else
	return 0;
#endif
}
#endif

/** @brief Tune the card
 *
 */
int tune_it(int fd_frontend, tune_p_t *tuneparams)
{
#ifndef DISABLE_DVB_API
	int res, hi_lo, dfd;
	struct dvb_frontend_parameters feparams;
	struct dvb_frontend_info fe_info;
	uint32_t lo_frequency=0;
	struct dvb_frontend_event event;
	int dvbt_bandwidth=0;

	//no warning
	memset(&feparams, 0, sizeof (struct dvb_frontend_parameters));
	hi_lo = 0;

	res = ioctl(fd_frontend,FE_GET_INFO, &fe_info);
	if (res < 0){
		log_message( log_module,  MSG_ERROR, "FE_GET_INFO: %s \n", strerror(errno));
		return -1;
	}

	/** @todo here check the capabilities of the card*/

	log_message( log_module,  MSG_INFO, "Using DVB card \"%s\" tuner %d\n",fe_info.name, tuneparams->tuner);

	// Save the frontend name for easy identification
	snprintf(tuneparams->fe_name, 256, "%s", fe_info.name);

	tuneparams->fe_type=fe_info.type;
	feparams.inversion=INVERSION_AUTO;


	// see if we need to change the frontend type. @todo : mix between DVB APIv3 and V5
#if DVB_API_VERSION >= 5
	int change_deliv=0;
	switch(fe_info.type) {
	case FE_OFDM: //DVB-T
		if((tuneparams->delivery_system!=SYS_UNDEFINED)&&(tuneparams->delivery_system!=SYS_DVBT)
#if ISDBT
				&&(tuneparams->delivery_system!=SYS_ISDBT)
#endif
#ifdef DVBT2
				&&(tuneparams->delivery_system!=SYS_DVBT2))
#else
		)
#endif
		{
			log_message( log_module,  MSG_WARN, "The delivery system does not fit with the card frontend type (DVB-T/T2 ISDBT).");
			change_deliv=1;
		}
		break;
case FE_QPSK: //DVB-S
	if((tuneparams->delivery_system!=SYS_UNDEFINED)&&(tuneparams->delivery_system!=SYS_DVBS)&&(tuneparams->delivery_system!=SYS_DVBS2))
	{
		log_message( log_module,  MSG_WARN, "The delivery system does not fit with the card frontend type (DVB-S).\n");
		change_deliv=1;
	}
	break;
case FE_QAM: //DVB-C
	if((tuneparams->delivery_system!=SYS_UNDEFINED)&&(tuneparams->delivery_system!=SYS_DVBC_ANNEX_AC)&&(tuneparams->delivery_system!=SYS_DVBC_ANNEX_B))
	{
		log_message( log_module,  MSG_WARN, "The delivery system does not fit with the card frontend type (DVB-C).\n");
		change_deliv=1;
	}
	break;
#ifdef ATSC
case FE_ATSC: //ATSC
	if((tuneparams->delivery_system!=SYS_UNDEFINED)&&(tuneparams->delivery_system!=SYS_ATSC))
	{
		log_message( log_module,  MSG_WARN, "The delivery system does not fit with the card frontend type (ATSC).\n");
		change_deliv=1;
	}
	break;
#endif
default:
	break;
	}
	if(change_deliv) //delivery system needs to be changed
	{
		if(change_delivery_system(tuneparams->delivery_system,fd_frontend))
			return -1;
		//get new info
		if ( (res = ioctl(fd_frontend,FE_GET_INFO, &fe_info) < 0)){
			log_message( log_module,  MSG_ERROR, "FE_GET_INFO: %s \n", strerror(errno));
			return -1;
		}
		// Save the frontend name for easy identification
		snprintf(tuneparams->fe_name, 256, "%s", fe_info.name);
		tuneparams->fe_type=fe_info.type;
		feparams.inversion=INVERSION_AUTO;
	}

#endif

	switch(fe_info.type) {
	case FE_OFDM: //DVB-T
		//Terrestrial want frequency in Hz and it is at least 1Mhz
		if (tuneparams->freq < 1000000)
			tuneparams->freq*=1000;
		feparams.frequency=(int)tuneparams->freq;
		feparams.u.ofdm.bandwidth=tuneparams->bandwidth;
		feparams.u.ofdm.code_rate_HP=tuneparams->HP_CodeRate;
		feparams.u.ofdm.code_rate_LP=tuneparams->LP_CodeRate;
		if(!tuneparams->modulation_set)
			tuneparams->modulation=MODULATION_DEFAULT;
		feparams.u.ofdm.constellation=tuneparams->modulation;
		feparams.u.ofdm.transmission_mode=tuneparams->TransmissionMode;
		feparams.u.ofdm.guard_interval=tuneparams->guardInterval;
		feparams.u.ofdm.hierarchy_information=tuneparams->hier;
		switch(tuneparams->bandwidth)
		{
		case BANDWIDTH_8_MHZ:
			dvbt_bandwidth=8000000;
			break;
		case BANDWIDTH_7_MHZ:
			dvbt_bandwidth=7000000;
			break;
		case BANDWIDTH_6_MHZ:
			dvbt_bandwidth=6000000;
			break;
		case BANDWIDTH_AUTO:
		default:
			dvbt_bandwidth=0;
			break;
		}
		log_message( log_module,  MSG_INFO, "Tuning Terrestrial to %d Hz, Bandwidth: %d\n", (int)tuneparams->freq,dvbt_bandwidth);
		break;
		case FE_QPSK: //DVB-S
			if(!tuneparams->modulation_set)
				tuneparams->modulation=SAT_MODULATION_DEFAULT;
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

			feparams.frequency=abs(tuneparams->freq-lo_frequency);


			log_message( log_module,  MSG_INFO, "Tuning DVB-S to Freq: %u kHz, Transp frequency: %f , LO frequency %u kHz  Pol:%c Srate=%d, LNB number: %d\n",
					feparams.frequency,
					tuneparams->freq,
					lo_frequency,
					tuneparams->pol,
					tuneparams->srate,
					tuneparams->sat_number);
			feparams.u.qpsk.symbol_rate=tuneparams->srate;
			feparams.u.qpsk.fec_inner=tuneparams->HP_CodeRate;
			dfd = fd_frontend;

			// Test whether it is a unicable switch - existing unicable frequency
			if(tuneparams->uni_freq > 0)
			{
				log_message( log_module,  MSG_INFO, "Unicable Switch: Sending messages");
				if(do_unicable( dfd,
						tuneparams->sat_number,
						tuneparams->switch_no,
						tuneparams->pin_no,
						tuneparams->switch_type,
						(tuneparams->pol == 'V' ? 1 : 0) + (tuneparams->pol == 'R' ? 1 : 0),
						hi_lo,
						tuneparams->diseqc_repeat,
						tuneparams->diseqc_time,
						&feparams.frequency,
						tuneparams->uni_freq) == 0)
					log_message( log_module,  MSG_INFO, "UNICABLE SETTING SUCCEEDED\n");
				else
				{
					log_message( log_module,  MSG_WARN, "UNICABLE SETTING FAILED\n");
					return -1;
				}

			}
			// its a diseqc switch - sending both messages for uncommitted then committed switch
			else
			{
				if(tuneparams->switch_type == 'B')
				{
					log_message( log_module,  MSG_INFO, "DiSEqC Switch: Sending Uncommitted and Committed messages");
					tuneparams->switch_type = 'U';
					//For diseqc vertical==circular right and horizontal == circular left
					if(do_diseqc( dfd,
							tuneparams->sat_number,
							tuneparams->switch_no,
							tuneparams->switch_type,
							(tuneparams->pol == 'V' ? 1 : 0) + (tuneparams->pol == 'R' ? 1 : 0),
							hi_lo,
							tuneparams->lnb_voltage_off,
							tuneparams->diseqc_repeat,
							tuneparams->diseqc_time) == 0)
						log_message( log_module,  MSG_INFO, "DISEQC SETTING SUCCEEDED\n");
					else
					{
						log_message( log_module,  MSG_WARN, "DISEQC SETTING FAILED\n");
						return -1;
					}
				tuneparams->switch_type = 'C';
				}
				//For diseqc vertical==circular right and horizontal == circular left
				if(do_diseqc( dfd,
						tuneparams->sat_number,
						tuneparams->switch_no,
						tuneparams->switch_type,
						(tuneparams->pol == 'V' ? 1 : 0) + (tuneparams->pol == 'R' ? 1 : 0),
						hi_lo,
						tuneparams->lnb_voltage_off,
						tuneparams->diseqc_repeat,
						tuneparams->diseqc_time) == 0)
					log_message( log_module,  MSG_INFO, "DISEQC SETTING SUCCEEDED - Frequency: %d\n", feparams.frequency);
				else
				{
					log_message( log_module,  MSG_WARN, "DISEQC SETTING FAILED\n");
					return -1;
				}
			}
			break;
		case FE_QAM: //DVB-C
			//If the user entered in MHz, we are right now in kHz
			if (tuneparams->freq < 1000000)
				tuneparams->freq*=1000;
			log_message( log_module,  MSG_INFO, "tuning DVB-C to %d Hz, srate=%d\n",(int)tuneparams->freq,tuneparams->srate);
			feparams.frequency=(int)tuneparams->freq;
			feparams.inversion=INVERSION_OFF;
			feparams.u.qam.symbol_rate = tuneparams->srate;
			feparams.u.qam.fec_inner = tuneparams->HP_CodeRate;
			if(!tuneparams->modulation_set)
				tuneparams->modulation=MODULATION_DEFAULT;
			feparams.u.qam.modulation = tuneparams->modulation;
			break;
#ifdef ATSC
		case FE_ATSC: //ATSC
			//If the user entered in MHz, we are right now in kHz
			if (tuneparams->freq < 1000000)
				tuneparams->freq*=1000;
			log_message( log_module,  MSG_INFO, "tuning ATSC to %d Hz, modulation=%d\n",(int)tuneparams->freq,tuneparams->modulation);
			feparams.frequency=(int)tuneparams->freq;
			if(!tuneparams->modulation_set)
				tuneparams->modulation=ATSC_MODULATION_DEFAULT;
			feparams.u.vsb.modulation = tuneparams->modulation;
			break;
#endif
		default:
			log_message( log_module,  MSG_ERROR, "Unknown FE type : %x. Aborting\n", fe_info.type);
			set_interrupted(ERROR_TUNE<<8);
			return -1;
	}
	usleep(100000);


	/* The tuning of the card*/
	while(1)  {
		if (ioctl(fd_frontend, FE_GET_EVENT, &event) < 0)	//EMPTY THE EVENT QUEUE
			break;
	}

	//If we support DVB API version 5 we check if the delivery system was defined
#if DVB_API_VERSION >= 5
	if(tuneparams->delivery_system==SYS_UNDEFINED)
#else
		if(1)
#endif
		{
			if (ioctl(fd_frontend,FE_SET_FRONTEND,&feparams) < 0) {
				log_message( log_module,  MSG_ERROR, "ERROR tuning channel : %s \n", strerror(errno));
				set_interrupted(ERROR_TUNE<<8);
				return -1;
			}
		}
#if DVB_API_VERSION >= 5
		else
		{
			/*  Memo : S2API Commands
    DTV_UNDEFINED            DTV_TUNE                 DTV_CLEAR
    DTV_FREQUENCY            DTV_MODULATION           DTV_BANDWIDTH_HZ
    DTV_INVERSION            DTV_DISEQC_MASTER        DTV_SYMBOL_RATE
    DTV_INNER_FEC            DTV_VOLTAGE              DTV_TONE
    DTV_PILOT                DTV_ROLLOFF              DTV_DISEQC_SLAVE_REPLY
    DTV_FE_CAPABILITY_COUNT  DTV_FE_CAPABILITY        DTV_DELIVERY_SYSTEM
    DTV_API_VERSION          DTV_API_VERSION          DTV_CODE_RATE_HP
    DTV_CODE_RATE_LP         DTV_GUARD_INTERVAL       DTV_TRANSMISSION_MODE
    DTV_HIERARCHY
			 */
			//DVB api version 5 and delivery system defined, we do DVB-API-5 tuning
			log_message( log_module,  MSG_INFO, "Tuning With DVB-API version 5. delivery system : %d\n",tuneparams->delivery_system);

#ifdef STREAM_ID
			int tune_stream_id;
			tune_stream_id = tuneparams->stream_id;
			if( tuneparams->pls_code )
				tune_stream_id = tuneparams->stream_id+((tuneparams->pls_code & 0x3FFFF)<<8);
			if( tuneparams->pls_type == PLS_GOLD)
				tune_stream_id = tune_stream_id | ( 1<<26 );
			if(tuneparams->stream_id || tune_stream_id)
				log_message( log_module,  MSG_INFO,  "Stream_id = %d, stream id with PLS parameters %d",tuneparams->stream_id, tune_stream_id);
#endif

			struct dtv_property pclear[] = {
					{ .cmd = DTV_CLEAR,},
			};
			struct dtv_properties cmdclear = {
					.num = 1,
					.props = pclear
			};
			struct dtv_properties *cmdseq;
			int commandnum =0;

			cmdseq = (struct dtv_properties*) calloc(1, sizeof(*cmdseq));
			if (!cmdseq)
			{
				log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
				return -1;
			}

			cmdseq->props = (struct dtv_property*) calloc(MAX_CMDSEQ_PROPS_NUM, sizeof(*(cmdseq->props)));
			if (!(cmdseq->props))
			{
				free(cmdseq);
				log_message( log_module, MSG_ERROR,"Problem with malloc : %s file : %s line %d\n",strerror(errno),__FILE__,__LINE__);
				return -1;
			}
			if((tuneparams->delivery_system==SYS_DVBS)||(tuneparams->delivery_system==SYS_DVBS2))
			{
				cmdseq->props[commandnum].cmd      = DTV_DELIVERY_SYSTEM;
				cmdseq->props[commandnum++].u.data = tuneparams->delivery_system;
				cmdseq->props[commandnum].cmd      = DTV_FREQUENCY;
				cmdseq->props[commandnum++].u.data = feparams.frequency;
				cmdseq->props[commandnum].cmd      = DTV_MODULATION;
				cmdseq->props[commandnum++].u.data = tuneparams->modulation;
				cmdseq->props[commandnum].cmd      = DTV_SYMBOL_RATE;
				cmdseq->props[commandnum++].u.data = tuneparams->srate;
				cmdseq->props[commandnum].cmd      = DTV_INNER_FEC;
				cmdseq->props[commandnum++].u.data = tuneparams->HP_CodeRate;
				cmdseq->props[commandnum].cmd      = DTV_INVERSION;
				cmdseq->props[commandnum++].u.data = INVERSION_AUTO;
				cmdseq->props[commandnum].cmd      = DTV_ROLLOFF;
				cmdseq->props[commandnum++].u.data = tuneparams->rolloff;
				cmdseq->props[commandnum].cmd      = DTV_PILOT;
				cmdseq->props[commandnum++].u.data = PILOT_AUTO;

#ifdef STREAM_ID
				if(tuneparams->stream_id)
				{
					cmdseq->props[commandnum].cmd      = DTV_STREAM_ID;
					cmdseq->props[commandnum++].u.data = tune_stream_id;
				}
#endif
				cmdseq->props[commandnum++].cmd    = DTV_TUNE;
			}
			else if((tuneparams->delivery_system==SYS_DVBT)
#ifdef DVBT2
					||(tuneparams->delivery_system==SYS_DVBT2))
#else
			)
#endif
		{
			cmdseq->props[commandnum].cmd      = DTV_DELIVERY_SYSTEM;
			cmdseq->props[commandnum++].u.data = tuneparams->delivery_system;
			cmdseq->props[commandnum].cmd      = DTV_FREQUENCY;
			cmdseq->props[commandnum++].u.data = feparams.frequency;
			cmdseq->props[commandnum].cmd      = DTV_BANDWIDTH_HZ;
			cmdseq->props[commandnum++].u.data = dvbt_bandwidth;
			cmdseq->props[commandnum].cmd      = DTV_CODE_RATE_HP;
			cmdseq->props[commandnum++].u.data = tuneparams->HP_CodeRate;
			cmdseq->props[commandnum].cmd      = DTV_CODE_RATE_LP;
			cmdseq->props[commandnum++].u.data = tuneparams->LP_CodeRate;
			cmdseq->props[commandnum].cmd      = DTV_MODULATION;
			cmdseq->props[commandnum++].u.data = tuneparams->modulation;
			cmdseq->props[commandnum].cmd      = DTV_GUARD_INTERVAL;
			cmdseq->props[commandnum++].u.data = tuneparams->guardInterval;
			cmdseq->props[commandnum].cmd      = DTV_TRANSMISSION_MODE;
			cmdseq->props[commandnum++].u.data = tuneparams->TransmissionMode;
			cmdseq->props[commandnum].cmd      = DTV_HIERARCHY;
			cmdseq->props[commandnum++].u.data = tuneparams->hier;
#ifdef STREAM_ID
			if(tuneparams->stream_id)
			{
				cmdseq->props[commandnum].cmd      = DTV_STREAM_ID;
				cmdseq->props[commandnum++].u.data = tune_stream_id;
			}
#endif
			cmdseq->props[commandnum++].cmd    = DTV_TUNE;
		}
#if ISDBT
		else if((tuneparams->delivery_system==SYS_ISDBT))
		{
			int isdbt_partial_reception = 0;

			if(tuneparams->isdbt_layer)
			{
				if(tuneparams->isdbt_layer & ISDBT_LAYER_A)
					log_message( log_module,  MSG_INFO, "ISDBT Layer A enabled");
				if(tuneparams->isdbt_layer & ISDBT_LAYER_B)
					log_message( log_module,  MSG_INFO, "ISDBT Layer B enabled");
				if(tuneparams->isdbt_layer & ISDBT_LAYER_C)
					log_message( log_module,  MSG_INFO, "ISDBT Layer C enabled");
				if(tuneparams->isdbt_layer != ISDBT_LAYER_ALL)
					isdbt_partial_reception = 1;
			}
			else
				tuneparams->isdbt_layer = ISDBT_LAYER_ALL;
			log_message( log_module,  MSG_INFO,  "IDSBT tuning");


			cmdseq->props[commandnum].cmd      = DTV_DELIVERY_SYSTEM;
			cmdseq->props[commandnum++].u.data = tuneparams->delivery_system;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_DELIVERY_SYSTEM %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_FREQUENCY;
			cmdseq->props[commandnum++].u.data = feparams.frequency;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_FREQUENCY %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_ISDBT_PARTIAL_RECEPTION;
			cmdseq->props[commandnum++].u.data = isdbt_partial_reception;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_ISDBT_PARTIAL_RECEPTION %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_ISDBT_SOUND_BROADCASTING;
			cmdseq->props[commandnum++].u.data = 0;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_ISDBT_SOUND_BROADCASTING %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_ISDBT_LAYER_ENABLED;
			cmdseq->props[commandnum++].u.data = tuneparams->isdbt_layer;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_ISDBT_LAYER_ENABLED %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_BANDWIDTH_HZ;//https://www.linuxtv.org/downloads/v4l-dvb-apis-old/frontend-properties.html
			cmdseq->props[commandnum++].u.data = 6000000; //1) For ISDB-T it should be always 6000000Hz (6MHz)
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_BANDWIDTH_HZ %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_INVERSION;
			cmdseq->props[commandnum++].u.data = INVERSION_AUTO;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_INVERSION %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_GUARD_INTERVAL;
			cmdseq->props[commandnum++].u.data = GUARD_INTERVAL_AUTO;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_GUARD_INTERVAL %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_TRANSMISSION_MODE;
			cmdseq->props[commandnum++].u.data = TRANSMISSION_MODE_AUTO;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_TRANSMISSION_MODE %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_ISDBT_SB_SUBCHANNEL_ID;
			cmdseq->props[commandnum++].u.data = 0;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_ISDBT_SB_SUBCHANNEL_ID %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_ISDBT_SB_SEGMENT_IDX;
			cmdseq->props[commandnum++].u.data = 0;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_ISDBT_SB_SEGMENT_IDX %d ",cmdseq->props[commandnum-1].u.data);
			cmdseq->props[commandnum].cmd      = DTV_ISDBT_SB_SEGMENT_COUNT;
			cmdseq->props[commandnum++].u.data = 0;
			log_message( log_module,  MSG_DEBUG,  "IDSBT tuning DTV_ISDBT_SB_SEGMENT_COUNT %d ",cmdseq->props[commandnum-1].u.data);

			cmdseq->props[commandnum++].cmd    = DTV_TUNE;
		}
#endif
		else if((tuneparams->delivery_system==SYS_DVBC_ANNEX_AC)||(tuneparams->delivery_system==SYS_DVBC_ANNEX_B))
		{
			cmdseq->props[commandnum].cmd      = DTV_DELIVERY_SYSTEM;
			cmdseq->props[commandnum++].u.data = tuneparams->delivery_system;
			cmdseq->props[commandnum].cmd      = DTV_FREQUENCY;
			cmdseq->props[commandnum++].u.data = feparams.frequency;
			cmdseq->props[commandnum].cmd      = DTV_MODULATION;
			cmdseq->props[commandnum++].u.data = tuneparams->modulation;
			cmdseq->props[commandnum].cmd      = DTV_SYMBOL_RATE;
			cmdseq->props[commandnum++].u.data = tuneparams->srate;
			cmdseq->props[commandnum].cmd      = DTV_INVERSION;
			cmdseq->props[commandnum++].u.data = INVERSION_OFF;
			cmdseq->props[commandnum].cmd      = DTV_INNER_FEC;
			cmdseq->props[commandnum++].u.data = tuneparams->HP_CodeRate;
			cmdseq->props[commandnum++].cmd    = DTV_TUNE;
		}
		else if(tuneparams->delivery_system==SYS_ATSC)
		{
			cmdseq->props[commandnum].cmd      = DTV_DELIVERY_SYSTEM;
			cmdseq->props[commandnum++].u.data = tuneparams->delivery_system;
			cmdseq->props[commandnum].cmd      = DTV_FREQUENCY;
			cmdseq->props[commandnum++].u.data = feparams.frequency;
			cmdseq->props[commandnum].cmd      = DTV_MODULATION;
			cmdseq->props[commandnum++].u.data = tuneparams->modulation;
			cmdseq->props[commandnum++].cmd    = DTV_TUNE;
		}
		else
		{
			log_message( log_module,  MSG_ERROR, "Unsupported delivery system. Try tuning using DVB API 3 (do not set delivery_system). And please contact so it can be implemented.\n");
			set_interrupted(ERROR_TUNE<<8);
			free(cmdseq->props);
			free(cmdseq);
			return -1;
		}

		cmdseq->num = commandnum;
		if ((ioctl(fd_frontend, FE_SET_PROPERTY, &cmdclear)) == -1) {
			log_message( log_module,  MSG_ERROR,"FE_SET_PROPERTY clear failed : %s\n", strerror(errno));
			set_interrupted(ERROR_TUNE<<8);
			free(cmdseq->props);
			free(cmdseq);
			return -1;
		}

		if ((ioctl(fd_frontend, FE_SET_PROPERTY, cmdseq)) == -1) {
			log_message( log_module,  MSG_ERROR,"FE_SET_PROPERTY failed : %s\n", strerror(errno));
			set_interrupted(ERROR_TUNE<<8);
			free(cmdseq->props);
			free(cmdseq);
			return -1;
		}
		free(cmdseq->props);
		free(cmdseq);

		}
#endif
	return(check_status(fd_frontend,fe_info.type,lo_frequency,tuneparams->display_strenght));
#else
	return 0;
#endif
}
