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
 * This file contains the definition of the parameters for tuning the card
 */


#ifndef _TUNE_H
#define _TUNE_H

#include <linux/dvb/frontend.h>

/* DVB-S */
/** lnb_slof: switch frequency of LNB */
#define DEFAULT_SLOF (11700*1000UL)
/** lnb_lof1: local frequency of lower LNB band */
#define DEFAULT_LOF1_UNIVERSAL (9750*1000UL)
/** lnb_lof2: local frequency of upper LNB band */
#define DEFAULT_LOF2_UNIVERSAL (10600*1000UL)
/** Lnb standard Local oscillator frequency*/
#define DEFAULT_LOF_STANDARD (10750*1000UL)


/* DVB-T */
/* default option : full auto except bandwith = 8MHz*/
/* AUTO settings */
#define BANDWIDTH_DEFAULT           BANDWIDTH_8_MHZ
#define HP_CODERATE_DEFAULT         FEC_AUTO
#define MODULATION_DEFAULT          QAM_AUTO
#define TRANSMISSION_MODE_DEFAULT   TRANSMISSION_MODE_AUTO
#define GUARD_INTERVAL_DEFAULT      GUARD_INTERVAL_AUTO
#define HIERARCHY_DEFAULT           HIERARCHY_NONE

#if HIERARCHY_DEFAULT == HIERARCHY_NONE && !defined (LP_CODERATE_DEFAULT)
#define LP_CODERATE_DEFAULT (FEC_NONE) /* unused if HIERARCHY_NONE */
#endif


/* The lnb type*/
#define LNB_UNIVERSAL 0
#define LNB_STANDARD 1


/** @brief Parameters for tuning the card*/
typedef struct tuning_parameters_t{
  /**The card number*/
  int card;
  /**Is the card actually tuned ?*/
  int card_tuned;
  /**Don't tune flag to skip the tuning part*/
  int dont_tune;
  /**The timeout for tuninh the card*/
  int tuning_timeout;
  /** the frequency (in MHz for dvb-s in kHz for dvb-t) */
  uint32_t freq;
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
  //int tone;
  /**spectral inversion. AUTO seems to work with all the hardware
     @todo : catch more information about this*/
  fe_spectral_inversion_t specInv;
  /**The satellite number ie the LNB number*/
  unsigned char sat_number;
  /** quadrature modulation 
      For cable and terrestrial frontends (QAM and OFDM) */
  fe_modulation_t modulation;
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
  /** The modulation for ATSC cards */
  fe_modulation_t atsc_modulation;
  /**The frontend type*/
  fe_type_t fe_type;
}tuning_parameters_t;



int tune_it(int, tuning_parameters_t *);

#endif
