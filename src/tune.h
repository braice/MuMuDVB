/* dvbtune - tune.c

   part of MuMuDVB

   last version availaible from http://mumudvb.net/

   Copyright (C) 2004-2010 Brice DUBOST
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
 * This file contains the definition of the parameters for tuning the card
 */


#ifndef _TUNE_H
#define _TUNE_H

#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>


/* DVB-S */
/** lnb_slof: switch frequency of LNB */
#define DEFAULT_SLOF (11700*1000UL)
/** lnb_lof1: local frequency of lower LNB band */
#define DEFAULT_LOF1_UNIVERSAL (9750*1000UL)
/** lnb_lof2: local frequency of upper LNB band */
#define DEFAULT_LOF2_UNIVERSAL (10600*1000UL)
/** Lnb standard Local oscillator frequency*/
#define DEFAULT_LOF_STANDARD (10750*1000UL)


/* DVB-T DVB-C */
/* default option : full auto except bandwith = 8MHz*/
/* AUTO settings */
#define BANDWIDTH_DEFAULT           BANDWIDTH_8_MHZ
#define HP_CODERATE_DEFAULT         FEC_AUTO
#define MODULATION_DEFAULT          QAM_AUTO
#define SAT_MODULATION_DEFAULT      QPSK
#define TRANSMISSION_MODE_DEFAULT   TRANSMISSION_MODE_AUTO
#define GUARD_INTERVAL_DEFAULT      GUARD_INTERVAL_AUTO
#define HIERARCHY_DEFAULT           HIERARCHY_NONE

#if HIERARCHY_DEFAULT == HIERARCHY_NONE && !defined (LP_CODERATE_DEFAULT)
#define LP_CODERATE_DEFAULT (FEC_NONE) /* unused if HIERARCHY_NONE */
#endif

/* ATSC */
#define ATSC_MODULATION_DEFAULT     VSB_8

/* ISDB-T */


/* The lnb type*/
#define LNB_UNIVERSAL 0
#define LNB_STANDARD 1

#if DVB_API_VERSION >= 5
#define MAX_CMDSEQ_PROPS_NUM 12
#endif


/*Do we support stream_id ?*/
#undef STREAM_ID
#if defined(DVB_API_VERSION_MINOR)
#if DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 8
#define STREAM_ID 1
#endif
#endif
#if DVB_API_VERSION >= 6
#define STREAM_ID 1
#endif

#undef ISDBT
#if defined(DVB_API_VERSION_MINOR)
#if DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR >= 1
#define ISDBT 1
#endif
#endif
#if DVB_API_VERSION >= 6
#define ISDBT 1
#endif


typedef enum pls_type {
	PLS_ROOT		= 0x00,
	PLS_GOLD		= 0x01,
	PLS_COMMON		= 0x02,
} pls_type_t;


#define ISDBT_LAYER_ALL 7
#define ISDBT_LAYER_A 1
#define ISDBT_LAYER_B 2
#define ISDBT_LAYER_C 4


/** @brief Parameters for tuning the card*/
typedef struct tune_p_t{
  /**The card number*/
  int card;
  /**The tuner number*/
  int tuner;
  /** The path of the card */
  char card_dev_path[256];
  /**Is the card actually tuned ?*/
  int card_tuned;
  /**The timeout for tuninh the card*/
  int tuning_timeout;
  /** the frequency (in MHz for dvb-s in kHz, MHz or Hz for all others) */
  double freq;
  /** The symbol rate (QPSK and QAM modulation ie cable and satellite) in symbols per second*/
  unsigned int srate;
  /**The polarisation H, V, L or R (for satellite)*/
  char pol;
  /**The lnb type : universal (two local oscilator frequencies), standard (one)*/
  int lnb_type;
  /**The lo frequency (in kHz) for single LO LNB*/
  uint32_t lnb_lof_standard;
  /**The lo switch frequency for dual LO LNB*/
  uint32_t lnb_slof;
  /**The low LO frequency (in kHz) for dual LO LNB*/
  uint32_t lnb_lof_low;
  /**The HIGH LO frequency (in kHz) for dual LO LNB*/
  uint32_t lnb_lof_high;
  /** Do we force the lnb voltage to be 0 ? (in case the LNB have it's own power supply (satellite only))*/
  int lnb_voltage_off;
  /**The satellite number ie the LNB number*/
  unsigned char sat_number;
  /** The switch input */
  int switch_no;
  /** The pin no input */
  int pin_no;
  /**The type of switch U uncommitted C committed*/
  char switch_type;
  /** Do we repeat DiseQC messages ? */
  int diseqc_repeat;
  /** Wait (ms) for DiseQC messages ? */
  int diseqc_time;
  /** The frequency for SCR/unicable */
  uint32_t uni_freq;
  /** The kind of modulation */
  fe_modulation_t modulation;
  int modulation_set;
  /** high priority stream code rate ie error correction, FEC */
  fe_code_rate_t HP_CodeRate;
  /** low priority stream code rate 
   * In order to achieve hierarchy, two different code rates may be applied to two different levels of the modulation.*/ 
  fe_code_rate_t LP_CodeRate;
  /** For DVB-T */
  fe_transmit_mode_t TransmissionMode;
  /** For DVB-T */
  fe_guard_interval_t guardInterval;
  /**For DVB-T : the bandwith (often 8MHz)*/
  fe_bandwidth_t bandwidth;
  /**For DVB-T 
   It seems that it's related to the capability to transmit information in parallel
  Not configurable for the moment, I have found no transponder to test it*/
  fe_hierarchy_t hier;
  /** do we periodically display the strenght of the signal ?*/
  int display_strenght;
  /** do we periodically check the status of the card ?*/
  int check_status;
  /**shutdown the thread for display strength */
  volatile int strengththreadshutdown;
  /**The frontend type*/
  fe_type_t fe_type;
  /** The frontend name */
  char fe_name[256];
#if DVB_API_VERSION >= 5
  /** DVB API version 5*/
  /**Delivery system : DVB-S DVB-T etc ...*/
  fe_delivery_system_t delivery_system;
  /** Rolloff (For DVB-S and DVB-S2)*/
  fe_rolloff_t rolloff;
#endif
#if STREAM_ID
  /** The substream id */
  int stream_id;
  /** The PLS code */
  int pls_code;
  /** The PLS type */
  pls_type_t pls_type;
#endif
  /** If we read directly from a file */
  char read_file_path[256];
#if ISDBT
  //ISDB T
  /** ISDBT */
  int isdbt_partial_reception;
  /** ISDBT */
  int isdbt_sound_broadcasting;
  /** ISDBT */
  int isdbt_sb_subchanel_id;
  /** ISDBT */
  int isdbt_layer;
#endif
  /** Spectral inversion */
  fe_spectral_inversion_t inversion;

}tune_p_t;





void init_tune_v(tune_p_t *);
int tune_it(int, tune_p_t *);
int read_tuning_configuration(tune_p_t *, char *);
void print_status(fe_status_t festatus);




#endif
